// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Json.h"
#include "JsonUtilities.h"
#include <functional>
#include "IAPI.generated.h"


/**
 * 
 */

USTRUCT()
struct FGitlabIntegrationIAPIProject {
    GENERATED_BODY()
    UPROPERTY() int id;
    UPROPERTY() FString name;
    UPROPERTY() FString name_with_namespace;
    UPROPERTY() FDateTime last_activity_at;

    FGitlabIntegrationIAPIProject() {
        id=-1;
        last_activity_at=FDateTime::FromUnixTimestamp(0);
    }
};

USTRUCT()
struct FGitlabIntegrationIAPIIssue {
    GENERATED_BODY()
    UPROPERTY() int id;
    UPROPERTY() FString title;
    UPROPERTY() FString state;
    UPROPERTY() FString web_url;
    UPROPERTY() int project_id;
    UPROPERTY() int iid;
    UPROPERTY() TArray<FString> labels;

    FGitlabIntegrationIAPIIssue() {
        id=-1;
        project_id=-1;
        iid=-1;
    }
    FGitlabIntegrationIAPIIssue(FGitlabIntegrationIAPIIssue &old) {
        id=old.id;
        title=old.title;
        state=old.state;
        labels=old.labels;
        project_id=old.project_id;
        iid=old.iid;
        web_url=old.web_url;
    }
};

USTRUCT()
struct FGitlabIntegrationIAPILabel {
    GENERATED_BODY()
    UPROPERTY() int id;
    UPROPERTY() FString name;
    UPROPERTY() FString color;
    UPROPERTY() FString text_color;
    UPROPERTY() FString description;

    FGitlabIntegrationIAPILabel() {
        id=-1;
    }
    FGitlabIntegrationIAPILabel(FGitlabIntegrationIAPILabel &old) {
        id=old.id;
        name=old.name;
        color=old.color;
        text_color=old.text_color;
        description=old.description;
    }
};

DECLARE_LOG_CATEGORY_EXTERN(LogGitlabIntegrationIAPI, Log, All);

class GITLABINTEGRATION_API IAPI {
public:
    IAPI();
	virtual ~IAPI();

    IAPI(FString base, FString token, FString LoadProject, std::function<void()> callback);
	virtual void SetBaseUrl(FString base);
    void SetToken(FString token);
    void SetLoadProject(FString project);
    void SetIssueCallback(std::function<void()> callback);
    void SetProject(FGitlabIntegrationIAPIProject project);
    FGitlabIntegrationIAPIProject GetProject();


    TSharedRef<IHttpRequest> RequestWithRoute(FString Subroute);
    TSharedRef<IHttpRequest> GetRequest(FString Subroute, int32 page);
    TSharedRef<IHttpRequest> PostRequest(FString Subroute, FString ContentJsonString);
    void Send(TSharedRef<IHttpRequest>& Request);
    bool ResponseIsValid(FHttpResponsePtr Response, bool bWasSuccessful);
    template <typename StructType>
    void GetJsonStringFromStruct(StructType FilledStruct, FString& StringOutput);
    template <typename StructType>
    void GetStructFromJsonString(FHttpResponsePtr Response, StructType& StructOutput);
    FString ApiBaseUrl = TEXT("");
    FString ApiToken = TEXT("");
    FString InitialProjectName = TEXT("");
    std::function<void()> GitlabIntegrationIssueCallback;

    TMap<int32, FGitlabIntegrationIAPIProject> Projects;
    FGitlabIntegrationIAPIProject SelectedProject;

    TMap<int32, TSharedPtr<FGitlabIntegrationIAPIIssue>> Issues;
    TMap<int32, TSharedPtr<FGitlabIntegrationIAPILabel>> Labels;

    void SetRequestHeaders(TSharedRef<IHttpRequest>& Request);

private:
    FHttpModule* Http;


public:
    // Actual API calls
        //Projects
    void GetProjectsRequest(int32 page);
    void ProjectsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
        //Issues
    void GetProjectIssuesRequest(int project_id, int32 page);
    void GetProjectLabels(int project_id, int32 page);
    void ProjectLabelsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void RefreshIssues();
    void ProjectIssuesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void RecordTimeSpent(TSharedPtr <FGitlabIntegrationIAPIIssue> issue, int time);
    void TimeSpentResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    TArray<FGitlabIntegrationIAPIProject> GetProjects();
    TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> GetIssues();
};
