/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "EvenlyPlaceStrategy.h"
#include "FormatterGraph.h"
#include "FormatterSettings.h"

FSlateRect FEvenlyPlaceStrategy::PlaceNodeInLayer(TArray<FFormatterNode*>& Layer, const FSlateRect& PreBound)
{
	FSlateRect Bound;
	const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
	FVector2D Position;
	if (PreBound.IsValid())
	{
		Position = FVector2D(PreBound.Right + Settings.HorizontalSpacing, 0);
	}
	else
	{
		Position = FVector2D(0, 0);
	}
	for (auto Node : Layer)
	{
		if (Node->OriginalNode == nullptr)
		{
			Node->SetPosition(Position);
			continue;
		}
		Node->SetPosition(Position);
		if (Bound.IsValid())
		{
			Bound = Bound.Expand(FSlateRect::FromPointAndExtent(Position, Node->Size));
		}
		else
		{
			Bound = FSlateRect::FromPointAndExtent(Position, Node->Size);
		}
		Position.Y += Node->Size.Y + Settings.VerticalSpacing;
	}
	return Bound;
}

FFormatterNode* FEvenlyPlaceStrategy::FindFirstNodeInLayeredList(TArray< TArray<FFormatterNode*> >& InLayeredNodes)
{
	for (const auto& Layer : InLayeredNodes)
	{
		for (auto Node : Layer)
		{
			return Node;
		}
	}
	return nullptr;
}

FEvenlyPlaceStrategy::FEvenlyPlaceStrategy(TArray< TArray<FFormatterNode*> >& InLayeredNodes)
	: IPositioningStrategy(InLayeredNodes)
{
	FVector2D StartPosition;
	FFormatterNode* FirstNode = FindFirstNodeInLayeredList(InLayeredNodes);
	if (FirstNode != nullptr)
	{
		StartPosition = FirstNode->GetPosition();
	}

	float MaxHeight = 0;
	FSlateRect PreBound;
	TArray<FSlateRect> Bounds;
	for (auto& Layer : InLayeredNodes)
	{
		PreBound = PlaceNodeInLayer(Layer, PreBound);
		Bounds.Add(PreBound);
		if (TotalBound.IsValid())
		{
			TotalBound = TotalBound.Expand(PreBound);
		}
		else
		{
			TotalBound = PreBound;
		}
		const float Height = PreBound.GetSize().Y;
		if (Height > MaxHeight)
		{
			MaxHeight = Height;
		}
	}

	StartPosition -= FVector2D(0, MaxHeight - Bounds[0].GetSize().Y) / 2.0f;

	for (int32 i = 0; i < InLayeredNodes.Num(); i++)
	{
		const FVector2D Offset = FVector2D(0, (MaxHeight - Bounds[i].GetSize().Y) / 2.0f) + StartPosition;
		for (auto Node : InLayeredNodes[i])
		{
			Node->SetPosition(Node->GetPosition() + Offset);
		}
	}
	TotalBound = FSlateRect::FromPointAndExtent(StartPosition, TotalBound.GetSize());
}
