using UnrealBuildTool;
using System.Collections.Generic;

public class EisenwalTarget : TargetRules
{
	public EisenwalTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Eisenwal");
	}
}
