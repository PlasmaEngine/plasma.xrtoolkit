#include <EditorPluginXRToolkit/EditorPluginXRToolkitPCH.h>

#include <EditorUi/Widgets.h>
#include <Foundation/Configuration/Plugin.h>
#include <XRToolkitPlugin/Components/XRGrabbableComponent.h>
#include <XRToolkitPlugin/Components/XRHandComponent.h>
#include <XRToolkitPlugin/Components/XRPointerComponent.h>
#include <XRToolkitPlugin/Components/XRTeleportComponent.h>
#include <XRToolkitPlugin/Components/XRTurnComponent.h>

// The editor side of the XR Toolkit: the package's icons and hiding properties that do nothing in the current mode.

namespace
{
  /// The package's icon folder (":/TypeIcons/plXRHandComponent.svg" is "EditorIcons/TypeIcons/...").
  /// The plugin binary lives in <package>/Bin/<configuration>.
  plString s_sIconFolder;

  void AddPackageIconFolder()
  {
    plStringBuilder sPluginFile, sCopiedFile;
    plPlugin::GetPluginPaths("plEditorPluginXRToolkit", sPluginFile, sCopiedFile, 0);

    plStringBuilder sFolder = sPluginFile.GetFileDirectory();
    sFolder.AppendPath("..", "..", "EditorIcons");
    sFolder.MakeCleanPath();

    s_sIconFolder = sFolder;
    plEditorUi::AddIconFolder(s_sIconFolder);
  }

  void RemovePackageIconFolder()
  {
    if (s_sIconFolder.IsEmpty())
      return;

    plEditorUi::RemoveIconFolder(s_sIconFolder);
    s_sIconFolder.Clear();
  }

  void Show(plPropertyMetaStateEvent& e, const char* szProperty, bool bVisible)
  {
    (*e.m_pPropertyStates)[szProperty].m_Visibility = bVisible ? plPropertyUiState::Default : plPropertyUiState::Invisible;
  }

  plInt64 GetInt(plPropertyMetaStateEvent& e, const char* szProperty)
  {
    return e.m_pObject->GetTypeAccessor().GetValue(szProperty).ConvertTo<plInt64>();
  }

  bool GetBool(plPropertyMetaStateEvent& e, const char* szProperty)
  {
    return e.m_pObject->GetTypeAccessor().GetValue(szProperty).ConvertTo<bool>();
  }

  float GetValueAsFloat(plPropertyMetaStateEvent& e, const char* szProperty)
  {
    return e.m_pObject->GetTypeAccessor().GetValue(szProperty).ConvertTo<float>();
  }

  void XRToolkitPropertyMetaState(plPropertyMetaStateEvent& e)
  {
    const plRTTI* pType = e.m_pObject->GetTypeAccessor().GetType();

    if (pType == plGetStaticRTTI<plXRGrabbableComponent>())
    {
      const bool bSnap = GetInt(e, "GrabMode") == plXRGrabMode::SnapToHand;
      Show(e, "SnapPosition", bSnap);
      Show(e, "SnapRotation", bSnap);

      const bool bHaptic = GetBool(e, "HapticOnGrab");
      Show(e, "HapticAmplitude", bHaptic);
      Show(e, "HapticDuration", bHaptic);
    }
    else if (pType == plGetStaticRTTI<plXRHandComponent>())
    {
      const bool bPull = GetValueAsFloat(e, "PullRange") > 0.0f;
      Show(e, "PullAngle", bPull);
      Show(e, "PullFlickRate", bPull);
      Show(e, "PullFlightTime", bPull);
    }
    else if (pType == plGetStaticRTTI<plXRPointerComponent>())
    {
      const bool bBeam = GetBool(e, "ShowBeam");
      Show(e, "ShowBeamWithoutHit", bBeam);
      Show(e, "BeamColor", bBeam);
      Show(e, "HitColor", bBeam);
      Show(e, "HitDotSize", bBeam);

      const bool bHaptic = GetBool(e, "HapticOnHover");
      Show(e, "HoverHapticAmplitude", bHaptic);
      Show(e, "HoverHapticDuration", bHaptic);
    }
    else if (pType == plGetStaticRTTI<plXRTeleportComponent>())
    {
      const bool bArc = GetBool(e, "ShowArc");
      Show(e, "ValidColor", bArc);
      Show(e, "InvalidColor", bArc);
      Show(e, "TargetRadius", bArc);

      const bool bBlink = e.m_pObject->GetTypeAccessor().GetValue("BlinkDuration").ConvertTo<plTime>() > plTime::MakeZero();
      Show(e, "BlinkColor", bBlink);
    }
    else if (pType == plGetStaticRTTI<plXRTurnComponent>())
    {
      const bool bSnap = GetInt(e, "Mode") == plXRTurnMode::Snap;
      Show(e, "SnapAngle", bSnap);
      Show(e, "SnapThreshold", bSnap);
      Show(e, "SmoothSpeed", !bSnap);
      Show(e, "DeadZone", !bSnap);
      Show(e, "Vignette", !bSnap);
      Show(e, "VignetteSpeed", !bSnap && GetValueAsFloat(e, "Vignette") > 0.0f);
    }
  }

  void OnLoadPlugin()
  {
    AddPackageIconFolder();
    plPropertyMetaState::GetSingleton()->m_Events.AddEventHandler(XRToolkitPropertyMetaState);
  }

  void OnUnloadPlugin()
  {
    plPropertyMetaState::GetSingleton()->m_Events.RemoveEventHandler(XRToolkitPropertyMetaState);
    RemovePackageIconFolder();
  }
} // namespace

PL_PLUGIN_DEPENDENCY(plXRToolkitPlugin);

PL_PLUGIN_ON_LOADED()
{
  OnLoadPlugin();
}

PL_PLUGIN_ON_UNLOADED()
{
  OnUnloadPlugin();
}
