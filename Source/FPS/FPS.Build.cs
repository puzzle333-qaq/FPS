// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class FPS : ModuleRules
{
	public FPS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "PhysicsCore", "UMG", "Niagara" });

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate", "SlateCore", "CoreOnline",
			"OnlineSubsystem", "OnlineSubsystemUtils",
			"OnlineSubsystemEOS", "EOSShared", "EOSSDK"
			// EOS：用 IOnlineSubsystemEOS 拿平台句柄做 DeviceId（Plugins/OnlineSubsystemEOS 是项目覆盖版，开了 ADD_USER_LOGIN_INFO）
		});
	}
}