// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GitlabIntegration.h"
#include <functional>
#include <Editor/UnrealEd/Public/UnrealEd.h>
#include "GitlabIntegrationStyle.h"
#include "GitlabIntegrationCommands.h"
#include "LevelEditor.h"
#include "EditorStyleSet.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"
#include "Settings/GitlabIntegrationSettings.h"
#include "GitlabAPI.h"

static const FName GitlabIntegrationTabName("Gitlab");

#define LOCTEXT_NAMESPACE "FGitlabIntegrationModule"
#define GITLAB_INTEGRATION_DEFAULT_SERVER TEXT("https://gitlab.com")
DEFINE_LOG_CATEGORY(LogGitlabIntegration);

void FGitlabIntegrationModule::StartupModule() {
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

    RegisterSettings();
    const UGitlabIntegrationSettings *Settings = GetDefault<UGitlabIntegrationSettings>();
    if (Settings->Server.IsEmpty()) {
        UE_LOG(LogGitlabIntegration, Warning, TEXT("Gitlab Server Empty using: %s (default)"),
               GITLAB_INTEGRATION_DEFAULT_SERVER);
    }

    Api = new GitlabAPI(Settings->Server, Settings->Token, Settings->Project,
                        std::bind(&FGitlabIntegrationModule::RefreshIssues, this));

    if (ProjectSelectionButtonText.IsValid()) {
        if (!Settings->Project.IsEmpty()) {
            ProjectSelectionButtonText->SetText(Settings->Project);
        } else {
            ProjectSelectionButtonText->SetText(LOCTEXT("GitlabIntegrationProjectSelection", "Select Project"));
        }
    }

    FGitlabIntegrationStyle::Initialize();
    FGitlabIntegrationStyle::ReloadTextures();

    FGitlabIntegrationCommands::Register();

    PluginCommands = MakeShareable(new FUICommandList);

    PluginCommands->MapAction(
            FGitlabIntegrationCommands::Get().OpenPluginWindow,
            FExecuteAction::CreateRaw(this, &FGitlabIntegrationModule::PluginButtonClicked),
            FCanExecuteAction());

    FLevelEditorModule &LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    {
        TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
        MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands,
                                       FMenuExtensionDelegate::CreateRaw(this,
                                                                         &FGitlabIntegrationModule::AddMenuExtension));

        LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
    }

    {
        TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
        ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands,
                                             FToolBarExtensionDelegate::CreateRaw(this,
                                                                                  &FGitlabIntegrationModule::AddToolbarExtension));

        LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
    }

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GitlabIntegrationTabName, FOnSpawnTab::CreateRaw(this,
                                                                                                       &FGitlabIntegrationModule::OnSpawnPluginTab))
            .SetDisplayName(LOCTEXT("FGitlabIntegrationTabTitle", "Gitlab"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FGitlabIntegrationModule::ShutdownModule() {
    // Submit started time tracking if there is any

    TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> Keys;
    TimeTrackingMap.GenerateKeyArray(Keys);
    for (auto &Issue : Keys) {
        FinishTimeTracking(Issue);
    }

    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    FGitlabIntegrationStyle::Shutdown();

    FGitlabIntegrationCommands::Unregister();
    UnregisterSettings();

    delete Api;
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GitlabIntegrationTabName);
}

TSharedRef <SDockTab> FGitlabIntegrationModule::OnSpawnPluginTab(const FSpawnTabArgs &SpawnTabArgs) {
    return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
    [
            SNew(SScrollBox)
                    .Orientation(EOrientation::Orient_Vertical)
                    .ConsumeMouseWheel(EConsumeMouseWheel::Always)
            + SScrollBox::Slot()
            [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                            .HAlign(HAlign_Fill)
                            .AutoHeight()
                    [
                            SNew(SExpandableArea)
                                    .AreaTitle(
                                            LOCTEXT("GitlabIntegrationSettings",
                                                    "Settings"))
                                    .InitiallyCollapsed(true)
                                    .Padding(8.0f)
                                    .BodyContent()
                            [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                            .Padding(0.0f,
                                                     3.0f,
                                                     6.0f,
                                                     3.0f)
                                            .FillWidth(0.25)
                                            .VAlign(VAlign_Center)
                                    [
                                            SNew(STextBlock)
                                                    .Text(LOCTEXT("GitlabIntegrationProject",
                                                                  "Project"))
                                    ]
                                    +
                                    SHorizontalBox::Slot()
                                    [
                                            SNew(SSpacer)
                                    ]
                                    +
                                    SHorizontalBox::Slot()
                                            .Padding(0.0f,
                                                     3.0f,
                                                     6.0f,
                                                     3.0f)
                                            .FillWidth(0.75)
                                            .VAlign(VAlign_Center)
                                    [
                                            CreateProjectSelectionButton()
                                    ]
                            ]

                    ]
                    + SVerticalBox::Slot()
                            .AutoHeight()
                    [
                            SNew(SExpandableArea)
                                    .AreaTitle(LOCTEXT("GitlabIntegrationIssues",
                                                       "Issues"))
                                    .InitiallyCollapsed(false)
                                    .Padding(8.0f)
                                    .BodyContent()
                            [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot()
                                            .AutoHeight()
                                    [
                                            SNew(SHorizontalBox)
                                            + SHorizontalBox::Slot()
                                                    .Padding(0.0f,
                                                             3.0f,
                                                             6.0f,
                                                             3.0f)

                                                    .VAlign(VAlign_Center)
                                                    .FillWidth(1.0)
                                            [SNew(SSearchBox).HintText(LOCTEXT("GISearchBoxHint",
                                                                               "Search issues"))
                                                            .OnTextChanged_Lambda(
                                                                    [this](const FText &NewText) {
                                                                        UE_LOG(LogGitlabIntegration, Log,
                                                                               TEXT("Searching for %s"),
                                                                               *NewText.ToString());
                                                                        IssueSearch = NewText.ToString();
                                                                        RefreshIssues();
                                                                    })]
                                            + SHorizontalBox::Slot()
                                                    .Padding(0.0f,
                                                             3.0f,
                                                             6.0f,
                                                             3.0f)
                                                    .AutoWidth()
                                                    .VAlign(VAlign_Center)
                                            [SNew(SBox)
                                                            .WidthOverride(
                                                                    30.0f)
                                                            .HeightOverride(
                                                                    30.0f)
                                                    [
                                                            SNew(SButton)
                                                                    .ButtonStyle(
                                                                            FEditorStyle::Get(),
                                                                            "NoBorder")
                                                                    .HAlign(HAlign_Fill)
                                                                    .VAlign(VAlign_Fill)
                                                                    .OnClicked_Lambda(
                                                                            [this]() -> FReply {
                                                                                Api->RefreshIssues();
                                                                                return FReply::Handled();
                                                                            })
                                                            [SNew(SBorder)
                                                                    // All brushes in Source/Editor/EditorStyle/Private/SlateEditorStyle.cpp
                                                                    .BorderImage(
                                                                            FEditorStyle::GetBrush(
                                                                                    "Icons.Refresh"))
                                                                    .Padding(
                                                                            FMargin(1.0f))
                                                                    .HAlign(HAlign_Fill)
                                                                    .VAlign(VAlign_Fill)]
                                                    ]
                                            ]
                                    ]
                                    + SVerticalBox::Slot()
                                            .AutoHeight()
                                    [
                                            SAssignNew(IssueListView,
                                                       SListView < TSharedPtr < FGitlabIntegrationIAPIIssue >> )
                                                    .ListItemsSource(&IssueList)
                                                    .SelectionMode(
                                                            ESelectionMode::Multi)
                                                    .OnGenerateRow_Lambda(
                                                            [this](TSharedPtr <FGitlabIntegrationIAPIIssue> IssueInfo,
                                                                   const TSharedRef <STableViewBase> &OwnerTable) -> TSharedRef <ITableRow> {
                                                                return GenerateIssueWidget(
                                                                        IssueInfo,
                                                                        OwnerTable);

                                                            })

                                    ]
                            ]
                    ]
            ]
    ];
}

TSharedRef <ITableRow> FGitlabIntegrationModule::GenerateIssueWidget(TSharedPtr <FGitlabIntegrationIAPIIssue> IssueInfo,
                                                                     const TSharedRef <STableViewBase> &OwnerTable) {
    FString separator = TEXT(", ");
    FString::Join(IssueInfo->labels, *separator);
    TSharedPtr<SBorder> TimeBorder= SNew(SBorder)
            // All brushes in Source/Editor/EditorStyle/Private/SlateEditorStyle.cpp
            .BorderImage(
                    FEditorStyle::GetBrush(
                            TimeTrackingMap.Contains(IssueInfo)
                            ? "PlayWorld.StopPlaySession" : "PlayWorld.PlayInViewport"))
            .Padding(
                    FMargin(1.0f))
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill);
//    TimeTrackingBorderMap
    return SNew(STableRow < TSharedPtr < FGitlabIntegrationIAPIIssue >> , OwnerTable)
    [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
                    .AutoHeight()
            [
                    SNew(SGridPanel)
                            .FillRow(0, 0.9f)
                            .FillColumn(1, 0.7f)
                    + SGridPanel::Slot(0, 0)
                            .Padding(0.0f, 4.0f, 5.0f, 4.0f)
                            .ColumnSpan(1)
                            .RowSpan(1)
                    [
                            SNew(SHyperlink)
                                    .Text(FText::FromString(TEXT("#") + FString::FromInt(IssueInfo->id)))
                                    .OnNavigate_Lambda([IssueInfo]() {
                                        FPlatformProcess::LaunchURL(*IssueInfo->web_url, nullptr, nullptr);
                                    })
                    ]
                    + SGridPanel::Slot(1, 0)
                            .Padding(0.0f, 4.0f, 4.0f, 4.0f)
                            .ColumnSpan(1)
                            .RowSpan(1)
                    [
                            SNew(STextBlock)
                                    .Text(FText::FromString(IssueInfo->title))
                    ]
                    + SGridPanel::Slot(0, 1)
                            .Padding(0.0f, 4.0f, 4.0f, 4.0f)
                            .ColumnSpan(1)
                            .RowSpan(1)
                    [
                            SNew(STextBlock)
                                    .Text(FText::FromString(IssueInfo->state))
                                    .ColorAndOpacity(IssueInfo->state.Equals(TEXT("opened"), ESearchCase::IgnoreCase)
                                                     ? FLinearColor(FColor(0xff57a64a)) : FLinearColor(
                                                    FColor(0xffcfcfcf)))
                    ]
                    + SGridPanel::Slot(1, 1)
                            .Padding(0.0f, 4.0f, 4.0f,
                                     4.0f)
                            .ColumnSpan(1)
                            .RowSpan(
                                    1)
                    [
                            SNew(STextBlock)
                                    .Text(FText::FromString(
                                            FString::Join(
                                                    IssueInfo->labels,
                                                    *separator)))
                    ]
                    + SGridPanel::Slot(2, 0)
                            .Padding(
                                    0.0f,
                                    4.0f,
                                    4.0f,
                                    4.0f)
                            .ColumnSpan(
                                    1)
                            .RowSpan(
                                    2)
                    [
                            SNew(SBox)
                                    .WidthOverride(
                                            40.0f)
                                    .HeightOverride(
                                            40.0f)
                            [
                                    SNew(SButton)
                                            .ButtonStyle(FEditorStyle::Get(), "NoBorder")
                                            .HAlign(HAlign_Fill)
                                            .VAlign(VAlign_Fill)
                                            .OnClicked_Lambda([this, IssueInfo, OwnerTable]() -> FReply {
                                                if (TimeTrackingMap.Contains(IssueInfo)) {
                                                    FinishTimeTracking(IssueInfo);
                                                } else {
                                                    TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> Keys;
                                                    TimeTrackingMap.GenerateKeyArray(Keys);
                                                    for (auto &Issue : Keys) {
                                                        FinishTimeTracking(Issue);
                                                    }
                                                    TimeTrackingMap.Add(IssueInfo, FDateTime::UtcNow());
                                                }
                                                OwnerTable->RebuildList();
                                                return FReply::Handled();
                                            })
                                    [TimeBorder.ToSharedRef()]
                            ]
                    ]
            ]
            + SVerticalBox::Slot()
                    .AutoHeight()
            [SNew(SBorder)
                            .BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
                            .Padding(FMargin(1.0f))
                            .HAlign(HAlign_Fill)]
    ];
}

void FGitlabIntegrationModule::FinishTimeTracking(TSharedPtr<FGitlabIntegrationIAPIIssue> issue) {
    FTimespan TimeSpent = FDateTime::UtcNow()-TimeTrackingMap[issue];
    Api->RecordTimeSpent(issue, FMath::RoundToInt(TimeSpent.GetTotalSeconds()));
    TimeTrackingMap.Remove(issue);
}

TSharedRef<SWidget> FGitlabIntegrationModule::CreateProjectSelectionButton() {
    return SNew(SComboButton)
                           .OnGetMenuContent_Lambda([this]() { return GenerateProjectList(); })
                           .ButtonContent()
                   [
                           CreateProjectSelectionButtonText()
                   ];
}

TSharedRef<STextBlock> FGitlabIntegrationModule::CreateProjectSelectionButtonText() {
    const UGitlabIntegrationSettings *Settings = GetDefault<UGitlabIntegrationSettings>();
    ProjectSelectionButtonText = SNew(STextBlock)
            .Text((Settings->Project.IsEmpty() ? LOCTEXT("GitlabIntegrationProjectSelection", "Select Project")
                                               : FText::FromString(*Settings->Project)));
    return ProjectSelectionButtonText.ToSharedRef();
}

TSharedRef<SWidget> FGitlabIntegrationModule::GenerateProjectList() {
    FMenuBuilder MenuBuilder(true, nullptr);
    for (auto &Project : Api->GetProjects()) {
        MenuBuilder.AddMenuEntry(
                FText::FromString(Project.name_with_namespace),
                FText::GetEmpty(),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([=] {
                              HandleProjectSelection(Project);
                          })
                )
        );
    }
    return MenuBuilder.MakeWidget();
}

void FGitlabIntegrationModule::HandleProjectSelection(FGitlabIntegrationIAPIProject project) {
    UGitlabIntegrationSettings *Settings = GetMutableDefault<UGitlabIntegrationSettings>();
    if (Settings != nullptr) {
        if (Settings->Project != project.name_with_namespace) {
            UE_LOG(LogGitlabIntegration, Log, TEXT("Selected project %s"), *project.name_with_namespace);
            Settings->Project = project.name_with_namespace;
            Settings->SaveConfig();
            Api->SetProject(project);
            if (project.id != -1) {
                ProjectSelectionButtonText->SetText(Settings->Project);
            } else {
                ProjectSelectionButtonText->SetText(LOCTEXT("GitlabIntegrationProjectSelection", "Select Project"));
            }
            IssueList.Empty();

            if (IssueListView.IsValid()) {
                IssueListView->RequestListRefresh();
            }
        }
    }
}

void FGitlabIntegrationModule::PluginButtonClicked() {
    FGlobalTabmanager::Get()->InvokeTab(GitlabIntegrationTabName);
}

void FGitlabIntegrationModule::AddMenuExtension(FMenuBuilder &Builder) {
    Builder.AddMenuEntry(FGitlabIntegrationCommands::Get().OpenPluginWindow);
}

void FGitlabIntegrationModule::AddToolbarExtension(FToolBarBuilder &Builder) {
    Builder.AddToolBarButton(FGitlabIntegrationCommands::Get().OpenPluginWindow);
}

void FGitlabIntegrationModule::RegisterSettings() {
    // Registering some settings is just a matter of exposing the default UObject of
    // your desired class, feel free to add here all those settings you want to expose
    // to your LDs or artists.
#if WITH_EDITOR
    if (ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings")) {
        ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins",
                                                                               "GitlabIntegration",
                                                                               LOCTEXT("GitlabIntegrationSettingsName",
                                                                                       "Gitlab Integration"),
                                                                               LOCTEXT("GitlabIntegrationSettingsDescription",
                                                                                       "Configure the Gitlab Integration plug-in."),
                                                                               GetMutableDefault<UGitlabIntegrationSettings>()
        );

        if (SettingsSection.IsValid()) {
            SettingsSection->OnModified().BindRaw(this, &FGitlabIntegrationModule::HandleSettingsSaved);
        }

    }
#endif
}

void FGitlabIntegrationModule::UnregisterSettings() {
    // Ensure to unregister all of your registered settings here, hot-reload would
    // otherwise yield unexpected results.
#if WITH_EDITOR
    ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

    if (SettingsModule != nullptr) {
        SettingsModule->UnregisterSettings("Project", "Plugins", "GitlabIntegration");
    }
#endif

}

bool FGitlabIntegrationModule::HandleSettingsSaved() {
    UGitlabIntegrationSettings *Settings = GetMutableDefault<UGitlabIntegrationSettings>();
    bool ResaveSettings = true;

    // You can put any validation code in here and resave the settings in case an invalid
    // value has been entered

    if (ResaveSettings) {
        Api->SetBaseUrl(Settings->Server);
        Api->SetToken(Settings->Token);
        Api->GetProjectsRequest(1);
        Settings->SaveConfig();
    }

    return true;
}

void FGitlabIntegrationModule::RefreshIssues() {
    UE_LOG(LogGitlabIntegration, Verbose, TEXT("Issue callback triggered"));

    for (auto &Issue : Api->GetIssues()) {
        if (Issue->title.Contains(IssueSearch, ESearchCase::IgnoreCase, ESearchDir::FromStart) ||
            IssueSearch.Contains(FString::Printf(TEXT("#%d"), Issue->id), ESearchCase::IgnoreCase,
                                 ESearchDir::FromStart) || IssueSearch.TrimStart().IsEmpty()) {
            if (IssueList.Contains(Issue)) continue;
            IssueList.Add(Issue);
        } else {
            IssueList.Remove(Issue);
        }
    }

    if (IssueListView.IsValid()) {
        IssueListView->RequestListRefresh();
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGitlabIntegrationModule, GitlabIntegration)