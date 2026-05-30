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
            string LibDir = Path.Combine(ModDir, "lib", "Win64");
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "arborist.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "sqlite3.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "ryml.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "c4core.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "brotlienc.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "brotlidec.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "brotlicommon.lib"));
            PublicSystemLibraries.Add("ws2_32.lib");
        }
    }
}
