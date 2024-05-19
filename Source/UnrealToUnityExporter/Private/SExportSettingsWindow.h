#pragma once

#include "Widgets/SWindow.h"

struct FExportSettings
{
	int32 TextureSize = 2048;
	bool bEnableReadWrite = false;
	bool bCanceled = false;
};

class SExportSettingsWindow : public SWindow
{
public:

	SLATE_BEGIN_ARGS(SExportSettingsWindow)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	const FExportSettings& GetExportSettings() const;

private:
	FExportSettings ExportSettings;
};
