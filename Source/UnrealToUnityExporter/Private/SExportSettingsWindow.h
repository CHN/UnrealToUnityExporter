#pragma once

#include "Widgets/SWindow.h"

struct FExportSettings
{
	int32 TextureSize = 2048;
	bool bEnableReadWrite = false;
	TArray<TSharedPtr<FAssetData>> SelectedAssets;
};

DECLARE_DELEGATE_OneParam(FOnExportSettingsDone, const FExportSettings& /*ExportSettings*/)

class SExportSettingsWindow : public SWindow
{
public:

	SLATE_BEGIN_ARGS(SExportSettingsWindow)
	{}

	SLATE_EVENT(FOnExportSettingsDone, OnExportSettingsDone)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	const FExportSettings& GetExportSettings() const;

private:
	TSharedRef<SWidget> CreateAssetSelectorWidget();
	TSharedRef<ITableRow> GenerateSelectedAssetsList(TSharedPtr<FAssetData> AssetData, const TSharedRef<STableViewBase>& TableViewBase) const;

	TSharedRef<SVerticalBox> CreateModeWidgetContainer();
	void ResetModeWidgetContainer();
	void AddSelectedAssets();

	TSharedRef<SWidget> AddSelectedAssetsModeWidget();
	TSharedRef<SWidget> AddAssetsBySearchModeWidget();
	TSharedRef<SWidget> AddAssetsBySearchAndExcludingModeWidget();

	void AddAssetsToSelectedAssetsUnique(const TArray<FAssetData>& Assets);
	
	FExportSettings ExportSettings;
	FOnExportSettingsDone OnExportSettingsDone;

	static inline const FString AddSelectedAssetsMode = TEXT("Add Selected Assets Mode");
	static inline const FString AddAssetsBySearchMode = TEXT("Add Assets by Search");
	static inline const FString AddAssetsBySearchAndExcludingMode = TEXT("Add Assets by Search and Excluding");
	TArray<TSharedPtr<FString>> AssetSelectionModes;
	TSharedPtr<FString> CurrentSelectedMode;
	TSharedPtr<SVerticalBox> ModeWidgetContainer;
	TSharedPtr<SListView<TSharedPtr<FAssetData>>> SelectedAssetsListView;
	int32 ErrorCount = 0;
};
