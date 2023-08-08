/*---------------------------------------------------------------------------------------------
*  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "FormatterGraph.h"
#include "FormatterBlueprintLibrary.generated.h"

USTRUCT(BlueprintType, Category = "Graph Formatter")
struct FGraphFormatterSettings
{
	GENERATED_BODY()

	UPROPERTY()
	int32 HorizontalSpacing;
	
	UPROPERTY()
	int32 VerticalSpacing;
	
	UPROPERTY()
	int32 MaxLayerNodes;
	
	UPROPERTY()
	int32 MaxOrderingIterations;
	
	UPROPERTY()
	EGraphFormatterPositioningAlgorithm PositioningAlgorithm;
};

UCLASS(BlueprintType, Category = "Graph Formatter")
class UFormatterPin : public UObject
{
	GENERATED_BODY()

public:
	FFormatterPin* Pin;
};

UCLASS(BlueprintType, Category = "Graph Formatter")
class UFormatterNode : public UObject
{
	GENERATED_BODY()

public:
	FFormatterNode* Node;
};

UCLASS(BlueprintType, Category = "Graph Formatter")
class UFormatterGraph : public UObject
{
	GENERATED_BODY()

public:
	FFormatterGraph* Graph;
};

UCLASS(BlueprintType, Category = "Graph Formatter")
class UGraphLayoutLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static UFormatterGraph* CreateGraph(bool InIsVertical);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static void DestroyGraph(UFormatterGraph* Graph);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static UFormatterNode* AddNode(UFormatterGraph* Graph, FVector2D Position, FVector2D Size, UFormatterGraph* SubGraph = nullptr);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static UFormatterPin* AddPin(UFormatterNode* Node, FVector2D Offset, EFormatterPinDirection Direction);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static void Connect(UFormatterPin* From, UFormatterPin* To);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static void ApplySettings(UFormatterGraph* Graph, FGraphFormatterSettings Settings);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static void Format(UFormatterGraph* Graph);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static FBox2D GetTotalBound(UFormatterGraph* Graph);

	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static void ShiftBy(UFormatterGraph* Graph, FVector2D Offset);
	
	UFUNCTION(BlueprintCallable, Category="Graph Formatter")
	static TMap<UFormatterNode*, FBox2D> GetBoundMap(UFormatterGraph* Graph);
private:
	static void SortNodes(UFormatterGraph* Graph);
	static void BuildIsolatedGraph(UFormatterGraph* Graph);
};
