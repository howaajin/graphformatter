#include "GraphFormatterToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphFormatterToolsetValidationTest,
    "GraphFormatter.MCP.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGraphFormatterToolsetValidationTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Toolset is registered for MCP discovery"),
        UToolsetRegistry::IsToolsetClassRegistered(UGraphFormatterToolset::StaticClass()));
    TestFalse(TEXT("Null graph is refused"), UGraphFormatterToolset::FormatBlueprintGraph(nullptr).bSuccess);
    UEdGraph* UnownedGraph = NewObject<UEdGraph>();
    UnownedGraph->Schema = UEdGraphSchema_K2::StaticClass();
    TestFalse(TEXT("Graph outside a Blueprint is refused"), UGraphFormatterToolset::FormatBlueprintGraph(UnownedGraph).bSuccess);
    UBlueprint* IncompleteBlueprint = NewObject<UBlueprint>();
    UEdGraph* IncompleteGraph = FBlueprintEditorUtils::CreateNewGraph(IncompleteBlueprint, TEXT("Incomplete"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    IncompleteBlueprint->UbergraphPages.Add(IncompleteGraph);
    TestFalse(TEXT("Blueprint without generated classes is refused"),
        UGraphFormatterToolset::FormatBlueprintGraph(IncompleteGraph).bSuccess);
    if (GEditor && !GEditor->PlayWorld)
    {
        // Scope only the state checked by the tool; no actual play session starts.
        TGuardValue<TObjectPtr<UWorld>> PlayWorldGuard(GEditor->PlayWorld, NewObject<UWorld>());
        const FGraphFormatterResult Result = UGraphFormatterToolset::FormatBlueprintGraph(UnownedGraph);
        TestFalse(TEXT("PIE is refused"), Result.bSuccess);
        TestTrue(TEXT("PIE refusal explains why"), Result.Error.Contains(TEXT("PIE")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphFormatterToolsetLayoutUndoTest,
    "GraphFormatter.MCP.LayoutAndUndo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGraphFormatterToolsetLayoutUndoTest::RunTest(const FString& Parameters)
{
    if (!GEditor || GEditor->IsPlaySessionInProgress())
    {
        AddError(TEXT("Run Graph Formatter tests in an idle editor."));
        return false;
    }
    // Global editor undo asks Kismet to refresh the Blueprint's generated class.
    // A bare NewObject<UBlueprint> is not a valid editor asset for that operation.
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),
        GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(),
            TEXT("GraphFormatterTest")), BPTYPE_Normal);
    if (!TestNotNull(TEXT("Initialized Blueprint created"), Blueprint)
        || !TestNotNull(TEXT("Generated class exists"), Blueprint->GeneratedClass.Get())
        || !TestNotNull(TEXT("Skeleton class exists"), Blueprint->SkeletonGeneratedClass.Get()))
    {
        return false;
    }
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("FormatterTest"),
        UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    Blueprint->UbergraphPages.Add(Graph);
    Graph->SetFlags(RF_Transactional);
    auto AddSequence = [Graph]()
    {
        UK2Node_ExecutionSequence* Node = NewObject<UK2Node_ExecutionSequence>(Graph, NAME_None, RF_Transactional);
        Node->CreateNewGuid();
        Graph->AddNode(Node, false, false);
        Node->AllocateDefaultPins();
        Node->NodePosX = 0;
        Node->NodePosY = 0;
        return Node;
    };
    UK2Node_ExecutionSequence* First = AddSequence();
    UK2Node_ExecutionSequence* Second = AddSequence();
    UEdGraphPin* Output = First->GetThenPinGivenIndex(0);
    UEdGraphPin* Input = Second->FindPinChecked(UEdGraphSchema_K2::PN_Execute);
    Output->MakeLinkTo(Input);
    UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Graph, NAME_None, RF_Transactional);
    Comment->CreateNewGuid();
    Comment->NodeComment = TEXT("Linked sequences");
    Comment->NodePosX = -300;
    Comment->NodePosY = -300;
    Comment->NodeWidth = 1500;
    Comment->NodeHeight = 1500;
    Graph->AddNode(Comment, false, false);
    const FGuid FirstGuid = First->NodeGuid;
    const FGuid SecondGuid = Second->NodeGuid;
    const int32 FirstPinCount = First->Pins.Num();
    const int32 SecondPinCount = Second->Pins.Num();

    const FGraphFormatterResult Result = UGraphFormatterToolset::FormatBlueprintGraph(Graph);
    if (!TestTrue(*FString::Printf(TEXT("Formatting succeeded: %s"), *Result.Error), Result.bSuccess))
    {
        return false;
    }
    TestEqual(TEXT("Nodes and comment counted"), Result.NodeCount, 3);
    TestTrue(TEXT("Overlapping nodes moved"), Result.MovedNodeCount > 0);
    TestTrue(TEXT("Consumer is to the right of producer"), Second->NodePosX > First->NodePosX);
    TestEqual(TEXT("Node count preserved"), Graph->Nodes.Num(), 3);
    TestEqual(TEXT("Comment was resized"), Result.ResizedCommentCount, 1);
    TestEqual(TEXT("Comment text preserved"), Comment->NodeComment, FString(TEXT("Linked sequences")));
    TestEqual(TEXT("First GUID preserved"), First->NodeGuid, FirstGuid);
    TestEqual(TEXT("Second GUID preserved"), Second->NodeGuid, SecondGuid);
    TestEqual(TEXT("First pins preserved"), First->Pins.Num(), FirstPinCount);
    TestEqual(TEXT("Second pins preserved"), Second->Pins.Num(), SecondPinCount);
    TestTrue(TEXT("Output link preserved"), Output->LinkedTo.Contains(Input));
    TestTrue(TEXT("Input link preserved"), Input->LinkedTo.Contains(Output));
    if (Result.MovedNodeCount > 0)
    {
        TestTrue(TEXT("Formatting is undoable"), GEditor->UndoTransaction());
        TestEqual(TEXT("First X restored"), First->NodePosX, 0);
        TestEqual(TEXT("First Y restored"), First->NodePosY, 0);
        TestEqual(TEXT("Second X restored"), Second->NodePosX, 0);
        TestEqual(TEXT("Second Y restored"), Second->NodePosY, 0);
        TestEqual(TEXT("Comment X restored"), Comment->NodePosX, -300);
        TestEqual(TEXT("Comment Y restored"), Comment->NodePosY, -300);
        TestEqual(TEXT("Comment width restored"), Comment->NodeWidth, 1500);
        TestEqual(TEXT("Comment height restored"), Comment->NodeHeight, 1500);
    }
    return true;
}

#endif
