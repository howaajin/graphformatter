/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterSettings.h"

UFormatterSettings::UFormatterSettings()
    : AutoDetectGraphEditor(false)
    , SupportedAssetTypes({
        {"Blueprint", true},
        {"AnimBlueprint", true},
        {"WidgetBlueprint", true},
        {"BehaviorTree", true},
        {"Material", true},
        {"SoundCue", true},
        {"NiagaraScript", true},
        {"NiagaraSystem", true},
        {"MetaSoundSource", true},
        {"LevelScriptBlueprint", true},
        {"EditorUtilityBlueprint", true},
        {"EditorUtilityWidgetBlueprint", true},
        {"PCGGraph", true},
        {"InterchangeBlueprintPipelineBase", true},
        {"MetaSoundPatch", true},
    })
    , DisableToolbar(false)
    , PositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian)
    , CommentBorder(45)
    , HorizontalSpacing(100)
    , VerticalSpacing(80)
    , bEnableBlueprintParameterGroup(true)
    , SpacingFactorOfParameterGroup(0.314)
    , MaxOrderingIterations(10)
{
}
