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
class FConnectedGraph;
class FFormatterNode;
class FFormatterGraph;

class UEdGraphNode;
class UEdGraphNode_Comment;

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
    friend class FConnectedGraph;
private:
    float OrderValue{0.0f};
    FVector2D Position;
};

class FFormatterGraph
{
public:
    static FFormatterGraph* BuildIsolated(TSet<UEdGraphNode*> Nodes);
    static TArray<FSlateRect> CalculateLayersBound(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection = true, bool IsParameterGroup = false);
    static TArray<UEdGraphNode_Comment*> GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes);
    static FFormatterNode* CollapseCommentNode(UEdGraphNode* CommentNode, TSet<UEdGraphNode*> NodesUnderComment);
    static FFormatterNode* CollapseGroup(UEdGraphNode* MainNode, TSet<UEdGraphNode*> Group);
    enum class EInOutOption
    {
        EIOO_ALL,
        EIOO_IN,
        EIOO_OUT
    };
    static TSet<UEdGraphNode*> GetDirectConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option);
    static TSet<UEdGraphNode*> GetNodesConnected(const TSet<UEdGraphNode*>& SelectedNodes, EInOutOption Option);
    static bool GetNodesConnectCenter(const TSet<UEdGraphNode*>& SelectedNodes, FVector2D& OutCenter, EInOutOption Option, bool bInvert = false);

    FFormatterGraph(const FFormatterGraph& Other);
    virtual ~FFormatterGraph();
    virtual FFormatterGraph* Clone();

    explicit FFormatterGraph(bool InIsParameterGroup = false);
    void BuildNodes(TSet<UEdGraphNode*> SelectedNodes);
    void BuildEdges(TSet<UEdGraphNode*> SelectedNodes);
    void BuildNodesAndEdges(TSet<UEdGraphNode*> SelectedNodes);

    TArray<FFormatterEdge> GetEdgeForNode(FFormatterNode* Node, TSet<UEdGraphNode*> SelectedNodes);
    TArray<TSet<UEdGraphNode*>> FindIsolated();
    void AddNode(FFormatterNode* InNode);
    virtual TMap<UEdGraphPin*, FVector2D> GetPinsOffset() { return {}; }
    virtual TArray<FFormatterPin*> GetInputPins() const { return {}; }
    virtual TArray<FFormatterPin*> GetOutputPins() const { return {}; }
    virtual TSet<UEdGraphNode*> GetOriginalNodes() const { return {}; }
    virtual void Format() { }
    virtual void OffsetBy(const FVector2D& InOffset) { };
    virtual void SetPosition(const FVector2D& Position);
    virtual TMap<UEdGraphNode*, FSlateRect> GetBoundMap() { return {}; }
    FSlateRect GetTotalBound() const { return TotalBound; }
    void SetBorder(float Left, float Top, float Right, float Bottom);
    FSlateRect GetBorder() const;

protected:
    FSlateRect TotalBound;
    bool IsParameterGroup = false;
    TArray<FFormatterNode*> Nodes;
    TMap<FGuid, FFormatterNode*> NodesMap;
    TMap<FGuid, FFormatterGraph*> SubGraphs;
    TMap<UEdGraphPin*, FFormatterPin*> OriginalPinsMap;
    TMap<FGuid, FFormatterPin*> PinsMap;
    FSlateRect Border = FSlateRect(0, 0, 0, 0);
};

class FDisconnectedGraph : public FFormatterGraph
{
    TArray<FFormatterGraph*> IsolatedGraphs;
public:
    void AddGraph(FFormatterGraph* Graph);
    virtual ~FDisconnectedGraph() override;
    virtual TMap<UEdGraphPin*, FVector2D> GetPinsOffset() override;
    virtual TArray<FFormatterPin*> GetInputPins() const override;
    virtual TArray<FFormatterPin*> GetOutputPins() const override;
    virtual TSet<UEdGraphNode*> GetOriginalNodes() const override;
    virtual void Format() override;
    virtual void OffsetBy(const FVector2D& InOffset) override;
    virtual TMap<UEdGraphNode*, FSlateRect> GetBoundMap() override;
};

class FConnectedGraph : public FFormatterGraph
{
public:
    FConnectedGraph(const TSet<UEdGraphNode*>& SelectedNodes, bool InIsParameterGroup = false);
    FConnectedGraph(const FConnectedGraph& Other);
    virtual FFormatterGraph* Clone() override;
    void RemoveNode(FFormatterNode* NodeToRemove);
    virtual void Format() override;
    virtual TMap<UEdGraphNode*, FSlateRect> GetBoundMap() override;
    virtual void OffsetBy(const FVector2D& InOffset) override;
    virtual TMap<UEdGraphPin*, FVector2D> GetPinsOffset() override;
    virtual TArray<FFormatterPin*> GetInputPins() const override;
    virtual TArray<FFormatterPin*> GetOutputPins() const override;
    virtual TSet<UEdGraphNode*> GetOriginalNodes() const override;

    static TArray<FFormatterEdge*> GetEdgeBetweenTwoLayer(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer, const FFormatterNode* ExcludedNode = nullptr);
    static void CalculatePinsIndex(const TArray<TArray<FFormatterNode*>>& Order);
    static void CalculatePinsIndexInLayer(const TArray<FFormatterNode*>& Layer);
    static TArray<UEdGraphNode_Comment*> GetSortedCommentNodes(TSet<UEdGraphNode*> SelectedNodes);

    const TArray<FFormatterNode*>& GetAllNodes() const;

private:
    static TArray<FFormatterNode*> GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes);
    TArray<FFormatterNode*> GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded);

    void DoLayering();
    void RemoveCycle();
    void AddDummyNodes();
    void SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction);
    void DoOrderingSweep();
    void DoPositioning();
    FFormatterNode* FindSourceNode() const;
    FFormatterNode* FindSinkNode() const;
    FFormatterNode* FindMaxDegreeDiffNode() const;
    TArray<FFormatterNode*> GetLeavesWithPathDepth0() const;
    int32 AssignPathDepthForNodes() const;

    TArray<TArray<FFormatterNode*>> LayeredList;
};
