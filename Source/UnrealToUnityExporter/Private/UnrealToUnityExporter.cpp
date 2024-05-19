// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealToUnityExporter.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ImageUtils.h"
#include "IMeshMergeUtilities.h"
#include "JsonObjectConverter.h"
#include "MaterialOptions.h"
#include "MeshMergeModule.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "SExportSettingsWindow.h"
#include "Sockets.h"
#include "ToolMenus.h"
#include "UnrealToUnityExporterStaticMeshAdapter.h"
#include "Common/TcpSocketBuilder.h"

#define LOCTEXT_NAMESPACE "FUnrealToUnityExporterModule"

void FUnrealToUnityExporterModule::StartupModule()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
	Menu->AddMenuEntry(TEXT("UnrealToUnityExporterSection"),
	FToolMenuEntry::InitMenuEntry(TEXT("UnrealToUnityExporterEntry"), LOCTEXT("UnrealToUnityExporterEntryLabel", "Unreal to Unity Exporter"), FText::GetEmpty(), FSlateIcon(),
	FUIAction(
		FExecuteAction::CreateStatic(&FUnrealToUnityExporterModule::RunUnrealToUnityExporter),
		FCanExecuteAction()
	)));
}

void FUnrealToUnityExporterModule::ShutdownModule()
{
	
}

void FUnrealToUnityExporterModule::RunUnrealToUnityExporter()
{
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UStaticMesh*> StaticMeshes;
	Algo::TransformIf(SelectedAssets, StaticMeshes, [] (const FAssetData& AssetData)
	{
		return AssetData.GetAsset()->IsA<UStaticMesh>();
	}, [] (const FAssetData& AssetData)
	{
		return Cast<UStaticMesh>(AssetData.GetAsset());
	});

	TSharedRef<SExportSettingsWindow> ExportSettingsWindow = SNew(SExportSettingsWindow);
	FSlateApplication::Get().AddModalWindow(ExportSettingsWindow, nullptr);

	const FExportSettings& ExportSettings = ExportSettingsWindow->GetExportSettings();
	
	if (ExportSettings.bCanceled)
	{
		return;
	}

	const FString RelativeExportDirectory = FPaths::ProjectSavedDir() / TEXT("UnrealToUnityExporter");
	const FString ExportDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*RelativeExportDirectory);

	FUnrealToUnityExporterImportDescriptor ImportDescriptor;
	ImportDescriptor.ExportDirectory = ExportDirectory;

	FScopedSlowTask SlowTask(5, LOCTEXT("BakeOutStaticMeshesSlowTask", "Baking out meshes"));
	SlowTask.MakeDialog();

	TMap<FName, UMaterialInterface*> OriginalPathsToMaterial;
	
	SlowTask.EnterProgressFrame(1.f);
	BakeOutStaticMeshes(StaticMeshes, OriginalPathsToMaterial, ExportSettings);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("ExportMeshesSlowTask", "Exporting meshes"));
	ExportMeshes(StaticMeshes, ExportDirectory, ImportDescriptor, ExportSettings);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("ExportMaterialsSlowTask", "Exporting materials"));
	ExportMaterials(OriginalPathsToMaterial, ExportDirectory, ImportDescriptor);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("RevertMeshesSlowTask", "Reverting mesh changes"));
	TArray<UMaterialInterface*> GeneratedMaterials;
	OriginalPathsToMaterial.GenerateValueArray(GeneratedMaterials);
	RevertChanges(StaticMeshes, GeneratedMaterials);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("SaveImportDescriptorSlowTask", "Saving mesh import descriptor"));
	const FString ImportDescriptorSavePath = SaveImportDescriptor(ImportDescriptor, ExportDirectory);

	SendUnityImportMessage(ImportDescriptorSavePath);
}

void FUnrealToUnityExporterModule::BakeOutStaticMeshes(const TArrayView<UStaticMesh*> StaticMeshes, TMap<FName, UMaterialInterface*>& OriginalPathsToMaterial, const FExportSettings& ExportSettings)
{
	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	
	FScopedTransaction Transaction(LOCTEXT("UnrealToUnityExporterDummyTransactionName", "Unreal to Unity Exporter Dummy Transaction"));
	
	for (UStaticMesh* StaticMesh : StaticMeshes)
	{
		UMaterialOptions* MaterialOptions = DuplicateObject(GetMutableDefault<UMaterialOptions>(), GetTransientPackage());
		UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
		UMaterialMergeOptions* MergeOptions = GetMutableDefault<UMaterialMergeOptions>();
		TArray<TWeakObjectPtr<>> Objects{ MergeOptions, AssetOptions, MaterialOptions };

		for (int32 LodIndex = 0; LodIndex < StaticMesh->GetNumLODs(); LodIndex++)
		{
			MaterialOptions->LODIndices.Add(LodIndex);	
		}
		
		MaterialOptions->TextureSize = FIntPoint(ExportSettings.TextureSize, ExportSettings.TextureSize);
		MaterialOptions->Properties.Empty();
			
		MaterialOptions->Properties.Emplace(MP_BaseColor);
		MaterialOptions->Properties.Emplace(MP_Metallic);
		MaterialOptions->Properties.Emplace(MP_Specular);
		MaterialOptions->Properties.Emplace(MP_Roughness);
		MaterialOptions->Properties.Emplace(MP_Normal);
		MaterialOptions->Properties.Emplace(MP_Opacity);
		MaterialOptions->Properties.Emplace(MP_EmissiveColor);

		TMap<int32, FName> LodSectionHashToMaterialPath;

		auto GetHashFromLodSection = [](int32 LodIndex, int32 SectionIndex) -> int32
		{
			return LodIndex * 10000 + SectionIndex;
		};

		{
			const int32 LodCount = StaticMesh->GetNumLODs();
			
			for (int32 LodIndex = 0; LodIndex < LodCount; LodIndex++)
			{
				const int32 SectionCount = StaticMesh->GetNumSections(LodIndex);

				for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
				{
					const FMeshSectionInfo& SectionInfo = StaticMesh->GetSectionInfoMap().Get(LodIndex, SectionIndex);
					const int32 MaterialIndex = SectionInfo.MaterialIndex;
					const FName MaterialPath = StaticMesh->GetMaterial(MaterialIndex)->GetPackage()->GetFName();
					LodSectionHashToMaterialPath.Add(GetHashFromLodSection(LodIndex, SectionIndex), MaterialPath);
				}
			}
		}
		
		// Bake out materials for static mesh asset
		StaticMesh->Modify();
		FUnrealToUnityExporterStaticMeshAdapter Adapter(StaticMesh);
		MeshMergeUtilities.BakeMaterialsForComponent(Objects, &Adapter);

		{
			TSet<int32> ProcessedMaterialIndices;
			TArray<FStaticMaterial> StaticMaterials = StaticMesh->GetStaticMaterials();
			const int32 LodCount = StaticMesh->GetNumLODs();
			
			for (int32 LodIndex = 0; LodIndex < LodCount; LodIndex++)
			{
				const int32 SectionCount = StaticMesh->GetNumSections(LodIndex);

				for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
				{
					const FMeshSectionInfo& SectionInfo = StaticMesh->GetSectionInfoMap().Get(LodIndex, SectionIndex);
					const int32 MaterialIndex = SectionInfo.MaterialIndex;

					if (ProcessedMaterialIndices.Contains(MaterialIndex))
					{
						continue;
					}
					
					const int32 LodSectionHash = GetHashFromLodSection(LodIndex, SectionIndex);
					const FName& OriginalMaterialPath = LodSectionHashToMaterialPath[LodSectionHash];
					const FString OriginalMaterialPathStr = OriginalMaterialPath.ToString();
					const FString OriginalMaterialName = FPaths::GetCleanFilename(OriginalMaterialPathStr);
					const FString MaterialName = FString::Printf(TEXT("%s_%s"), *OriginalMaterialName, *FMD5::HashAnsiString(*OriginalMaterialPathStr));
					StaticMaterials[MaterialIndex].MaterialInterface = DuplicateObject(StaticMaterials[MaterialIndex].MaterialInterface, nullptr, FName(MaterialName));

					ProcessedMaterialIndices.Add(MaterialIndex);
					OriginalPathsToMaterial.Add(OriginalMaterialPath, StaticMaterials[MaterialIndex].MaterialInterface);
				}
			}

			StaticMesh->SetStaticMaterials(StaticMaterials);
		}
	}
}

void FUnrealToUnityExporterModule::ExportMeshes(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory, FUnrealToUnityExporterImportDescriptor& ImportDescriptor, const FExportSettings& ExportSettings)
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UObject*> Objects;
	Algo::Transform(StaticMeshes, Objects, [] (UStaticMesh* StaticMesh)
	{
		return StaticMesh;
	});

	const FString ExportFolder = TEXT("Models");
	const FString MeshExportDirectory = ExportDirectory / ExportFolder;
	
	AssetToolsModule.Get().ExportAssets(Objects, MeshExportDirectory);

	for (const UStaticMesh* StaticMesh : StaticMeshes)
	{
		FUnrealToUnityExporterMeshDescriptor MeshDescriptor;
		MeshDescriptor.MeshPath = ExportFolder / StaticMesh->GetPackage()->GetPathName() + TEXT(".fbx");
		MeshDescriptor.bEnableReadWrite = ExportSettings.bEnableReadWrite;
		ImportDescriptor.MeshDescriptors.Add(MoveTemp(MeshDescriptor));
	}
}

void FUnrealToUnityExporterModule::ExportMaterials(const TMap<FName, UMaterialInterface*>& OriginalPathsToMaterial, const FString& ExportDirectory, FUnrealToUnityExporterImportDescriptor& ImportDescriptor)
{
	for (const auto& [OriginalPath, MaterialInterface] : OriginalPathsToMaterial)
	{
		FUnrealToUnityExporterMaterialDescriptor MaterialDescriptor;
		const FString OriginalPathStr = FPaths::GetPath(OriginalPath.ToString()) / MaterialInterface->GetName();
		MaterialDescriptor.MaterialPath = TEXT("Materials") / OriginalPathStr;
		const FString ExportFolder = TEXT("Textures");
		ExportTextures(*MaterialInterface, ExportDirectory, ExportFolder / OriginalPathStr, MaterialDescriptor);

		ImportDescriptor.MaterialDescriptors.Add(MoveTemp(MaterialDescriptor));
	}
}

void FUnrealToUnityExporterModule::ExportTextures(const UMaterialInterface& MaterialInterface, const FString& ExportDirectory, const FString& ExportFolder, FUnrealToUnityExporterMaterialDescriptor& MaterialDescriptor)
{
	TArray<FMaterialParameterInfo> OutParameterInfo;
	TArray<FGuid> OutParameterIds;
	MaterialInterface.GetAllTextureParameterInfo(OutParameterInfo, OutParameterIds);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	for (const FMaterialParameterInfo& ParameterInfo : OutParameterInfo)
	{
		UTexture* Texture;

		if (MaterialInterface.GetTextureParameterValue(ParameterInfo, Texture, true /*bOveriddenOnly*/))
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
			{
				FImage OutImage;
				Texture2D->Source.GetMipImage(OutImage, 0);
				const FString TexturePath = ExportFolder / ParameterInfo.Name.ToString() + TEXT(".png");
				const FString ExportPath = ExportDirectory / TexturePath;
				
				if (FPaths::FileExists(ExportPath))
				{
					PlatformFile.DeleteFile(*ExportPath);
				}
				
				FImageUtils::SaveImageByExtension(*ExportPath, OutImage);

				MaterialDescriptor.TextureNames.Add(ParameterInfo.Name);
				MaterialDescriptor.TexturePaths.Add(TexturePath);
			}
		}
	}
}

void FUnrealToUnityExporterModule::RevertChanges(const TArrayView<UStaticMesh*> StaticMeshes, const TArrayView<UMaterialInterface*> MaterialInterfaces)
{
	GEditor->UndoTransaction(false /*bCanRedo*/);

	TArray<UPackage*> PackagesToReload;
	Algo::Transform(StaticMeshes, PackagesToReload, [] (const UStaticMesh* StaticMesh)
	{
		return StaticMesh->GetPackage();
	});
	
	UPackageTools::ReloadPackages(PackagesToReload);
}

FString FUnrealToUnityExporterModule::SaveImportDescriptor(const FUnrealToUnityExporterImportDescriptor& ImportDescriptor, const FString& ExportDirectory)
{
	TSharedRef<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(ImportDescriptor).ToSharedRef();
	FString JsonString;
	const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter, true);
	const FString SavePath = ExportDirectory / TEXT("ImportDescriptor.txt");
	FFileHelper::SaveStringToFile(JsonString, *SavePath);

	return SavePath;
}

void FUnrealToUnityExporterModule::SendUnityImportMessage(const FString& ImportDescriptorSavePath)
{
	const FIPv4Endpoint ClientEndpoint(FIPv4Address(127, 0, 0, 1), 55720);
	
	FSocket* Socket = FTcpSocketBuilder(TEXT("UnrealToUnityMeshExporterClient")).AsBlocking();

	if (!Socket->Connect(ClientEndpoint.ToInternetAddr().Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Socket couldn't connect"));
		return;
	}
	
	FString Message = TEXT("unrealToUnityImporter?");

	if (!ImportDescriptorSavePath.IsEmpty())
	{
		Message += TEXT("ImportDescriptorPath=");
		Message += ImportDescriptorSavePath;
	}

	int32 Utf8Length = FTCHARToUTF8_Convert::ConvertedLength(*Message, Message.Len());
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(Utf8Length);
	FTCHARToUTF8_Convert::Convert((UTF8CHAR*)Buffer.GetData(), Buffer.Num(), *Message, Message.Len());
	
	int32 MessageBufferSize;
	Socket->Send(Buffer.GetData(), Buffer.Num(), MessageBufferSize);

	Socket->Shutdown(ESocketShutdownMode::ReadWrite);
	Socket->Close();

	ISocketSubsystem& SocketSubsystem = *(ISocketSubsystem::Get());
	SocketSubsystem.DestroySocket(Socket);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealToUnityExporterModule, UnrealToUnityExporter)