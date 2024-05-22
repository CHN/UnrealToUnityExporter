#include "SExportSettingsWindow.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Algo/RemoveIf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "UnrealToUnityExporter"

namespace
{
	const TArray<FTopLevelAssetPath> SupportedTypes =
	{
		FTopLevelAssetPath(TEXT("/Script/Engine.StaticMesh"))
	};
}

void SExportSettingsWindow::Construct(const FArguments& InArgs)
{
	OnExportSettingsDone = InArgs._OnExportSettingsDone;
	
	CurrentSelectedMode = AssetSelectionModes.Add_GetRef(MakeShared<FString>(AddSelectedAssetsMode));
	AssetSelectionModes.Add(MakeShared<FString>(AddAssetsBySearchMode));
	AssetSelectionModes.Add(MakeShared<FString>(AddAssetsBySearchAndExcludingMode));
	
	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Unreal to Unity Exporter Settings"))
		.SizingRule(ESizingRule::Autosized)
		.IsTopmostWindow(true)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TextureSizeLabel", "Texture Size"))
				]
				+ SHorizontalBox::Slot()
				[
					SNew(SNumericEntryBox<int32>)
					.Value_Lambda([this]
					{
						return ExportSettings.TextureSize;
					})
					.OnValueCommitted_Lambda([this] (int32 NewValue, ETextCommit::Type)
					{
						ExportSettings.TextureSize = NewValue;
					})
				]
			]
			+ SVerticalBox::Slot()
			.Padding(8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnableReadWriteLabel", "Enable Read/Write"))
				]
				+ SHorizontalBox::Slot()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]
					{
						return ExportSettings.bEnableReadWrite ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this] (ECheckBoxState CheckBoxState)
					{
						ExportSettings.bEnableReadWrite = CheckBoxState == ECheckBoxState::Checked;
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.f)
			[
				CreateAssetSelectorWidget()	
			]
			+ SVerticalBox::Slot()
			.Padding(8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("ExportButton", "Export"))
						.OnClicked_Lambda([this]
						{
							FSlateApplication::Get().RequestDestroyWindow(StaticCastSharedRef<SWindow>(AsShared()));
							OnExportSettingsDone.ExecuteIfBound(ExportSettings);
							return FReply::Handled();
						})
					]
				]
			]
		]
		);
}

const FExportSettings& SExportSettingsWindow::GetExportSettings() const
{
	return ExportSettings;
}

TSharedRef<SWidget> SExportSettingsWindow::CreateAssetSelectorWidget()
{
	return SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(8.f)
	[
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&AssetSelectionModes)
		.OnGenerateWidget_Lambda([] (const TSharedPtr<FString>& Item)
		{
			return SNew(STextBlock)
			.Text(FText::FromString(*Item));
		})
		.OnSelectionChanged_Lambda([this] (const TSharedPtr<FString>& Item, ESelectInfo::Type SelectInfo)
		{
			CurrentSelectedMode = Item;
			ResetModeWidgetContainer();
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]
			{
				return FText::FromString(*CurrentSelectedMode);
			})
		]
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(4.f)
	.VAlign(VAlign_Center)
	[
		CreateModeWidgetContainer()
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.MaxHeight(400.f)
	.Padding(8.f)
	[
		SAssignNew(SelectedAssetsListView, SListView<TSharedPtr<FAssetData>>)
		.ListItemsSource(&ExportSettings.SelectedAssets)
		.OnGenerateRow(this, &SExportSettingsWindow::GenerateSelectedAssetsList)
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(2.f)
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.Text(LOCTEXT("RemoveSelectedAssets", "-"))
		.OnClicked_Lambda([this]
		{
			const TArray<TSharedPtr<FAssetData>> ItemsToRemove = SelectedAssetsListView->GetSelectedItems();
			ExportSettings.SelectedAssets.SetNum(Algo::RemoveIf(ExportSettings.SelectedAssets, [this, &ItemsToRemove] (const TSharedPtr<FAssetData>& Item)
			{
				return ItemsToRemove.Contains(Item);
			}));
			return FReply::Handled();
		})
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(2.f)
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]
			{
				return FText::Format(LOCTEXT("AssetCount", "Assets: {0}"), ExportSettings.SelectedAssets.Num());
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]
			{
				return FText::Format(LOCTEXT("ErrorCount", "Errors: {0}"), ErrorCount);
			})
		]
	];
}

TSharedRef<ITableRow> SExportSettingsWindow::GenerateSelectedAssetsList(TSharedPtr<FAssetData> AssetData, const TSharedRef<STableViewBase>& TableViewBase) const
{
	return SNew(STableRow<TSharedPtr<FAssetData>>, TableViewBase)
	[
		SNew(STextBlock)
		.Text(FText::FromString(AssetData->GetAsset()->GetPackage()->GetName()))
	];
}

TSharedRef<SVerticalBox> SExportSettingsWindow::CreateModeWidgetContainer()
{
	ModeWidgetContainer = SNew(SVerticalBox);
	ResetModeWidgetContainer();
	return ModeWidgetContainer.ToSharedRef();
}

void SExportSettingsWindow::ResetModeWidgetContainer()
{
	ModeWidgetContainer->ClearChildren();
	
	if (*CurrentSelectedMode == AddSelectedAssetsMode)
	{
		ModeWidgetContainer->AddSlot()
		[
			AddSelectedAssetsModeWidget()
		];
	}
	else if (*CurrentSelectedMode == AddAssetsBySearchMode)
	{
		ModeWidgetContainer->AddSlot()
		[
			AddAssetsBySearchModeWidget()
		];
	}
	else if (*CurrentSelectedMode == AddAssetsBySearchAndExcludingMode)
	{
		ModeWidgetContainer->AddSlot()
		[
			AddAssetsBySearchAndExcludingModeWidget()
		];
	}
}

void SExportSettingsWindow::AddSelectedAssets()
{
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> Assets;
	ContentBrowserModule.Get().GetSelectedAssets(Assets);
	AddAssetsToSelectedAssetsUnique(Assets);
}

TSharedRef<SWidget> SExportSettingsWindow::AddSelectedAssetsModeWidget()
{
	return SNew(SButton)
		.Text(LOCTEXT("ExecuteAddSelectedAssets", "Add Selected Assets"))
		.OnClicked_Lambda([this]
		{
			AddSelectedAssets();
			SelectedAssetsListView->RebuildList();
			return FReply::Handled();
		});
}

TSharedRef<SWidget> SExportSettingsWindow::AddAssetsBySearchModeWidget()
{
	const TSharedRef<SMultiLineEditableTextBox> AssetSearchEditableText = SNew(SMultiLineEditableTextBox)
		.AlwaysShowScrollbars(true)
		.HintText(LOCTEXT("AssetSearchHintText", "Add references separated by new line"));
	
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.f)
		[
			AssetSearchEditableText
		]
		+ SVerticalBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("ExecuteAddAssetsBySearch", "Add Assets by Search"))
			.OnClicked_Lambda([this, AssetSearchEditableText]
			{
				TArray<FString> ObjectPathStrings;
				AssetSearchEditableText->GetText().ToString().ParseIntoArrayLines(ObjectPathStrings);
				
				FARFilter Filter;
				Filter.bIncludeOnlyOnDiskAssets = true;
				Filter.ClassPaths = SupportedTypes;
				
				Algo::Transform(ObjectPathStrings, Filter.SoftObjectPaths, [] (const FString& ObjectPathString)
				{
					return FSoftObjectPath(ObjectPathString);
				});

				const IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
				TArray<FAssetData> Assets;
				
				if (AssetRegistry.GetAssets(Filter, Assets))
				{
					for (const FSoftObjectPath& ObjectPath : Filter.SoftObjectPaths)
					{
						const bool bIsFound = Assets.ContainsByPredicate([&ObjectPath] (const FAssetData& AssetData)
						{
							return AssetData.ToSoftObjectPath() == ObjectPath;
						});
						
						if (!bIsFound)
						{
							ErrorCount++;
						}
					}
					
					AddAssetsToSelectedAssetsUnique(Assets);
					SelectedAssetsListView->RebuildList();
				}
				
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> SExportSettingsWindow::AddAssetsBySearchAndExcludingModeWidget()
{
	const TSharedRef<SMultiLineEditableTextBox> AssetFolderPathsEditableTextBox = SNew(SMultiLineEditableTextBox)
		.AlwaysShowScrollbars(true)
		.HintText(LOCTEXT("AssetFolderPathsEditableTextBoxHintText", "Add folder paths separated by new line"));

	const TSharedRef<SMultiLineEditableTextBox> ExcludeStringsEditableTextBox = SNew(SMultiLineEditableTextBox)
		.AlwaysShowScrollbars(true)
		.HintText(LOCTEXT("ExcludeStringsEditableTextBoxHintText", "Add exclude strings separated by new line"));
	
	const TSharedRef<SMultiLineEditableTextBox> ExcludeAssetPathsEditableTextBox = SNew(SMultiLineEditableTextBox)
		.AlwaysShowScrollbars(true)
		.HintText(LOCTEXT("ExcludeAssetPathsEditableTextBoxHintText", "Add exclude asset paths separated by new line"));
	
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.f)
		[
			AssetFolderPathsEditableTextBox
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.f)
		[
			ExcludeStringsEditableTextBox
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300.f)
		[
			ExcludeAssetPathsEditableTextBox
		]
		+ SVerticalBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("ExecuteAddAssetsBySearch", "Add Assets by Search"))
			.OnClicked_Lambda([this, AssetFolderPathsEditableTextBox, ExcludeStringsEditableTextBox, ExcludeAssetPathsEditableTextBox]
			{
				TArray<FString> FolderPaths;
				AssetFolderPathsEditableTextBox->GetText().ToString().ParseIntoArrayLines(FolderPaths);

				TArray<FString> ExcludeStrings;
				ExcludeStringsEditableTextBox->GetText().ToString().ParseIntoArrayLines(ExcludeStrings);

				TArray<FString> ExcludeAssetPaths;
				ExcludeAssetPathsEditableTextBox->GetText().ToString().ParseIntoArrayLines(ExcludeAssetPaths);
				
				FARFilter Filter;
				Filter.bIncludeOnlyOnDiskAssets = true;
				Filter.bRecursivePaths = true;
				Filter.ClassPaths = SupportedTypes;
				
				Algo::Transform(FolderPaths, Filter.PackagePaths, [] (const FString& FolderPath)
				{
					return FName(FolderPath);
				});

				const IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
				TArray<FAssetData> Assets;

				TArray<FAssetData> ExcludeAssets;
				FARFilter ExcludeAssetsFilter;
				ExcludeAssetsFilter.bIncludeOnlyOnDiskAssets = true;
				
				Algo::Transform(ExcludeAssetPaths, ExcludeAssetsFilter.SoftObjectPaths, [] (const FString& AssetPath)
				{
					return FSoftObjectPath(AssetPath);
				});

				AssetRegistry.GetAssets(ExcludeAssetsFilter, ExcludeAssets);

				ErrorCount += ExcludeAssetPaths.Num() - ExcludeAssets.Num();
				
				if (AssetRegistry.GetAssets(Filter, Assets))
				{
					Assets.SetNum(Algo::RemoveIf(Assets, [this, &ExcludeAssets] (const FAssetData& AssetData)
					{
						const int32 Index = ExcludeAssets.IndexOfByKey(AssetData);
						
						if (Index != INDEX_NONE)
						{
							ExcludeAssets.RemoveAt(Index);
							return true;
						}
						
						return false;
					}));

					ErrorCount += ExcludeAssets.Num();

					Assets.SetNum(Algo::RemoveIf(Assets, [&ExcludeStrings] (const FAssetData& AssetData)
					{
						for (const FString& ExcludeString : ExcludeStrings)
						{
							if (AssetData.AssetName.ToString().Contains(ExcludeString))
							{
								return true;
							}
						}

						return false;
					}));
					
					AddAssetsToSelectedAssetsUnique(Assets);
					SelectedAssetsListView->RebuildList();
				}
				
				return FReply::Handled();
			})
		];
}

void SExportSettingsWindow::AddAssetsToSelectedAssetsUnique(const TArray<FAssetData>& Assets)
{
	Algo::TransformIf(Assets, ExportSettings.SelectedAssets, [this] (const FAssetData& AssetData)
	{
		return !ExportSettings.SelectedAssets.ContainsByPredicate([&AssetData] (const TSharedPtr<FAssetData>& SelectedAsset)
		{
			return *SelectedAsset == AssetData;
		});
	},[] (const FAssetData& AssetData)
	{
		return MakeShared<FAssetData>(AssetData);
	});
}

#undef LOCTEXT_NAMESPACE
