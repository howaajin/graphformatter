/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Layout/SlateRect.h"
#include "FormatterDelegates.h"

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
};

class FormatterEdge
{
public:
	FFormatterPin* From;
	FFormatterPin* To;
};

class FFormatterIndexedEdge
{
public:
	int32 LayerIndex1;
	int32 LayerIndex2;
	bool IsCrossing(const FFormatterIndexedEdge& Edge) const;
};

class FFormatterNode
{
public:
	FGuid Guid;
	UEdGraphNode* OriginalNode;
	FFormatterGraph* SubGraph;
	FVector2D Size;
	TArray<FormatterEdge*> InEdges;
	TArray<FormatterEdge*> OutEdges;
	TArray<FFormatterPin*> InPins;
	TArray<FFormatterPin*> OutPins;
	int32 PathDepth;
	int32 PositioningPriority;
	FFormatterNode(UEdGraphNode* InNode);
	FFormatterNode(const FFormatterNode& Other);
	FFormatterNode();
	~FFormatterNode();
	void Connect(FFormatterPin* SourcePin, FFormatterPin* TargetPin);
	void Disconnect(FFormatterPin* SourcePin, FFormatterPin* TargetPin);
	TArray<FFormatterNode*> GetSuccessors() const;
	TArray<FFormatterNode*> GetPredecessors() const;
	bool IsSource() const;
	bool IsSink() const;
	bool AnySuccessorPathDepthEqu0() const;
	int32 GetInputPinCount() const;
	int32 GetInputPinIndex(FFormatterPin* InputPin) const;
	int32 GetOutputPinCount() const;
	int32 GetOutputPinIndex(FFormatterPin* OutputPin) const;
	TArray<FFormatterIndexedEdge> GetIndexedEdge(const TArray<FFormatterNode*>& Layer, int32 StartIndex, EEdGraphPinDirection Direction) const;
	float CalcBarycenter(const TArray<FFormatterNode*>& Layer, int32 StartIndex, EEdGraphPinDirection Direction) const;
	float CalcMedianValue(const TArray<FFormatterNode*>& Layer, int32 StartIndex, EEdGraphPinDirection Direction) const;
	int32 CalcPriority(EEdGraphPinDirection Direction) const;
	void InitPosition(FVector2D InPosition);
	void SetPosition(FVector2D InPosition);
	FVector2D GetPosition() const;
	void SetSubGraph(FFormatterGraph* InSubGraph);
	void UpdatePinsOffset();
	friend class FFormatterGraph;
private:
	float OrderValue;
	FVector2D Position;
};

class FFormatterGraph
{
public:
	FFormatterGraph(UEdGraph* InGraph, const TSet<UEdGraphNode*>&, FFormatterDelegates InDelegates, bool IsSingleMode = false);
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
private:
	static TArray<TSet<UEdGraphNode*>> FindIsolated(UEdGraph* InGraph, const TSet<UEdGraphNode*>& SelectedNodes);
	void CalculateNodesSize(FCalculateNodeBoundDelegate SizeCalculator);
	void CalculatePinsOffset(FOffsetCalculatorDelegate OffsetCalculator);
	TArray<UEdGraphNode_Comment*> GetSortedCommentNodes(UEdGraph* InGraph, TSet<UEdGraphNode*> SelectedNodes);
	TArray<FormatterEdge> GetEdgeForNode(FFormatterNode* Node, TSet<UEdGraphNode*> SelectedNodes);
	static TArray<FFormatterNode*> GetSuccessorsForNodes(TSet<FFormatterNode*> Nodes);
	TArray<FFormatterNode*> GetNodesGreaterThan(int32 i, TSet<FFormatterNode*>& Excluded);
	void BuildNodes(UEdGraph* InGraph, TSet<UEdGraphNode*> SelectedNodes);
	void BuildEdges(TSet<UEdGraphNode*> SelectedNodes);
	void BuildNodesAndEdges(UEdGraph* InGraph, TSet<UEdGraphNode*> SelectedNodes);

	void DoLayering();
	void RemoveCycle();
	void AddDummyNodes();
	void SortInLayer(TArray<TArray<FFormatterNode*>>& Order, EEdGraphPinDirection Direction);
	void DoOrderingSweep();
	void DoPositioning();
	FFormatterNode* CollapseNode(UEdGraphNode* InNode, TSet<UEdGraphNode*> SelectedNodes);
	FFormatterGraph* BuildSubGraph(const UEdGraphNode* InNode, TSet<UEdGraphNode*> SelectedNodes);
	FFormatterNode* FindSourceNode() const;
	FFormatterNode* FindSinkNode() const;
	FFormatterNode* FindMedianNode() const;
	TArray<FFormatterNode*> GetLeavesWidthPathDepthEqu0() const;
	int32 CalculateLongestPath() const;
	TSet<UEdGraphNode*> GetChildren(const UEdGraphNode* InNode, TSet<UEdGraphNode*> SelectedNodes) const;
	TSet<UEdGraphNode*> PickChildren(const UEdGraphNode* InNode, TSet<UEdGraphNode*> SelectedNodes);
	TSet<UEdGraphNode*> GetChildren(const UEdGraphNode* InNode) const;

	UEdGraph* UEGraph;
	FFormatterDelegates Delegates;
	TArray<FFormatterNode*> Nodes;
	TMap<FGuid, FFormatterNode*> NodesMap;
	TMap<FGuid, FFormatterGraph*> SubGraphs;
	TMap<FGuid, FFormatterPin*> PinsMap;
	TMap<UEdGraphPin*, FFormatterPin*> OriginalPinsMap;
	TSet<UEdGraphNode*> PickedNodes;
	TArray<TArray<FFormatterNode*>> LayeredList;
	TArray<FFormatterGraph*> IsolatedGraphs;
	FSlateRect TotalBound;
};
