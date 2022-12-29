/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

class FBlueprintEditor;
class FMaterialEditor;
class FSoundCueEditor;
class FBehaviorTreeEditor;
class UEdGraphNode_Comment;

struct FFormatter
{
    bool IsVerticalLayout = false;
    bool IsBehaviorTree = false;
    bool IsBlueprint = false;

    void SetCurrentEditor(SGraphEditor* Editor, UObject* Object);

    bool IsAssetSupported(const UObject* Object) const;

    SGraphEditor* FindGraphEditorForTopLevelWindow() const;
    SGraphEditor* FindGraphEditorByCursor() const;

    SGraphPanel* GetCurrentPanel() const;
    SGraphNode* GetWidget(const UEdGraphNode* Node) const;
    TSet<UEdGraphNode*> GetAllNodes() const;
    float GetCommentNodeTitleHeight(const UEdGraphNode* Node) const;
    FVector2D GetNodeSize(const UEdGraphNode* Node) const;
    FVector2D GetNodePosition(const UEdGraphNode* Node) const;
    FVector2D GetPinOffset(const UEdGraphPin* Pin) const;
    FSlateRect GetNodesBound(const TSet<UEdGraphNode*> Nodes) const;
    TSet<UEdGraphNode*> GetNodesUnderComment(const UEdGraphNode_Comment* CommentNode) const;

    bool IsExecPin(const UEdGraphPin* Pin) const;

    void Translate(TSet<UEdGraphNode*> Nodes, FVector2D Offset) const;
    void Format() const;
    void PlaceBlock() const;
    static FFormatter& Instance()
    {
        static FFormatter Context;
        return Context;
    }

private:
    FFormatter() {}
    FFormatter(FFormatter const&) = delete;
    void operator=(FFormatter const&) = delete;

    SGraphEditor* CurrentEditor = nullptr;
};
