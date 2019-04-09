#include "FastAndSimplePositioningStrategy.h"

void FastAndSimplePositioningStrategy::MarkConflicts()
{
	for (int32 i = 1; i < LayeredNodes.Num() - 1; i++)
	{
		int32 k0 = 0;
		int32 l = 1;
		for (int32 l1 = 0; l1 < LayeredNodes[i].Num(); l1++)
		{

		}
	}
}

FastAndSimplePositioningStrategy::FastAndSimplePositioningStrategy(TArray<TArray<FFormatterNode*>>& InLayeredNodes)
	:IPositioningStrategy(InLayeredNodes)
{
}
