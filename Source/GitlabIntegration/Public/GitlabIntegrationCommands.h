// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "GitlabIntegrationStyle.h"

class FGitlabIntegrationCommands : public TCommands<FGitlabIntegrationCommands>
{
public:

	FGitlabIntegrationCommands()
		: TCommands<FGitlabIntegrationCommands>(TEXT("GitlabIntegration"), NSLOCTEXT("Contexts", "GitlabIntegration", "GitlabIntegration Plugin"), NAME_None, FGitlabIntegrationStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};