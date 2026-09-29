using UnrealBuildTool;
using System.Collections.Generic;

public class EisenwalEditorTarget : TargetRules
{
	public EisenwalEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Eisenwal");
	}
}
