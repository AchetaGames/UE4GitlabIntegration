// Fill out your copyright notice in the Description page of Project Settings.


#include "GitlabAPI.h"

DEFINE_LOG_CATEGORY(LogGitlabIntegrationAPI);

GitlabAPI::GitlabAPI(): IAPI() {
    UE_LOG(LogGitlabIntegrationAPI, Log, TEXT("Creating Gitlab API"));
}

GitlabAPI::~GitlabAPI() {
}

void GitlabAPI::SetBaseUrl(FString server) {
    Projects.Empty();
    ApiBaseUrl = server + TEXT("/api/v4/");
    UE_LOG(LogGitlabIntegrationAPI, Warning, TEXT("Changing Gitlab API BaseURL to: %s"), *ApiBaseUrl);
}

GitlabAPI::GitlabAPI(FString base, FString token, FString LoadProject, std::function<void()> callback): IAPI() {
    UE_LOG(LogGitlabIntegrationAPI, Log, TEXT("Creating Gitlab API"));
    SetBaseUrl(base);
    ApiToken = token;
    InitialProjectName = LoadProject;
    GitlabIntegrationIssueCallback = callback;
    GetProjectsRequest(1);
}