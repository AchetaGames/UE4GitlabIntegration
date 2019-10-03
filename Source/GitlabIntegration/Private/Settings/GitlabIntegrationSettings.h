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
    FString Server = TEXT("https://gitlab.com");

    /**
     * The token to access the Gitlab instance.
     */
    UPROPERTY(config, EditAnywhere)
    FString Token;

    /**
     * Selected project
     */
    UPROPERTY(config, EditAnywhere)
    FString Project = TEXT("");
};
