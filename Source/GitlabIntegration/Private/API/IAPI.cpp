// Fill out your copyright notice in the Description page of Project Settings.


#include "../../Public/API/IAPI.h"

DEFINE_LOG_CATEGORY(LogGitlabIntegrationIAPI);

IAPI::IAPI() {
    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Creating Generic API"));
    Http = &FHttpModule::Get();
}

IAPI::~IAPI() {
}

void IAPI::SetBaseUrl(FText server) {
    Projects.Empty();
    ApiBaseUrl = server;
    UE_LOG(LogGitlabIntegrationIAPI, Warning, TEXT("Changing Generic API BaseURL to: %s"), *ApiBaseUrl.ToString());
}

void IAPI::SetToken(FText token) {
    ApiToken = token;
}

void IAPI::SetLoadProject(FText project) {
    InitialProjectName = project;
}

void IAPI::SetRequestHeaders(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> &Request) {
    Request->SetHeader(TEXT("User-Agent"), TEXT("X-UnrealEngine-Agent"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accepts"), TEXT("application/json"));
    if (!ApiToken.IsEmpty()) {
        Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiToken.ToString());
    }
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> IAPI::RequestWithRoute(FString Subroute) {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    UE_LOG(LogGitlabIntegrationIAPI, Warning, TEXT("Sending request to: %s/%s"), *ApiBaseUrl.ToString(), *Subroute);
    Request->SetURL(ApiBaseUrl.ToString() + TEXT("/") + Subroute);
    SetRequestHeaders(Request);
    return Request;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> IAPI::GetRequest(FString Subroute, int32 page) {
    if (page > 1) {
        FString separator = TEXT("?");
        if (Subroute.Contains(TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromEnd)) { separator = TEXT("&"); }
        Subroute += separator + TEXT("page=") + FString::FromInt(page);
    }
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = RequestWithRoute(Subroute);
    Request->SetVerb("GET");
    return Request;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> IAPI::PostRequest(FString Subroute, FString ContentJsonString) {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = RequestWithRoute(Subroute);
    Request->SetVerb("POST");
    Request->SetContentAsString(ContentJsonString);
    return Request;
}

void IAPI::Send(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> &Request) {
    Request->ProcessRequest();
}

bool IAPI::ResponseIsValid(FHttpResponsePtr Response, bool bWasSuccessful) {
    if (!bWasSuccessful || !Response.IsValid()) return false;
    if (EHttpResponseCodes::IsOk(Response->GetResponseCode())) return true;
    else {
        UE_LOG(LogGitlabIntegrationIAPI, Error, TEXT("Http Response returned error code: %d"),
               Response->GetResponseCode());
        return false;
    }
}

template<typename StructType>
void IAPI::GetJsonStringFromStruct(StructType FilledStruct, FString &StringOutput) {
    FJsonObjectConverter::UStructToJsonObjectString(StructType::StaticStruct(), &FilledStruct, StringOutput, 0, 0);
}

template<typename StructType>
void IAPI::GetStructFromJsonString(FHttpResponsePtr Response, StructType &StructOutput) {
    StructType StructData;
    FString JsonString = Response->GetContentAsString();
    FJsonObjectConverter::JsonObjectStringToUStruct<StructType>(JsonString, &StructOutput, 0, 1);
}

void IAPI::GetProjectsRequest(int32 page) {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = GetRequest("projects", page);
    Request->OnProcessRequestComplete().BindRaw(this, &IAPI::ProjectsResponse);
    Send(Request);
}

void IAPI::ProjectsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
    if (!ResponseIsValid(Response, bWasSuccessful)) return;

    TArray<FGitlabIntegrationIAPIProject> LocalProjects;
    FString JsonString = Response->GetContentAsString();
    FJsonObjectConverter::JsonArrayStringToUStruct(JsonString, &LocalProjects, 0, 0);
    for (auto &Project : LocalProjects) {
        if (!Projects.Contains(Project.id)) {
            Projects.Emplace(Project.id, Project);
            if (SelectedProject.id == -1) {
                if (Project.name_with_namespace == InitialProjectName.ToString()) {
                    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Found selected project"))
                    SetProject(Project);
                }
            }
        }
    }

    int current_page = FCString::Atoi(*Response->GetHeader(TEXT("X-Page")));
    int next_page = FCString::Atoi(*Response->GetHeader(TEXT("X-Next-Page")));

    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Current page: %d"), current_page)
    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Next page: %d"), next_page)

    if (next_page > current_page) {
        UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Trying to get next page of projects"));
        GetProjectsRequest(next_page);
    } else {

        UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Got list of projects"));
        for (auto &Project : Projects) {
            UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT(" %s"), *(Project.Value).name);
        }
    }
}

TArray<FGitlabIntegrationIAPIProject> IAPI::GetProjects() {
    TArray<FGitlabIntegrationIAPIProject> result;
    Projects.GenerateValueArray(result);
    result.Sort([](const FGitlabIntegrationIAPIProject &One, const FGitlabIntegrationIAPIProject &Two) {
        return One.name_with_namespace.Compare(Two.name_with_namespace, ESearchCase::IgnoreCase) < 0;
    });

    return result;
}

void IAPI::SetProject(FGitlabIntegrationIAPIProject project) {
    SelectedProject = project;
    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Project Last Activity: %s"), *project.last_activity_at.ToHttpDate());
    Issues.Empty();
    Labels.Empty();
    StringLabels.Empty();
    GetProjectIssuesRequest(project.id, 1);
    GetProjectLabels(project.id, 1);
}

void IAPI::GetProjectLabels(int project_id, int32 page) {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = GetRequest("projects/" + FString::FromInt(project_id) + "/labels", page);
    Request->OnProcessRequestComplete().BindRaw(this, &IAPI::ProjectLabelsResponse);
    Send(Request);
}

void IAPI::GetProjectIssuesRequest(int project_id, int32 page) {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = GetRequest("projects/" + FString::FromInt(project_id) + "/issues?state=opened", page);
    Request->OnProcessRequestComplete().BindRaw(this, &IAPI::ProjectIssuesResponse);
    Send(Request);
}

FGitlabIntegrationIAPIProject IAPI::GetProject() {
    return SelectedProject;
}

void IAPI::ProjectIssuesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
    if (!ResponseIsValid(Response, bWasSuccessful)) return;

    TArray<FGitlabIntegrationIAPIIssue> LocalIssues;
    FString JsonString = Response->GetContentAsString();
    FJsonObjectConverter::JsonArrayStringToUStruct(JsonString, &LocalIssues, 0, 0);

    for (auto &Issue : LocalIssues) {
        if (!Issues.Contains(Issue.id)) {
            TSharedPtr<FGitlabIntegrationIAPIIssue> TempIssue = MakeShareable(new FGitlabIntegrationIAPIIssue(Issue));
            Issues.Emplace(Issue.id, TempIssue);
        }
    }

    int current_page = FCString::Atoi(*Response->GetHeader(TEXT("X-Page")));
    int next_page = FCString::Atoi(*Response->GetHeader(TEXT("X-Next-Page")));

    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Current page: %d"), current_page)
    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Next page: %d"), next_page)

    if(IssueCallback) {
        IssueCallback();
    }

    if (next_page > current_page) {
        UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Trying to get next page of issues"));
        GetProjectIssuesRequest(SelectedProject.id, next_page);
    } else {
        UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Got list of issues"));
        for (auto &Issue : Issues) {
            UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT(" %s"), *(Issue.Value)->title);
        }
    }
}

void IAPI::ProjectLabelsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
    if (!ResponseIsValid(Response, bWasSuccessful)) return;

    TArray<FGitlabIntegrationIAPILabel> LocalLabels;
    FString JsonString = Response->GetContentAsString();
    FJsonObjectConverter::JsonArrayStringToUStruct(JsonString, &LocalLabels, 0, 0);

    for (auto &Label : LocalLabels) {
        if (!Labels.Contains(Label.id)) {

            TSharedPtr<FGitlabIntegrationIAPILabel> TempLabel= MakeShareable(new FGitlabIntegrationIAPILabel(Label));
            Labels.Emplace(Label.id, TempLabel);
            StringLabels.Emplace(Label.name, TempLabel);
        }
    }

    int current_page = FCString::Atoi(*Response->GetHeader(TEXT("X-Page")));
    int next_page = FCString::Atoi(*Response->GetHeader(TEXT("X-Next-Page")));

    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Current page: %d"), current_page)
    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Next page: %d"), next_page)

    if(LabelCallback) {
        LabelCallback();
    }

    if (next_page > current_page) {
        UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Trying to get next page of labels"));
        GetProjectLabels(SelectedProject.id, next_page);
    } else {
        UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Got list of labels"));
        for (auto &Label : Labels) {
            UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT(" %s"), *(Label.Value)->name);
        }
    }
}

void IAPI::SetIssueCallback(std::function<void()> callback) {
    IssueCallback = callback;
}

void IAPI::SetLabelCallback(std::function<void()> callback) {
    LabelCallback = callback;
}

IAPI::IAPI(FText base, FText token, FText LoadProject, std::function<void()> IssueCallback, std::function<void()> LabelCallback) {
    UE_LOG(LogGitlabIntegrationIAPI, Log, TEXT("Creating Gitlab API"));
    Http = &FHttpModule::Get();
    SetBaseUrl(base);
    ApiToken = token;
    InitialProjectName = LoadProject;
    SetIssueCallback(IssueCallback);
    GetProjectsRequest(1);
}

TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> IAPI::GetIssues() {
    TArray<TSharedPtr<FGitlabIntegrationIAPIIssue>> result;
    Issues.GenerateValueArray(result);
    return result;
}

TArray<TSharedPtr<FGitlabIntegrationIAPILabel>> IAPI::GetLabels() {
    TArray<TSharedPtr<FGitlabIntegrationIAPILabel>> result;
    Labels.GenerateValueArray(result);
    return result;
}

void IAPI::RefreshIssues() {
    GetProjectIssuesRequest(SelectedProject.id,1);
}

void IAPI::RecordTimeSpent(TSharedPtr <FGitlabIntegrationIAPIIssue> issue, int time) {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = PostRequest(
        FString::Printf(TEXT("projects/%d/issues/%d/add_spent_time?duration=%ds"), issue->project_id, issue->iid, time),
        TEXT(""));
    Request->OnProcessRequestComplete().BindRaw(this, &IAPI::ProjectsResponse);
    Send(Request);
}

TSharedPtr <FGitlabIntegrationIAPILabel> IAPI::GetLabel(FString &name) {
    if (StringLabels.Contains(name)) {
        return *(StringLabels.Find(name));
    } else {
        return MakeShareable(new FGitlabIntegrationIAPILabel());
    }
}

void IAPI::TimeSpentResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
    if (!ResponseIsValid(Response, bWasSuccessful)) return;
//    FGitlabIssueTime TimeResponse;
//    GetStructFromJsonString<FGitlabIssueTime>(Response, TimeResponse);
}
