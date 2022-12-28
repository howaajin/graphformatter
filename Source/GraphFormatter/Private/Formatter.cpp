/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "Formatter.h"
#include "FormatterCommands.h"
#include "FormatterGraph.h"
#include "FormatterSettings.h"
#include "FormatterLog.h"

#include "AIGraphEditor.h"
#include "AudioEditor/Private/SoundCueEditor.h"
#include "BehaviorTree/BehaviorTree.h"
#include "EdGraphNode_Comment.h"
#include "Editor/BehaviorTreeEditor/Private/BehaviorTreeEditor.h"
#include "GraphEditor/Private/SGraphEditorImpl.h"
#include "MaterialEditor/Private/MaterialEditor.h"
#include "Math/Ray.h"
#include "SGraphNodeComment.h"
#include "SGraphPanel.h"

#include "BlueprintEditor.h"
#include "PrivateAccessor.h"

DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessBlueprintGraphEditor, FBlueprintEditor, TWeakPtr<SGraphEditor>, FocusedGraphEdPtr)
#if ENGINE_MAJOR_VERSION >= 5
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessMaterialGraphEditor, FMaterialEditor, TWeakPtr<SGraphEditor>, FocusedGraphEdPtr)
#else
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessMaterialGraphEditor, FMaterialEditor, TSharedPtr<SGraphEditor>, GraphEditor)
#endif
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSoundCueGraphEditor, FSoundCueEditor, TSharedPtr<SGraphEditor>, SoundCueGraphEditor)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessAIGraphEditor, FAIGraphEditor, TWeakPtr<SGraphEditor>, UpdateGraphEdPtr)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphEditorImpl, SGraphEditor, TSharedPtr<SGraphEditor>, Implementation)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphEditorPanel, SGraphEditorImpl, TSharedPtr<SGraphPanel>, GraphPanel)
DECLARE_PRIVATE_CONST_FUNC_ACCESSOR(FAccessSGraphNodeCommentHandleSelection, SGraphNodeComment, HandleSelection, void, bool, bool)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphPanelZoomLevels, SNodePanel, TUniquePtr<FZoomLevelsContainer>, ZoomLevels)
DECLARE_PRIVATE_FUNC_ACCESSOR(FAccessSGraphPanelPostChangedZoom, SNodePanel, PostChangedZoom, void)

SGraphEditor* GetGraphEditor(const FBlueprintEditor* Editor)
{
    auto& Ptr = Editor->*FPrivateAccessor<FAccessBlueprintGraphEditor>::Member;
    if (Ptr.IsValid())
    {
        return Ptr.Pin().Get();
    }
    return nullptr;
}

SGraphEditor* GetGraphEditor(const FMaterialEditor* Editor)
{
#if ENGINE_MAJOR_VERSION >= 5
    auto& FocusedGraphEd = Editor->*FPrivateAccessor<FAccessMaterialGraphEditor>::Member;
    auto GraphEditor = FocusedGraphEd.Pin();
#else
    auto& GraphEditor = Editor->*FPrivateAccessor<FAccessMaterialGraphEditor>::Member;
#endif
    if (GraphEditor.IsValid())
    {
        return GraphEditor.Get();
    }
    return nullptr;
}

SGraphEditor* GetGraphEditor(const FSoundCueEditor* Editor)
{
    auto& GraphEditor = Editor->*FPrivateAccessor<FAccessSoundCueGraphEditor>::Member;
    if (GraphEditor.IsValid())
    {
        return GraphEditor.Get();
    }
    return nullptr;
}

SGraphEditor* GetGraphEditor(const FBehaviorTreeEditor* Editor)
{
    auto& GraphEditor = Editor->*FPrivateAccessor<FAccessAIGraphEditor>::Member;
    if (GraphEditor.IsValid())
    {
        return GraphEditor.Pin().Get();
    }
    return nullptr;
}

void FFormatter::SetCurrentEditor(SGraphEditor* Editor, UObject* Object)
{
    CurrentEditor = Editor;
    IsVerticalLayout = false;
    IsBehaviorTree = false;
    IsBlueprint = false;
    if (Cast<UBehaviorTree>(Object))
    {
        IsVerticalLayout = true;
        IsBehaviorTree = true;
    }
    if (Cast<UBlueprint>(Object))
    {
        IsBlueprint = true;
    }
}

bool FFormatter::IsAssetSupported(UObject* Object) const
{
    const UFormatterSettings* Settings = GetDefault<UFormatterSettings>();
    return Settings->SupportedAssetTypes.Contains(Object->GetClass()->GetName());
}

/** Matches widgets by InName */
struct FWidgetTypeMatcher
{
    FWidgetTypeMatcher(const FName& InType)
        : TypeName(InType)
    {}

    bool IsMatch(const TSharedRef<const SWidget>& InWidget) const
    {
        return TypeName == InWidget->GetType();
    }

    const FName& TypeName;
};

SGraphEditor* FFormatter::FindGraphEditor() const
{
    FSlateApplication& Application = FSlateApplication::Get();
    auto ActiveWindow = Application.GetActiveTopLevelWindow();
    if (!ActiveWindow.IsValid())
    {
        return nullptr;
    }
    FGeometry InnerWindowGeometry = ActiveWindow->GetWindowGeometryInWindow();
    FArrangedChildren JustWindow(EVisibility::Visible);
    JustWindow.AddWidget(FArrangedWidget(ActiveWindow.ToSharedRef(), InnerWindowGeometry));

    FWidgetPath WidgetPath(ActiveWindow.ToSharedRef(), JustWindow);
    if (WidgetPath.ExtendPathTo(FWidgetTypeMatcher("SGraphEditor"), EVisibility::Visible))
    {
        return StaticCast<SGraphEditor*>(&WidgetPath.GetLastWidget().Get());
    }
    return nullptr;
}

bool FFormatter::CaptureGraphEditor()
{
    FSlateApplication& Application = FSlateApplication::Get();
    FWidgetPath WidgetPath = Application.LocateWindowUnderMouse(Application.GetCursorPos(), Application.GetInteractiveTopLevelWindows());
    UE_LOG(LogTemp, Warning, TEXT("%s"), *WidgetPath.Widgets[0].Widget.Get().ToString());
    for (int i = WidgetPath.Widgets.Num() - 1; i >= 0; i--)
    {
        if (WidgetPath.Widgets[i].Widget->GetTypeAsString() == "SGraphEditor")
        {
            CurrentEditor = StaticCast<SGraphEditor*>(&WidgetPath.Widgets[i].Widget.Get());
            if (CurrentEditor)
            {
                return true;
            }
        }
    }
    return false;
}

SGraphPanel* FFormatter::GetCurrentPanel() const
{
    auto& Impl = CurrentEditor->*FPrivateAccessor<FAccessSGraphEditorImpl>::Member;
    SGraphEditorImpl* GraphEditorImpl = StaticCast<SGraphEditorImpl*>(Impl.Get());
    auto& GraphPanel = GraphEditorImpl->*FPrivateAccessor<FAccessSGraphEditorPanel>::Member;
    if (GraphPanel.IsValid())
    {
        return GraphPanel.Get();
    }
    return nullptr;
}

SGraphNode* FFormatter::GetWidget(const UEdGraphNode* Node) const
{
    SGraphPanel* GraphPanel = GetCurrentPanel();
    if (GraphPanel != nullptr)
    {
        TSharedPtr<SGraphNode> NodeWidget = GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
        return NodeWidget.Get();
    }
    return nullptr;
}

TSet<UEdGraphNode*> FFormatter::GetAllNodes() const
{
    TSet<UEdGraphNode*> Nodes;
    if (CurrentEditor)
    {
        for (UEdGraphNode* Node : CurrentEditor->GetCurrentGraph()->Nodes)
        {
            Nodes.Add(Node);
        }
    }
    return Nodes;
}

float FFormatter::GetCommentNodeTitleHeight(const UEdGraphNode* Node) const
{
    /** Titlebar Offset - taken from SGraphNodeComment.cpp */
    static const FSlateRect TitleBarOffset(13, 8, -3, 0);

    SGraphNode* CommentNode = GetWidget(Node);
    if (CommentNode)
    {
        SGraphNodeComment* NodeWidget = StaticCast<SGraphNodeComment*>(CommentNode);
        FSlateRect Rect = NodeWidget->GetTitleRect();
        return Rect.GetSize().Y + TitleBarOffset.Top;
    }
    return 0;
}

FVector2D FFormatter::GetNodeSize(const UEdGraphNode* Node) const
{
    auto GraphNode = GetWidget(Node);
    if (GraphNode != nullptr)
    {
        FVector2D Size = GraphNode->GetDesiredSize();
        return Size;
    }
    return FVector2D(Node->NodeWidth, Node->NodeHeight);
}

FVector2D FFormatter::GetNodePosition(const UEdGraphNode* Node) const
{
    auto GraphNode = GetWidget(Node);
    if (GraphNode != nullptr)
    {
        return GraphNode->GetPosition();
    }
    return FVector2D();
}

FVector2D FFormatter::GetPinOffset(const UEdGraphPin* Pin) const
{
    auto GraphNode = GetWidget(Pin->GetOwningNodeUnchecked());
    if (GraphNode != nullptr)
    {
        auto PinWidget = GraphNode->FindWidgetForPin(const_cast<UEdGraphPin*>(Pin));
        if (PinWidget.IsValid())
        {
            auto Offset = PinWidget->GetNodeOffset();
            return Offset;
        }
    }
    return FVector2D::ZeroVector;
}

FSlateRect FFormatter::GetNodesBound(const TSet<UEdGraphNode*> Nodes) const
{
    FSlateRect Bound;
    for (auto Node : Nodes)
    {
        FVector2D Pos = GetNodePosition(Node);
        FVector2D Size = GetNodeSize(Node);
        FSlateRect NodeBound = FSlateRect::FromPointAndExtent(Pos, Size);
        Bound = Bound.IsValid() ? Bound.Expand(NodeBound) : NodeBound;
    }
    return Bound;
}

bool FFormatter::IsExecPin(const UEdGraphPin* Pin) const
{
    return Pin->PinType.PinCategory == "Exec";
}

void FFormatter::Translate(TSet<UEdGraphNode*> Nodes, FVector2D Offset)
{
    UEdGraph* Graph = CurrentEditor->GetCurrentGraph();
    if (!Graph || !CurrentEditor)
    {
        return;
    }
    if (Offset.X == 0 && Offset.Y == 0)
    {
        return;
    }
    for (auto Node : Nodes)
    {
        auto WidgetNode = GetWidget(Node);
        SGraphPanel::SNode::FNodeSet Filter;
        WidgetNode->MoveTo(WidgetNode->GetPosition() + Offset, Filter, true);
    }
}

void FFormatter::UpdateCommentNodes() const
{
    // No need to do this hack when AutoSizeComments is used.
    auto AutoSizeCommentModule = FModuleManager::Get().GetModule(FName("AutoSizeComments"));
    if (AutoSizeCommentModule)
    {
        return;
    }

    for (UEdGraphNode* Node : CurrentEditor->GetCurrentGraph()->Nodes)
    {
        if (!Node->IsA(UEdGraphNode_Comment::StaticClass()))
        {
            continue;
        }
        SGraphNode* NodeWidget = GetWidget(Node);
        // Actually, we can't invoke HandleSelection if NodeWidget is not a SGraphNodeComment.
        SGraphNodeComment* CommentNode = StaticCast<SGraphNodeComment*>(NodeWidget);
        (CommentNode->*FPrivateAccessor<FAccessSGraphNodeCommentHandleSelection>::Member)(false, true);
    }
}

static TUniquePtr<FZoomLevelsContainer> TopZoomLevels;
static TUniquePtr<FZoomLevelsContainer> TempZoomLevels;

class FTopZoomLevelContainer : public FZoomLevelsContainer
{
public:
    virtual float GetZoomAmount(int32 InZoomLevel) const override
    {
        return 1.0f;
    }
    virtual int32 GetNearestZoomLevel(float InZoomAmount) const override
    {
        return 0;
    }
    virtual FText GetZoomText(int32 InZoomLevel) const override
    {
        return FText::FromString(TEXT("1:1"));
    }
    virtual int32 GetNumZoomLevels() const override
    {
        return 1;
    }
    virtual int32 GetDefaultZoomLevel() const override
    {
        return 0;
    }
    virtual EGraphRenderingLOD::Type GetLOD(int32 InZoomLevel) const override
    {
        return EGraphRenderingLOD::DefaultDetail;
    }
};

static void TickWidgetRecursively(SWidget* Widget)
{
    Widget->GetChildren();
    if (auto Children = Widget->GetChildren())
    {
        for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            auto ChildWidget = &Children->GetChildAt(ChildIndex).Get();
            TickWidgetRecursively(ChildWidget);
        }
    }
    Widget->Tick(Widget->GetCachedGeometry(), FSlateApplication::Get().GetCurrentTime(), FSlateApplication::Get().GetDeltaTime());
}

void FFormatter::SetZoomLevelTo11Scale() const
{
    if (CurrentEditor)
    {
        auto NodePanel = StaticCast<SNodePanel*>(GetCurrentPanel());
        if (NodePanel)
        {
            if (!TopZoomLevels.IsValid())
            {
                TopZoomLevels = MakeUnique<FTopZoomLevelContainer>();
            }
            auto& ZoomLevels = NodePanel->*FPrivateAccessor<FAccessSGraphPanelZoomLevels>::Member;
            TempZoomLevels = MoveTemp(ZoomLevels);
            ZoomLevels = MoveTemp(TopZoomLevels);
            (NodePanel->*FPrivateAccessor<FAccessSGraphPanelPostChangedZoom>::Member)();
#if (ENGINE_MAJOR_VERSION >= 5)
            NodePanel->Invalidate(EInvalidateWidgetReason::Prepass);
#else
            NodePanel->InvalidatePrepass();
#endif
            auto Nodes = GetAllNodes();
            for (auto Node : Nodes)
            {
                auto GraphNode = GetWidget(Node);
                if (GraphNode != nullptr)
                {
                    TickWidgetRecursively(GraphNode);
                    GraphNode->SlatePrepass();
                }
            }
        }
    }
}

void FFormatter::RestoreZoomLevel() const
{
    if (CurrentEditor)
    {
        auto NodePanel = StaticCast<SNodePanel*>(GetCurrentPanel());
        if (NodePanel)
        {
            if (!TopZoomLevels.IsValid())
            {
                TopZoomLevels = MakeUnique<FTopZoomLevelContainer>();
            }
            auto& ZoomLevels = NodePanel->*FPrivateAccessor<FAccessSGraphPanelZoomLevels>::Member;
            ZoomLevels = MoveTemp(TempZoomLevels);
            (NodePanel->*FPrivateAccessor<FAccessSGraphPanelPostChangedZoom>::Member)();
#if (ENGINE_MAJOR_VERSION >= 5)
            NodePanel->Invalidate(EInvalidateWidgetReason::Prepass);
#else
            NodePanel->InvalidatePrepass();
#endif
        }
    }
}

static TSet<UEdGraphNode*> GetSelectedNodes(SGraphEditor* GraphEditor)
{
    TSet<UEdGraphNode*> SelectedGraphNodes;
    TSet<UObject*> SelectedNodes = GraphEditor->GetSelectedNodes();
    for (UObject* Node : SelectedNodes)
    {
        UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Node);
        if (GraphNode)
        {
            SelectedGraphNodes.Add(GraphNode);
        }
    }
    return SelectedGraphNodes;
}

static TSet<UEdGraphNode*> DoSelectionStrategy(UEdGraph* Graph, TSet<UEdGraphNode*> Selected)
{
    if (Selected.Num() != 0)
    {
        TSet<UEdGraphNode*> SelectedGraphNodes;
        for (UEdGraphNode* GraphNode : Selected)
        {
            SelectedGraphNodes.Add(GraphNode);
            if (GraphNode->IsA(UEdGraphNode_Comment::StaticClass()))
            {
                UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(GraphNode);
                auto NodesInComment = CommentNode->GetNodesUnderComment();
                for (UObject* ObjectInComment : NodesInComment)
                {
                    UEdGraphNode* NodeInComment = Cast<UEdGraphNode>(ObjectInComment);
                    SelectedGraphNodes.Add(NodeInComment);
                }
            }
        }
        return SelectedGraphNodes;
    }
    TSet<UEdGraphNode*> SelectedGraphNodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        SelectedGraphNodes.Add(Node);
    }
    return SelectedGraphNodes;
}

void FFormatter::Format() const
{
    UEdGraph* Graph = CurrentEditor->GetCurrentGraph();
    if (!Graph || !CurrentEditor)
    {
        return;
    }
    UpdateCommentNodes();
    auto SelectedNodes = GetSelectedNodes(CurrentEditor);
    SelectedNodes = DoSelectionStrategy(Graph, SelectedNodes);
    SetZoomLevelTo11Scale();
    FFormatterGraph GraphData(SelectedNodes);
    GraphData.Format();
    RestoreZoomLevel();
    auto FormatData = GraphData.GetBoundMap();
    const FScopedTransaction Transaction(FFormatterCommands::Get().FormatGraph->GetLabel());
    for (auto formatData : FormatData)
    {
        formatData.Key->Modify();
        if (formatData.Key->IsA(UEdGraphNode_Comment::StaticClass()))
        {
            auto CommentNode = Cast<UEdGraphNode_Comment>(formatData.Key);
            CommentNode->SetBounds(formatData.Value);
        }
        else
        {
            formatData.Key->NodePosX = formatData.Value.GetTopLeft().X;
            formatData.Key->NodePosY = formatData.Value.GetTopLeft().Y;
        }
    }
    Graph->NotifyGraphChanged();
}

void FFormatter::PlaceBlock()
{
    UEdGraph* Graph = CurrentEditor->GetCurrentGraph();
    if (!Graph || !CurrentEditor)
    {
        return;
    }
    auto SelectedNodes = GetSelectedNodes(CurrentEditor);
    auto ConnectedNodesLeft = FFormatterGraph::GetNodesConnected(SelectedNodes, FFormatterGraph::EInOutOption::EIOO_IN);
    FVector2D ConnectCenter;
    const UFormatterSettings& Settings = *GetDefault<UFormatterSettings>();
    const FScopedTransaction Transaction(FFormatterCommands::Get().PlaceBlock->GetLabel());
    if (FFormatterGraph::GetNodesConnectCenter(SelectedNodes, ConnectCenter, FFormatterGraph::EInOutOption::EIOO_IN))
    {
        auto Center = FVector(ConnectCenter.X, ConnectCenter.Y, 0);
        auto Direction = IsVerticalLayout ? FVector(0, 1, 0) : FVector(1, 0, 0);
        auto RightRay = FRay(Center, Direction, true);
        FSlateRect Bound = GetNodesBound(ConnectedNodesLeft);
        auto RightBound = IsVerticalLayout ? FVector(0, Bound.Bottom, 0) : FVector(Bound.Right, 0, 0);
        auto LinkedCenter3D = RightRay.PointAt(RightRay.GetParameter(RightBound));
        auto LinkedCenterTo = FVector2D(LinkedCenter3D) + (IsVerticalLayout ? FVector2D(0, Settings.HorizontalSpacing) : FVector2D(Settings.HorizontalSpacing, 0));
        FFormatterGraph::GetNodesConnectCenter(SelectedNodes, ConnectCenter, FFormatterGraph::EInOutOption::EIOO_IN, true);
        Center = FVector(ConnectCenter.X, ConnectCenter.Y, 0);
        Direction = IsVerticalLayout ? FVector(0, -1, 0) : FVector(-1, 0, 0);
        auto LeftRay = FRay(Center, Direction, true);
        Bound = GetNodesBound(SelectedNodes);
        auto LeftBound = IsVerticalLayout ? FVector(0, Bound.Top, 0) : FVector(Bound.Left, 0, 0);
        LinkedCenter3D = LeftRay.PointAt(LeftRay.GetParameter(LeftBound));
        auto LinkedCenterFrom = FVector2D(LinkedCenter3D);
        FVector2D Offset = LinkedCenterTo - LinkedCenterFrom;
        Translate(SelectedNodes, Offset);
    }
    auto ConnectedNodesRight = FFormatterGraph::GetNodesConnected(SelectedNodes, FFormatterGraph::EInOutOption::EIOO_OUT);
    if (FFormatterGraph::GetNodesConnectCenter(SelectedNodes, ConnectCenter, FFormatterGraph::EInOutOption::EIOO_OUT))
    {
        auto Center = FVector(ConnectCenter.X, ConnectCenter.Y, 0);
        auto Direction = IsVerticalLayout ? FVector(0, -1, 0) : FVector(-1, 0, 0);
        auto LeftRay = FRay(Center, Direction, true);
        FSlateRect Bound = GetNodesBound(ConnectedNodesRight);
        auto LeftBound = IsVerticalLayout ? FVector(0, Bound.Top, 0) : FVector(Bound.Left, 0, 0);
        auto LinkedCenter3D = LeftRay.PointAt(LeftRay.GetParameter(LeftBound));
        auto LinkedCenterTo = FVector2D(LinkedCenter3D) - (IsVerticalLayout ? FVector2D(0, Settings.HorizontalSpacing) : FVector2D(Settings.HorizontalSpacing, 0));
        FFormatterGraph::GetNodesConnectCenter(SelectedNodes, ConnectCenter, FFormatterGraph::EInOutOption::EIOO_OUT, true);
        Center = FVector(ConnectCenter.X, ConnectCenter.Y, 0);
        Direction = IsVerticalLayout ? FVector(0, 1, 0) : FVector(1, 0, 0);
        auto RightRay = FRay(Center, Direction, true);
        Bound = GetNodesBound(SelectedNodes);
        auto RightBound = IsVerticalLayout ? FVector(0, Bound.Bottom, 0) : FVector(Bound.Right, 0, 0);
        LinkedCenter3D = RightRay.PointAt(RightRay.GetParameter(RightBound));
        auto LinkedCenterFrom = FVector2D(LinkedCenter3D);
        FVector2D Offset = LinkedCenterFrom - LinkedCenterTo;
        Translate(ConnectedNodesRight, Offset);
    }
}
