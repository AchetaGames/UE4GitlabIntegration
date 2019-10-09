// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SWrapBox.h"
#include "API/GitlabAPI.h"
#include "EditorStyleSet.h"

class FToolBarBuilder;
class FMenuBuilder;

DECLARE_LOG_CATEGORY_EXTERN(LogGitlabIntegration, Log, All);

class FGitlabIntegrationModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();

	
private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);
	void RefreshIssues();
    void RefreshLabels();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

    TSharedRef<SWidget> CreateProjectSelectionButton();
    TSharedRef<STextBlock> CreateProjectSelectionButtonText();
    TSharedRef<SWidget> GenerateProjectList();

    TSharedRef<ITableRow> GenerateIssueWidget(TSharedPtr<FGitlabIntegrationIAPIIssue> IssueInfo, const TSharedRef<STableViewBase>& OwnerTable);

    TSharedRef<SHorizontalBox> GenerateLabelWidget(TSharedPtr<FGitlabIntegrationIAPILabel> LabelInfo, bool OnIssue);

    IAPI* Api;

    FTimerHandle IssueTracking;
    FString IssueSearch;
    bool IssueSortNewFirst;

private:
	TSharedPtr<class FUICommandList> PluginCommands;

    void RegisterSettings();
    void UnregisterSettings();
    bool HandleSettingsSaved();
    void HandleProjectSelection(FGitlabIntegrationIAPIProject project);
    void FinishTimeTracking(TSharedPtr<FGitlabIntegrationIAPIIssue> issue);

    TSharedPtr<STextBlock> ProjectSelectionButtonText;
    /** Holds the filtered list of issues */
    TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> IssueList;
    TMap<TSharedPtr<FGitlabIntegrationIAPIIssue>, FDateTime> TimeTrackingMap;
    TMap<TSharedPtr<FGitlabIntegrationIAPIIssue>, TSharedPtr<SBorder>> TimeTrackingBorderMap;

    /** Holds the message type list view. */
    TSharedPtr<SListView<TSharedPtr<FGitlabIntegrationIAPIIssue>>> IssueListView;

    TArray<TSharedPtr<FGitlabIntegrationIAPILabel>> LabelList;
    TSharedPtr<SWrapBox> LabelWrapBox;
    TArray<FString> SelectedLabels;

};
