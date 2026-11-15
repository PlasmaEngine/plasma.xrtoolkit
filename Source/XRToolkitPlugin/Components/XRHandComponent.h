#pragma once

#include <Core/World/Component.h>
#include <Core/World/World.h>
#include <Foundation/Containers/StaticArray.h>
#include <XRToolkitPlugin/Core/XRToolkitTypes.h>

class plXRGrabbableComponent;
class plXRHandComponent;

using plXRHandComponentManager = plXRToolkitComponentManager<plXRHandComponent>;

/// \brief Picks up plXRGrabbableComponent objects with a controller's grip button, holds them and throws them on release.
///
/// Sits on the object that a plDeviceTrackingComponent moves to a controller's grip pose (or on a child of it). When the grip is
/// pressed, the nearest grabbable in reach is picked up: one whose Grab Radius contains the hand's position, or one whose physics shape
/// touches the hand's reach sphere. Releasing the grip lets go, with the hand's velocity averaged over its last Throw Samples frames.
class PL_XRTOOLKIT_DLL plXRHandComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plXRHandComponent, plComponent, plXRHandComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plXRHandComponent

public:
  plXRHandComponent();
  ~plXRHandComponent();

  static constexpr float UpdatePriority = plXRToolkit::UpdatePriorityHands;

  /// Which controller's grip button and haptics this hand uses. Auto: the tracked controller, else the right one.
  plEnum<plXRHandSide> m_Hand;                    // [ property ]

  /// Radius of the sphere around the hand that physics shapes of grabbables must touch, in metres.
  float m_fReach = 0.08f;                         // [ property ]
  plUInt8 m_uiCollisionLayer = 0;                 // [ property ]

  /// Input action of a plInputComponent on this object or a parent. Empty: the input slot is read directly.
  plString m_sGripAction;                         // [ property ]
  /// Input slot read when no action is used. Empty: xr_hand_<left|right>_grip_value.
  plString m_sGripSlot;                           // [ property ]
  /// Grip value at which the hand closes. It opens again below half of it.
  float m_fGripThreshold = 0.5f;                  // [ property ]

  /// How many frames of hand movement the throw velocity is averaged over.
  plUInt8 m_uiThrowSamples = 5;                   // [ property ]

  /// \brief Picks up the nearest grabbable in reach. Returns false when there is none or the hand already holds something.
  bool Grab(); // [ scriptable ]

  /// \brief Lets go of the held object. With \a bThrow it leaves with the hand's velocity, otherwise it comes to rest.
  void Release(bool bThrow); // [ scriptable ]

  bool IsHolding() const { return !m_hHeld.IsInvalidated(); } // [ scriptable ]

  /// \brief The held object, or an invalid handle.
  plGameObjectHandle GetHeldObject() const;

  /// \brief The controller this hand uses.
  plXRDeviceType::Enum GetController() const;

  /// \brief The hand's velocity averaged over the last Throw Samples frames, in metres per second.
  plVec3 GetHandVelocity() const;

  /// \brief Called by the component manager every frame while simulating.
  void Update();

private:
  struct Sample
  {
    plVec3 m_vPosition;
    plTime m_Time;
  };

  plXRGrabbableComponent* FindNearestGrabbable(const plVec3& vHandPosition, plXRDeviceType::Enum controller);
  void GrabObject(plXRGrabbableComponent* pGrabbable, const plTransform& handTransform);
  void AddSample(const plVec3& vPosition);
  plVec3 GetLastFrameVelocity() const;

  plComponentHandle m_hHeld;
  plTransform m_HeldOffset = plTransform::MakeIdentity(); ///< the held object relative to the hand
  bool m_bGripDown = false;

  static constexpr plUInt32 MaxSamples = 16;
  plStaticArray<Sample, MaxSamples> m_Samples; ///< the hand's last positions, oldest first
};
