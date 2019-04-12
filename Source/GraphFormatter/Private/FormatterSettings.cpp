/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterSettings.h"

UFormatterSettings::UFormatterSettings()
	: PositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop)
	, CommentBorder(45)
	, HorizontalSpacing(100)
	, VerticalSpacing(80)
	, MaxOrderingIterations(10)
{
}
