using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class EditorPluginXRToolkitModule : ModuleRules
{
    public EditorPluginXRToolkitModule(BuildContext context) : base(context)
    {
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/EditorPluginXRToolkit", "EditorPluginXRToolkitPCH.h",
            "BUILDSYSTEM_BUILDING_EDITORPLUGINXRTOOLKIT_LIB");

        PlasmaPackageSdk.ConfigureEditorSdkConsumer(this, context);
        PlasmaPackageSdk.AddPackagePluginImport(this, context, "Source/XRToolkitPlugin", "plXRToolkitPlugin");

        // the package's icon folder is registered with the engine-drawn editor UI
        PlasmaPackageSdk.AddImportLibrary(this, context, "plEditorUi");
    }
}
