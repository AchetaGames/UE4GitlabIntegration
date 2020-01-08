//
// Created by stastny on 8/23/19.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "GitlabIntegrationSettings.generated.h"


UCLASS(config = Engine)
class UGitlabIntegrationSettings
        : public UObject {
    GENERATED_BODY()

public:
    /**
     * The url of the Gitlab instance.
     */
    UPROPERTY(config, EditAnywhere)
    FText Server = FText::GetEmpty();

    /**
     * The token to access the Gitlab instance.
     */
    UPROPERTY(config, EditAnywhere)
    FText Token = FText::GetEmpty();

    /**
     * Selected project
     */
    UPROPERTY(config, EditAnywhere)
    FText Project = FText::GetEmpty();

    /**
     * Sort Issues newest first
     */
    UPROPERTY(config, EditAnywhere)
    bool SortIssuesNewestFirst = true;
};
