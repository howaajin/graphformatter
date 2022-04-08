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
#if (ENGINE_MINOR_VERSION >= 24 || ENGINE_MAJOR_VERSION >= 5)
#else
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#endif
#include "FormatterHacker.h"
#include "ScopedTransaction.h"
#include "EdGraphNode_Comment.h"
#include "FormatterGraph.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "GraphFormatter"

class FFormatterModule : public IGraphFormatterModule
{
	void StartupModule() override;
	void ShutdownModule() override;
	void HandleAssetEditorOpened(UObject* Object, IAssetEditorInstance* Instance);
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	void FormatGraph(FFormatterDelegates GraphDelegates);
	void ToggleStraightenConnections();
	bool IsStraightenConnectionsEnabled();
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
#if (ENGINE_MINOR_VERSION >= 24 || ENGINE_MAJOR_VERSION >= 5)
	GraphEditorDelegateHandle = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetOpenedInEditor().AddRaw(this, &FFormatterModule::HandleAssetEditorOpened);
#else
	GraphEditorDelegateHandle = FAssetEditorManager::Get().OnAssetOpenedInEditor().AddRaw(this, &FFormatterModule::HandleAssetEditorOpened);
#endif
	AlgorithmOptions.Add(MakeShareable(new EGraphFormatterPositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EEvenlyInLayer)));
	AlgorithmOptions.Add(MakeShareable(new EGraphFormatterPositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop)));
	AlgorithmOptions.Add(MakeShareable(new EGraphFormatterPositioningAlgorithm(EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian)));
	AlgorithmOptions.Add(MakeShareable(new EGraphFormatterPositioningAlgorithm(EGraphFormatterPositioningAlgorithm::ELayerSweep)));
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
		                           FCanExecuteAction());
		TSharedPtr<FExtender> Extender = FAssetEditorToolkit::GetSharedToolBarExtensibilityManager()->GetAllExtenders();
		assetEditorToolkit->AddToolbarExtender(Extender);
		Extender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			ToolkitCommands,
			FToolBarExtensionDelegate::CreateRaw(this, &FFormatterModule::FillToolbar)
		);
		if (!ToolkitCommands->IsActionMapped(Commands.StraightenConnections))
		{
			ToolkitCommands->MapAction(Commands.StraightenConnections,
				FExecuteAction::CreateRaw(this, &FFormatterModule::ToggleStraightenConnections),
				FCanExecuteAction(),
				FIsActionChecked::CreateRaw(this, &FFormatterModule::IsStraightenConnectionsEnabled));
		}
	}
}

static FText GetEnumAsString(EGraphFormatterPositioningAlgorithm EnumValue)
{
	switch (EnumValue)
	{
	case EGraphFormatterPositioningAlgorithm::EEvenlyInLayer:
		return FText::FromString("Place node evenly in layer");
	case EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodTop:
		return FText::FromString("FAS Top");
	case EGraphFormatterPositioningAlgorithm::EFastAndSimpleMethodMedian:
		return FText::FromString("FAS Median");
	case EGraphFormatterPositioningAlgorithm::ELayerSweep:
		return FText::FromString("Layer sweep");
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
		auto HorizontalEditArea = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.MaxValue(1000)
				.MinSliderValue(0)
				.MaxSliderValue(1000)
				.ToolTipText(LOCTEXT("GraphFormatterHorizontalSpacingiToolTips", "Spacing between two layers."))
				.Value(Settings->HorizontalSpacing)
				.MinDesiredWidth(80)
				.Visibility_Lambda([]() { return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnValueCommitted_Lambda([MutableSettings](int32 Number, ETextCommit::Type CommitInfo)
				{
					MutableSettings->HorizontalSpacing = Number;
					MutableSettings->PostEditChange();
					MutableSettings->SaveConfig();
				})
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			[
				SAssignNew(AlgorithmComboBox, SComboBox<TSharedPtr<EGraphFormatterPositioningAlgorithm> >)
				.Visibility_Lambda([]() { return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
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
			auto VerticalEditArea = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				SNew(SSpinBox<int32>)
				.Visibility_Lambda([]() { return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible; })
				.MinValue(0)
				.MaxValue(1000)
				.MinSliderValue(0)
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
			.Padding(2.0f)
			[
				SAssignNew(AlgorithmComboBox, SComboBox<TSharedPtr<EGraphFormatterPositioningAlgorithm> >)
				.Visibility_Lambda([]() { return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible; })
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
		ToolbarBuilder.AddWidget(HorizontalEditArea);
		ToolbarBuilder.AddWidget(VerticalEditArea);
		ToolbarBuilder.AddToolBarButton
		(
			Commands.StraightenConnections,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(FSlateIcon(FFormatterStyle::Get()->GetStyleSetName(), "GraphFormatter.StraightenIcon")),
			FName(TEXT("GraphFormatter"))
		);
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
	FFormatterHacker::ComputeNodesSizeAtRatioOne(GraphDelegates, SelectedNodes);
	FFormatterGraph GraphData(SelectedNodes, GraphDelegates);
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
			if(GraphDelegates.MoveTo.IsBound())
			{
			    GraphDelegates.MoveTo.Execute(formatData.Key, formatData.Value.GetTopLeft());
			}
		    else
		    {
		        formatData.Key->NodePosX = formatData.Value.GetTopLeft().X;
		        formatData.Key->NodePosY = formatData.Value.GetTopLeft().Y;
		    }
		}
	}
	Graph->NotifyGraphChanged();
	GraphDelegates.MarkGraphDirty.ExecuteIfBound();
}

void FFormatterModule::ToggleStraightenConnections()
{
	auto GraphEditorSettings = GetMutableDefault<UGraphEditorSettings>();

	if(IsStraightenConnectionsEnabled())
	{
		const UFormatterSettings* Settings = GetDefault<UFormatterSettings>();
		GraphEditorSettings->ForwardSplineTangentFromHorizontalDelta = Settings->ForwardSplineTangentFromHorizontalDelta;
		GraphEditorSettings->ForwardSplineTangentFromVerticalDelta= Settings->ForwardSplineTangentFromVerticalDelta;
		GraphEditorSettings->BackwardSplineTangentFromHorizontalDelta= Settings->BackwardSplineTangentFromHorizontalDelta;
		GraphEditorSettings->BackwardSplineTangentFromVerticalDelta = Settings->BackwardSplineTangentFromVerticalDelta;
		GraphEditorSettings->PostEditChange();
		GraphEditorSettings->SaveConfig();
	}
	else
	{
		UFormatterSettings* Settings = GetMutableDefault<UFormatterSettings>();
		Settings->ForwardSplineTangentFromHorizontalDelta = GraphEditorSettings->ForwardSplineTangentFromHorizontalDelta;
		Settings->ForwardSplineTangentFromVerticalDelta = GraphEditorSettings->ForwardSplineTangentFromVerticalDelta;
		Settings->BackwardSplineTangentFromHorizontalDelta = GraphEditorSettings->BackwardSplineTangentFromHorizontalDelta;
		Settings->BackwardSplineTangentFromVerticalDelta = GraphEditorSettings->BackwardSplineTangentFromVerticalDelta;
		Settings->PostEditChange();
		Settings->SaveConfig();
		GraphEditorSettings->ForwardSplineTangentFromHorizontalDelta = FVector2D(0,0);
		GraphEditorSettings->ForwardSplineTangentFromVerticalDelta= FVector2D(0,0);
		GraphEditorSettings->BackwardSplineTangentFromHorizontalDelta= FVector2D(0,0);
		GraphEditorSettings->BackwardSplineTangentFromVerticalDelta = FVector2D(0,0);
		GraphEditorSettings->PostEditChange();
		GraphEditorSettings->SaveConfig();
	}
}

bool FFormatterModule::IsStraightenConnectionsEnabled()
{
	auto GraphEditorSettings = GetDefault<UGraphEditorSettings>();
	if (GraphEditorSettings->ForwardSplineTangentFromHorizontalDelta == FVector2D(0, 0) &&
		GraphEditorSettings->ForwardSplineTangentFromVerticalDelta== FVector2D(0, 0) &&
		GraphEditorSettings->BackwardSplineTangentFromHorizontalDelta == FVector2D(0, 0) &&
		GraphEditorSettings->BackwardSplineTangentFromVerticalDelta == FVector2D(0, 0))
	{
		return true;
	}
	return false;
}

void FFormatterModule::ShutdownModule()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "GraphFormatter");
	}
#if (ENGINE_MINOR_VERSION >= 24 || ENGINE_MAJOR_VERSION >= 5)
	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetOpenedInEditor().Remove(GraphEditorDelegateHandle);
	}
#else
	FAssetEditorManager::Get().OnAssetOpenedInEditor().Remove(GraphEditorDelegateHandle);
#endif
	FFormatterStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
