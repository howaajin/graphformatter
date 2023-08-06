#include "UEGraphAdapter.h"

#include "EdGraphNode_Comment.h"
#include "Formatter.h"
#include "FormatterSettings.h"

FFormatterGraph* UEGraphAdapter::Build(TSet<UEdGraphNode*> Nodes, bool InIsVerticalLayout, bool InIsParameterGroup)
{
    auto Graph = new FFormatterGraph(InIsVerticalLayout, InIsParameterGroup);
    BuildNodesAndEdges(Graph, Graph->Nodes, Graph->OriginalPinsMap, Nodes);

    auto FoundIsolatedGraphs = FindIsolated(Graph->Nodes);
    delete Graph;
    if (FoundIsolatedGraphs.Num() > 1)
    {
        auto DisconnectedGraph = new FDisconnectedGraph();
        for (const auto& IsolatedNodes : FoundIsolatedGraphs)
        {
            auto NewGraph = BuildConnectedGraph(IsolatedNodes, InIsVerticalLayout, InIsParameterGroup);
            DisconnectedGraph->AddGraph(NewGraph);
        }
        return DisconnectedGraph;
    }
    else
    {
        return BuildConnectedGraph(Nodes, InIsVerticalLayout, InIsParameterGroup);
    }
}

TArray<UEdGraphNode_Comment*> UEGraphAdapter::GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes)
{
    TArray<UEdGraphNode_Comment*> CommentNodes;
    for (auto Node : SelectedNodes)
    {
        if (Node->IsA(UEdGraphNode_Comment::StaticClass()))
        {
            auto CommentNode = Cast<UEdGraphNode_Comment>(Node);
            CommentNodes.Add(CommentNode);
        }
    }
    CommentNodes.Sort([](const UEdGraphNode_Comment& A, const UEdGraphNode_Comment& B)
    {
        return A.CommentDepth < B.CommentDepth;
    });
    return CommentNodes;
}

FFormatterNode* UEGraphAdapter::CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> NodesUnderComment, bool InIsVerticalLayout)
{
    FFormatterNode* Node = FormatterNodeFromUEGraphNode(CommentNode);
    if (NodesUnderComment.Num() > 0)
    {
        auto SubGraph = Build(NodesUnderComment, InIsVerticalLayout);
        float BorderHeight = FFormatter::Instance().GetCommentNodeTitleHeight(CommentNode);
        const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
        SubGraph->SetBorder(Settings.CommentBorder, BorderHeight + Settings.CommentBorder, Settings.CommentBorder, Settings.CommentBorder);
        Node->SetSubGraph(SubGraph);
    }
    return Node;
}

FFormatterNode* UEGraphAdapter::CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group, bool InIsVerticalLayout)
{
    FFormatterNode* Node = new FFormatterNode();
    Node->OriginalNode = MainNode;
    FConnectedGraph* SubGraph = BuildConnectedGraph(Group, InIsVerticalLayout, true);
    Node->SetSubGraph(SubGraph);
    SubGraph->SetBorder(0, 0, 0, 0);
    return Node;
}

TSet<UEdGraphNode*> UEGraphAdapter::GetDirectConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option)
{
    TSet<UEdGraphNode*> DirectConnectedNodes;
    for (auto Node : SelectedNodes)
    {
        for (auto Pin : Node->Pins)
        {
            if (Option == EInOutOption::EIOO_IN && Pin->Direction == EGPD_Input ||
                Option == EInOutOption::EIOO_OUT && Pin->Direction == EGPD_Output ||
                Option == EInOutOption::EIOO_ALL)
            {
                for (auto LinkedPin : Pin->LinkedTo)
                {
                    auto LinkedNode = LinkedPin->GetOwningNodeUnchecked();
                    if (!SelectedNodes.Contains(LinkedNode))
                    {
                        DirectConnectedNodes.Add(LinkedNode);
                    }
                }
            }
        }
    }
    return DirectConnectedNodes;
}

static void GetNodesConnectedRecursively(UEdGraphNode* RootNode, const TSet<UEdGraphNode*>& Excluded, TSet<UEdGraphNode*>& OutSet)
{
    TSet<UEdGraphNode*> Set;
    for (auto Pin : RootNode->Pins)
    {
        for (auto LinkedToPin : Pin->LinkedTo)
        {
            auto LinkedNode = LinkedToPin->GetOwningNodeUnchecked();
            if (!Excluded.Contains(LinkedNode) && !OutSet.Contains(LinkedNode))
            {
                Set.Add(LinkedNode);
            }
        }
    }
    if (Set.Num())
    {
        OutSet.Append(Set);
        for (auto Node : Set)
        {
            GetNodesConnectedRecursively(Node, Excluded, OutSet);
        }
    }
}

TSet<UEdGraphNode*> UEGraphAdapter::GetNodesConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option)
{
    TSet<UEdGraphNode*> DirectConnectedNodes = GetDirectConnected(SelectedNodes, Option);
    TSet<UEdGraphNode*> Result;
    Result.Append(DirectConnectedNodes);
    for (auto Node : DirectConnectedNodes)
    {
        GetNodesConnectedRecursively(Node, SelectedNodes, Result);
    }
    return Result;
}

bool UEGraphAdapter::GetNodesConnectCenter(const TSet<UEdGraphNode*>& SelectedNodes, FVector2D& OutCenter, EInOutOption Option, bool bInvert)
{
    FBox2D Bound(ForceInit);
    for (auto Node : SelectedNodes)
    {
        for (auto Pin : Node->Pins)
        {
            if (FFormatter::Instance().IsBlueprint && !FFormatter::Instance().IsExecPin(Pin))
            {
                continue;
            }
            if (Option == EInOutOption::EIOO_IN && Pin->Direction == EGPD_Input ||
                Option == EInOutOption::EIOO_OUT && Pin->Direction == EGPD_Output ||
                Option == EInOutOption::EIOO_ALL)
            {
                for (auto LinkedPin : Pin->LinkedTo)
                {
                    auto LinkedNode = LinkedPin->GetOwningNodeUnchecked();
                    if (!SelectedNodes.Contains(LinkedNode))
                    {
                        auto Pos = FFormatter::Instance().GetNodePosition(bInvert ? Node : LinkedNode);
                        auto PinOffset = FFormatter::Instance().GetPinOffset(bInvert ? Pin : LinkedPin);
                        auto LinkedPos = Pos + PinOffset;
                        FBox2D PosZeroBound = FBox2D(LinkedPos, LinkedPos);
                        Bound = Bound.bIsValid ? Bound + PosZeroBound : PosZeroBound;
                    }
                }
            }
        }
    }
    if (Bound.bIsValid)
    {
        OutCenter = Bound.GetCenter();
        return true;
    }
    else
    {
        return false;
    }
}

TSet<UEdGraphNode*> FindParamGroupForExecNode(UEdGraphNode* Node, const TSet<UEdGraphNode*> Included, const TSet<UEdGraphNode*>& Excluded)
{
    TSet<UEdGraphNode*> VisitedNodes;
    TArray<UEdGraphNode*> Stack;
    Stack.Push(Node);
    while (!Stack.IsEmpty())
    {
        auto StackNode = Stack.Pop();
        VisitedNodes.Add(StackNode);
        for (auto Pin : StackNode->Pins)
        {
            if (Pin->Direction != EGPD_Input || FFormatter::IsExecPin(Pin))
            {
                continue;
            }
            for (auto LinkedPin : Pin->LinkedTo)
            {
                auto LinkedNode = LinkedPin->GetOwningNodeUnchecked();
                if (!Included.Contains(LinkedNode) ||
                    VisitedNodes.Contains(LinkedNode) ||
                    Excluded.Contains(LinkedNode) ||
                    FFormatter::HasExecPin(LinkedNode))
                {
                    continue;
                }
                Stack.Add(LinkedNode);
            }
        }
    }
    return VisitedNodes;
}

void UEGraphAdapter::BuildNodes(FFormatterGraph* Graph, TSet<UEdGraphNode*> SelectedNodes)
{
    while (true)
    {
        TArray<UEdGraphNode_Comment*> SortedCommentNodes = GetSortedCommentNodes(SelectedNodes);
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
                    NodesUnderComment = SelectedNodes.Intersect(NodesUnderComment);
                    SelectedNodes = SelectedNodes.Difference(NodesUnderComment);
                    FFormatterNode* CollapsedNode = CollapseCommentNode(CommentNode, NodesUnderComment, FFormatter::Instance().IsVerticalLayout);
                    Graph->AddNode(CollapsedNode);
                    SelectedNodes.Remove(CommentNode);
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
    if (FFormatter::Instance().IsBlueprint && !Graph->GetIsParameterGroup() && Settings.bEnableBlueprintParameterGroup)
    {
        TArray<UEdGraphNode*> ExecNodes;
        for (auto Node : SelectedNodes)
        {
            if (FFormatter::HasExecPin(Node))
            {
                ExecNodes.Add(Node);
            }
        }
        for (auto Node : ExecNodes)
        {
            TSet<UEdGraphNode*> Group;
            TSet<UEdGraphNode*> Excluded;
            Group = FindParamGroupForExecNode(Node, SelectedNodes, Excluded);
            if (Group.Num() >= 2)
            {
                FFormatterNode* CollapsedNode = CollapseGroup(Node, Group, FFormatter::Instance().IsVerticalLayout);
                Graph->AddNode(CollapsedNode);
                SelectedNodes = SelectedNodes.Difference(Group);
            }
        }
    }
    for (auto Node : SelectedNodes)
    {
        FFormatterNode* NodeData = FormatterNodeFromUEGraphNode(Node);
        Graph->AddNode(NodeData);
    }
}

void UEGraphAdapter::BuildEdges(TArray<FFormatterNode*>& Nodes, TMap<void*, FFormatterPin*>& PinsMap, TSet<UEdGraphNode*> SelectedNodes)
{
    for (auto Node : Nodes)
    {
        auto Edges = GetEdgeForNode(Node, PinsMap, SelectedNodes);
        for (auto Edge : Edges)
        {
            Node->Connect(Edge.From, Edge.To, Edge.Weight);
        }
    }
}

void UEGraphAdapter::BuildNodesAndEdges(FFormatterGraph* Graph, TArray<FFormatterNode*>& Nodes, TMap<void*, FFormatterPin*>& PinsMap, const TSet<UEdGraphNode*>& SelectedNodes)
{
    BuildNodes(Graph, SelectedNodes);
    BuildEdges(Nodes, PinsMap, SelectedNodes);
    Nodes.Sort([Graph](const FFormatterNode& A, const FFormatterNode& B)
    {
        return Graph->GetIsVerticalLayout() ? A.GetPosition().X < B.GetPosition().X : A.GetPosition().Y < B.GetPosition().Y;
    });
}

static float GetEdgeWeight(UEdGraphPin* StartPin)
{
    float weight = 1;
    if (StartPin->PinType.PinCategory == "exec")
    {
        weight = 99;
    }
    return weight;
}

TArray<FFormatterEdge> UEGraphAdapter::GetEdgeForNode(FFormatterNode* Node, TMap<void*, FFormatterPin*>& PinsMap, TSet<UEdGraphNode*> SelectedNodes)
{
    TArray<FFormatterEdge> Result;
    if (Node->SubGraph)
    {
        const TSet<void*> InnerSelectedNodes = Node->SubGraph->GetOriginalNodes();
        for (void* Node2 : InnerSelectedNodes)
        {
            UEdGraphNode* SelectedNode = static_cast<UEdGraphNode*>(Node2);
            for (auto Pin : SelectedNode->Pins)
            {
                for (auto LinkedToPin : Pin->LinkedTo)
                {
                    const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
                    if (InnerSelectedNodes.Contains(LinkedToNode) || !SelectedNodes.Contains(LinkedToNode))
                    {
                        continue;
                    }
                    FFormatterPin* From = PinsMap[Pin];
                    FFormatterPin* To = PinsMap[LinkedToPin];
                    Result.Add(FFormatterEdge{From, To, GetEdgeWeight(Pin)});
                }
            }
        }
    }
    else
    {
        UEdGraphNode* OriginalNode = static_cast<UEdGraphNode*>(Node->OriginalNode);
        for (auto Pin : OriginalNode->Pins)
        {
            for (auto LinkedToPin : Pin->LinkedTo)
            {
                const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
                if (!SelectedNodes.Contains(LinkedToNode))
                {
                    continue;
                }
                FFormatterPin* From = PinsMap[Pin];
                FFormatterPin* To = PinsMap[LinkedToPin];
                Result.Add(FFormatterEdge{From, To, GetEdgeWeight(Pin)});
            }
        }
    }
    return Result;
}

TArray<TSet<UEdGraphNode*>> UEGraphAdapter::FindIsolated(const TArray<FFormatterNode*>& Nodes)
{
    TArray<TSet<UEdGraphNode*>> Result;
    TSet<FFormatterNode*> CheckedNodes;
    TArray<FFormatterNode*> Stack;
    for (auto Node : Nodes)
    {
        if (!CheckedNodes.Contains(Node))
        {
            CheckedNodes.Add(Node);
            Stack.Push(Node);
        }
        TSet<UEdGraphNode*> IsolatedNodes;
        while (Stack.Num() != 0)
        {
            FFormatterNode* Top = Stack.Pop();
            IsolatedNodes.Add(static_cast<UEdGraphNode*>(Top->OriginalNode));
            if (Top->SubGraph != nullptr)
            {
                TSet<void*> OriginalNodes = Top->SubGraph->GetOriginalNodes();

                for (auto OriginalNode : OriginalNodes)
                {
                    UEdGraphNode* N = static_cast<UEdGraphNode*>(OriginalNode);
                    IsolatedNodes.Add(N);
                }
            }
            TArray<FFormatterNode*> ConnectedNodes = Top->GetSuccessors();
            TArray<FFormatterNode*> Predecessors = Top->GetPredecessors();
            ConnectedNodes.Append(Predecessors);
            for (auto ConnectedNode : ConnectedNodes)
            {
                if (!CheckedNodes.Contains(ConnectedNode))
                {
                    Stack.Push(ConnectedNode);
                    CheckedNodes.Add(ConnectedNode);
                }
            }
        }
        if (IsolatedNodes.Num() != 0)
        {
            Result.Add(IsolatedNodes);
        }
    }
    return Result;
}

FFormatterNode* UEGraphAdapter::FormatterNodeFromUEGraphNode(UEdGraphNode* InNode)
{
    FFormatterNode* Node = new FFormatterNode();
    Node->Guid = InNode->NodeGuid;
    Node->OriginalNode = InNode;
    Node->SubGraph = nullptr;
    Node->Size = FFormatter::Instance().GetNodeSize(InNode);
    Node->PathDepth = 0;
    Node->SetPosition(FVector2D(InNode->NodePosX, InNode->NodePosY));
    for (const auto Pin : InNode->Pins)
    {
        auto NewPin = new FFormatterPin;
        NewPin->Guid = FGuid::NewGuid();
        NewPin->OriginalPin = Pin;
        NewPin->Direction = Pin->Direction;
        NewPin->OwningNode = Node;
        NewPin->NodeOffset = FFormatter::Instance().GetPinOffset(Pin);
        if (Pin->Direction == EGPD_Input)
        {
            Node->InPins.Add(NewPin);
        }
        else
        {
            Node->OutPins.Add(NewPin);
        }
    }
    return Node;
}

FConnectedGraph* UEGraphAdapter::BuildConnectedGraph(const TSet<UEdGraphNode*>& SelectedNodes, bool InIsVerticalLayout, bool InIsParameterGroup)
{
    FConnectedGraph* Graph = new FConnectedGraph(InIsVerticalLayout, InIsParameterGroup);
    BuildNodesAndEdges(Graph, Graph->Nodes, Graph->OriginalPinsMap, SelectedNodes);
    return Graph;
}
