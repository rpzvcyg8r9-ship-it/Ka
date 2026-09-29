using UnrealBuildTool;

public class Eisenwal : ModuleRules
{
	public Eisenwal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Erlaubt Includes wie "Ocean/EisenwalOceanSubsystem.h" aus allen Unterordnern.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"DeveloperSettings",
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore",
			"Niagara"
		});
	}
}
