using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class XRToolkitPluginModule : ModuleRules
{
    public XRToolkitPluginModule(BuildContext context) : base(context)
    {
        // Physics queries go through Core's plPhysicsWorldModuleInterface, so no physics package is linked.
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/XRToolkitPlugin",
            "XRToolkitPCH.h", "BUILDSYSTEM_BUILDING_XRTOOLKITPLUGIN_LIB");
    }
}
