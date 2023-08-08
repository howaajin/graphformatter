/*---------------------------------------------------------------------------------------------
*  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "NewGraphAdapter.h"

#include "EdGraphNode_Comment.h"
#include "Formatter.h"
#include "FormatterSettings.h"
#include "UEGraphAdapter.h"
#include "graph_layout/graph_layout.h"

#include <algorithm>

using namespace graph_layout;

TArray<TArray<FFormatterNode*>> NewGraphAdapter::GetLayeredListFromNewGraph(const FConnectedGraph* Graph)
{
    connected_graph_t g;
    TMap<FFormatterPin*, pin_t*> PinsMap;
    TMap<node_t*, FFormatterNode*> NodesMap;
    for (auto Node : Graph->Nodes)
    {
        auto n = g.add_node();
        for (auto Pin : Node->InPins)
        {
            auto pin = n->add_pin(pin_type_t::in);
            PinsMap.Add(Pin, pin);
        }
        for (auto Pin : Node->OutPins)
        {
            auto pin = n->add_pin(pin_type_t::out);
            PinsMap.Add(Pin, pin);
        }
        NodesMap.Add(n, Node);
    }
    for (auto Node : Graph->Nodes)
    {
        for (auto Edge : Node->InEdges)
        {
            g.add_edge(PinsMap[Edge->To], PinsMap[Edge->From]);
        }
        for (auto Edge : Node->OutEdges)
        {
            g.add_edge(PinsMap[Edge->From], PinsMap[Edge->To]);
        }
    }

    g.rank();
    TArray<TArray<FFormatterNode*>> LayeredList;
    TMap<int, TArray<FFormatterNode*>> LayerMap;
    for (auto n : g.nodes)
    {
        auto& Layer = LayerMap.FindOrAdd(n->rank, TArray<FFormatterNode*>());
        Layer.Push(NodesMap[n]);
    }
    LayerMap.KeySort([](auto Rank1, auto Rank2)
    {
        return Rank1 < Rank2;
    });
    for (auto [Key, Layer] : LayerMap)
    {
        LayeredList.Push(Layer);
    }
    return LayeredList;
}

graph_t* NewGraphAdapter::CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> NodesUnderComment)
{
    if (NodesUnderComment.Num() > 0)
    {
        auto SubGraph = BuildGraph(NodesUnderComment);
        float BorderHeight = FFormatter::Instance().GetCommentNodeTitleHeight(CommentNode);
        const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
        SubGraph->border = rect_t{(float) Settings.CommentBorder, BorderHeight + Settings.CommentBorder, (float) Settings.CommentBorder, (float) Settings.CommentBorder};
        return SubGraph;
    }
    return nullptr;
}

void NewGraphAdapter::AddNode(graph_t* Graph, UEdGraphNode* Node, graph_t* SubGraph)
{
    node_t* n = Graph->add_node(SubGraph);
    n->user_ptr = Node;
    FVector2D Size = FFormatter::Instance().GetNodeSize(Node);
    n->size = vector2_t{(float) Size.X, (float) Size.Y};
    if (SubGraph)
    {
        auto NodePointers = SubGraph->get_user_pointers();
        for (auto NodePtr : NodePointers)
        {
            auto SubNode = (UEdGraphNode*) NodePtr;
            for (auto Pin : SubNode->Pins)
            {
                pin_t* p = n->add_pin(Pin->Direction == EGPD_Input ? pin_type_t::in : pin_type_t::out);
                p->user_pointer = Pin;
                Graph->user_ptr_to_pin[Pin] = p;
            }
        }
    }
    else
    {
        for (auto Pin : Node->Pins)
        {
            pin_t* p = n->add_pin(Pin->Direction == EGPD_Input ? pin_type_t::in : pin_type_t::out);
            FVector2D Offset = FFormatter::Instance().GetPinOffset(Pin);
            p->offset = vector2_t{(float) Offset.X, (float) Offset.Y};
            p->user_pointer = Pin;
            Graph->user_ptr_to_pin[Pin] = p;
        }
    }
}

void NewGraphAdapter::BuildEdges(graph_layout::graph_t* Graph, TSet<UEdGraphNode*> SelectedNodes)
{
    for (auto node : Graph->nodes)
    {
        BuildEdgeForNode(Graph, node, SelectedNodes);
    }
    auto Comparer = [](const node_t* A, const node_t* B)
    {
        return A->position.y < B->position.y;
    };
    std::sort(Graph->nodes.begin(), Graph->nodes.end(), Comparer);
}

graph_t* NewGraphAdapter::BuildGraph(TSet<UEdGraphNode*> Nodes, bool IsParameterGroup)
{
    graph_t* Graph = new graph_t;
    BuildNodes(Graph, Nodes, true);
    BuildEdges(Graph, Nodes);

    graph_t* RealGraph = Graph->to_connected_or_disconnected();
    delete Graph;

    const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
    RealGraph->spacing = vector2_t{(float) Settings.HorizontalSpacing, (float) Settings.VerticalSpacing};
    return RealGraph;
}

TMap<UEdGraphNode*, FSlateRect> NewGraphAdapter::GetBoundMap(graph_layout::graph_t* Graph)
{
    auto Bounds = Graph->get_bounds();
    TMap<UEdGraphNode*, FSlateRect> Result;
    for (auto [Node, Rect] : Bounds)
    {
        Result.Add((UEdGraphNode*) Node->user_ptr, FSlateRect(Rect.l, Rect.t, Rect.r, Rect.b));
    }
    return Result;
}

void NewGraphAdapter::BuildNodes(graph_t* Graph, TSet<UEdGraphNode*> Nodes, bool IsParameterGroup)
{
    while (true)
    {
        TArray<UEdGraphNode_Comment*> SortedCommentNodes = UEGraphAdapter::GetSortedCommentNodes(Nodes);
        if (SortedCommentNodes.Num() != 0)
        {
            // Topmost comment node has smallest negative depth value
            const int32 Depth = SortedCommentNodes[0]->CommentDepth;

            // Collapse all topmost comment nodes into virtual nodes.
            for (auto CommentNode : SortedCommentNodes)
            {
                if (CommentNode->CommentDepth == Depth)
                {
                    auto NodesUnderComment = FFormatter::Instance().GetNodesUnderComment(Cast<UEdGraphNode_Comment>(CommentNode));
                    NodesUnderComment = Nodes.Intersect(NodesUnderComment);
                    Nodes = Nodes.Difference(NodesUnderComment);
                    graph_t* SubGraph = CollapseCommentNode(CommentNode, NodesUnderComment);
                    AddNode(Graph, CommentNode, SubGraph);
                    Nodes.Remove(CommentNode);
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }

    const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
    if (FFormatter::Instance().IsBlueprint && !IsParameterGroup && Settings.bEnableBlueprintParameterGroup)
    {
        TArray<UEdGraphNode*> ExecNodes;
        for (auto Node : Nodes)
        {
            if (UEGraphAdapter::HasExecPin(Node))
            {
                ExecNodes.Add(Node);
            }
        }
        for (auto Node : ExecNodes)
        {
            TSet<UEdGraphNode*> Group;
            TSet<UEdGraphNode*> Excluded;
            Group = UEGraphAdapter::FindParamGroupForExecNode(Node, Nodes, Excluded);
            if (Group.Num() >= 2)
            {
                graph_t* SubGraph = CollapseGroup(Node, Group);
                AddNode(Graph, Node, SubGraph);
                Nodes = Nodes.Difference(Group);
            }
        }
    }
    for (auto Node : Nodes)
    {
        AddNode(Graph, Node, nullptr);
    }
}

graph_t* NewGraphAdapter::CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group)
{
    auto SubGraph = BuildGraph(Group, true);
    SubGraph->border = rect_t{0, 0, 0, 0};
    return SubGraph;
}

void NewGraphAdapter::BuildEdgeForNode(graph_t* Graph, node_t* Node, TSet<UEdGraphNode*> SelectedNodes)
{
    if (Node->graph)
    {
        const std::set<void*>& InnerSelectedNodes = Node->graph->get_user_pointers();
        for (auto SelectedNode : InnerSelectedNodes)
        {
            for (auto Pin : static_cast<UEdGraphNode*>(SelectedNode)->Pins)
            {
                for (auto LinkedToPin : Pin->LinkedTo)
                {
                    const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
                    if (InnerSelectedNodes.find(LinkedToNode) != InnerSelectedNodes.end() || !SelectedNodes.Contains(LinkedToNode))
                    {
                        continue;
                    }
                    pin_t* Tail = Graph->user_ptr_to_pin[Pin];
                    pin_t* Head = Graph->user_ptr_to_pin[LinkedToPin];
                    if (Pin->Direction == EGPD_Input)
                    {
                        std::swap(Tail, Head);
                    }
                    Graph->add_edge(Tail, Head);
                }
            }
        }
    }
    else
    {
        UEdGraphNode* OriginalNode = static_cast<UEdGraphNode*>(Node->user_ptr);
        for (auto Pin : OriginalNode->Pins)
        {
            for (auto LinkedToPin : Pin->LinkedTo)
            {
                const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
                if (!SelectedNodes.Contains(LinkedToNode))
                {
                    continue;
                }
                pin_t* Tail = Graph->user_ptr_to_pin[Pin];
                pin_t* Head = Graph->user_ptr_to_pin[LinkedToPin];
                if (Pin->Direction == EGPD_Input)
                {
                    std::swap(Tail, Head);
                }
                Graph->add_edge(Tail, Head);
            }
        }
    }
}
