// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UnrealToUnityExporter.generated.h"

struct FExportSettings;

USTRUCT()
struct FUnrealToUnityExporterMaterialDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FString MaterialPath;
	
	UPROPERTY()
	TArray<FName> TextureNames;
	
	UPROPERTY()
	TArray<FString> TexturePaths;
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

class FUnrealToUnityExporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static void RunUnrealToUnityExporter();
	static void BakeOutStaticMeshes(const TArrayView<UStaticMesh*> StaticMeshes, TMap<FName, UMaterialInterface*>& OriginalPathsToMaterial, const FExportSettings& ExportSettings);
	static void ExportMeshes(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory, FUnrealToUnityExporterImportDescriptor& ImportDescriptor, const FExportSettings& ExportSettings);
	static void ExportMaterials(const TMap<FName, UMaterialInterface*>& OriginalPathsToMaterial, const FString& ExportDirectory, FUnrealToUnityExporterImportDescriptor& ImportDescriptor);
	static void ExportTextures(const UMaterialInterface& MaterialInterface, const FString& ExportDirectory, const FString& ExportFolder, FUnrealToUnityExporterMaterialDescriptor& MaterialDescriptor);
	static void RevertChanges(const TArrayView<UStaticMesh*> StaticMeshes, const TArrayView<UMaterialInterface*> MaterialInterfaces);
	static FString SaveImportDescriptor(const FUnrealToUnityExporterImportDescriptor& ImportDescriptor, const FString& ExportDirectory);
	static void SendUnityImportMessage(const FString& ImportDescriptorSavePath);
};
