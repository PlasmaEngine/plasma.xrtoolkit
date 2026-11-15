#pragma once

#include <Core/World/ComponentManager.h>
#include <Core/World/World.h>
#include <Foundation/Reflection/Reflection.h>
#include <GameEngine/XR/Declarations.h>
#include <XRToolkitPlugin/XRToolkitDLL.h>

class plStageSpaceComponent;

/// \brief Which controller a hand, pointer, teleport or turn component listens to.
struct PL_XRTOOLKIT_DLL plXRHandSide
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Auto,  ///< The controller of the plDeviceTrackingComponent on the object or one of its parents. Each component has its own fallback.
    Left,  ///< The left controller.
    Right, ///< The right controller.

    Default = Auto
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_XRTOOLKIT_DLL, plXRHandSide);

/// \brief Which hands may pick up a grabbable object.
struct PL_XRTOOLKIT_DLL plXRGrabHand
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Any,   ///< Either hand.
    Left,  ///< Only the left hand.
    Right, ///< Only the right hand.

    Default = Any
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_XRTOOLKIT_DLL, plXRGrabHand);

/// \brief Where a grabbed object sits in the hand.
struct PL_XRTOOLKIT_DLL plXRGrabMode
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    SnapToHand, ///< The object's origin moves to the grip pose, plus the grabbable's snap offset.
    KeepOffset, ///< The object stays where it was relative to the hand at the moment of the grab.

    Default = SnapToHand
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_XRTOOLKIT_DLL, plXRGrabMode);

/// \brief How a plXRTurnComponent turns the player.
struct PL_XRTOOLKIT_DLL plXRTurnMode
{
  using StorageType = plUInt8;

  enum Enum : StorageType
  {
    Snap,   ///< Turns by a fixed angle each time the stick is pushed to the side.
    Smooth, ///< Turns continuously while the stick is held to the side.

    Default = Snap
  };
};

PL_DECLARE_REFLECTABLE_TYPE(PL_XRTOOLKIT_DLL, plXRTurnMode);

/// \brief Shared helpers of the XR toolkit components.
namespace plXRToolkit
{
  /// \brief The controller \a side refers to. Auto looks for a plDeviceTrackingComponent tracking a controller on \a pObject and its
  /// parents, and returns \a fallback when there is none.
  PL_XRTOOLKIT_DLL plXRDeviceType::Enum ResolveController(const plGameObject* pObject, plXRHandSide::Enum side, plXRDeviceType::Enum fallback);

  /// \brief "left" or "right", as used in the XR input slot names.
  PL_XRTOOLKIT_DLL plStringView GetSideName(plXRDeviceType::Enum controller);

  /// \brief Reads an input value between 0 and 1.
  ///
  /// When \a sAction is set and a plInputComponent sits on \a pObject or one of its parents, the action's value is returned. Otherwise the
  /// input slot \a sSlot is read, and when that is empty "xr_hand_<left|right>_<sDefaultSlotSuffix>" of \a controller.
  PL_XRTOOLKIT_DLL float ReadInput(const plGameObject* pObject, plStringView sAction, plStringView sSlot, plXRDeviceType::Enum controller, plStringView sDefaultSlotSuffix);

  /// \brief Vibrates \a controller, if an XR session runs and the controller has haptics.
  PL_XRTOOLKIT_DLL void HapticPulse(plXRDeviceType::Enum controller, plTime duration, float fAmplitude);

  /// \brief The world's plStageSpaceComponent, or nullptr when it has none.
  PL_XRTOOLKIT_DLL plStageSpaceComponent* FindStageSpace(plWorld* pWorld);

  /// \brief The head's pose in stage space. Returns false (and identity) without an XR session or a valid head pose.
  PL_XRTOOLKIT_DLL bool GetHeadPoseInStage(plTransform& out_pose);

  /// \brief The object's global transform after applying this frame's changes to it and its parents.
  ///
  /// Global transforms of dynamic objects are otherwise only updated after the PostAsync phase, and the toolkit's components run in
  /// PreAsync, right after plDeviceTrackingComponent moved the controllers.
  PL_XRTOOLKIT_DLL plTransform GetCurrentGlobalTransform(plGameObject* pObject);

  /// \brief Draws a circle around \a vCenter, perpendicular to \a vNormal.
  PL_XRTOOLKIT_DLL void DrawCircle(const plWorld* pWorld, const plVec3& vCenter, const plVec3& vNormal, float fRadius, const plColor& color);

  /// \brief Update priorities within PreAsync. Higher runs earlier: device tracking (0), then the stage movers, then hands and pointers,
  /// all before the physics step starts (-100000) so held objects are where the hand is.
  constexpr float UpdatePriorityStage = -100.0f;
  constexpr float UpdatePriorityHands = -200.0f;
} // namespace plXRToolkit

/// \brief Calls Update() on each active component in the PreAsync phase while the world simulates, at ComponentType::UpdatePriority.
template <typename ComponentType>
class plXRToolkitComponentManager final : public plComponentManager<ComponentType, plBlockStorageType::FreeList>
{
  using SUPER = plComponentManager<ComponentType, plBlockStorageType::FreeList>;

public:
  plXRToolkitComponentManager(plWorld* pWorld)
    : SUPER(pWorld)
  {
  }

  virtual void Initialize() override
  {
    plStringBuilder sName = ComponentType::GetStaticRTTI()->GetTypeName();
    sName.Append("::Update");

    auto desc = plWorldModule::UpdateFunctionDesc(plWorldModule::UpdateFunction(&plXRToolkitComponentManager::Update, this), sName);
    desc.m_Phase = plWorldModule::UpdateFunctionDesc::Phase::PreAsync;
    desc.m_bOnlyUpdateWhenSimulating = true;
    desc.m_fPriority = ComponentType::UpdatePriority;

    this->RegisterUpdateFunction(desc);
  }

private:
  void Update(const plWorldModule::UpdateContext& context)
  {
    for (auto it = this->m_ComponentStorage.GetIterator(context.m_uiFirstComponentIndex, context.m_uiComponentCount); it.IsValid(); ++it)
    {
      ComponentType* pComponent = it;
      if (pComponent->IsActiveAndInitialized())
      {
        pComponent->Update();
      }
    }
  }
};
