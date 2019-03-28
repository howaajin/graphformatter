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
#include "Editor/BehaviorTreeEditor/Private/BehaviorTreeEditor.h"
#include "Editor/AudioEditor/Private/SoundCueEditor.h"
#include "Sound/SoundCue.h"

DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessMaterialGraphEditor, FMaterialEditor, TSharedPtr<SGraphEditor>, GraphEditor)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSoundCueGraphEditor, FSoundCueEditor, TSharedPtr<SGraphEditor>, SoundCueGraphEditor)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessBlueprintGraphEditor, FBlueprintEditor, TWeakPtr<SGraphEditor>, FocusedGraphEdPtr)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessAIGraphEditor, FAIGraphEditor, TWeakPtr<SGraphEditor>, UpdateGraphEdPtr)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphEditorImpl, SGraphEditor, TSharedPtr<SGraphEditor>, Implementation)
DECLARE_PRIVATE_MEMBER_ACCESSOR(FAccessSGraphEditorPanel, SGraphEditorImpl, TSharedPtr<SGraphPanel>, GraphPanel)
DECLARE_PRIVATE_CONST_FUNC_ACCESSOR(FAccessSGraphNodeCommentHandleSelection, SGraphNodeComment, HandleSelection, void, bool, bool)

template<typename TType>
SGraphEditor* GraphEditorConvertor(TType& Member)
{
	return Member;
}

template<>
SGraphEditor* GraphEditorConvertor(TWeakPtr<SGraphEditor>& Member)
{
	if (Member.IsValid())
	{
		return Member.Pin().Get();
	}
	return nullptr;
}

template<>
SGraphEditor* GraphEditorConvertor(TSharedPtr<SGraphEditor>& Member)
{
	if (Member.IsValid())
	{
		return Member.Get();
	}
	return nullptr;
}

template<typename TAdapter, typename TGraphEditor>
SGraphEditor* GetGraphEditor(TGraphEditor* Editor)
{
	if (Editor)
	{
		return GraphEditorConvertor(Editor->*FPrivateAccessor<TAdapter>::Member);
	}
	return nullptr;
}

static TSharedPtr<SGraphNode> GetGraphNode(const SGraphEditor* GraphEditor, const UEdGraphNode* Node)
{
	TSharedPtr<SGraphEditor> Impl = GraphEditor->*FPrivateAccessor<FAccessSGraphEditorImpl>::Member;
	SGraphEditorImpl* GraphEditorImpl = StaticCast<SGraphEditorImpl*>(Impl.Get());
	TSharedPtr<SGraphPanel> GraphPanel = GraphEditorImpl->*FPrivateAccessor<FAccessSGraphEditorPanel>::Member;
	TSharedPtr<SGraphNode> NodeWidget = GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
	return NodeWidget;
}

static FVector2D GetNodeSize(const SGraphEditor* GraphEditor, const UEdGraphNode* Node)
{
	auto GraphNode = GetGraphNode(GraphEditor, Node);
	if (GraphNode.IsValid())
	{
		GraphNode->SlatePrepass(1.0f);
		return GraphNode->GetDesiredSize();
	}
	return FVector2D(Node->NodeWidth, Node->NodeHeight);
}

static FVector2D GetPinOffset(const SGraphEditor* GraphEditor, const UEdGraphPin* Pin)
{
	auto GraphNode = GetGraphNode(GraphEditor, Pin->GetOwningNodeUnchecked());
	auto PinWidget = GraphNode->FindWidgetForPin(const_cast<UEdGraphPin*>(Pin));
	if (PinWidget.IsValid())
	{
		const auto Offset = PinWidget->GetNodeOffset();
		return Offset;
	}
	return FVector2D::ZeroVector;
}

template<typename TAdapter, typename TGraphEditor>
FFormatterDelegates GetDelegates(TGraphEditor* Editor)
{
	FFormatterDelegates GraphFormatterDelegates;
	GraphFormatterDelegates.BoundCalculator.BindLambda([=](UEdGraphNode* Node)
	{
		SGraphEditor* GraphEditor = GetGraphEditor<TAdapter>(Editor);
		return GetNodeSize(GraphEditor, Node);
	});
	GraphFormatterDelegates.OffsetCalculator.BindLambda([=](UEdGraphPin* Pin)
	{
		SGraphEditor* GraphEditor = GetGraphEditor<TAdapter>(Editor);
		return GetPinOffset(GraphEditor, Pin);
	});
	GraphFormatterDelegates.GetGraphDelegate.BindLambda([=]()
	{
		SGraphEditor* GraphEditor = GetGraphEditor<TAdapter>(Editor);
		if (GraphEditor)
		{
			return GraphEditor->GetCurrentGraph();
		}
		return static_cast<UEdGraph*>(nullptr);
	});
	GraphFormatterDelegates.GetGraphEditorDelegate.BindLambda([=]()
	{
		return GetGraphEditor<TAdapter>(Editor);
	});
	return GraphFormatterDelegates;
}

void FFormatterHacker::UpdateCommentNodes(SGraphEditor* GraphEditor, UEdGraph* Graph)
{
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node->IsA(UEdGraphNode_Comment::StaticClass()))
		{
			continue;
		}
		TSharedPtr<SGraphNode> NodeWidget = GetGraphNode(GraphEditor, Node);
		TSharedPtr<SGraphNodeComment> CommentNode = StaticCastSharedPtr<SGraphNodeComment>(NodeWidget);
		(CommentNode.Get()->*FPrivateAccessor<FAccessSGraphNodeCommentHandleSelection>::Member)(false, true);
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
			GraphFormatterDelegates = ::GetDelegates<FAccessBlueprintGraphEditor>(BlueprintEditor);
			return GraphFormatterDelegates;
		}
	}
	if (Cast<UMaterial>(Object))
	{
		FMaterialEditor* MaterialEditor = StaticCast<FMaterialEditor*>(Instance);
		if (MaterialEditor)
		{
			GraphFormatterDelegates = ::GetDelegates<FAccessMaterialGraphEditor>(MaterialEditor);
			return GraphFormatterDelegates;
		}
	}
	if (Cast<USoundCue>(Object))
	{
		FSoundCueEditor* SoundCueEditor = StaticCast<FSoundCueEditor*>(Instance);
		if (SoundCueEditor)
		{
			GraphFormatterDelegates = ::GetDelegates<FAccessSoundCueGraphEditor>(SoundCueEditor);
			return GraphFormatterDelegates;
		}
	}
	return GraphFormatterDelegates;
}
