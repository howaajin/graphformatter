/*---------------------------------------------------------------------------------------------
*  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "FormatterGraph.h"

class UEdGraphNode_Comment;

class UEGraphAdapter
{
public:
    static FFormatterGraph* Build(TSet<UEdGraphNode*> Nodes, bool InIsVerticalLayout = false, bool InIsParameterGroup = false);
    static TArray<UEdGraphNode_Comment*> GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes);
    static FFormatterNode* CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> NodesUnderComment, bool InIsVerticalLayout = false);
    static FFormatterNode* CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group, bool InIsVerticalLayout = false);
    static TSet<UEdGraphNode*> GetDirectConnected(const TSet<UEdGraphNode*>& SelectedNodes, EFormatterPinDirection Option);
    static TSet<UEdGraphNode*> GetNodesConnected(const TSet<UEdGraphNode*>& SelectedNodes, EFormatterPinDirection Option);
    static bool GetNodesConnectCenter(const TSet<UEdGraphNode*>& SelectedNodes, FVector2D& OutCenter, EFormatterPinDirection Option, bool bInvert = false);
    static void BuildNodes(FFormatterGraph* Graph, TSet<UEdGraphNode*> SelectedNodes);
    static void BuildEdges(TArray<FFormatterNode*>& Nodes, TMap<void*, FFormatterPin*>& PinsMap, TSet<UEdGraphNode*> SelectedNodes);
    static void BuildNodesAndEdges(FFormatterGraph* Graph, TArray<FFormatterNode*>& Nodes, TMap<void*, FFormatterPin*>& PinsMap, const TSet<UEdGraphNode*>& SelectedNodes);
    static TArray<FFormatterEdge> GetEdgeForNode(FFormatterNode* Node, TMap<void*, FFormatterPin*>& PinsMap, TSet<UEdGraphNode*> SelectedNodes);
    static TArray<TSet<UEdGraphNode*>> FindIsolated(const TArray<FFormatterNode*>& Nodes);
    static TSet<UEdGraphNode*> FindParamGroupForExecNode(UEdGraphNode* Node, const TSet<UEdGraphNode*> Included, const TSet<UEdGraphNode*>& Excluded);

    static bool IsExecPin(const UEdGraphPin* Pin);
    static bool HasExecPin(const UEdGraphNode* Node);

    static FFormatterNode* FormatterNodeFromUEGraphNode(UEdGraphNode* InNode);
    static FConnectedGraph* BuildConnectedGraph(const TSet<UEdGraphNode*>& SelectedNodes, bool InIsVerticalLayout = false, bool InIsParameterGroup = false);
};
