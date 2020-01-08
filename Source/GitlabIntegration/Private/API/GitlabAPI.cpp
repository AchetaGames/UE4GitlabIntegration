// Fill out your copyright notice in the Description page of Project Settings.


#include "GitlabAPI.h"

DEFINE_LOG_CATEGORY(LogGitlabIntegrationAPI);

GitlabAPI::GitlabAPI(): IAPI() {
    UE_LOG(LogGitlabIntegrationAPI, Log, TEXT("Creating Gitlab API"));
}

GitlabAPI::~GitlabAPI() {
}

void GitlabAPI::SetBaseUrl(FText server) {
    Projects.Empty();
    ApiBaseUrl = FText::FromString(server.ToString() + TEXT("/api/v4/"));
    UE_LOG(LogGitlabIntegrationAPI, Warning, TEXT("Changing Gitlab API BaseURL to: %s"), *ApiBaseUrl.ToString());
}

GitlabAPI::GitlabAPI(FText base, FText token, FText LoadProject, std::function<void()> IssueCallback, std::function<void()> LabelCallback): IAPI() {
    UE_LOG(LogGitlabIntegrationAPI, Log, TEXT("Creating Gitlab API"));
    SetBaseUrl(base);
    ApiToken = token;
    InitialProjectName = LoadProject;
    SetIssueCallback(IssueCallback);
    SetLabelCallback(LabelCallback);
    GetProjectsRequest(1);
}