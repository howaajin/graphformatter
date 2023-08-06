/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"

class FConnectedGraph;
class FFormatterNode;
class FFormatterGraph;

UENUM()
enum class EGraphFormatterPositioningAlgorithm
{
    EEvenlyInLayer UMETA(DisplayName = "Place node evenly in layer"),
    EFastAndSimpleMethodTop UMETA(DisplayName = "FAS Top"),
    EFastAndSimpleMethodMedian UMETA(DisplayName = "FAS Median"),
    ELayerSweep UMETA(DisplayName = "Layer sweep"),
};

enum class EFormatterPinDirection : int
{
    In,
    Out,
    InOut,
};

class FFormatterPin
{
public:
    FGuid Guid;
    void* OriginalPin{nullptr};
    EFormatterPinDirection Direction{EFormatterPinDirection::In};
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
    bool IsInnerSegment() const;
};

class FFormatterNode
{
public:
    static FFormatterNode* CreateDummy();
    static TArray<FFormatterEdge*> GetEdgeBetweenTwoLayer(const TArray<FFormatterNode*>& LowerLayer, const TArray<FFormatterNode*>& UpperLayer, const FFormatterNode* ExcludedNode = nullptr);
    static TArray<FFormatterNode*> GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes);
    static void CalculatePinsIndexInLayer(const TArray<FFormatterNode*>& Layer);
    static void CalculatePinsIndex(const TArray<TArray<FFormatterNode*>>& Order);
    static int32 CalculateCrossing(const TArray<TArray<FFormatterNode*>>& Order);

    FGuid Guid;
    void* OriginalNode = nullptr;
    FFormatterGraph* SubGraph = nullptr;
    FVector2D Size;
    TArray<FFormatterEdge*> InEdges;
    TArray<FFormatterEdge*> OutEdges;
    TArray<FFormatterPin*> InPins;
    TArray<FFormatterPin*> OutPins;
    int32 PathDepth;
    int32 PositioningPriority;
    FFormatterNode(const FFormatterNode& Other);
    FFormatterNode();

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
    static TArray<FBox2D> CalculateLayersBound(TArray<TArray<FFormatterNode*>>& InLayeredNodes, bool IsHorizontalDirection = true, bool IsParameterGroup = false);
    inline static int32 HorizontalSpacing;
    inline static int32 VerticalSpacing;
    inline static FVector2D SpacingFactorOfGroup;
    inline static int32 MaxLayerNodes;
    inline static int32 MaxOrderingIterations;
    inline static EGraphFormatterPositioningAlgorithm PositioningAlgorithm;

    FFormatterGraph(const FFormatterGraph& Other);
    virtual ~FFormatterGraph();
    virtual FFormatterGraph* Clone();

    explicit FFormatterGraph(bool InIsVerticalLayout = false, bool InIsParameterGroup = false);

    void AddNode(FFormatterNode* InNode);

    virtual TMap<void*, FVector2D> GetPinsOffset() { return {}; }

    virtual TArray<FFormatterPin*> GetInputPins() const { return {}; }

    virtual TArray<FFormatterPin*> GetOutputPins() const { return {}; }

    virtual TSet<void*> GetOriginalNodes() const { return {}; }

    virtual void Format() { }
    
    virtual void OffsetBy(const FVector2D& InOffset) { }
    virtual void SetPosition(const FVector2D& Position);

    virtual TMap<void*, FBox2D> GetBoundMap() { return {}; }

    FBox2D GetTotalBound() const { return TotalBound; }

    void SetBorder(float Left, float Top, float Right, float Bottom);
    FBox2D GetBorder() const;
    void SetIsParameterGroup(bool InIsParameterGroup);
    bool GetIsParameterGroup() const;
    bool GetIsVerticalLayout() const;
    void SetIsVerticalLayout(bool InIsVerticalLayout);
    bool (*NodeComparer)(const FFormatterNode& A, const FFormatterNode& B) = nullptr;
    TMap<void*, FFormatterPin*> OriginalPinsMap;
    TArray<FFormatterNode*> Nodes;

protected:
    TMap<FGuid, FFormatterNode*> NodesMap;
    TMap<FGuid, FFormatterGraph*> SubGraphs;
    TMap<FGuid, FFormatterPin*> PinsMap;
    
    FBox2D TotalBound = FBox2D(ForceInit);
    FBox2D Border = {FVector2D(ForceInitToZero), FVector2D(ForceInitToZero)};
    bool IsParameterGroup = false;
    bool IsVerticalLayout = false;
};

class FDisconnectedGraph : public FFormatterGraph
{
    TArray<FFormatterGraph*> ConnectedGraphs;

public:
    void AddGraph(FFormatterGraph* Graph);
    virtual ~FDisconnectedGraph() override;
    virtual TMap<void*, FVector2D> GetPinsOffset() override;
    virtual TArray<FFormatterPin*> GetInputPins() const override;
    virtual TArray<FFormatterPin*> GetOutputPins() const override;
    virtual TSet<void*> GetOriginalNodes() const override;
    virtual void Format() override;
    virtual void OffsetBy(const FVector2D& InOffset) override;
    virtual TMap<void*, FBox2D> GetBoundMap() override;
};

class FConnectedGraph : public FFormatterGraph
{
public:
    FConnectedGraph(bool InIsVerticalLayout = false, bool InIsParameterGroup = false);
    FConnectedGraph(const FConnectedGraph& Other);
    virtual FFormatterGraph* Clone() override;
    void RemoveNode(FFormatterNode* NodeToRemove);
    virtual void Format() override;
    virtual TMap<void*, FBox2D> GetBoundMap() override;
    virtual void OffsetBy(const FVector2D& InOffset) override;
    virtual TMap<void*, FVector2D> GetPinsOffset() override;
    virtual TArray<FFormatterPin*> GetInputPins() const override;
    virtual TArray<FFormatterPin*> GetOutputPins() const override;
    virtual TSet<void*> GetOriginalNodes() const override;

private:
    
    TArray<FFormatterNode*> GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded);

    void DoLayering();
    void RemoveCycle();
    void AddDummyNodes();
    void SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction);
    void DoOrderingSweep();
    void DoPositioning();
    FFormatterNode* FindSourceNode() const;
    FFormatterNode* FindSinkNode() const;
    FFormatterNode* FindMaxInOutWeightDiffNode() const;
    TArray<FFormatterNode*> GetLeavesWithPathDepth0() const;
    int32 AssignPathDepthForNodes() const;

    TArray<TArray<FFormatterNode*>> LayeredList;
};

