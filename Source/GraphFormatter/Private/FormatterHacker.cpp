/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterHacker.h"
#include "EdGraph/EdGraph.h"
#include "GraphEditor.h"
#include "BlueprintEditor.h"
#include "SGraphNodeComment.h"
#include "GraphEditor/Private/SGraphEditorImpl.h"
#include "SGraphPanel.h"
#include "EdGraphNode_Comment.h"
#include "MaterialEditor/Private/MaterialEditor.h"
#include "MaterialGraph/MaterialGraph.h"
#include "PrivateAccessor.h"
#include "BehaviorTree/BehaviorTree.h"
#include "AIGraphEditor.h"
#include "Editor/AudioEditor/Private/SoundCueEditor.h"
#include "Sound/SoundCue.h"
#include "FormatterDelegates.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/BehaviorTreeEditor/Private/BehaviorTreeEditor.h"
#include "FormatterGraph.h"

/* 
** Offsets for the purpose of debug, only tested on Win64
** On Windows, acquiring the pointer to private member of a class in debug build don't work as expect
** So I have to do this hack
*/
#if UE_BUILD_DEBUG
static const int32 OffsetOf_FBlueprintEditor_FocusedGraphEdPtr = 0x208;
static const int32 OffsetOf_SGraphEditor_Implementation = 0x308;
static const int32 OffsetOf_SGraphEditorImpl_GraphPanel = 0x390;
static const int32 OffsetOf_FMaterialEditor_GraphEditor = 0x2b0;
static const int32 OffsetOf_FSoundCueEditor_SoundCueGraphEditor = 0x1e0;
static const int32 OffsetOf_SNodePanel_ZoomLevels = 0x218;
#else
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessMaterialGraphEditor, FMaterialEditor, TSharedPtr<SGraphEditor>, GraphEditor)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSoundCueGraphEditor, FSoundCueEditor, TSharedPtr<SGraphEditor>, SoundCueGraphEditor)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessBlueprintGraphEditor, FBlueprintEditor, TWeakPtr<SGraphEditor>, FocusedGraphEdPtr)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessAIGraphEditor, FAIGraphEditor, TWeakPtr<SGraphEditor>, UpdateGraphEdPtr)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphEditorImpl, SGraphEditor, TSharedPtr<SGraphEditor>, Implementation)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphEditorPanel, SGraphEditorImpl, TSharedPtr<SGraphPanel>, GraphPanel)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphPanelZoomLevels, SNodePanel, TUniquePtr<FZoomLevelsContainer>, ZoomLevels)
#endif
struct FAccessSGraphPanelPostChangedZoom
{
	using MemberType = FunctionWrapper<SNodePanel, void>::Signature;
};

template struct FPrivateRob<FAccessSGraphPanelPostChangedZoom, &SNodePanel::PostChangedZoom>;
DECLARE_PRIVATE_CONST_FUNC_ACCESSOR(FAccessSGraphNodeCommentHandleSelection, SGraphNodeComment, HandleSelection, void, bool, bool)

template <typename TType, int32 offset, typename TClass>
TType* OffsetBy(TClass* Class)
{
	return (TType*)((char*)(Class) + offset);
}

SGraphEditor* GetGraphEditor(const FBlueprintEditor* Editor)
{
#if UE_BUILD_DEBUG
	auto& Ptr = *OffsetBy<TWeakPtr<SGraphEditor>, OffsetOf_FBlueprintEditor_FocusedGraphEdPtr>(Editor);
#else
	auto& Ptr = Editor->*FPrivateAccessor<FAccessBlueprintGraphEditor>::Member;
#endif
	if (Ptr.IsValid())
	{
		return Ptr.Pin().Get();
	}
	return nullptr;
}

SGraphEditor* GetGraphEditor(const FMaterialEditor* Editor)
{
#if UE_BUILD_DEBUG
	auto& GraphEditor = *OffsetBy<TSharedPtr<SGraphEditor>, OffsetOf_FMaterialEditor_GraphEditor>(Editor);
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
#if UE_BUILD_DEBUG
	auto& GraphEditor = *OffsetBy<TSharedPtr<SGraphEditor>, OffsetOf_FSoundCueEditor_SoundCueGraphEditor>(Editor);
#else
	auto& GraphEditor = Editor->*FPrivateAccessor<FAccessSoundCueGraphEditor>::Member;
#endif
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

SGraphPanel* GetGraphPanel(const SGraphEditor* GraphEditor)
{
#if UE_BUILD_DEBUG
	auto& Impl = *OffsetBy<TSharedPtr<SGraphEditor>, OffsetOf_SGraphEditor_Implementation>(GraphEditor);
#else
	auto& Impl = GraphEditor->*FPrivateAccessor<FAccessSGraphEditorImpl>::Member;
#endif
	SGraphEditorImpl* GraphEditorImpl = StaticCast<SGraphEditorImpl*>(Impl.Get());
#if UE_BUILD_DEBUG
	TSharedPtr<SGraphPanel> GraphPanel = *OffsetBy<TSharedPtr<SGraphPanel>, OffsetOf_SGraphEditorImpl_GraphPanel>(GraphEditorImpl);
#else
	auto& GraphPanel = GraphEditorImpl->*FPrivateAccessor<FAccessSGraphEditorPanel>::Member;
#endif
	if (GraphPanel.IsValid())
	{
		return GraphPanel.Get();
	}
	return nullptr;
}

static TUniquePtr<FZoomLevelsContainer> TopZoomLevels;
static TUniquePtr<FZoomLevelsContainer> TempZoomLevels;

class FTopZoomLevelContainer : public FZoomLevelsContainer
{
public:
	float GetZoomAmount(int32 InZoomLevel) const override { return 1.0f; }
	int32 GetNearestZoomLevel(float InZoomAmount) const override { return 0; }
	FText GetZoomText(int32 InZoomLevel) const override { return FText::FromString(TEXT("1:1")); }
	int32 GetNumZoomLevels() const override { return 1; }
	int32 GetDefaultZoomLevel() const override { return 0; }
	EGraphRenderingLOD::Type GetLOD(int32 InZoomLevel) const override { return EGraphRenderingLOD::DefaultDetail; }
};

static SGraphNode* GetGraphNode(const SGraphEditor* GraphEditor, const UEdGraphNode* Node)
{
	SGraphPanel* GraphPanel = GetGraphPanel(GraphEditor);
	if (GraphPanel != nullptr)
	{
		TSharedPtr<SGraphNode> NodeWidget = GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
		return NodeWidget.Get();
	}
	return nullptr;
}

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

static FVector2D GetNodeSize(const SGraphEditor* GraphEditor, const UEdGraphNode* Node)
{
	auto GraphNode = GetGraphNode(GraphEditor, Node);
	if (GraphNode != nullptr)
	{
		FVector2D Size = GraphNode->GetDesiredSize();
		return Size;
	}
	return FVector2D(Node->NodeWidth, Node->NodeHeight);
}

static FVector2D GetPinOffset(const SGraphEditor* GraphEditor, const UEdGraphPin* Pin)
{
	auto GraphNode = GetGraphNode(GraphEditor, Pin->GetOwningNodeUnchecked());
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

template <typename TGraphEditor>
FFormatterDelegates GetDelegates(TGraphEditor* Editor)
{
	FFormatterDelegates GraphFormatterDelegates;
	GraphFormatterDelegates.BoundCalculator.BindLambda([=](UEdGraphNode* Node)
	{
		SGraphEditor* GraphEditor = GetGraphEditor(Editor);
		return GetNodeSize(GraphEditor, Node);
	});
	GraphFormatterDelegates.OffsetCalculator.BindLambda([=](UEdGraphPin* Pin)
	{
		SGraphEditor* GraphEditor = GetGraphEditor(Editor);
		return GetPinOffset(GraphEditor, Pin);
	});
	GraphFormatterDelegates.GetGraphDelegate.BindLambda([=]()
	{
		SGraphEditor* GraphEditor = GetGraphEditor(Editor);
		if (GraphEditor)
		{
			return GraphEditor->GetCurrentGraph();
		}
		return static_cast<UEdGraph*>(nullptr);
	});
	GraphFormatterDelegates.GetGraphEditorDelegate.BindLambda([=]()
	{
		return GetGraphEditor(Editor);
	});
	return GraphFormatterDelegates;
}

// SGraphNodeComment do not tell us what nodes under it until we select it.
// This function invoke HandleSelection to do the work.
void FFormatterHacker::UpdateCommentNodes(SGraphEditor* GraphEditor, UEdGraph* Graph)
{
	// No need to do this hack when AutoSizeComments is used.
	auto AutoSizeCommentModule = FModuleManager::Get().GetModule(FName("AutoSizeComments"));
	if (AutoSizeCommentModule)
	{
		return;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node->IsA(UEdGraphNode_Comment::StaticClass()))
		{
			continue;
		}
		SGraphNode* NodeWidget = GetGraphNode(GraphEditor, Node);
		// Actually, we can't invoke HandleSelection if NodeWidget is not a SGraphNodeComment.
		SGraphNodeComment* CommentNode = StaticCast<SGraphNodeComment*>(NodeWidget);
		(CommentNode->*FPrivateAccessor<FAccessSGraphNodeCommentHandleSelection>::Member)(false, true);
	}
}

FFormatterDelegates FFormatterHacker::GetDelegates(UObject* Object, IAssetEditorInstance* Instance)
{
	FFormatterDelegates GraphFormatterDelegates;

	if (Cast<UBlueprint>(Object))
	{
		FBlueprintEditor* BlueprintEditor = StaticCast<FBlueprintEditor*>(Instance);
		if (BlueprintEditor)
		{
			GraphFormatterDelegates = ::GetDelegates(BlueprintEditor);
			return GraphFormatterDelegates;
		}
	}
	if (Cast<UMaterial>(Object))
	{
		FMaterialEditor* MaterialEditor = StaticCast<FMaterialEditor*>(Instance);
		if (MaterialEditor)
		{
			GraphFormatterDelegates = ::GetDelegates(MaterialEditor);
			GraphFormatterDelegates.MarkGraphDirty.BindLambda([MaterialEditor]()
			{
				MaterialEditor->bMaterialDirty = true;
			});
			return GraphFormatterDelegates;
		}
	}
	if (Cast<USoundCue>(Object))
	{
		FSoundCueEditor* SoundCueEditor = StaticCast<FSoundCueEditor*>(Instance);
		if (SoundCueEditor)
		{
			GraphFormatterDelegates = ::GetDelegates(SoundCueEditor);
			return GraphFormatterDelegates;
		}
	}
	if (Cast<UBehaviorTree>(Object))
	{
		FBehaviorTreeEditor* BehaviorTreeEditor = StaticCast<FBehaviorTreeEditor*>(Instance);
		if (BehaviorTreeEditor)
		{
			GraphFormatterDelegates = ::GetDelegates(BehaviorTreeEditor);
			GraphFormatterDelegates.IsVerticalPositioning.BindLambda([]()
			{
				return true;
			});
			GraphFormatterDelegates.NodeComparer.BindLambda([](const FFormatterNode& A, const FFormatterNode& B)
			{
				UBehaviorTreeGraphNode* StateNodeA = (UBehaviorTreeGraphNode*)(A.OriginalNode);
				UBTNode* BTNodeA = (UBTNode*)(StateNodeA->NodeInstance);
				int32 IndexA = 0;
				IndexA = (BTNodeA && BTNodeA->GetExecutionIndex() < 0xffff) ? BTNodeA->GetExecutionIndex() : -1;
				UBehaviorTreeGraphNode* StateNodeB = (UBehaviorTreeGraphNode*)(B.OriginalNode);
				UBTNode* BTNodeB = (UBTNode*)(StateNodeB->NodeInstance);
				int32 IndexB = 0;
				IndexB = (BTNodeB && BTNodeB->GetExecutionIndex() < 0xffff) ? BTNodeB->GetExecutionIndex() : -1;
				return IndexA < IndexB;
			});
			return GraphFormatterDelegates;
		}
	}
	return GraphFormatterDelegates;
}

void FFormatterHacker::ComputeNodesSizeAtRatioOne(FFormatterDelegates GraphDelegates, TSet<UEdGraphNode*> Nodes)
{
	auto GraphEditor = GraphDelegates.GetGraphEditorDelegate.Execute();
	if (GraphEditor)
	{
		auto NodePanel = StaticCast<SNodePanel*>(GetGraphPanel(GraphEditor));
		if (NodePanel)
		{
			if (!TopZoomLevels.IsValid())
			{
				TopZoomLevels = MakeUnique<FTopZoomLevelContainer>();
			}
#if UE_BUILD_DEBUG
			auto& ZoomLevels = *OffsetBy<TUniquePtr<FZoomLevelsContainer>, OffsetOf_SNodePanel_ZoomLevels>(NodePanel);
#else
			auto& ZoomLevels = NodePanel->*FPrivateAccessor<FAccessSGraphPanelZoomLevels>::Member;
#endif
			TempZoomLevels = MoveTemp(ZoomLevels);
			ZoomLevels = MoveTemp(TopZoomLevels);
			(NodePanel->*FPrivateAccessor<FAccessSGraphPanelPostChangedZoom>::Member)();
			NodePanel->InvalidatePrepass();
			for (auto Node : Nodes)
			{
				auto GraphNode = GetGraphNode(GraphEditor, Node);
				if (GraphNode != nullptr)
				{
					TickWidgetRecursively(GraphNode);
					GraphNode->SlatePrepass();
				}
			}
		}
	}
}

void FFormatterHacker::RestoreZoomLevel(FFormatterDelegates GraphDelegates)
{
	auto GraphEditor = GraphDelegates.GetGraphEditorDelegate.Execute();
	if (GraphEditor)
	{
		auto NodePanel = StaticCast<SNodePanel*>(GetGraphPanel(GraphEditor));
		if (NodePanel)
		{
			if (!TopZoomLevels.IsValid())
			{
				TopZoomLevels = MakeUnique<FTopZoomLevelContainer>();
			}
#if UE_BUILD_DEBUG
			auto& ZoomLevels = *OffsetBy<TUniquePtr<FZoomLevelsContainer>, OffsetOf_SNodePanel_ZoomLevels>(NodePanel);
#else
			auto& ZoomLevels = NodePanel->*FPrivateAccessor<FAccessSGraphPanelZoomLevels>::Member;
#endif
			ZoomLevels = MoveTemp(TempZoomLevels);
			(NodePanel->*FPrivateAccessor<FAccessSGraphPanelPostChangedZoom>::Member)();
			NodePanel->InvalidatePrepass();
		}
	}
}
