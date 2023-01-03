/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterCommands.h"

#define LOCTEXT_NAMESPACE "GraphFormatter"

void FFormatterCommands::RegisterCommands()
{
    UI_COMMAND(FormatGraph, "Format Graph", "Auto format graph using Layered Graph Drawing algorithm", EUserInterfaceActionType::Button, FInputChord(EKeys::F, true, false, false, false));
    UI_COMMAND(StraightenConnections, "Straight Lines", "Straighten connections", EUserInterfaceActionType::ToggleButton, FInputChord());
    UI_COMMAND(PlaceBlock, "Place block", "Place selected nodes to appropriate position", EUserInterfaceActionType::Button, FInputChord(EKeys::E, true, false, false, false));
}

#undef LOCTEXT_NAMESPACE
