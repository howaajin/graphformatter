#pragma once
#include "FormatterGraph.h"

class NewGraphAdapter
{
public:
    static TArray<TArray<FFormatterNode*>> GetLayeredListFromNewGraph(const FConnectedGraph* Graph);
};
