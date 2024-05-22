// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UnrealToUnityExporter.generated.h"

struct FExportSettings;

USTRUCT()
struct FUnrealToUnityExporterTextureDescriptor
{
	GENERATED_BODY()
	
	UPROPERTY()
	bool bUseTexture = false;

	UPROPERTY()
	bool bUseColor = false;
	
	UPROPERTY()
	bool bUseScalar = false;

	UPROPERTY()
	FLinearColor Color = FLinearColor::Black;

	UPROPERTY()
	float Scalar = 0.f;
	
	UPROPERTY()
	FString ParameterName;
	
	UPROPERTY()
	FString TexturePath;
};

USTRUCT()
struct FUnrealToUnityExporterMaterialDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FString MaterialPath;

	UPROPERTY()
	int32 BlendMode = 0;
	
	UPROPERTY()
	TArray<FUnrealToUnityExporterTextureDescriptor> TextureDescriptors;

	// TODO: EmissiveScale
	// TODO: AO
};

USTRUCT()
struct FUnrealToUnityExporterMeshDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FString MeshPath;

	UPROPERTY()
	bool bEnableReadWrite = false;
};

USTRUCT()
struct FUnrealToUnityExporterImportDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FString ExportDirectory;

	UPROPERTY()
	TArray<FUnrealToUnityExporterMaterialDescriptor> MaterialDescriptors;
	
	UPROPERTY()
	TArray<FUnrealToUnityExporterMeshDescriptor> MeshDescriptors;
};

USTRUCT()
struct FUnrealToUnityExporterMaterialData
{
	GENERATED_BODY()

	FName OriginalMaterialName;
	
	UPROPERTY()
	TObjectPtr<UMaterialInterface> BakedMaterialInterface = nullptr;

	TEnumAsByte<EBlendMode> OriginalBlendMode = BLEND_Opaque;
};

class FUnrealToUnityExporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static void OpenExportSettingsWindow();
	static void RunUnrealToUnityExporter(const FExportSettings& ExportSettings );
	static void BakeOutStaticMeshes(const TArrayView<UStaticMesh*> StaticMeshes, TMap<FName, FUnrealToUnityExporterMaterialData>& OriginalPathsToMaterialData, const FExportSettings& ExportSettings);
	static void ExportMeshes(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory, FUnrealToUnityExporterImportDescriptor& ImportDescriptor, const FExportSettings& ExportSettings);
	static void ExportMaterials(const TMap<FName, FUnrealToUnityExporterMaterialData>& OriginalPathsToMaterialData, const FString& ExportDirectory, FUnrealToUnityExporterImportDescriptor& ImportDescriptor);
	static void ExportTextures(const UMaterialInterface& MaterialInterface, const FString& ExportDirectory, const FString& ExportFolder, FUnrealToUnityExporterMaterialDescriptor& MaterialDescriptor);
	static void RevertChanges(const TArrayView<UStaticMesh*> StaticMeshes, const TArrayView<UMaterialInterface*> MaterialInterfaces);
	static FString SaveImportDescriptor(const FUnrealToUnityExporterImportDescriptor& ImportDescriptor, const FString& ExportDirectory);
	static void SendUnityImportMessage(const FString& ImportDescriptorSavePath);
};
