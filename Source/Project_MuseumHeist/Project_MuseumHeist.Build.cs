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
			"StateTreeModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ImageCore",
			"SlateCore"
		});
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
