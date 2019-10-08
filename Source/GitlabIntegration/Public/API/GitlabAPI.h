// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "Json.h"
#include "JsonUtilities.h"
#include <functional>
#include "IAPI.h"


/**
 * 
 */


DECLARE_LOG_CATEGORY_EXTERN(LogGitlabIntegrationAPI, Log, All);

class GITLABINTEGRATION_API GitlabAPI: public IAPI {
public:
	GitlabAPI();
	~GitlabAPI();

    GitlabAPI(FString base, FString token, FString LoadProject, std::function<void()> IssueCallback,
              std::function<void()> LabelCallback);

    void SetBaseUrl(FString server);
};
