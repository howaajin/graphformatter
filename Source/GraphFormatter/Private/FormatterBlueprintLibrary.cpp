/*---------------------------------------------------------------------------------------------
*  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterBlueprintLibrary.h"

UFormatterGraph* UGraphLayoutLibrary::CreateGraph(bool InIsVertical)
{
	FFormatterGraph* Graph = new FFormatterGraph(InIsVertical);
	UFormatterGraph* FormatterGraph = NewObject<UFormatterGraph>(GetTransientPackage());
	FormatterGraph->Graph = Graph;
	return FormatterGraph;
}

void UGraphLayoutLibrary::DestroyGraph(UFormatterGraph* Graph)
{
	delete Graph->Graph;
}

UFormatterNode* UGraphLayoutLibrary::AddNode(UFormatterGraph* Graph, FVector2D Position, FVector2D Size, UFormatterGraph* SubGraph)
{
	FFormatterNode* Node = new FFormatterNode();
	Node->Size = Size;
	Node->SetPosition(Position);
	if (SubGraph && SubGraph->Graph)
	{
		Node->SetSubGraph(SubGraph->Graph);
	}
	UFormatterNode* FormatterNode = NewObject<UFormatterNode>(Graph);
	FormatterNode->Node = Node;
	Node->OriginalNode = FormatterNode;
	Graph->Graph->AddNode(Node);
	return FormatterNode;
}

UFormatterPin* UGraphLayoutLibrary::AddPin(UFormatterNode* Node, FVector2D Offset, EFormatterPinDirection Direction)
{
	FFormatterPin* Pin = new FFormatterPin();
	Pin->Guid = FGuid::NewGuid();
	Pin->Direction = Direction;
	Pin->NodeOffset = Offset;
	Pin->OwningNode = Node->Node;
	UFormatterPin* FormatterPin = NewObject<UFormatterPin>(Node);
	FormatterPin->Pin = Pin;
	Pin->OriginalPin = FormatterPin;
	Node->Node->AddPin(Pin);
	return FormatterPin;
}

TArray<TSet<UFormatterNode*>> FindIsolated(const TArray<FFormatterNode*>& Nodes)
{
	TArray<TSet<UFormatterNode*>> Result;
	TSet<FFormatterNode*> CheckedNodes;
	TArray<FFormatterNode*> Stack;
	for (auto Node : Nodes)
	{
		if (!CheckedNodes.Contains(Node))
		{
			CheckedNodes.Add(Node);
			Stack.Push(Node);
		}
		TSet<UFormatterNode*> IsolatedNodes;
		while (Stack.Num() != 0)
		{
			FFormatterNode* Top = Stack.Pop();
			IsolatedNodes.Add(static_cast<UFormatterNode*>(Top->OriginalNode));
			if (Top->SubGraph != nullptr)
			{
				TSet<void*> OriginalNodes = Top->SubGraph->GetOriginalNodes();

				for (auto OriginalNode : OriginalNodes)
				{
					UFormatterNode* N = static_cast<UFormatterNode*>(OriginalNode);
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

void UGraphLayoutLibrary::Connect(UFormatterPin* From, UFormatterPin* To)
{
	UFormatterNode* FromNode = Cast<UFormatterNode>(From->GetOuter());
	UFormatterNode* ToNode = Cast<UFormatterNode>(To->GetOuter());
	FromNode->Node->Connect(From->Pin, To->Pin);
	ToNode->Node->Connect(To->Pin, From->Pin);
}

void UGraphLayoutLibrary::ApplySettings(UFormatterGraph* Graph, FGraphFormatterSettings Settings)
{
	Graph->Graph->HorizontalSpacing = Settings.HorizontalSpacing;
	Graph->Graph->VerticalSpacing = Settings.VerticalSpacing;
	Graph->Graph->MaxLayerNodes = Settings.MaxLayerNodes;
	Graph->Graph->MaxOrderingIterations = Settings.MaxOrderingIterations;
	Graph->Graph->PositioningAlgorithm = Settings.PositioningAlgorithm;
}

void UGraphLayoutLibrary::SortNodes(UFormatterGraph* Graph)
{
	Graph->Graph->Nodes.Sort([Graph](const FFormatterNode& A, const FFormatterNode& B)
	{
		return Graph->Graph->GetIsVerticalLayout() ? A.GetPosition().X < B.GetPosition().X : A.GetPosition().Y < B.GetPosition().Y;
	});
}

void UGraphLayoutLibrary::BuildIsolatedGraph(UFormatterGraph* Graph)
{
	if (!Graph || !Graph->Graph)
	{
		return;
	}
	auto FoundIsolatedGraphs = FindIsolated(Graph->Graph->Nodes);
	if (FoundIsolatedGraphs.Num() > 1)
	{
		auto DisconnectedGraph = new FDisconnectedGraph();
		for (const auto& IsolatedNodes : FoundIsolatedGraphs)
		{
			auto NewGraph = new FConnectedGraph(Graph->Graph->GetIsVerticalLayout());
			for (auto Node : IsolatedNodes)
			{
				NewGraph->AddNode(Node->Node);
			}
			DisconnectedGraph->AddGraph(NewGraph);
		}
		Graph->Graph->DetachAndDestroy();
		Graph->Graph = DisconnectedGraph;
	}
	else if (FoundIsolatedGraphs.Num() == 1)
	{
		auto NewGraph = new FConnectedGraph(Graph->Graph->GetIsVerticalLayout());
		auto Nodes = FoundIsolatedGraphs[0];
		for (const auto Node : Nodes)
		{
			NewGraph->AddNode(Node->Node);
		}
		Graph->Graph->DetachAndDestroy();
		Graph->Graph = NewGraph;
	}
}

void UGraphLayoutLibrary::Format(UFormatterGraph* Graph)
{
	if (!Graph || !Graph->Graph || Graph->Graph->Nodes.Num() == 0)
	{
		return;
	}
	SortNodes(Graph);
	BuildIsolatedGraph(Graph);
	Graph->Graph->Format();
}

FBox2D UGraphLayoutLibrary::GetTotalBound(UFormatterGraph* Graph)
{
	if (Graph && Graph->Graph)
	{
		return Graph->Graph->GetTotalBound();
	}
	else
	{
		return FBox2d(ForceInit);
	}
}

void UGraphLayoutLibrary::ShiftBy(UFormatterGraph* Graph, FVector2D Offset)
{
	if (!Graph || !Graph->Graph)
	{
		return;
	}
	Graph->Graph->OffsetBy(Offset);
}

TMap<UFormatterNode*, FBox2D> UGraphLayoutLibrary::GetBoundMap(UFormatterGraph* Graph)
{
	if (!Graph || !Graph->Graph)
	{
		return TMap<UFormatterNode*, FBox2D>();
	}
	TMap<void*, FBox2D> BoundMap = Graph->Graph->GetBoundMap();
	TMap<UFormatterNode*, FBox2D> Result;
	for (auto [Key, Value] : BoundMap)
	{
		Result.Add(static_cast<UFormatterNode*>(Key), Value);
	}
	return Result;
}
