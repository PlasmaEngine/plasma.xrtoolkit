// #pl-version 1

using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class PlasmaXRToolkitWorkspace : WorkspaceRules
{
    public PlasmaXRToolkitWorkspace(BuildContext context) : base(context)
    {
        TargetNames.Add("XRToolkitPlugin");
        TargetNames.Add("EditorPluginXRToolkit");
    }
}
