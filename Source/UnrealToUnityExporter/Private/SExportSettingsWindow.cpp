#include "SExportSettingsWindow.h"

#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "UnrealToUnityExporter"

void SExportSettingsWindow::Construct(const FArguments& InArgs)
{
	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Unreal to Unity Exporter Settings"))
		.SizingRule(ESizingRule::Autosized)
		.HasCloseButton(false)
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
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.OnClicked_Lambda([this]
						{
							ExportSettings.bCanceled = true;
							FSlateApplication::Get().RequestDestroyWindow(StaticCastSharedRef<SWindow>(AsShared()));
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

#undef LOCTEXT_NAMESPACE
