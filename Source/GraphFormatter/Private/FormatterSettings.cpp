/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterSettings.h"

UFormatterSettings::UFormatterSettings()
    : SupportedAssetTypes({
            {"Blueprint", true},
            {"BehaviorTree", true},
            {"Material", true},
            {"SoundCue", true},
            {"WidgetBlueprint", true},
            {"NiagaraScript", true},
            {"MetaSoundSource", true},
        })
    , DisableToolbar(false)
    , PositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian)
    , CommentBorder(45)
    , HorizontalSpacing(100)
    , VerticalSpacing(80)
    , MaxOrderingIterations(10)
{
}
