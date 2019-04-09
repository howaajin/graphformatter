/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "IPositioningStrategy.h"

class FastAndSimplePositioningStrategy : public IPositioningStrategy
{
	void MarkConflicts();
public:
	explicit FastAndSimplePositioningStrategy(TArray<TArray<FFormatterNode*> >& InLayeredNodes);
};
