#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"
#include "GraphFormatterToolset.generated.h"

class UEdGraph;

USTRUCT(BlueprintType)
struct FGraphFormatterResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Graph Formatter")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "Graph Formatter")
    FString Error;

    UPROPERTY(BlueprintReadOnly, Category = "Graph Formatter")
    int32 NodeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Graph Formatter")
    int32 MovedNodeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Graph Formatter")
    int32 ResizedCommentCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Graph Formatter")
    bool bPackageDirty = false;
};

/** Applies the installed Graph Formatter to an explicit Blueprint graph without opening a window. */
UCLASS()
class UGraphFormatterToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /** Formats all nodes and comments in one editable K2 Blueprint graph.
     * Uses the plugin's current settings and an undo transaction. Refuses PIE.
     * Does not compile, save, check out files, open editors, or change focus.
     * Inspect bSuccess and Error, then validate and save the intended asset separately.
     */
    UFUNCTION(meta = (AICallable), Category = "Graph Formatter")
    static FGraphFormatterResult FormatBlueprintGraph(UEdGraph* Graph);
};
