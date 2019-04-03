/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "FormatterModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "EditorStyle.h"
#include "GraphEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "ISettingsSection.h"
#include "ISettingsModule.h"
#include "FormatterSettings.h"
#include "FormatterStyle.h"
#include "FormatterCommands.h"
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "FormatterHacker.h"
#include "ScopedTransaction.h"
#include "EdGraphNode_Comment.h"
#include "FormatterGraph.h"

#define LOCTEXT_NAMESPACE "GraphFormatter"

class FFormatterModule : public IGraphFormatterModule
{
	void StartupModule() override;
	void ShutdownModule() override;
	void HandleAssetEditorOpened(UObject* Object, IAssetEditorInstance* Instance);
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	void FormatGraph(FFormatterDelegates GraphDelegates);
	FDelegateHandle GraphEditorDelegateHandle;
	TArray<TSharedPtr<EGraphFormatterPositioningAlgorithm> > AlgorithmOptions;
};

IMPLEMENT_MODULE(FFormatterModule, GraphFormatter)

void FFormatterModule::StartupModule()
{
	FFormatterStyle::Initialize();

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Editor", "Plugins", "GraphFormatter",
		                                                                       LOCTEXT("GraphFormatterSettingsName", "Graph Formatter"),
		                                                                       LOCTEXT("GraphFormatterSettingsDescription", "Configure the Graph Formatter plug-in."),
		                                                                       GetMutableDefault<UFormatterSettings>()
		);
	}

	check(GEditor);
	FFormatterCommands::Register();
	GraphEditorDelegateHandle = FAssetEditorManager::Get().OnAssetOpenedInEditor().AddRaw(this, &FFormatterModule::HandleAssetEditorOpened);
	AlgorithmOptions.Add(MakeShareable(new EGraphFormatterPositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EEvenlyInLayer)));
	AlgorithmOptions.Add(MakeShareable(new EGraphFormatterPositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EPriorityMethod)));
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
			if (GraphNode->IsA(UEdGraphNode_Comment::StaticClass()))
			{
				UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node);
				auto NodesInComment = CommentNode->GetNodesUnderComment();
				for (UObject* ObjectInComment : NodesInComment)
				{
					UEdGraphNode* NodeInComment = Cast<UEdGraphNode>(ObjectInComment);
					SelectedGraphNodes.Add(NodeInComment);
				}
			}
		}
	}
	return SelectedGraphNodes;
}

static TSet<UEdGraphNode*> GetNodesUnderComment(const UEdGraphNode* InNode)
{
	const UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(InNode);
	auto ObjectsUnderComment = CommentNode->GetNodesUnderComment();
	TSet<UEdGraphNode*> SubSelectedNodes;
	for (auto Object : ObjectsUnderComment)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(Object);
		if (Node != nullptr)
		{
			SubSelectedNodes.Add(Node);
		}
	}
	return SubSelectedNodes;
}

static TSet<UEdGraphNode*> DoSelectionStrategy(UEdGraph* Graph, TSet<UEdGraphNode*> Selected)
{
	if (Selected.Num() != 0)
	{
		return Selected;
	}
	TSet<UEdGraphNode*> SelectedGraphNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		SelectedGraphNodes.Add(Node);
	}
	TSet<UEdGraphNode*> NodesUnderComment;
	for (auto SelectedNode : SelectedGraphNodes)
	{
		if (SelectedNode->IsA(UEdGraphNode_Comment::StaticClass()))
		{
			NodesUnderComment.Append(GetNodesUnderComment(SelectedNode));
		}
	}
	SelectedGraphNodes.Append(NodesUnderComment);
	return SelectedGraphNodes;
}

void FFormatterModule::HandleAssetEditorOpened(UObject* Object, IAssetEditorInstance* Instance)
{
	FFormatterDelegates GraphDelegates = FFormatterHacker::GetDelegates(Object, Instance);
	if (GraphDelegates.GetGraphDelegate.IsBound())
	{
		FAssetEditorToolkit* assetEditorToolkit = StaticCast<FAssetEditorToolkit*>(Instance);
		const FFormatterCommands& Commands = FFormatterCommands::Get();
		TSharedRef<FUICommandList> ToolkitCommands = assetEditorToolkit->GetToolkitCommands();
		if (ToolkitCommands->IsActionMapped(Commands.FormatGraph))
		{
			return;
		}
		ToolkitCommands->MapAction(Commands.FormatGraph,
		                           FExecuteAction::CreateRaw(this, &FFormatterModule::FormatGraph, GraphDelegates),
		                           FCanExecuteAction::CreateLambda([]()-> bool { return true; })
		);
		TSharedPtr<FExtender> Extender = FAssetEditorToolkit::GetSharedToolBarExtensibilityManager()->GetAllExtenders();
		assetEditorToolkit->AddToolbarExtender(Extender);
		Extender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			ToolkitCommands,
			FToolBarExtensionDelegate::CreateRaw(this, &FFormatterModule::FillToolbar)
		);
	}
}

static FText GetEnumAsString(EGraphFormatterPositioningAlgorithm EnumValue)
{
	switch (EnumValue)
	{
	case EGraphFormatterPositioningAlgorithm::EEvenlyInLayer:
		return FText::FromString("Place node evenly in layer");
	case EGraphFormatterPositioningAlgorithm::EPriorityMethod:
		return FText::FromString("Use priority method");
	default:
		return FText::FromString("Invalid");
	}
}

void FFormatterModule::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	const UFormatterSettings* Settings = GetDefault<UFormatterSettings>();
	UFormatterSettings* MutableSettings = GetMutableDefault<UFormatterSettings>();
	const FFormatterCommands& Commands = FFormatterCommands::Get();
	ToolbarBuilder.BeginSection("GraphFormatter");
	{
		TSharedPtr<SComboBox<TSharedPtr<EGraphFormatterPositioningAlgorithm> > > AlgorithmComboBox;
		int32 SelectedAlgorithmIndex = AlgorithmOptions.IndexOfByPredicate([Settings](const TSharedPtr<EGraphFormatterPositioningAlgorithm>& Option)
		{
			return *Option == Settings->PositioningAlgorithm;
		});
		ToolbarBuilder.AddToolBarButton
		(
			Commands.FormatGraph,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(FSlateIcon(FFormatterStyle::Get()->GetStyleSetName(), "GraphFormatter.ApplyIcon")),
			FName(TEXT("GraphFormatter"))
		);
		auto EditArea = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(100)
				.MaxValue(1000)
				.MinSliderValue(100)
				.MaxSliderValue(1000)
				.ToolTipText(LOCTEXT("GraphFormatterHorizontalSpacingiToolTips", "Spacing between two layers."))
				.Value(Settings->HorizontalSpacing)
				.MinDesiredWidth(80)
				.OnValueCommitted_Lambda([MutableSettings](int32 Number, ETextCommit::Type CommitInfo)
				{
					MutableSettings->HorizontalSpacing = Number;
					MutableSettings->PostEditChange();
					MutableSettings->SaveConfig();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(AlgorithmComboBox, SComboBox<TSharedPtr<EGraphFormatterPositioningAlgorithm> >)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.OptionsSource(&AlgorithmOptions)
				.ToolTipText_Lambda([]() { return FText::FromString("Positioning Algorithm"); })
				.InitiallySelectedItem(AlgorithmOptions[SelectedAlgorithmIndex])
				.OnSelectionChanged_Lambda([=](TSharedPtr<EGraphFormatterPositioningAlgorithm> NewOption, ESelectInfo::Type SelectInfo)
				{
					if (SelectInfo != ESelectInfo::Direct)
					{
						MutableSettings->PositioningAlgorithm = *NewOption;
						MutableSettings->PostEditChange();
						MutableSettings->SaveConfig();
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<EGraphFormatterPositioningAlgorithm> AlgorithmEnum)
				{
					return SNew( STextBlock ).Text(GetEnumAsString(*AlgorithmEnum.Get()));
				})
				[
					SNew( STextBlock )
					.Text_Lambda([Settings]() { return GetEnumAsString(Settings->PositioningAlgorithm); })
				]
			];
		ToolbarBuilder.AddWidget(EditArea);
	}
	ToolbarBuilder.EndSection();
}

void FFormatterModule::FormatGraph(FFormatterDelegates GraphDelegates)
{
	UEdGraph* Graph = GraphDelegates.GetGraphDelegate.Execute();
	SGraphEditor* GraphEditor = GraphDelegates.GetGraphEditorDelegate.Execute();
	if (!Graph || !GraphEditor)
	{
		return;
	}
	FFormatterHacker::UpdateCommentNodes(GraphEditor, Graph);
	auto SelectedNodes = GetSelectedNodes(GraphEditor);
	SelectedNodes = DoSelectionStrategy(Graph, SelectedNodes);
	FFormatterHacker::ComputeLayoutAtRatioOne(GraphDelegates, SelectedNodes);
	FFormatterGraph GraphData(Graph, SelectedNodes, GraphDelegates);
	GraphData.Format();
	FFormatterHacker::RestoreZoomLevel(GraphDelegates);
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

void FFormatterModule::ShutdownModule()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "GraphFormatter");
	}
	FAssetEditorManager::Get().OnAssetOpenedInEditor().Remove(GraphEditorDelegateHandle);
	FFormatterStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
