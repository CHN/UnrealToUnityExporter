// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UnrealToUnityExporter.generated.h"

USTRUCT()
struct FUnrealToUnityExporterMaterialDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FName MaterialName;
	
	UPROPERTY()
	TArray<FName> TextureNames;
	
	UPROPERTY()
	TArray<FString> TexturePaths;
};

USTRUCT()
struct FUnrealToUnityExporterMaterialDescriptors
{
	GENERATED_BODY()

	UPROPERTY()
	FString MeshPath;
	
	UPROPERTY()
	TArray<FUnrealToUnityExporterMaterialDescriptor> MaterialDescriptors;
};

USTRUCT()
struct FUnrealToUnityExporterMeshImportDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	FString ExportDirectory;
	
	UPROPERTY()
	TArray<FUnrealToUnityExporterMaterialDescriptors> MeshToMaterialDescriptors;
};

class FUnrealToUnityExporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static void RunUnrealToUnityExporter();
	static void BakeOutStaticMeshes(const TArrayView<UStaticMesh*> StaticMeshes);
	static void SetMaterialNames(const TArrayView<UStaticMesh*> StaticMeshes);
	static void ExportMeshes(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory);
	static void ExportTextures(const TArrayView<UStaticMesh*> StaticMeshes, const FString& ExportDirectory, FUnrealToUnityExporterMeshImportDescriptor& MeshImportDescriptor);
	static void RevertStaticMeshChanges(const TArrayView<UStaticMesh*> StaticMeshes);
	static FString SaveMeshImportDescriptor(const FUnrealToUnityExporterMeshImportDescriptor& MeshImportDescriptor, const FString& ExportDirectory);
	static void SendUnityImportMessage(const FString& MeshImportDescriptorSavePath);

	static FString ConvertPathNameToDiskPathFormat(const FString& PathName);
	static FString GetStaticMeshExportDirectory(const FString& ExportDirectory);
	static FString GetStaticMeshPathOnDisk(const FString& ExportDirectory, const FString& StaticMeshPathName);
};
