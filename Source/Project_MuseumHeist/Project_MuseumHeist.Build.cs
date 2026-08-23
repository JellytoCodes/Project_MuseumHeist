// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_MuseumHeist : ModuleRules
{
	public Project_MuseumHeist(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NetCore",
			"GameplayTags",
			"UMG",
			"ModelViewViewModel",
			"AIModule",
			"GameplayStateTreeModule",
			"StateTreeModule",
			"OnlineSubsystem",
			"OpenCV",
			"OpenCVHelper"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"CoreOnline",
			"ImageCore",
			"NavigationSystem",
			"Niagara",
			"OnlineBase",
			"OnlineSubsystemUtils",
			"RHI",
			"SlateCore"
		});

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
			"OnlineSubsystemSteam"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
			PrivateDefinitions.Add("WITH_HEIST_STEAM_AVATAR=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_HEIST_STEAM_AVATAR=0");
		}

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
		}
	}
}
