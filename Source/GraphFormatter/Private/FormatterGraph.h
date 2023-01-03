/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Formatter.h"
#include "EdGraph/EdGraph.h"
#include "Layout/SlateRect.h"

class IPositioningStrategy;
class FFormatterGraph;
class FFormatterNode;

class UEdGraphNode;
class UEdGraphNode_Comment;

DECLARE_DELEGATE_RetVal_OneParam(FVector2D, FSizeCalculator, UEdGraphNode*);

class FFormatterPin
{
public:
    FGuid Guid;
    UEdGraphPin* OriginalPin{nullptr};
    EEdGraphPinDirection Direction{EGPD_Input};
    FFormatterNode* OwningNode{nullptr};
    FVector2D NodeOffset;
    int32 IndexInLayer{-1};
};

class FFormatterEdge
{
public:
    FFormatterPin* From;
    FFormatterPin* To;
    float Weight = 1;
    bool IsCrossing(const FFormatterEdge* Edge) const;
    bool IsInnerSegment();
};

class FFormatterNode
{
public:
    FGuid Guid;
    UEdGraphNode* OriginalNode;
    FFormatterGraph* SubGraph;
    FVector2D Size;
    TArray<FFormatterEdge*> InEdges;
    TArray<FFormatterEdge*> OutEdges;
    TArray<FFormatterPin*> InPins;
    TArray<FFormatterPin*> OutPins;
    int32 PathDepth;
    int32 PositioningPriority;
    FFormatterNode(UEdGraphNode* InNode);
    FFormatterNode(const FFormatterNode& Other);
    FFormatterNode();
    static FFormatterNode* CreateDummy();
    ~FFormatterNode();
    void Connect(FFormatterPin* SourcePin, FFormatterPin* TargetPin, float Weight = 1);
    void Disconnect(FFormatterPin* SourcePin, FFormatterPin* TargetPin);
    TArray<FFormatterNode*> GetSuccessors() const;
    TArray<FFormatterNode*> GetPredecessors() const;
    bool IsSource() const;
    bool IsSink() const;
    bool AnySuccessorPathDepthEqu0() const;
    bool IsCrossingInnerSegment(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer) const;
    FFormatterNode* GetMedianUpper() const;
    FFormatterNode* GetMedianLower() const;
    TArray<FFormatterNode*> GetUppers() const;
    TArray<FFormatterNode*> GetLowers() const;
    int32 GetInputPinCount() const;
    int32 GetInputPinIndex(FFormatterPin* InputPin) const;
    int32 GetOutputPinCount() const;
    int32 GetOutputPinIndex(FFormatterPin* OutputPin) const;
    TArray<FFormatterEdge*> GetEdgeLinkedToLayer(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const;
    float CalcBarycenter(const TArray<FFormatterNode*>& Layer, EEdGraphPinDirection Direction) const;
    float GetLinkedPositionToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction, bool IsHorizontalDirection = true);
    float GetMaxWeight(EEdGraphPinDirection Direction);
    float GetMaxWeightToNode(const FFormatterNode* Node, EEdGraphPinDirection Direction);
    int32 CalcPriority(EEdGraphPinDirection Direction) const;
    void InitPosition(FVector2D InPosition);
    void SetPosition(FVector2D InPosition);
    FVector2D GetPosition() const;
    void SetSubGraph(FFormatterGraph* InSubGraph);
    void UpdatePinsOffset(FVector2D Border);
    friend class FFormatterGraph;
private:
    float OrderValue{0.0f};
    FVector2D Position;
};

class FFormatterGraph
{
public:
    void BuildIsolated();
    FFormatterGraph(const TSet<UEdGraphNode*>& SelectedNodes, bool InIsParameterGroup = false);
    FFormatterGraph(const FFormatterGraph& Other);
    ~FFormatterGraph();
    void AddNode(FFormatterNode* InNode);
    void RemoveNode(FFormatterNode* NodeToRemove);
    void Format();
    FSlateRect GetTotalBound() const;
    TMap<UEdGraphNode*, FSlateRect> GetBoundMap();
    void SetPosition(const FVector2D& Position);
    void OffsetBy(const FVector2D& InOffset);
    TMap<UEdGraphPin*, FVector2D> GetPinsOffset();
    TArray<FFormatterPin*> GetInputPins() const;
    TArray<FFormatterPin*> GetOutputPins() const;
    TSet<UEdGraphNode*> GetOriginalNodes() const;
    void SetBorder(float Left, float Top, float Right, float Bottom);
    FSlateRect GetBorder() const;

    static TArray<FFormatterEdge*> GetEdgeBetweenTwoLayer(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer, const FFormatterNode* ExcludedNode = nullptr);
    static TArray<FSlateRect> CalculateLayersBound(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection = true, bool IsParameterGroup = false);
    static void CalculatePinsIndex(const TArray<TArray<FFormatterNode*>>& Order);
    static void CalculatePinsIndexInLayer(const TArray<FFormatterNode*>& Layer);
    static TArray<UEdGraphNode_Comment*> GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes);

    enum class EInOutOption
    {
        EIOO_ALL,
        EIOO_IN,
        EIOO_OUT
    };
    static TSet<UEdGraphNode*> GetDirectConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option);
    static TSet<UEdGraphNode*> GetNodesConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option);
    static bool GetNodesConnectCenter(const TSet<UEdGraphNode*>& SelectedNodes, FVector2D& OutCenter, EInOutOption Option, bool bInvert = false);

    void CalculateNodesSize();
    void CalculatePinsOffset();

    const TArray<FFormatterNode*>& GetAllNodes() const;

private:
    TArray<TSet<UEdGraphNode*>> FindIsolated();
    TArray<FFormatterEdge> GetEdgeForNode(FFormatterNode* Node, TSet<UEdGraphNode*> SelectedNodes);
    static TArray<FFormatterNode*> GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes);
    TArray<FFormatterNode*> GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded);
    void BuildNodes(TSet<UEdGraphNode*> SelectedNodes);
    void BuildEdges(TSet<UEdGraphNode*> SelectedNodes);
    void BuildNodesAndEdges(TSet<UEdGraphNode*> SelectedNodes);

    void DoLayering();
    void RemoveCycle();
    void AddDummyNodes();
    void SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction);
    void DoOrderingSweep();
    void DoPositioning();
    FFormatterNode* CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> SelectedNodes) const;
    FFormatterNode* CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group) const;
    FFormatterNode* FindSourceNode() const;
    FFormatterNode* FindSinkNode() const;
    FFormatterNode* FindMaxDegreeDiffNode() const;
    TArray<FFormatterNode*> GetLeavesWithPathDepth0() const;
    int32 AssignPathDepthForNodes() const;

    TArray<FFormatterNode*> Nodes;
    TMap<FGuid, FFormatterNode*> NodesMap;
    TMap<FGuid, FFormatterGraph*> SubGraphs;
    TMap<FGuid, FFormatterPin*> PinsMap;
    TMap<UEdGraphPin*, FFormatterPin*> OriginalPinsMap;
    TArray<TArray<FFormatterNode*>> LayeredList;
    TArray<FFormatterGraph*> IsolatedGraphs;
    FSlateRect TotalBound;
    FSlateRect Border = FSlateRect(0, 0, 0, 0);
    bool IsParameterGroup = false;
};
