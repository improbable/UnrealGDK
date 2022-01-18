// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

using UnrealBuildTool;

public class SpatialGDKServices : ModuleRules
{
	public SpatialGDKServices(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegacyPublicIncludePaths = false;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Buildkite can store environment variables like "True" and "False" as "1" and "0", which can't be parsed by bool.TryParse() or bool.Parse()
		string unity_mode_env = Environment.GetEnvironmentVariable("GDK_UNITY_MODE");
		bool unity_mode;
        if (bool.TryParse(unity_mode_env, unity_mode)) {
            // unity_mode_env is "True", "False" or case-insensitive versions of these
    		bUseUnity = unity_mode;
        } else if (unity_mode_env.Equals("0")) {
            bUseUnity = false;
        } else if (unity_mode_env.Equals("1")) {
            bUseUnity = true;
        } else {
            throw new FormatException("Value of GDK_UNITY_MODE environment variable was not recognized as a valid Boolean.");
        }

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"EditorStyle",
				"Engine",
				"OutputLog",
				"Slate",
				"SlateCore",
				"Core",
				"CoreUObject",
				"EngineSettings",
				"Json",
				"JsonUtilities",
				"UnrealEd",
				"Sockets",
				"HTTP"
			}
		);
	}
}
