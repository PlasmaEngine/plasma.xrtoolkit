#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/Input/InputManager.h>
#include <Foundation/Configuration/Singleton.h>
#include <GameEngine/Gameplay/InputComponent.h>
#include <GameEngine/XR/DeviceTrackingComponent.h>
#include <GameEngine/XR/StageSpaceComponent.h>
#include <GameEngine/XR/XRInputDevice.h>
#include <GameEngine/XR/XRInterface.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <XRToolkitPlugin/Core/XRToolkitTypes.h>

// clang-format off
PL_BEGIN_STATIC_REFLECTED_ENUM(plXRHandSide, 1)
  PL_ENUM_CONSTANTS(plXRHandSide::Auto, plXRHandSide::Left, plXRHandSide::Right)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plXRGrabHand, 1)
  PL_ENUM_CONSTANTS(plXRGrabHand::Any, plXRGrabHand::Left, plXRGrabHand::Right)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plXRGrabMode, 1)
  PL_ENUM_CONSTANTS(plXRGrabMode::SnapToHand, plXRGrabMode::KeepOffset)
PL_END_STATIC_REFLECTED_ENUM;

PL_BEGIN_STATIC_REFLECTED_ENUM(plXRTurnMode, 1)
  PL_ENUM_CONSTANTS(plXRTurnMode::Snap, plXRTurnMode::Smooth)
PL_END_STATIC_REFLECTED_ENUM;
// clang-format on

namespace plXRToolkit
{
  namespace
  {
    plXRInterface* GetRunningXR()
    {
      plXRInterface* pXR = plSingletonRegistry::GetSingletonInstance<plXRInterface>();
      return (pXR != nullptr && pXR->IsInitialized()) ? pXR : nullptr;
    }
  } // namespace

  plXRDeviceType::Enum ResolveController(const plGameObject* pObject, plXRHandSide::Enum side, plXRDeviceType::Enum fallback)
  {
    if (side == plXRHandSide::Left)
      return plXRDeviceType::LeftController;

    if (side == plXRHandSide::Right)
      return plXRDeviceType::RightController;

    for (const plGameObject* pCur = pObject; pCur != nullptr; pCur = pCur->GetParent())
    {
      const plDeviceTrackingComponent* pTracking = nullptr;
      if (pCur->TryGetComponentOfBaseType(pTracking))
      {
        const plXRDeviceType::Enum type = pTracking->GetDeviceType();
        if (type == plXRDeviceType::LeftController || type == plXRDeviceType::RightController)
          return type;
      }
    }

    return fallback;
  }

  plStringView GetSideName(plXRDeviceType::Enum controller)
  {
    return controller == plXRDeviceType::LeftController ? "left" : "right";
  }

  float ReadInput(const plGameObject* pObject, plStringView sAction, plStringView sSlot, plXRDeviceType::Enum controller, plStringView sDefaultSlotSuffix)
  {
    if (!sAction.IsEmpty())
    {
      for (const plGameObject* pCur = pObject; pCur != nullptr; pCur = pCur->GetParent())
      {
        const plInputComponent* pInput = nullptr;
        if (pCur->TryGetComponentOfBaseType(pInput) && pInput->IsActiveAndInitialized())
        {
          plStringBuilder sTmp;
          return pInput->GetCurrentInputState(sAction.GetData(sTmp));
        }
      }
    }

    plStringBuilder sSlotName;
    if (!sSlot.IsEmpty())
    {
      sSlotName = sSlot;
    }
    else
    {
      sSlotName.Set("xr_hand_", GetSideName(controller), "_", sDefaultSlotSuffix);
    }

    plKeyState::Enum state = plKeyState::Up;
    float fValue = 0.0f;
    if (plInputManager::TryGetInputSlotState(sSlotName, state, fValue))
      return plMath::Clamp(fValue, 0.0f, 1.0f);

    return 0.0f;
  }

  void HapticPulse(plXRDeviceType::Enum controller, plTime duration, float fAmplitude)
  {
    plXRInterface* pXR = GetRunningXR();
    if (pXR == nullptr || duration <= plTime::MakeZero() || fAmplitude <= 0.0f)
      return;

    const plXRDeviceID deviceID = pXR->GetXRInput().GetDeviceIDByType(controller);
    if (deviceID == -1)
      return;

    if (!pXR->GetXRInput().GetDeviceFeatures(deviceID).IsSet(plXRDeviceFeatures::Haptics))
      return;

    pXR->TriggerHapticPulse(deviceID, duration, plMath::Clamp(fAmplitude, 0.0f, 1.0f)).IgnoreResult();
  }

  plStageSpaceComponent* FindStageSpace(plWorld* pWorld)
  {
    if (pWorld == nullptr)
      return nullptr;

    if (plStageSpaceComponentManager* pManager = pWorld->GetComponentManager<plStageSpaceComponentManager>())
      return pManager->GetSingletonComponent();

    return nullptr;
  }

  bool GetHeadPoseInStage(plTransform& out_pose)
  {
    out_pose = plTransform::MakeIdentity();

    plXRInterface* pXR = GetRunningXR();
    if (pXR == nullptr)
      return false;

    const plXRDeviceID deviceID = pXR->GetXRInput().GetDeviceIDByType(plXRDeviceType::HMD);
    if (deviceID == -1)
      return false;

    const plXRDeviceState& state = pXR->GetXRInput().GetDeviceState(deviceID);
    if (!state.m_bGripPoseIsValid)
      return false;

    out_pose = plTransform(state.m_vGripPosition, state.m_qGripRotation);
    return true;
  }

  plTransform GetCurrentGlobalTransform(plGameObject* pObject)
  {
    pObject->UpdateGlobalTransform();
    return pObject->GetGlobalTransform();
  }

  void DrawCircle(const plWorld* pWorld, const plVec3& vCenter, const plVec3& vNormal, float fRadius, const plColor& color)
  {
    constexpr plUInt32 uiSegments = 32;

    const plVec3 vUp = vNormal.GetNormalized();
    plVec3 vTangent = vUp.GetOrthogonalVector();
    vTangent.Normalize();
    const plVec3 vBitangent = vUp.CrossRH(vTangent);

    plHybridArray<plDebugRenderer::Line, uiSegments> lines;

    plVec3 vPrev = vCenter + vTangent * fRadius;
    for (plUInt32 i = 1; i <= uiSegments; ++i)
    {
      const plAngle a = plAngle::MakeFromDegree(360.0f * i / uiSegments);
      const plVec3 vNext = vCenter + (vTangent * plMath::Cos(a) + vBitangent * plMath::Sin(a)) * fRadius;
      lines.PushBack(plDebugRenderer::Line(vPrev, vNext));
      vPrev = vNext;
    }

    plDebugRenderer::DrawLines(pWorld, lines, color);
  }
} // namespace plXRToolkit
