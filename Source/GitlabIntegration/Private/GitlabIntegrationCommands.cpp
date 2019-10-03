// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GitlabIntegrationCommands.h"

#define LOCTEXT_NAMESPACE "FGitlabIntegrationModule"

void FGitlabIntegrationCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Gitlab", "Bring up Gitlab Integration window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
