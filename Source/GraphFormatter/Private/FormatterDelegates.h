/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"

class FFormatterNode;
class UEdGraph;
class SGraphEditor;
class UEdGraphNode;
class UEdGraphPin;

DECLARE_DELEGATE_RetVal(UEdGraph*, FGetGraphDelegate);
DECLARE_DELEGATE_RetVal(SGraphEditor*, FGetGraphEditorDelegate);
DECLARE_DELEGATE_RetVal(void, FMarkGraphDirty);
DECLARE_DELEGATE_RetVal(bool, FIsVerticalPositioning);
DECLARE_DELEGATE_RetVal_OneParam(FVector2D, FCalculateNodeBoundDelegate, UEdGraphNode*);
DECLARE_DELEGATE_RetVal_OneParam(FVector2D, FOffsetCalculatorDelegate, UEdGraphPin*);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FNodeComparer, const FFormatterNode&, const FFormatterNode&);
DECLARE_DELEGATE_RetVal_TwoParams(void, FMoveNodeTo, UEdGraphNode*, const FVector2D&);
DECLARE_DELEGATE_RetVal_OneParam(float, FCommentHeightDelegate, UEdGraphNode*);

struct FFormatterDelegates
{
	FCalculateNodeBoundDelegate BoundCalculator;
	FGetGraphDelegate GetGraphDelegate;
	FGetGraphEditorDelegate GetGraphEditorDelegate;
	FOffsetCalculatorDelegate OffsetCalculator;
	FCommentHeightDelegate CommentHeight;
	FMarkGraphDirty MarkGraphDirty;
	FIsVerticalPositioning IsVerticalPositioning;
	FNodeComparer NodeComparer;
	FMoveNodeTo MoveTo;
};
