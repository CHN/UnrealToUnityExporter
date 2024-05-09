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
#include "Sockets.h"
#include "ToolMenus.h"
#include "UnrealToUnityExporterStaticMeshAdapter.h"
#include "Algo/RemoveIf.h"
#include "Common/TcpSocketBuilder.h"

#define LOCTEXT_NAMESPACE "FUnrealToUnityExporterModule"

namespace
{
	const TArray<FName> TextureMaterialParameterNames =
	{
		TEXT("BaseColorTexture"),
		TEXT("MetallicTexture"),
		TEXT("NormalTexture"),
		TEXT("RoughnessTexture"),
		TEXT("EmissiveTexture"),
	};
}

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

	const FString RelativeExportDirectory = FPaths::ProjectSavedDir() / TEXT("UnrealToUnityExporter");
	const FString ExportDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*RelativeExportDirectory);

	FUnrealToUnityExporterMeshImportDescriptor MeshImportDescriptor;
	MeshImportDescriptor.ExportDirectory = ExportDirectory;

	FScopedSlowTask SlowTask(5, LOCTEXT("BakeOutStaticMeshesSlowTask", "Baking out meshes"));
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame(1.f);
	BakeOutStaticMeshes(StaticMeshes);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("SetMaterialNamesSlowTask", "Setting material names"));
	SetMaterialNames(StaticMeshes);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("ExportMeshesSlowTask", "Exporting meshes"));
	ExportMeshes(StaticMeshes, ExportDirectory);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("ExportTexturesSlowTask", "Exporting textures"));
	ExportTextures(StaticMeshes, ExportDirectory, MeshImportDescriptor);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("RevertMeshesSlowTask", "Reverting mesh changes"));
	RevertStaticMeshChanges(StaticMeshes);

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("SaveMeshImportDescriptorSlowTask", "Saving mesh import descriptor"));
	const FString MeshImportDescriptorSavePath = SaveMeshImportDescriptor(MeshImportDescriptor, ExportDirectory);

	SendUnityImportMessage(MeshImportDescriptorSavePath);
}

void FUnrealToUnityExporterModule::BakeOutStaticMeshes(const TArrayView<UStaticMesh*> StaticMeshes)
{
	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	
	FScopedTransaction Transaction(LOCTEXT("UnrealToUnityExporterDummyTransactionName", "Unreal to Unity Exporter Dummy Transaction"));
	
	for (UStaticMesh* StaticMesh : StaticMeshes)
	{
		UMaterialOptions* MaterialOptions = DuplicateObject(GetMutableDefault<UMaterialOptions>(), GetTransientPackage());
		UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
		UMaterialMergeOptions* MergeOptions = GetMutableDefault<UMaterialMergeOptions>();
		TArray<TWeakObjectPtr<>> Objects{ MergeOptions, AssetOptions, MaterialOptions };
			
		MaterialOptions->LODIndices = { 0 };
		MaterialOptions->TextureSize = FIntPoint(2048, 2048);
		MaterialOptions->Properties.Empty();
			
		MaterialOptions->Properties.Emplace(MP_BaseColor);
		MaterialOptions->Properties.Emplace(MP_Metallic);
		MaterialOptions->Properties.Emplace(MP_Specular);
		MaterialOptions->Properties.Emplace(MP_Roughness);
		MaterialOptions->Properties.Emplace(MP_Normal);
		MaterialOptions->Properties.Emplace(MP_Opacity);
		MaterialOptions->Properties.Emplace(MP_EmissiveColor);
			
		// Bake out materials for static mesh asset
		StaticMesh->Modify();
		FUnrealToUnityExporterStaticMeshAdapter Adapter(StaticMesh);
		MeshMergeUtilities.BakeMaterialsForComponent(Objects, &Adapter);
	}
}

void FUnrealToUnityExporterModule::SetMaterialNames(const TArrayView<UStaticMesh*> StaticMeshes)
{
	for (UStaticMesh* StaticMesh : StaticMeshes)
	{
		TArray<FStaticMaterial> StaticMaterials = StaticMesh->GetStaticMaterials();

		constexpr int32 TargetLod = 0;
		const int32 SectionCount = StaticMesh->GetNumSections(TargetLod);
		TArray<int32> UsedMaterialIndices;

		for (int32 SectionIndex = 0; SectionIndex < SectionCount; SectionIndex++)
		{
			UsedMaterialIndices.AddUnique(StaticMesh->GetSectionInfoMap().Get(TargetLod, SectionIndex).MaterialIndex);
		}

		int32 RemovedMaterialCount = 0;
		
		for (int32 Index = 0; Index < StaticMaterials.Num(); Index++)
		{
			if (!UsedMaterialIndices.Contains(Index + RemovedMaterialCount))
			{
				StaticMaterials.RemoveAt(Index);
				RemovedMaterialCount++;
				Index--;
			}
		}
		
		const FString StaticMeshName = StaticMesh->GetName();

		int32 MaterialIndex = 0;
		
		for (FStaticMaterial& StaticMaterial : StaticMaterials)
		{
			StaticMaterial.MaterialInterface = DuplicateObject(StaticMaterial.MaterialInterface, nullptr, FName(FString::Printf(TEXT("%s_%d"), *StaticMeshName, MaterialIndex++)));
		}

		StaticMesh->SetStaticMaterials(StaticMaterials);
	}
}

void FUnrealToUnityExporterModule::ExportMeshes(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory)
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	TArray<UObject*> Objects;
	Algo::Transform(StaticMeshes, Objects, [] (UStaticMesh* StaticMesh)
	{
		return StaticMesh;
	});

	AssetToolsModule.Get().ExportAssets(Objects, GetStaticMeshExportDirectory(ExportDirectory));
}

void FUnrealToUnityExporterModule::ExportTextures(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory, FUnrealToUnityExporterMeshImportDescriptor& MeshImportDescriptor)
{
	for (const UStaticMesh* StaticMesh : StaticMeshes)
	{
		TArray<FUnrealToUnityExporterMaterialDescriptor> MaterialDescriptors;

		const TArray<FStaticMaterial> StaticMaterials = StaticMesh->GetStaticMaterials();
		
		for (const FStaticMaterial& StaticMaterial : StaticMaterials)
		{
			const FString TexturesExportDirectory = ExportDirectory / TEXT("Textures") / ConvertPathNameToDiskPathFormat(StaticMesh->GetPathName()) / StaticMaterial.MaterialInterface->GetName();
			
			FUnrealToUnityExporterMaterialDescriptor MaterialDescriptor;
			MaterialDescriptor.MaterialName = StaticMaterial.MaterialInterface->GetFName();
			UE_LOG(LogTemp, Error, TEXT("Material index isn't valid. Index: %d"), StaticMesh->GetRenderData()->LODResources[0].Sections.Num());  
			
			for (const FName& TextureMaterialParameterName : TextureMaterialParameterNames)
			{
				FHashedMaterialParameterInfo HashedMaterialParameterInfo;
				HashedMaterialParameterInfo.Name = FScriptName(TextureMaterialParameterName);
				HashedMaterialParameterInfo.Association = GlobalParameter;
			
				UTexture* Texture;

				if (StaticMaterial.MaterialInterface->GetTextureParameterValue(HashedMaterialParameterInfo, Texture, true /*bOveriddenOnly*/))
				{
					if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
					{
						FImage OutImage;
						Texture2D->Source.GetMipImage(OutImage, 0);
						const FString TexturePath = (TexturesExportDirectory / TextureMaterialParameterName.ToString() + TEXT(".png"));
						FImageUtils::SaveImageByExtension(*TexturePath, OutImage);

						MaterialDescriptor.TextureNames.Add(TextureMaterialParameterName);
						MaterialDescriptor.TexturePaths.Add(TexturePath);
					}
				}
			}

			if (!MaterialDescriptor.TextureNames.IsEmpty())
			{
				MaterialDescriptors.Add(MoveTemp(MaterialDescriptor));
			}
		}

		if (!MaterialDescriptors.IsEmpty())
		{
			const FString StaticMeshExportDirectory = GetStaticMeshPathOnDisk(ExportDirectory, StaticMesh->GetPathName());
			MeshImportDescriptor.MeshToMaterialDescriptors.Emplace(StaticMeshExportDirectory, MoveTemp(MaterialDescriptors));
		}
	}
}

void FUnrealToUnityExporterModule::RevertStaticMeshChanges(const TArrayView<UStaticMesh*> StaticMeshes)
{
	GEditor->UndoTransaction(false /*bCanRedo*/);

	TArray<UPackage*> PackagesToReload;
	Algo::Transform(StaticMeshes, PackagesToReload, [] (UStaticMesh* StaticMesh)
	{
		return StaticMesh->GetPackage();
	});
	
	UPackageTools::ReloadPackages(PackagesToReload);
}

FString FUnrealToUnityExporterModule::SaveMeshImportDescriptor(const FUnrealToUnityExporterMeshImportDescriptor& MeshImportDescriptor, const FString& ExportDirectory)
{
	TSharedRef<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(MeshImportDescriptor).ToSharedRef();
	FString JsonString;
	const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter, true);
	const FString SavePath = ExportDirectory / TEXT("MeshImportDescriptor.txt");
	FFileHelper::SaveStringToFile(JsonString, *SavePath);

	return SavePath;
}

void FUnrealToUnityExporterModule::SendUnityImportMessage(const FString& MeshImportDescriptorSavePath)
{
	const FIPv4Endpoint ClientEndpoint(FIPv4Address(127, 0, 0, 1), 55720);
	
	FSocket* Socket = FTcpSocketBuilder(TEXT("UnrealToUnityMeshExporterClient")).AsBlocking();

	if (!Socket->Connect(ClientEndpoint.ToInternetAddr().Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Socket couldn't connect"));
		return;
	}
	
	FString Message = TEXT("unrealToUnityImporter?");

	if (!MeshImportDescriptorSavePath.IsEmpty())
	{
		Message += TEXT("meshImportDescriptorPath=");
		Message += MeshImportDescriptorSavePath;
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

FString FUnrealToUnityExporterModule::ConvertPathNameToDiskPathFormat(const FString& PathName)
{
	return FPaths::GetPath(PathName) / FPaths::GetBaseFilename(PathName);
}

FString FUnrealToUnityExporterModule::GetStaticMeshExportDirectory(const FString& ExportDirectory)
{
	return ExportDirectory / TEXT("Models");
}

FString FUnrealToUnityExporterModule::GetStaticMeshPathOnDisk(const FString& ExportDirectory, const FString& StaticMeshPathName)
{
	return GetStaticMeshExportDirectory(ExportDirectory) / ConvertPathNameToDiskPathFormat(StaticMeshPathName) + TEXT(".fbx");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealToUnityExporterModule, UnrealToUnityExporter)