/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once
#include "CoreMinimal.h"
#include "FormatterDelegates.h"

class UEdGraph;
class SGraphEditor;
class IAssetEditorInstance;

class FFormatterHacker
{
public:
	static void UpdateCommentNodes(SGraphEditor* GraphEditor, UEdGraph* Graph);
	static FFormatterDelegates GetDelegates(UObject* Object, IAssetEditorInstance* Instance);
	static void ComputeLayoutAtRatioOne(FFormatterDelegates GraphDelegates, TSet<UEdGraphNode*> Nodes);
	static void RestoreZoomLevel(FFormatterDelegates GraphDelegates);
};
