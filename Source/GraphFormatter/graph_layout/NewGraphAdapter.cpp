#include "NewGraphAdapter.h"
#include "graph_layout.h"

TArray<TArray<FFormatterNode*>> NewGraphAdapter::GetLayeredListFromNewGraph(const FConnectedGraph* Graph)
{
    graph_layout::connected_graph_t g;
    TMap<FFormatterPin*, graph_layout::pin_t*> PinsMap;
    TMap<graph_layout::node_t*, FFormatterNode*> NodesMap;
    for (auto Node : Graph->Nodes)
    {
        auto n = g.add_node();
        for (auto Pin : Node->InPins)
        {
            auto pin = n->add_pin(graph_layout::pin_type_t::in);
            PinsMap.Add(Pin, pin);
        }
        for (auto Pin : Node->OutPins)
        {
            auto pin = n->add_pin(graph_layout::pin_type_t::out);
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
