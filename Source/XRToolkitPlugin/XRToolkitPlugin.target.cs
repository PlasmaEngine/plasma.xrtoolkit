using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class XRToolkitPluginTarget : TargetRules
{
    public XRToolkitPluginTarget(BuildContext context) : base(context)
    {
        Type = TargetType.SharedLibrary;
        OutputName = "plXRToolkitPlugin";
        OutputDirectory = PlasmaPackageSdk.PackageBinaryDirectory(context);
        UsePCHFiles = true;
        UseUnityBuild = true;
        UseAdaptiveUnityBuild = true;
        UseIncrementalLinking = true;
        ExtraModules.Add("XRToolkitPluginModule");
    }
}
