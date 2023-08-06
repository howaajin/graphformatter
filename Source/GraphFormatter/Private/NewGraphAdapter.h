#pragma once
#include "FormatterGraph.h"
#include "graph_layout/graph_layout.h"

class NewGraphAdapter
{
public:
    static TArray<TArray<FFormatterNode*>> GetLayeredListFromNewGraph(const FConnectedGraph* Graph);
    static graph_layout::graph_t* BuildGraph(TSet<UEdGraphNode*> Nodes, bool IsParameterGroup = false);
    static void BuildNodes(graph_layout::graph_t* Graph, TSet<UEdGraphNode*> Nodes, bool IsParameterGroup = false);
    static void BuildEdges(graph_layout::graph_t* Graph, TSet<UEdGraphNode*> SelectedNodes);
    static void AddNode(graph_layout::graph_t* Graph, UEdGraphNode* Node, graph_layout::graph_t* SubGraph);
    static graph_layout::graph_t* CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> NodesUnderComment);
    static graph_layout::graph_t* CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group);
    static void BuildEdgeForNode(graph_layout::graph_t* Graph, graph_layout::node_t* Node, TSet<UEdGraphNode*> SelectedNodes);
    static TMap<UEdGraphNode*, FSlateRect> GetBoundMap(graph_layout::graph_t* Graph);
};
