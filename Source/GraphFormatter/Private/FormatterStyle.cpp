/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

void FFormatterStyle::Initialize()
{
	if (!StyleSet.IsValid())
	{
		StyleSet = MakeShareable(new FSlateStyleSet("GraphFormatterStyle"));
		StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/Icons"));
		StyleSet->Set("GraphFormatter.ApplyIcon.Small", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Profiler/profiler_Calls_32x"), TEXT(".png")), FVector2D(16.0f, 16.0f)));
		StyleSet->Set("GraphFormatter.ApplyIcon", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Profiler/profiler_Calls_32x"), TEXT(".png")), FVector2D(32.0f, 32.0f)));
		StyleSet->Set("GraphFormatter.StraightenIcon.Small", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("icon_CurveEditor_Straighten_40x"), TEXT(".png")), FVector2D(16.0f, 16.0f)));
		StyleSet->Set("GraphFormatter.StraightenIcon", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("icon_CurveEditor_Straighten_40x"), TEXT(".png")), FVector2D(32.0f, 32.0f)));
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}
}

void FFormatterStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

TSharedPtr<ISlateStyle> FFormatterStyle::Get()
{
	return StyleSet;
}

TSharedPtr<class FSlateStyleSet > FFormatterStyle::StyleSet = nullptr;

