#include "GraphFormatterToolset.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Formatter.h"
#include "Framework/Application/SlateApplication.h"
#include "GraphEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/ScopeExit.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "UObject/Package.h"

FGraphFormatterResult UGraphFormatterToolset::FormatBlueprintGraph(UEdGraph* Graph)
{
    FGraphFormatterResult Result;
    if (!IsInGameThread() || !GEditor || !FSlateApplication::IsInitialized())
    {
        Result.Error = TEXT("Formatting requires the editor game thread and initialized Slate.");
        return Result;
    }
    if (GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
    {
        Result.Error = TEXT("Stop PIE or simulation before formatting graphs.");
        return Result;
    }
    if (!IsValid(Graph) || !Graph->bEditable || !Graph->GetSchema()
        || !Graph->GetSchema()->IsA<UEdGraphSchema_K2>())
    {
        Result.Error = TEXT("Expected a valid, editable K2 Blueprint graph.");
        return Result;
    }
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    TArray<UEdGraph*> BlueprintGraphs;
    if (Blueprint)
    {
        Blueprint->GetAllGraphs(BlueprintGraphs);
    }
    if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->SkeletonGeneratedClass
        || Blueprint->bBeingCompiled || !BlueprintGraphs.Contains(Graph)
        || !FFormatter::IsAssetSupported(Blueprint))
    {
        Result.Error = TEXT("Graph must belong to an initialized, supported Blueprint with generated and skeleton classes that is not compiling.");
        return Result;
    }

    struct FNodeLayout
    {
        UEdGraphNode* Node;
        int32 X;
        int32 Y;
        int32 Width;
        int32 Height;
    };
    TArray<FNodeLayout> Before;
    TSet<FGuid> NodeGuids;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!IsValid(Node) || Node->GetOuter() != Graph || !Node->NodeGuid.IsValid()
            || NodeGuids.Contains(Node->NodeGuid))
        {
            Result.Error = TEXT("Graph contains invalid nodes, ownership, or duplicate node GUIDs.");
            return Result;
        }
        NodeGuids.Add(Node->NodeGuid);
        Before.Add({Node, Node->NodePosX, Node->NodePosY, Node->NodeWidth, Node->NodeHeight});
    }
    Result.NodeCount = Before.Num();
    if (Before.IsEmpty())
    {
        Result.bSuccess = true;
        Result.bPackageDirty = Graph->GetOutermost()->IsDirty();
        return Result;
    }

    // This widget is never attached to a window and cannot claim desktop focus.
    TSharedRef<SGraphEditor> Editor = SNew(SGraphEditor).GraphToEdit(Graph).IsEditable(true);
    SGraphPanel* Panel = Editor->GetGraphPanel();
    if (!Panel)
    {
        Result.Error = TEXT("Could not create an offscreen graph panel.");
        return Result;
    }
    Panel->Update();
    Panel->SlatePrepass(1.0f);
    for (const FNodeLayout& Layout : Before)
    {
        TSharedPtr<SGraphNode> Widget = Panel->GetNodeWidgetFromGuid(Layout.Node->NodeGuid);
        if (Widget.IsValid())
        {
            // The panel prepass visits visible children only in UE 5.8.
            Widget->SlatePrepass(1.0f);
        }
        if (!Widget.IsValid() || Widget->GetDesiredSize().X <= 0 || Widget->GetDesiredSize().Y <= 0)
        {
            Result.Error = FString::Printf(TEXT("Could not measure node %s; graph was not formatted."), *Layout.Node->GetName());
            return Result;
        }
    }

    FFormatter& Formatter = FFormatter::Instance();
    Formatter.SetCurrentEditor(&Editor.Get(), Blueprint);
    ON_SCOPE_EXIT { Formatter.ClearCurrentEditor(); };
    Formatter.Format(true);

    for (const FNodeLayout& Layout : Before)
    {
        Result.MovedNodeCount += Layout.X != Layout.Node->NodePosX || Layout.Y != Layout.Node->NodePosY;
        if (Layout.Node->IsA<UEdGraphNode_Comment>())
        {
            Result.ResizedCommentCount += Layout.Width != Layout.Node->NodeWidth || Layout.Height != Layout.Node->NodeHeight;
        }
    }
    Graph->NotifyGraphChanged();
    Result.bPackageDirty = Graph->GetOutermost()->IsDirty();
    Result.bSuccess = true;
    return Result;
}
