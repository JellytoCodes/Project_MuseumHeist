// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_MuseumHeist.h"
#include "Core/HeistGameplayTags.h"
#include "Modules/ModuleManager.h"

class FProjectMuseumHeistModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		FHeistGameplayTags::InitializeNativeGameplayTags();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FProjectMuseumHeistModule, Project_MuseumHeist, "Project_MuseumHeist");
