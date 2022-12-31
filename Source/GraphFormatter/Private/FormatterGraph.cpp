/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EvenlyPlaceStrategy.h"
#include "FastAndSimplePositioningStrategy.h"
#include "Formatter.h"
#include "FormatterSettings.h"
#include "IPositioningStrategy.h"
#include "PriorityPositioningStrategy.h"

#include "BehaviorTree/BTNode.h"
#include "Editor/BehaviorTreeEditor/Classes/BehaviorTreeGraphNode.h"

bool FFormatterEdge::IsCrossing(const FFormatterEdge* Edge) const
{
    return (From->IndexInLayer < Edge->From->IndexInLayer && To->IndexInLayer > Edge->To->IndexInLayer) || (From->IndexInLayer > Edge->From->IndexInLayer && To->IndexInLayer < Edge->To->IndexInLayer);
}

bool FFormatterEdge::IsInnerSegment()
{
    return From->OwningNode->OriginalNode == nullptr && To->OwningNode->OriginalNode == nullptr;
}

FFormatterNode::FFormatterNode(UEdGraphNode* InNode)
    : Guid(InNode->NodeGuid)
    , OriginalNode(InNode)
    , SubGraph(nullptr)
    , Size(FVector2D())
    , PathDepth(0)
    , Position(FVector2D(InNode->NodePosX, InNode->NodePosY))
{
    for (auto Pin : InNode->Pins)
    {
        auto NewPin = new FFormatterPin;
        NewPin->Guid = FGuid::NewGuid();
        NewPin->OriginalPin = Pin;
        NewPin->Direction = Pin->Direction;
        NewPin->OwningNode = this;
        if (Pin->Direction == EGPD_Input)
        {
            InPins.Add(NewPin);
        }
        else
        {
            OutPins.Add(NewPin);
        }
    }
}

FFormatterNode::FFormatterNode(const FFormatterNode& Other)
    : Guid(Other.Guid)
    , OriginalNode(Other.OriginalNode)
    , Size(Other.Size)
    , PathDepth(Other.PathDepth)
    , Position(Other.Position)
{
    if (Other.SubGraph != nullptr)
    {
        SubGraph = new FFormatterGraph(*Other.SubGraph);
    }
    else
    {
        SubGraph = nullptr;
    }
    for (auto Pin : Other.InPins)
    {
        auto NewPin = new FFormatterPin;
        NewPin->Guid = Pin->Guid;
        NewPin->OriginalPin = Pin->OriginalPin;
        NewPin->Direction = Pin->Direction;
        NewPin->OwningNode = this;
        NewPin->NodeOffset = Pin->NodeOffset;
        InPins.Add(NewPin);
    }
    for (auto Pin : Other.OutPins)
    {
        auto NewPin = new FFormatterPin;
        NewPin->Guid = Pin->Guid;
        NewPin->OriginalPin = Pin->OriginalPin;
        NewPin->Direction = Pin->Direction;
        NewPin->OwningNode = this;
        NewPin->NodeOffset = Pin->NodeOffset;
        OutPins.Add(NewPin);
    }
}

FFormatterNode::FFormatterNode()
    : Guid(FGuid::NewGuid())
    , OriginalNode(nullptr)
    , SubGraph(nullptr)
    , Size(FVector2D(1, 1))
    , PathDepth(0)
    , PositioningPriority(INT_MAX)
    , Position(FVector2D::ZeroVector)
{
    auto InPin = new FFormatterPin;
    InPin->Guid = FGuid::NewGuid();
    InPin->OriginalPin = nullptr;
    InPin->Direction = EGPD_Input;
    InPin->OwningNode = this;
    InPin->NodeOffset = FVector2D::ZeroVector;

    auto OutPin = new FFormatterPin;
    OutPin->Guid = FGuid::NewGuid();
    OutPin->OriginalPin = nullptr;
    OutPin->Direction = EGPD_Output;
    OutPin->OwningNode = this;
    OutPin->NodeOffset = FVector2D::ZeroVector;

    InPins.Add(InPin);
    OutPins.Add(OutPin);
}

void FFormatterNode::Connect(FFormatterPin* SourcePin, FFormatterPin* TargetPin, float Weight)
{
    const auto Edge = new FFormatterEdge;
    Edge->From = SourcePin;
    Edge->To = TargetPin;
    Edge->Weight = Weight;
    if (SourcePin->Direction == EGPD_Output)
    {
        OutEdges.Add(Edge);
    }
    else
    {
        InEdges.Add(Edge);
    }
}

void FFormatterNode::Disconnect(FFormatterPin* SourcePin, FFormatterPin* TargetPin)
{
    const auto Predicate = [SourcePin, TargetPin](const FFormatterEdge* Edge)
    {
        return Edge->From == SourcePin && Edge->To == TargetPin;
    };
    if (SourcePin->Direction == EGPD_Output)
    {
        const auto Index = OutEdges.IndexOfByPredicate(Predicate);
        if (Index != INDEX_NONE)
        {
            OutEdges.RemoveAt(Index);
        }
    }
    else
    {
        const auto Index = InEdges.IndexOfByPredicate(Predicate);
        if (Index != INDEX_NONE)
        {
            InEdges.RemoveAt(Index);
        }
    }
}

TArray<FFormatterNode*> FFormatterNode::GetSuccessors() const
{
    TArray<FFormatterNode*> Result;
    for (auto Edge : OutEdges)
    {
        Result.Add(Edge->To->OwningNode);
    }
    return Result;
}

TArray<FFormatterNode*> FFormatterNode::GetPredecessors() const
{
    TArray<FFormatterNode*> Result;
    for (auto Edge : InEdges)
    {
        Result.Add(Edge->To->OwningNode);
    }
    return Result;
}

bool FFormatterNode::IsSource() const
{
    return InEdges.Num() == 0;
}

bool FFormatterNode::IsSink() const
{
    return OutEdges.Num() == 0;
}

bool FFormatterNode::AnySuccessorPathDepthEqu0() const
{
    for (auto OutEdge : OutEdges)
    {
        if (OutEdge->To->OwningNode->PathDepth == 0)
        {
            return true;
        }
    }
    return false;
}

float FFormatterNode::GetLinkedPositionToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction, bool IsHorizontalDirection)
{
    auto& Edges = Direction == EGPD_Input ? InEdges : OutEdges;
    float MedianPosition = 0.0f;
    int32 Count = 0;
    for (auto Edge : Edges)
    {
        if (Edge->To->OwningNode == Node)
        {
            if (IsHorizontalDirection)
            {
                MedianPosition += Edge->From->NodeOffset.Y;
            }
            else
            {
                MedianPosition += Edge->From->NodeOffset.X;
            }
            ++Count;
        }
    }
    if (Count == 0)
    {
        return 0.0f;
    }
    return MedianPosition / Count;
}

float FFormatterNode::GetMaxWeight(EEdGraphPinDirection Direction)
{
    auto& Edges = Direction == EGPD_Input ? InEdges : OutEdges;
    float MaxWeight = 0.0f;
    for (auto Edge : Edges)
    {
        if (MaxWeight < Edge->Weight)
        {
            MaxWeight = Edge->Weight;
        }
    }
    return MaxWeight;
}

float FFormatterNode::GetMaxWeightToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction)
{
    auto& Edges = Direction == EGPD_Input ? InEdges : OutEdges;
    float MaxWeight = 0.0f;
    for (auto Edge : Edges)
    {
        if (Edge->To->OwningNode == Node && MaxWeight < Edge->Weight)
        {
            MaxWeight = Edge->Weight;
        }
    }
    return MaxWeight;
}

bool FFormatterNode::IsCrossingInnerSegment(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer) const
{
    auto EdgesLinkedToUpper = GetEdgeLinkedToLayer(UpperLayer, EGPD_Input);
    auto EdgesBetweenTwoLayers = FFormatterGraph::GetEdgeBetweenTwoLayer(LowerLayer, UpperLayer, this);
    for (auto EdgeLinkedToUpper : EdgesLinkedToUpper)
    {
        for (auto EdgeBetweenTwoLayers : EdgesBetweenTwoLayers)
        {
            if (EdgeBetweenTwoLayers->IsInnerSegment() && EdgeLinkedToUpper->IsCrossing(EdgeBetweenTwoLayers))
            {
                return true;
            }
        }
    }
    return false;
}

FFormatterNode* FFormatterNode::GetMedianUpper() const
{
    TArray<FFormatterNode*> UpperNodes;
    for (auto InEdge : InEdges)
    {
        if (!UpperNodes.Contains(InEdge->To->OwningNode))
        {
            UpperNodes.Add(InEdge->To->OwningNode);
        }
    }
    if (UpperNodes.Num() > 0)
    {
        const int32 m = UpperNodes.Num() / 2;
        return UpperNodes[m];
    }
    return nullptr;
}

FFormatterNode* FFormatterNode::GetMedianLower() const
{
    TArray<FFormatterNode*> LowerNodes;
    for (auto OutEdge : OutEdges)
    {
        if (!LowerNodes.Contains(OutEdge->To->OwningNode))
        {
            LowerNodes.Add(OutEdge->To->OwningNode);
        }
    }
    if (LowerNodes.Num() > 0)
    {
        const int32 m = LowerNodes.Num() / 2;
        return LowerNodes[m];
    }
    return nullptr;
}

TArray<FFormatterNode*> FFormatterNode::GetUppers() const
{
    TSet<FFormatterNode*> UpperNodes;
    for (auto InEdge : InEdges)
    {
        UpperNodes.Add(InEdge->To->OwningNode);
    }
    return UpperNodes.Array();
}

TArray<FFormatterNode*> FFormatterNode::GetLowers() const
{
    TSet<FFormatterNode*> LowerNodes;
    for (auto OutEdge : OutEdges)
    {
        LowerNodes.Add(OutEdge->To->OwningNode);
    }
    return LowerNodes.Array();
}

int32 FFormatterNode::GetInputPinCount() const
{
    return InPins.Num();
}

int32 FFormatterNode::GetInputPinIndex(FFormatterPin* InputPin) const
{
    return InPins.Find(InputPin);
}

int32 FFormatterNode::GetOutputPinCount() const
{
    return OutPins.Num();
}

int32 FFormatterNode::GetOutputPinIndex(FFormatterPin* OutputPin) const
{
    return OutPins.Find(OutputPin);
}

TArray<FFormatterEdge*> FFormatterNode::GetEdgeLinkedToLayer(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const
{
    TArray<FFormatterEdge*> Result;
    const TArray<FFormatterEdge*>& Edges = Direction == EGPD_Output ? OutEdges : InEdges;
    for (auto Edge : Edges)
    {
        for (auto NextLayerNode : Layer)
        {
            if (Edge->To->OwningNode == NextLayerNode)
            {
                Result.Add(Edge);
            }
        }
    }
    return Result;
}

float FFormatterNode::CalcBarycenter(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const
{
    auto Edges = GetEdgeLinkedToLayer(Layer, Direction);
    if (Edges.Num() == 0)
    {
        return 0.0f;
    }
    float Sum = 0.0f;
    for (auto Edge : Edges)
    {
        Sum += Edge->To->IndexInLayer;
    }
    return Sum / Edges.Num();
}

int32 FFormatterNode::CalcPriority(EEdGraphPinDirection Direction) const
{
    if (OriginalNode == nullptr)
    {
        return 0;
    }
    return Direction == EGPD_Output ? OutEdges.Num() : InEdges.Num();
}

FFormatterNode::~FFormatterNode()
{
    for (auto Edge : InEdges)
    {
        delete Edge;
    }
    for (auto Edge : OutEdges)
    {
        delete Edge;
    }
    for (auto Pin : InPins)
    {
        delete Pin;
    }
    for (auto Pin : OutPins)
    {
        delete Pin;
    }
    delete SubGraph;
}

void FFormatterNode::InitPosition(FVector2D InPosition)
{
    Position = InPosition;
}

void FFormatterNode::SetPosition(FVector2D InPosition)
{
    const FVector2D Offset = InPosition - Position;
    Position = InPosition;
    if (SubGraph != nullptr)
    {
        SubGraph->OffsetBy(Offset);
    }
}

FVector2D FFormatterNode::GetPosition() const
{
    return Position;
}

void FFormatterNode::SetSubGraph(FFormatterGraph* InSubGraph)
{
    SubGraph = InSubGraph;
    auto SubGraphInPins = SubGraph->GetInputPins();
    auto SubGraphOutPins = SubGraph->GetOutputPins();
    for (auto Pin : SubGraphInPins)
    {
        auto NewPin = new FFormatterPin();
        NewPin->Guid = Pin->Guid;
        NewPin->OwningNode = this;
        NewPin->Direction = Pin->Direction;
        NewPin->NodeOffset = Pin->NodeOffset;
        NewPin->OriginalPin = Pin->OriginalPin;
        InPins.Add(NewPin);
    }
    for (auto Pin : SubGraphOutPins)
    {
        auto NewPin = new FFormatterPin();
        NewPin->Guid = Pin->Guid;
        NewPin->OwningNode = this;
        NewPin->Direction = Pin->Direction;
        NewPin->NodeOffset = Pin->NodeOffset;
        NewPin->OriginalPin = Pin->OriginalPin;
        OutPins.Add(NewPin);
    }
}

void FFormatterNode::UpdatePinsOffset(FVector2D Border)
{
    if (SubGraph != nullptr)
    {
        auto PinsOffset = SubGraph->GetPinsOffset();
        for (auto Pin : InPins)
        {
            if (PinsOffset.Contains(Pin->OriginalPin))
            {
                Pin->NodeOffset = PinsOffset[Pin->OriginalPin] + Border;
            }
        }
        for (auto Pin : OutPins)
        {
            if (PinsOffset.Contains(Pin->OriginalPin))
            {
                Pin->NodeOffset = PinsOffset[Pin->OriginalPin] + Border;
            }
        }
        InPins.Sort([](const FFormatterPin& A, const FFormatterPin& B)
                    { return A.NodeOffset.Y < B.NodeOffset.Y; });
        OutPins.Sort([](const FFormatterPin& A, const FFormatterPin& B)
                     { return A.NodeOffset.Y < B.NodeOffset.Y; });
    }
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

TArray<FFormatterEdge> FFormatterGraph::GetEdgeForNode(FFormatterNode* Node, TSet<UEdGraphNode*> SelectedNodes)
{
    TArray<FFormatterEdge> Result;
    if (Node->SubGraph)
    {
        const TSet<UEdGraphNode*> InnerSelectedNodes = Node->SubGraph->GetOriginalNodes();
        for (auto SelectedNode : InnerSelectedNodes)
        {
            for (auto Pin : SelectedNode->Pins)
            {
                for (auto LinkedToPin : Pin->LinkedTo)
                {
                    const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
                    if (InnerSelectedNodes.Contains(LinkedToNode) || !SelectedNodes.Contains(LinkedToNode))
                    {
                        continue;
                    }
                    FFormatterPin* From = OriginalPinsMap[Pin];
                    FFormatterPin* To = OriginalPinsMap[LinkedToPin];
                    Result.Add(FFormatterEdge{From, To, GetEdgeWeight(Pin)});
                }
            }
        }
    }
    else
    {
        for (auto Pin : Node->OriginalNode->Pins)
        {
            for (auto LinkedToPin : Pin->LinkedTo)
            {
                const auto LinkedToNode = LinkedToPin->GetOwningNodeUnchecked();
                if (!SelectedNodes.Contains(LinkedToNode))
                {
                    continue;
                }
                FFormatterPin* From = OriginalPinsMap[Pin];
                FFormatterPin* To = OriginalPinsMap[LinkedToPin];
                Result.Add(FFormatterEdge{From, To, GetEdgeWeight(Pin)});
            }
        }
    }
    return Result;
}

TArray<FFormatterNode*> FFormatterGraph::GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes)
{
    TArray<FFormatterNode*> Result;
    for (auto Node : Nodes)
    {
        for (auto OutEdge : Node->OutEdges)
        {
            if (!Nodes.Contains(OutEdge->To->OwningNode))
            {
                Result.Add(OutEdge->To->OwningNode);
            }
        }
    }
    return Result;
}

TArray<FFormatterNode*> FFormatterGraph::GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded)
{
    TArray<FFormatterNode*> Result;
    for (auto Node : Nodes)
    {
        if (!Excluded.Contains(Node) && Node->PathDepth >= i)
        {
            Result.Add(Node);
        }
    }
    return Result;
}

void FFormatterGraph::BuildNodes(TSet<UEdGraphNode*> SelectedNodes)
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
                    FFormatterNode* CollapsedNode = CollapseCommentNode(CommentNode, NodesUnderComment);
                    AddNode(CollapsedNode);
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

    for (auto Node : SelectedNodes)
    {
        FFormatterNode* NodeData = new FFormatterNode(Node);
        AddNode(NodeData);
    }
}

void FFormatterGraph::BuildEdges(TSet<UEdGraphNode*> SelectedNodes)
{
    for (auto Node : Nodes)
    {
        auto Edges = GetEdgeForNode(Node, SelectedNodes);
        for (auto Edge : Edges)
        {
            Node->Connect(Edge.From, Edge.To, Edge.Weight);
        }
    }
}

TArray<UEdGraphNode_Comment*> FFormatterGraph::GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes)
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
                      { return A.CommentDepth < B.CommentDepth; });
    return CommentNodes;
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

TSet<UEdGraphNode*> FFormatterGraph::GetDirectConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option)
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

TSet<UEdGraphNode*> FFormatterGraph::GetNodesConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option)
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

bool FFormatterGraph::GetNodesConnectCenter(const TSet<UEdGraphNode*>& SelectedNodes, FVector2D& OutCenter, EInOutOption Option, bool bInvert)
{
    FSlateRect Bound;
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
                        FSlateRect PosZeroBound = FSlateRect::FromPointAndExtent(LinkedPos, FVector2D(0, 0));
                        Bound = Bound.IsValid() ? Bound.Expand(PosZeroBound) : PosZeroBound;
                    }
                }
            }
        }
    }
    if (Bound.IsValid())
    {
        OutCenter = Bound.GetCenter();
        return true;
    }
    else
    {
        return false;
    }
}

FFormatterNode* FFormatterGraph::CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> SelectedNodes) const
{
    FFormatterNode* Node = new FFormatterNode(CommentNode);
    if (SelectedNodes.Num() > 0)
    {
        FFormatterGraph* SubGraph = new FFormatterGraph(SelectedNodes);
        float BorderHeight = FFormatter::Instance().GetCommentNodeTitleHeight(CommentNode);
        const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
        SubGraph->SetBorder(Settings.CommentBorder, BorderHeight + Settings.CommentBorder, Settings.CommentBorder, Settings.CommentBorder);
        Node->SetSubGraph(SubGraph);
    }
    return Node;
}

void FFormatterGraph::AddNode(FFormatterNode* InNode)
{
    Nodes.Add(InNode);
    NodesMap.Add(InNode->Guid, InNode);
    if (InNode->SubGraph != nullptr)
    {
        SubGraphs.Add(InNode->Guid, InNode->SubGraph);
    }
    for (auto Pin : InNode->InPins)
    {
        if (Pin->OriginalPin != nullptr)
        {
            OriginalPinsMap.Add(Pin->OriginalPin, Pin);
        }
        PinsMap.Add(Pin->Guid, Pin);
    }
    for (auto Pin : InNode->OutPins)
    {
        if (Pin->OriginalPin != nullptr)
        {
            OriginalPinsMap.Add(Pin->OriginalPin, Pin);
        }
        PinsMap.Add(Pin->Guid, Pin);
    }
}

void FFormatterGraph::RemoveNode(FFormatterNode* NodeToRemove)
{
    TArray<FFormatterEdge*> Edges = NodeToRemove->InEdges;
    for (auto Edge : Edges)
    {
        Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
    }
    Edges = NodeToRemove->OutEdges;
    for (auto Edge : Edges)
    {
        Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
    }
    Nodes.Remove(NodeToRemove);
    NodesMap.Remove(NodeToRemove->Guid);
    SubGraphs.Remove(NodeToRemove->Guid);
    for (auto Pin : NodeToRemove->InPins)
    {
        OriginalPinsMap.Remove(Pin->OriginalPin);
        PinsMap.Remove(Pin->Guid);
    }
    for (auto Pin : NodeToRemove->OutPins)
    {
        OriginalPinsMap.Remove(Pin->OriginalPin);
        PinsMap.Remove(Pin->Guid);
    }
    delete NodeToRemove;
}

void FFormatterGraph::RemoveCycle()
{
    auto ClonedGraph = new FFormatterGraph(*this);
    while (auto SourceNode = ClonedGraph->FindSourceNode())
    {
        ClonedGraph->RemoveNode(SourceNode);
    }
    while (auto SinkNode = ClonedGraph->FindSinkNode())
    {
        ClonedGraph->RemoveNode(SinkNode);
    }
    while (auto MedianNode = ClonedGraph->FindMaxDegreeDiffNode())
    {
        for (auto Edge : MedianNode->InEdges)
        {
            FFormatterPin* From = PinsMap[Edge->From->Guid];
            FFormatterPin* To = PinsMap[Edge->To->Guid];
            NodesMap[MedianNode->Guid]->Disconnect(From, To);
            To->OwningNode->Disconnect(To, From);
        }
        ClonedGraph->RemoveNode(MedianNode);
    }
    delete ClonedGraph;
}

FFormatterNode* FFormatterGraph::FindSourceNode() const
{
    for (auto Node : Nodes)
    {
        if (Node->IsSource())
        {
            return Node;
        }
    }
    return nullptr;
}

FFormatterNode* FFormatterGraph::FindSinkNode() const
{
    for (auto Node : Nodes)
    {
        if (Node->IsSink())
        {
            return Node;
        }
    }
    return nullptr;
}

FFormatterNode* FFormatterGraph::FindMaxDegreeDiffNode() const
{
    FFormatterNode* Result = nullptr;
    int32 MaxDegreeDiff = -INT32_MAX;
    for (auto Node : Nodes)
    {
        const int32 DegreeDiff = Node->OutEdges.Num() - Node->InEdges.Num();
        if (DegreeDiff > MaxDegreeDiff)
        {
            MaxDegreeDiff = DegreeDiff;
            Result = Node;
        }
    }
    return Result;
}

void FFormatterGraph::BuildNodesAndEdges(TSet<UEdGraphNode*> SelectedNodes)
{
    BuildNodes(SelectedNodes);
    BuildEdges(SelectedNodes);
    Nodes.Sort([](const FFormatterNode& A, const FFormatterNode& B)
               { return A.GetPosition().Y < B.GetPosition().Y; });
}

void FFormatterGraph::BuildIsolated()
{
    auto FoundIsolatedGraphs = FindIsolated();
    if (FoundIsolatedGraphs.Num() > 1)
    {
        for (const auto& IsolatedNodes : FoundIsolatedGraphs)
        {
            auto NewGraph = new FFormatterGraph(IsolatedNodes);
            IsolatedGraphs.Add(NewGraph);
        }
    }
}

FFormatterGraph::FFormatterGraph(const TSet<UEdGraphNode*>& SelectedNodes)
{
    BuildNodesAndEdges(SelectedNodes);
    BuildIsolated();
}

FFormatterGraph::FFormatterGraph(const FFormatterGraph& Other)
{
    for (auto Node : Other.Nodes)
    {
        FFormatterNode* Cloned = new FFormatterNode(*Node);
        AddNode(Cloned);
    }
    for (auto Node : Other.Nodes)
    {
        for (auto Edge : Node->InEdges)
        {
            FFormatterPin* From = PinsMap[Edge->From->Guid];
            FFormatterPin* To = PinsMap[Edge->To->Guid];
            NodesMap[Node->Guid]->Connect(From, To, Edge->Weight);
        }
        for (auto Edge : Node->OutEdges)
        {
            FFormatterPin* From = PinsMap[Edge->From->Guid];
            FFormatterPin* To = PinsMap[Edge->To->Guid];
            NodesMap[Node->Guid]->Connect(From, To, Edge->Weight);
        }
    }
    for (auto Isolated : Other.IsolatedGraphs)
    {
        IsolatedGraphs.Add(new FFormatterGraph(*Isolated));
    }
}

FFormatterGraph::~FFormatterGraph()
{
    for (auto Node : Nodes)
    {
        delete Node;
    }
    for (auto Graph : IsolatedGraphs)
    {
        delete Graph;
    }
}

TArray<TSet<UEdGraphNode*>> FFormatterGraph::FindIsolated()
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
            IsolatedNodes.Add(Top->OriginalNode);
            if (Top->SubGraph != nullptr)
            {
                IsolatedNodes.Append(Top->SubGraph->GetOriginalNodes());
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

int32 FFormatterGraph::AssignPathDepthForNodes() const
{
    int32 LongestPath = 1;
    while (true)
    {
        auto Leaves = GetLeavesWithPathDepth0();
        if (Leaves.Num() == 0)
        {
            break;
        }
        for (auto leaf : Leaves)
        {
            leaf->PathDepth = LongestPath;
        }
        LongestPath++;
    }
    LongestPath--;
    return LongestPath;
}

void FFormatterGraph::CalculatePinsIndex(const TArray<TArray<FFormatterNode*>>& Order)
{
    for (int32 i = 0; i < Order.Num(); i++)
    {
        auto& Layer = Order[i];
        CalculatePinsIndexInLayer(Layer);
    }
}

void FFormatterGraph::CalculatePinsIndexInLayer(const TArray<FFormatterNode*>& Layer)
{
    int32 InPinStartIndex = 0, OutPinStartIndex = 0;
    for (int32 j = 0; j < Layer.Num(); j++)
    {
        for (auto InPin : Layer[j]->InPins)
        {
            InPin->IndexInLayer = InPinStartIndex + Layer[j]->GetInputPinIndex(InPin);
        }
        for (auto OutPin : Layer[j]->OutPins)
        {
            OutPin->IndexInLayer = OutPinStartIndex + Layer[j]->GetOutputPinIndex(OutPin);
        }
        OutPinStartIndex += Layer[j]->GetOutputPinCount();
        InPinStartIndex += Layer[j]->GetInputPinCount();
    }
}

TArray<FFormatterNode*> FFormatterGraph::GetLeavesWithPathDepth0() const
{
    TArray<FFormatterNode*> Result;
    for (auto Node : Nodes)
    {
        if (Node->PathDepth != 0 || Node->AnySuccessorPathDepthEqu0())
        {
            continue;
        }
        Result.Add(Node);
    }
    return Result;
}

static bool BehaviorTreeNodeComparer(const FFormatterNode& A, const FFormatterNode& B)
{
    UBehaviorTreeGraphNode* StateNodeA = static_cast<UBehaviorTreeGraphNode*>(A.OriginalNode);
    UBTNode* BTNodeA = static_cast<UBTNode*>(StateNodeA->NodeInstance);
    int32 IndexA = 0;
    IndexA = (BTNodeA && BTNodeA->GetExecutionIndex() < 0xffff) ? BTNodeA->GetExecutionIndex() : -1;
    UBehaviorTreeGraphNode* StateNodeB = static_cast<UBehaviorTreeGraphNode*>(B.OriginalNode);
    UBTNode* BTNodeB = static_cast<UBTNode*>(StateNodeB->NodeInstance);
    int32 IndexB = 0;
    IndexB = (BTNodeB && BTNodeB->GetExecutionIndex() < 0xffff) ? BTNodeB->GetExecutionIndex() : -1;
    return IndexA < IndexB;
}

void FFormatterGraph::DoLayering()
{
    LayeredList.Empty();
    TSet<FFormatterNode*> Set;
    for (int32 i = AssignPathDepthForNodes(); i != 0; i--)
    {
        TSet<FFormatterNode*> Layer;
        auto Successors = GetSuccessorsForNodes(Set);
        auto NodesToProcess = GetNodesGreaterThan(i, Set);
        NodesToProcess.Append(Successors);
        for (auto Node : NodesToProcess)
        {
            auto Predecessors = Node->GetPredecessors();
            bool bPredecessorsFinished = true;
            for (auto Predecessor : Predecessors)
            {
                if (!Set.Contains(Predecessor))
                {
                    bPredecessorsFinished = false;
                    break;
                }
            }
            if (bPredecessorsFinished)
            {
                Layer.Add(Node);
            }
        }
        Set.Append(Layer);
        TArray<FFormatterNode*> Array = Layer.Array();
        if (FFormatter::Instance().IsBehaviorTree)
        {
            Array.Sort(BehaviorTreeNodeComparer);
        }
        LayeredList.Add(Array);
    }
}

void FFormatterGraph::AddDummyNodes()
{
    for (int i = 0; i < LayeredList.Num() - 1; i++)
    {
        auto& Layer = LayeredList[i];
        auto& NextLayer = LayeredList[i + 1];
        for (auto Node : Layer)
        {
            TArray<FFormatterEdge*> LongEdges;
            for (auto Edge : Node->OutEdges)
            {
                if (!NextLayer.Contains(Edge->To->OwningNode))
                {
                    LongEdges.Add(Edge);
                }
            }
            for (auto Edge : LongEdges)
            {
                Node->Disconnect(Edge->From, Edge->To);
                auto dummyNode = new FFormatterNode();
                AddNode(dummyNode);
                Node->Connect(Edge->From, dummyNode->InPins[0], Edge->Weight);
                dummyNode->Connect(dummyNode->InPins[0], Edge->From, Edge->Weight);
                dummyNode->Connect(dummyNode->OutPins[0], Edge->To, Edge->Weight);
                Edge->To->OwningNode->Disconnect(Edge->To, Edge->From);
                Edge->To->OwningNode->Connect(Edge->To, dummyNode->OutPins[0], Edge->Weight);
                NextLayer.Add(dummyNode);
            }
        }
    }
}

void FFormatterGraph::SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction)
{
    if (Order.Num() < 2)
    {
        return;
    }
    const int Start = Direction == EGPD_Output ? Order.Num() - 2 : 1;
    const int End = Direction == EGPD_Output ? -1 : Order.Num();
    const int Step = Direction == EGPD_Output ? -1 : 1;
    for (int i = Start; i != End; i += Step)
    {
        auto& FixedLayer = Order[i - Step];
        auto& FreeLayer = Order[i];
        for (FFormatterNode* Node : FreeLayer)
        {
            Node->OrderValue = Node->CalcBarycenter(FixedLayer, Direction);
        }
        FreeLayer.StableSort([](const FFormatterNode& A, const FFormatterNode& B) -> bool
                             { return A.OrderValue < B.OrderValue; });
        CalculatePinsIndexInLayer(FreeLayer);
    }
}

TArray<FFormatterEdge*> FFormatterGraph::GetEdgeBetweenTwoLayer(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer, const FFormatterNode* ExcludedNode)
{
    TArray<FFormatterEdge*> Result;
    for (auto Node : LowerLayer)
    {
        if (ExcludedNode == Node)
        {
            continue;
        }
        Result += Node->GetEdgeLinkedToLayer(UpperLayer, EGPD_Input);
    }
    return Result;
}

TArray<FSlateRect> FFormatterGraph::CalculateLayersBound(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection)
{
    TArray<FSlateRect> LayersBound;
    const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
    FSlateRect TotalBound;
    FVector2D Spacing;
    if (IsHorizontalDirection)
    {
        Spacing = FVector2D(Settings.HorizontalSpacing, 0);
    }
    else
    {
        Spacing = FVector2D(0, Settings.VerticalSpacing);
    }
    for (int32 i = 0; i < InLayeredNodes.Num(); i++)
    {
        const auto& Layer = InLayeredNodes[i];
        FSlateRect Bound;
        FVector2D Position;
        if (TotalBound.IsValid())
        {
            Position = TotalBound.GetBottomRight() + Spacing;
        }
        else
        {
            Position = FVector2D(0, 0);
        }
        for (auto Node : Layer)
        {
            if (Bound.IsValid())
            {
                Bound = Bound.Expand(FSlateRect::FromPointAndExtent(Position, Node->Size));
            }
            else
            {
                Bound = FSlateRect::FromPointAndExtent(Position, Node->Size);
            }
        }
        LayersBound.Add(Bound);
        if (TotalBound.IsValid())
        {
            TotalBound = TotalBound.Expand(Bound);
        }
        else
        {
            TotalBound = Bound;
        }
    }
    return LayersBound;
}

static int32 CalculateCrossing(const TArray<TArray<FFormatterNode*>>& Order)
{
    FFormatterGraph::CalculatePinsIndex(Order);
    int32 CrossingValue = 0;
    for (int i = 1; i < Order.Num(); i++)
    {
        const auto& UpperLayer = Order[i - 1];
        const auto& LowerLayer = Order[i];
        TArray<FFormatterEdge*> NodeEdges = FFormatterGraph::GetEdgeBetweenTwoLayer(LowerLayer, UpperLayer);
        while (NodeEdges.Num() != 0)
        {
            const auto Edge1 = NodeEdges.Pop();
            for (const auto Edge2 : NodeEdges)
            {
                if (Edge1->IsCrossing(Edge2))
                {
                    CrossingValue++;
                }
            }
        }
    }
    return CrossingValue;
}

void FFormatterGraph::DoOrderingSweep()
{
    const UFormatterSettings* Settings = GetDefault<UFormatterSettings>();
    auto Best = LayeredList;
    auto Order = LayeredList;
    int32 BestCrossing = INT_MAX;
    for (int i = 0; i < Settings->MaxOrderingIterations; i++)
    {
        SortInLayer(Order, i % 2 == 0 ? EGPD_Input : EGPD_Output);
        const int32 NewCrossing = CalculateCrossing(Order);
        if (NewCrossing < BestCrossing)
        {
            Best = Order;
            BestCrossing = NewCrossing;
        }
    }
    LayeredList = Best;
}

void FFormatterGraph::DoPositioning()
{
    const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();

    if (FFormatter::Instance().IsVerticalLayout)
    {
        FFastAndSimplePositioningStrategy FastAndSimplePositioningStrategy(LayeredList, false);
        TotalBound = FastAndSimplePositioningStrategy.GetTotalBound();
        return;
    }

    if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EEvenlyInLayer)
    {
        FEvenlyPlaceStrategy LeftToRightPositioningStrategy(LayeredList);
        TotalBound = LeftToRightPositioningStrategy.GetTotalBound();
    }
    else if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian || Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop)
    {
        FFastAndSimplePositioningStrategy FastAndSimplePositioningStrategy(LayeredList);
        TotalBound = FastAndSimplePositioningStrategy.GetTotalBound();
    }
    else if (Settings.PositioningAlgorithm == EGraphFormatterPositioningAlgorithm::ELayerSweep)
    {
        FPriorityPositioningStrategy PriorityPositioningStrategy(LayeredList);
        TotalBound = PriorityPositioningStrategy.GetTotalBound();
    }
}

TMap<UEdGraphPin*, FVector2D> FFormatterGraph::GetPinsOffset()
{
    TMap<UEdGraphPin*, FVector2D> Result;
    if (IsolatedGraphs.Num() > 0)
    {
        for (auto IsolatedGraph : IsolatedGraphs)
        {
            auto SubBound = IsolatedGraph->GetTotalBound();
            auto Offset = SubBound.GetTopLeft() - TotalBound.GetTopLeft();
            auto SubOffsets = IsolatedGraph->GetPinsOffset();
            for (auto& SubOffsetPair : SubOffsets)
            {
                SubOffsetPair.Value = SubOffsetPair.Value + Offset;
            }
            Result.Append(SubOffsets);
        }
        return Result;
    }
    for (auto Node : Nodes)
    {
        for (auto OutPin : Node->OutPins)
        {
            FVector2D PinOffset = Node->Position + OutPin->NodeOffset - TotalBound.GetTopLeft();
            Result.Add(OutPin->OriginalPin, PinOffset);
        }
        for (auto InPin : Node->InPins)
        {
            FVector2D PinOffset = Node->Position + InPin->NodeOffset - TotalBound.GetTopLeft();
            Result.Add(InPin->OriginalPin, PinOffset);
        }
    }
    return Result;
}

TArray<FFormatterPin*> FFormatterGraph::GetInputPins() const
{
    TSet<FFormatterPin*> Result;
    if (IsolatedGraphs.Num() > 0)
    {
        for (auto IsolatedGraph : IsolatedGraphs)
        {
            Result.Append(IsolatedGraph->GetInputPins());
        }
        return Result.Array();
    }
    for (auto Node : Nodes)
    {
        for (auto Pin : Node->InPins)
        {
            Result.Add(Pin);
        }
    }
    return Result.Array();
}

TArray<FFormatterPin*> FFormatterGraph::GetOutputPins() const
{
    TSet<FFormatterPin*> Result;
    if (IsolatedGraphs.Num() > 0)
    {
        for (auto IsolatedGraph : IsolatedGraphs)
        {
            Result.Append(IsolatedGraph->GetOutputPins());
        }
        return Result.Array();
    }
    for (auto Node : Nodes)
    {
        for (auto Pin : Node->OutPins)
        {
            Result.Add(Pin);
        }
    }
    return Result.Array();
}

TSet<UEdGraphNode*> FFormatterGraph::GetOriginalNodes() const
{
    TSet<UEdGraphNode*> Result;
    if (IsolatedGraphs.Num() > 0)
    {
        for (auto IsolatedGraph : IsolatedGraphs)
        {
            Result.Append(IsolatedGraph->GetOriginalNodes());
        }
        return Result;
    }
    for (auto Node : Nodes)
    {
        if (SubGraphs.Contains(Node->Guid))
        {
            Result.Append(SubGraphs[Node->Guid]->GetOriginalNodes());
        }
        if (Node->OriginalNode != nullptr)
        {
            Result.Add(Node->OriginalNode);
        }
    }
    return Result;
}

void FFormatterGraph::SetBorder(float Left, float Top, float Right, float Bottom)
{
    this->Border = FSlateRect(Left, Top, Right, Bottom);
}

FSlateRect FFormatterGraph::GetBorder() const
{
    return Border;
}

void FFormatterGraph::CalculateNodesSize()
{
    if (IsolatedGraphs.Num() > 1)
    {
        for (auto IsolatedGraph : IsolatedGraphs)
        {
            IsolatedGraph->CalculateNodesSize();
        }
    }
    else
    {
        for (auto Node : Nodes)
        {
            if (Node->OriginalNode != nullptr)
            {
                if (SubGraphs.Contains(Node->Guid))
                {
                    SubGraphs[Node->Guid]->CalculateNodesSize();
                }
                Node->Size = FFormatter::Instance().GetNodeSize(Node->OriginalNode);
            }
        }
    }
}

void FFormatterGraph::CalculatePinsOffset()
{
    if (IsolatedGraphs.Num() > 1)
    {
        for (auto IsolatedGraph : IsolatedGraphs)
        {
            IsolatedGraph->CalculatePinsOffset();
        }
    }
    else
    {
        for (auto Node : Nodes)
        {
            if (Node->OriginalNode != nullptr)
            {
                if (SubGraphs.Contains(Node->Guid))
                {
                    SubGraphs[Node->Guid]->CalculatePinsOffset();
                }
                for (auto Pin : Node->InPins)
                {
                    Pin->NodeOffset = FFormatter::Instance().GetPinOffset(Pin->OriginalPin);
                }
                for (auto Pin : Node->OutPins)
                {
                    Pin->NodeOffset = FFormatter::Instance().GetPinOffset(Pin->OriginalPin);
                }
            }
        }
    }
}

void FFormatterGraph::Format()
{
    const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
    CalculateNodesSize();
    CalculatePinsOffset();
    if (IsolatedGraphs.Num() > 1)
    {
        FSlateRect PreBound;
        for (auto isolatedGraph : IsolatedGraphs)
        {
            isolatedGraph->Format();

            if (PreBound.IsValid())
            {
                FVector2D StartCorner = FFormatter::Instance().IsVerticalLayout ? PreBound.GetTopRight() : PreBound.GetBottomLeft();
                isolatedGraph->SetPosition(StartCorner);
            }
            auto Bound = isolatedGraph->GetTotalBound();
            if (TotalBound.IsValid())
            {
                TotalBound = TotalBound.Expand(Bound);
            }
            else
            {
                TotalBound = Bound;
            }

            FVector2D Offset = FFormatter::Instance().IsVerticalLayout ? FVector2D(Settings.VerticalSpacing, 0) : FVector2D(0, Settings.VerticalSpacing);
            PreBound = TotalBound.OffsetBy(Offset);
        }
    }
    else
    {
        for (auto SubGraphPair : SubGraphs)
        {
            auto SubGraph = SubGraphPair.Value;
            auto Node = NodesMap[SubGraphPair.Key];
            SubGraph->Format();
            auto SubGraphBorder = SubGraph->GetBorder();
            Node->UpdatePinsOffset(FVector2D(SubGraphBorder.Left, SubGraphBorder.Top));
            auto Bound = SubGraph->GetTotalBound();
            Node->InitPosition(Bound.GetTopLeft() - FVector2D(SubGraphBorder.Left, SubGraphBorder.Top));
            Node->Size = SubGraph->GetTotalBound().GetSize() + FVector2D(SubGraphBorder.Left + SubGraphBorder.Right, SubGraphBorder.Bottom + SubGraphBorder.Top);
        }
        if (Nodes.Num() > 0)
        {
            RemoveCycle();
            DoLayering();
            AddDummyNodes();
            if (!FFormatter::Instance().IsBehaviorTree)
            {
                DoOrderingSweep();
            }
            DoPositioning();
        }
    }
}

FSlateRect FFormatterGraph::GetTotalBound() const
{
    return TotalBound;
}

void FFormatterGraph::OffsetBy(const FVector2D& InOffset)
{
    if (IsolatedGraphs.Num() > 0)
    {
        for (auto isolatedGraph : IsolatedGraphs)
        {
            isolatedGraph->OffsetBy(InOffset);
        }
    }
    else
    {
        for (auto Node : Nodes)
        {
            Node->SetPosition(Node->GetPosition() + InOffset);
        }
    }
    TotalBound = TotalBound.OffsetBy(InOffset);
}

void FFormatterGraph::SetPosition(const FVector2D& Position)
{
    const FVector2D Offset = Position - TotalBound.GetTopLeft();
    OffsetBy(Offset);
}

TMap<UEdGraphNode*, FSlateRect> FFormatterGraph::GetBoundMap()
{
    TMap<UEdGraphNode*, FSlateRect> Result;
    if (IsolatedGraphs.Num() > 0)
    {
        for (auto Graph : IsolatedGraphs)
        {
            Result.Append(Graph->GetBoundMap());
        }
        return Result;
    }
    for (auto Node : Nodes)
    {
        if (Node->OriginalNode == nullptr)
        {
            continue;
        }
        Result.Add(Node->OriginalNode, FSlateRect::FromPointAndExtent(Node->GetPosition(), Node->Size));
        if (SubGraphs.Contains(Node->Guid))
        {
            Result.Append(SubGraphs[Node->Guid]->GetBoundMap());
        }
    }
    return Result;
}
