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

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
		}
	}
}
