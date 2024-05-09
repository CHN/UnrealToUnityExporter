#include "UnrealToUnityExporterStaticMeshAdapter.h"

#include "MaterialBakingStructures.h"
#include "MeshUtilities.h"
#include "Developer/MeshMergeUtilities/Private/MeshMergeHelpers.h"

FUnrealToUnityExporterStaticMeshAdapter::FUnrealToUnityExporterStaticMeshAdapter(UStaticMesh* InStaticMesh)
	: StaticMesh(InStaticMesh)
{
	checkf(StaticMesh != nullptr, TEXT("Invalid static mesh in adapter"));
	NumLODs = StaticMesh->GetNumLODs();
}

int32 FUnrealToUnityExporterStaticMeshAdapter::GetNumberOfLODs() const
{
	return NumLODs;
}

void FUnrealToUnityExporterStaticMeshAdapter::RetrieveRawMeshData(int32 LODIndex, FMeshDescription& InOutRawMesh, bool bPropogateMeshData) const
{
	FMeshMergeHelpers::RetrieveMesh(StaticMesh, LODIndex, InOutRawMesh);
}

void FUnrealToUnityExporterStaticMeshAdapter::RetrieveMeshSections(int32 LODIndex, TArray<FSectionInfo>& InOutSectionInfo) const
{
	FMeshMergeHelpers::ExtractSections(StaticMesh, LODIndex, InOutSectionInfo);
}

int32 FUnrealToUnityExporterStaticMeshAdapter::GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const
{
	return StaticMesh->GetSectionInfoMap().Get(LODIndex, SectionIndex).MaterialIndex;
}

void FUnrealToUnityExporterStaticMeshAdapter::ApplySettings(int32 LODIndex, FMeshData& InOutMeshData) const
{
	InOutMeshData.LightMapIndex = StaticMesh->GetLightMapCoordinateIndex();
}

UPackage* FUnrealToUnityExporterStaticMeshAdapter::GetOuter() const
{
	return nullptr;
}

FString FUnrealToUnityExporterStaticMeshAdapter::GetBaseName() const
{
	return StaticMesh->GetOutermost()->GetName();
}

FName FUnrealToUnityExporterStaticMeshAdapter::GetMaterialSlotName(int32 MaterialIndex) const
{
	return StaticMesh->GetStaticMaterials()[MaterialIndex].MaterialSlotName;
}

FName FUnrealToUnityExporterStaticMeshAdapter::GetImportedMaterialSlotName(int32 MaterialIndex) const
{
	return StaticMesh->GetStaticMaterials()[MaterialIndex].ImportedMaterialSlotName;
}

void FUnrealToUnityExporterStaticMeshAdapter::SetMaterial(int32 MaterialIndex, UMaterialInterface* Material)
{
	const FStaticMaterial& OriginalMaterialSlot = StaticMesh->GetStaticMaterials()[MaterialIndex];
	StaticMesh->GetStaticMaterials()[MaterialIndex] = FStaticMaterial(Material, OriginalMaterialSlot.MaterialSlotName, OriginalMaterialSlot.ImportedMaterialSlotName);
}

void FUnrealToUnityExporterStaticMeshAdapter::RemapMaterialIndex(int32 LODIndex, int32 SectionIndex, int32 NewMaterialIndex)
{
	FMeshSectionInfo SectionInfo = StaticMesh->GetSectionInfoMap().Get(LODIndex, SectionIndex);
	SectionInfo.MaterialIndex = NewMaterialIndex;
	StaticMesh->GetSectionInfoMap().Set(LODIndex, SectionIndex, SectionInfo);
}

int32 FUnrealToUnityExporterStaticMeshAdapter::AddMaterial(UMaterialInterface* Material)
{
	int32 Index = StaticMesh->GetStaticMaterials().Emplace(Material);
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.FixupMaterialSlotNames(StaticMesh);
	return Index;
}

int32 FUnrealToUnityExporterStaticMeshAdapter::AddMaterial(UMaterialInterface* Material, const FName& SlotName, const FName& ImportedSlotName)
{
	int32 Index = StaticMesh->GetStaticMaterials().Emplace(Material, SlotName, ImportedSlotName);
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.FixupMaterialSlotNames(StaticMesh);
	return Index;
}

void FUnrealToUnityExporterStaticMeshAdapter::UpdateUVChannelData()
{
	StaticMesh->UpdateUVChannelData(false);
}

bool FUnrealToUnityExporterStaticMeshAdapter::IsAsset() const
{
	return true;
}

int32 FUnrealToUnityExporterStaticMeshAdapter::LightmapUVIndex() const
{
	return StaticMesh->GetLightMapCoordinateIndex();
}

FBoxSphereBounds FUnrealToUnityExporterStaticMeshAdapter::GetBounds() const
{
	return StaticMesh->GetBounds();
}