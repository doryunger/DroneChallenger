using UnrealBuildTool;

public class DroneChallenger : ModuleRules
{
	public DroneChallenger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "CesiumRuntime", "UMG", "WebBrowser", "WebBrowserWidget", "AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "ArboristLib", "Sockets", "Networking", "ImageWrapper" });
	}
}
