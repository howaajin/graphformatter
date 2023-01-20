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

namespace graph_layout
{
    struct graph_t;
    struct node_t;
}

struct FFormatter
{
    bool IsVerticalLayout = false;
    bool IsBehaviorTree = false;
    bool IsBlueprint = false;
    inline static bool IsAutoSizeComment = false;

    void SetCurrentEditor(SGraphEditor* Editor, UObject* Object);
    void SetZoomLevelTo11Scale() const;
    void RestoreZoomLevel() const;

    static bool IsAssetSupported(const UObject* Object);
    static bool IsExecPin(const UEdGraphPin* Pin);
    static bool HasExecPin(const UEdGraphNode* Node);

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

    bool PreCommand();
    void PostCommand();
    void Translate(TSet<UEdGraphNode*> Nodes, FVector2D Offset) const;
    void Format();
    void PlaceBlock();

    static FFormatter& Instance();

    static TArray<UEdGraphNode_Comment*> GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes);
    static TSet<UEdGraphNode*> FindParamGroupForExecNode(UEdGraphNode* Node, const TSet<UEdGraphNode*> Included, const TSet<UEdGraphNode*>& Excluded);
    static graph_layout::graph_t* BuildGraph(TSet<UEdGraphNode*> Nodes, bool IsParameterGroup = false);
    static void BuildNodes(graph_layout::graph_t* Graph, TSet<UEdGraphNode*> Nodes, bool IsParameterGroup = false);
    static void AddNode(graph_layout::graph_t* Graph, UEdGraphNode* Node, graph_layout::graph_t* SubGraph);
    static graph_layout::graph_t* CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> NodesUnderComment);
    static graph_layout::graph_t* CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group);
    static void BuildEdgeForNode(graph_layout::graph_t* Graph, graph_layout::node_t* Node, TSet<UEdGraphNode*> SelectedNodes);

private:
    FFormatter();
    FFormatter(FFormatter const&) = delete;
    void operator=(FFormatter const&) = delete;

    SGraphEditor* CurrentEditor = nullptr;
    SGraphPanel* CurrentPanel = nullptr;
    UEdGraph* CurrentGraph = nullptr;
};
