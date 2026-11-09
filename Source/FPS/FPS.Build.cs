using UnrealBuildTool;

public class FPS : ModuleRules
{
	public FPS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"GameplayTags", "PhysicsCore", "UMG", "Niagara", "AnimGraphRuntime",
			"AIModule", "NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate", "SlateCore", "NavigationSystem",
			"MoviePlayer", "AssetRegistry"
		});
	}
}
