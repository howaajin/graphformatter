/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterCommands.h"

#define LOCTEXT_NAMESPACE "GraphFormatter"

void FFormatterCommands::RegisterCommands()
{
	UI_COMMAND(FormatGraph, "Format Graph", "Auto format graph using Layered Graph Drawing algorithm", EUserInterfaceActionType::Button, FInputChord(EKeys::F, false, false, true, false));
	UI_COMMAND(StraightenConnections, "Straight Lines", "Straighten connections", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
