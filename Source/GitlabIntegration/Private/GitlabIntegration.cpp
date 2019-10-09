// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GitlabIntegration.h"
#include <functional>
#include <Editor/UnrealEd/Public/UnrealEd.h>
#include "GitlabIntegrationStyle.h"
#include "GitlabIntegrationCommands.h"
#include "LevelEditor.h"
#include "Math/Color.h"
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
    IssueSortNewFirst = Settings->SortIssuesNewestFirst;

    Api = new GitlabAPI(Settings->Server, Settings->Token, Settings->Project,
                        std::bind(&FGitlabIntegrationModule::RefreshIssues, this),
                        std::bind(&FGitlabIntegrationModule::RefreshLabels, this));

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

TSharedRef<SDockTab> FGitlabIntegrationModule::OnSpawnPluginTab(const FSpawnTabArgs &SpawnTabArgs) {
    UGitlabIntegrationSettings *Settings = GetMutableDefault<UGitlabIntegrationSettings>();
    if (Settings != nullptr) {
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
                                                       SNew(SVerticalBox)
                                                       + SVerticalBox::Slot()
                                                           .HAlign(HAlign_Fill)
                                                           .AutoHeight()
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
                                                       + SVerticalBox::Slot()
                                                           .HAlign(HAlign_Fill)
                                                           .AutoHeight()
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
                                                                       .Text(LOCTEXT("GitlabIntegrationIssueSortNewest",
                                                                                     "Newest Issue First"))
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
                                                                       SNew(SCheckBox)
                                                                           .IsChecked(Settings->SortIssuesNewestFirst
                                                                                      ? ECheckBoxState::Checked
                                                                                      : ECheckBoxState::Unchecked)
                                                                           .OnCheckStateChanged_Lambda(
                                                                               [this, Settings](
                                                                                   ECheckBoxState CheckBoxState) {
                                                                                   Settings->SortIssuesNewestFirst =
                                                                                       CheckBoxState ==
                                                                                       ECheckBoxState::Checked;
                                                                                   IssueSortNewFirst = (CheckBoxState ==
                                                                                                        ECheckBoxState::Checked);
                                                                                   RefreshIssues();
                                                                               })

                                                               ]
                                                       ]
                                               ]

                                       ]
                                       + SVerticalBox::Slot()
                                           .AutoHeight()
                                       [
                                               SNew(SExpandableArea)
                                                   .AreaTitle(
                                                       LOCTEXT("GitlabIntegrationIssues",
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
                                                               +
                                                               SHorizontalBox::Slot()
                                                                   .Padding(
                                                                       0.0f,
                                                                       3.0f,
                                                                       6.0f,
                                                                       3.0f)

                                                                   .VAlign(VAlign_Center)
                                                                   .FillWidth(
                                                                       1.0)
                                                               [SNew(SSearchBox).HintText(
                                                                       LOCTEXT("GISearchBoxHint",
                                                                               "Search issues"))
                                                                   .OnTextChanged_Lambda(
                                                                       [this](const FText &NewText) {
                                                                           UE_LOG(LogGitlabIntegration,
                                                                                  Log,
                                                                                  TEXT("Searching for %s"),
                                                                                  *NewText.ToString());
                                                                           IssueSearch = NewText.ToString();
                                                                           RefreshIssues();
                                                                       })]
                                                               +
                                                               SHorizontalBox::Slot()
                                                                   .Padding(
                                                                       0.0f,
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
                                                           .VAlign(VAlign_Center)
                                                       [
                                                               SAssignNew(LabelWrapBox, SWrapBox).UseAllottedWidth(true)
                                                       ]
                                                       + SVerticalBox::Slot()
                                                           .AutoHeight()
                                                       [
                                                               SAssignNew(
                                                                   IssueListView,
                                                                   SListView<TSharedPtr<FGitlabIntegrationIAPIIssue >>)
                                                                   .ListItemsSource(
                                                                       &IssueList)
                                                                   .SelectionMode(
                                                                       ESelectionMode::SingleToggle)
                                                                   .OnGenerateRow_Lambda(
                                                                       [this](
                                                                           TSharedPtr<FGitlabIntegrationIAPIIssue> IssueInfo,
                                                                           const TSharedRef<STableViewBase> &OwnerTable) -> TSharedRef<ITableRow> {
                                                                           RefreshLabels(); //FIXME: Dirty hack to load labels on start should be in the above slot
                                                                           return GenerateIssueWidget(
                                                                               IssueInfo,
                                                                               OwnerTable);

                                                                       })

                                                       ]
                                               ]
                                       ]
                               ]
                   ];
    } else {
        return SNew(SDockTab)
                   .TabRole(ETabRole::NomadTab)
                   [SNew(STextBlock).Text(LOCTEXT("GISettingsFailed",
                                                  "Failed To Load Settings"))
                   ];
    }
}

TSharedRef<ITableRow> FGitlabIntegrationModule::GenerateIssueWidget(TSharedPtr<FGitlabIntegrationIAPIIssue> IssueInfo,
                                                                    const TSharedRef<STableViewBase> &OwnerTable) {
    FString separator = TEXT(", ");

    TSharedRef<SWrapBox> IssueLabels = SNew(SWrapBox).UseAllottedWidth(true);
    for (auto &label: IssueInfo->labels) {
        IssueLabels->AddSlot()[
            GenerateLabelWidget(Api->GetLabel(label), true)
        ];
    }
    TSharedPtr<SBorder> TimeBorder = SNew(SBorder)
                                         // All brushes in Source/Editor/EditorStyle/Private/SlateEditorStyle.cpp
                                             .BorderImage(
                                                 FEditorStyle::GetBrush(
                                                     TimeTrackingMap.Contains(IssueInfo)
                                                     ? "PlayWorld.StopPlaySession"
                                                     : "PlayWorld.PlayInViewport"))
                                             .Padding(
                                                 FMargin(1.0f))
                                             .HAlign(HAlign_Fill)
                                             .VAlign(VAlign_Fill);
//    TimeTrackingBorderMap
    return SNew(STableRow<TSharedPtr<FGitlabIntegrationIAPIIssue >>, OwnerTable)
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
                                               .Text(FText::FromString(TEXT("#") +
                                                                       FString::FromInt(
                                                                           IssueInfo->iid)))
                                               .OnNavigate_Lambda([IssueInfo]() {
                                                   FPlatformProcess::LaunchURL(
                                                       *IssueInfo->web_url,
                                                       nullptr, nullptr);
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
                                               .Text(FText::FromString(
                                                   IssueInfo->state))
                                               .ColorAndOpacity(
                                                   IssueInfo->state.Equals(
                                                       TEXT("opened"),
                                                       ESearchCase::IgnoreCase)
                                                   ? FLinearColor(
                                                       FColor(0xff57a64a))
                                                   : FLinearColor(
                                                       FColor(0xffcfcfcf)))
                                   ]
                                   + SGridPanel::Slot(1, 1)
                                       .Padding(0.0f, 4.0f, 4.0f,
                                                4.0f)
                                       .ColumnSpan(1)
                                       .RowSpan(
                                           1)
                                   [
                                       IssueLabels
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
                                                       .ButtonStyle(
                                                           FEditorStyle::Get(),
                                                           "NoBorder")
                                                       .HAlign(HAlign_Fill)
                                                       .VAlign(VAlign_Fill)
                                                       .OnClicked_Lambda(
                                                           [this, IssueInfo, OwnerTable]() -> FReply {
                                                               if (TimeTrackingMap.Contains(
                                                                   IssueInfo)) {
                                                                   FinishTimeTracking(
                                                                       IssueInfo);
                                                               } else {
                                                                   TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> Keys;
                                                                   TimeTrackingMap.GenerateKeyArray(
                                                                       Keys);
                                                                   for (auto &Issue : Keys) {
                                                                       FinishTimeTracking(
                                                                           Issue);
                                                                   }
                                                                   TimeTrackingMap.Add(
                                                                       IssueInfo,
                                                                       FDateTime::UtcNow());
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

TSharedRef<SHorizontalBox>
FGitlabIntegrationModule::GenerateLabelWidget(TSharedPtr<FGitlabIntegrationIAPILabel> LabelInfo, bool OnIssue) {
    return SNew(SHorizontalBox)
               + SHorizontalBox::Slot()
                   .AutoWidth()
               [
                       SNew(SBorder)
                           .BorderImage(FEditorStyle::GetBrush("None"))
                           .Padding(1)
                       [
                               SNew(SButton)
                                   .Text(FText::FromString(LabelInfo->name))
                                   .ForegroundColor_Lambda([this, LabelInfo, OnIssue]() -> FLinearColor {
                                       return SelectedLabels.Contains(LabelInfo->name) || OnIssue ? FColor::FromHex(
                                           LabelInfo->text_color) : FLinearColor::Black;
                                   })
                                   .ToolTipText(FText::FromString(LabelInfo->description))
                                   .ButtonColorAndOpacity_Lambda([this, LabelInfo, OnIssue]() -> FLinearColor {
                                       return SelectedLabels.Contains(LabelInfo->name) || OnIssue ? FColor::FromHex(
                                           LabelInfo->color) : FLinearColor::Gray;
                                   })
                                   .OnClicked_Lambda([this, LabelInfo]() -> FReply {
                                       if (SelectedLabels.Contains(LabelInfo->name))
                                           SelectedLabels.Remove(LabelInfo->name);
                                       else {
                                           SelectedLabels.Add(LabelInfo->name);
                                       }
                                       RefreshIssues();
                                       return FReply::Handled();
                                   })
                       ]
               ];
}

void FGitlabIntegrationModule::FinishTimeTracking(TSharedPtr<FGitlabIntegrationIAPIIssue> issue) {
    FTimespan TimeSpent = FDateTime::UtcNow() - TimeTrackingMap[issue];
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
                                                                               LOCTEXT(
                                                                                   "GitlabIntegrationSettingsDescription",
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

    Api->SetBaseUrl(Settings->Server);
    Api->SetToken(Settings->Token);
    Api->GetProjectsRequest(1);
    IssueSortNewFirst = Settings->SortIssuesNewestFirst;
    Settings->SaveConfig();
    RefreshIssues();

    return true;
}

void FGitlabIntegrationModule::RefreshIssues() {
    UE_LOG(LogGitlabIntegration, Verbose, TEXT("Issue Refresh triggered"));

    for (auto &Issue : Api->GetIssues()) {
        bool show = SelectedLabels.Num() <= 0;
        for (auto &Label: SelectedLabels) {
            if (Issue->labels.Contains(Label)) {
                show = true;
            }
        }
        if ((Issue->title.Contains(IssueSearch, ESearchCase::IgnoreCase, ESearchDir::FromStart) ||
             IssueSearch.Contains(FString::Printf(TEXT("#%d"), Issue->id), ESearchCase::IgnoreCase,
                                  ESearchDir::FromStart) || IssueSearch.TrimStart().IsEmpty()) && show) {
            if (IssueList.Contains(Issue)) continue;
            IssueList.Add(Issue);
        } else {
            IssueList.Remove(Issue);
        }
    }

    IssueList.Sort(
        [this](const TSharedPtr<FGitlabIntegrationIAPIIssue> &A, const TSharedPtr<FGitlabIntegrationIAPIIssue> &B) {
            return IssueSortNewFirst ? A->iid > B->iid : A->iid < B->iid;
        });

    if (IssueListView.IsValid()) {
        IssueListView->RequestListRefresh();
    }
}

void FGitlabIntegrationModule::RefreshLabels() {
    UE_LOG(LogGitlabIntegration, Verbose, TEXT("Label callback triggered"));

    LabelList.Empty();
    for (auto &Label: Api->GetLabels()) {
        LabelList.Add(Label);
    }

    if (LabelWrapBox.IsValid()) {
        LabelWrapBox->ClearChildren();
        for (auto &Label: LabelList) {

            LabelWrapBox->AddSlot()
            [
                GenerateLabelWidget(Label, false)
            ];
        }
    }

}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGitlabIntegrationModule, GitlabIntegration)