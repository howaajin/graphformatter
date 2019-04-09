/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

class FFormatterNode;

class IPositioningStrategy
{
protected:
	FSlateRect TotalBound;
	TArray< TArray<FFormatterNode*> > LayeredNodes;
public:
	explicit IPositioningStrategy(TArray< TArray<FFormatterNode*> >& InLayeredNodes)
		: LayeredNodes(InLayeredNodes)
	{
	}
	virtual ~IPositioningStrategy() = default;
	FSlateRect GetTotalBound() const { return  TotalBound; }
};
