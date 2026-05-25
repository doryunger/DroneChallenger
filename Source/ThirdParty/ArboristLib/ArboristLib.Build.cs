using UnrealBuildTool;
using System.IO;

public class ArboristLib : ModuleRules
{
    public ArboristLib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string ModDir = ModuleDirectory;

        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModDir, "include"),
            Path.Combine(ModDir, "include", "httplib"),
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(ModDir, "lib", "Win64", "arborist.lib"));
            PublicSystemLibraries.Add("ws2_32.lib");
        }
    }
}
