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
///
/// A grabbable out of reach can be pulled. While the hand is open, the grabbable it points at within Pull Range is marked with a ring.
/// Closing the hand locks onto it, and a flick of the wrist launches it on an arc that ends at the hand Pull Flight Time later. The hand
/// catches it when it arrives while the grip is closed; otherwise it falls past.
///
/// The open hand also pushes: a dynamic physics actor that touches its reach sphere and is not a grabbable (a door, a swing) gets a push
/// along the hand's movement.
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

  /// How far away a grabbable can be pulled, in metres. 0 turns pulling off.
  float m_fPullRange = 6.0f;                      // [ property ]
  /// How far off the pointing direction a grabbable may be and still be picked for pulling.
  plAngle m_PullAngle = plAngle::MakeFromDegree(12.0f); // [ property ]
  /// How fast the wrist has to turn, per second, to launch the locked grabbable.
  plAngle m_PullFlickRate = plAngle::MakeFromDegree(250.0f); // [ property ]
  /// How long a launched grabbable takes to reach the hand.
  plTime m_PullFlightTime = plTime::MakeFromSeconds(0.7); // [ property ]

  /// How hard the open hand pushes dynamic objects it touches: newtons per metre per second of hand speed. 0 turns pushing off.
  float m_fPushStrength = 40.0f;                  // [ property ]

  /// \brief Picks up the nearest grabbable in reach. Returns false when there is none or the hand already holds something.
  bool Grab(); // [ scriptable ]

  /// \brief Lets go of the held object. With \a bThrow it leaves with the hand's velocity, otherwise it comes to rest.
  void Release(bool bThrow); // [ scriptable ]

  bool IsHolding() const { return !m_hHeld.IsInvalidated(); } // [ scriptable ]

  /// \brief Whether a grabbable this hand launched is flying towards it.
  bool IsPulling() const { return !m_hIncoming.IsInvalidated(); } // [ scriptable ]

  /// \brief The object the ring marks: the one a closing hand would lock onto, or the locked one. Invalid when there is none.
  plGameObjectHandle GetPullTarget() const;

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
    plQuat m_qStageRotation; ///< the hand's rotation in stage space, so turning the play area is not mistaken for a flick
    plTime m_Time;
  };

  plXRGrabbableComponent* FindNearestGrabbable(const plVec3& vHandPosition, plXRDeviceType::Enum controller);
  plXRGrabbableComponent* FindPullTarget(const plTransform& pointer, plXRDeviceType::Enum controller);
  plTransform GetPointer(const plTransform& handTransform, plXRDeviceType::Enum controller);
  plTransform GetStageTransform();
  void GrabObject(plXRGrabbableComponent* pGrabbable, const plTransform& handTransform);
  void UpdatePull(const plTransform& handTransform, plXRDeviceType::Enum controller);
  void Launch(plXRGrabbableComponent* pGrabbable, const plVec3& vHandPosition);
  void TryCatch(const plTransform& handTransform);
  void StartFling(plXRGrabbableComponent* pGrabbable, const plVec3& vVelocity, bool bRelease);
  void FinishFling();
  void Push(const plVec3& vHandPosition);
  void AddSample(const plVec3& vPosition, const plQuat& qStageRotation);
  float GetWristRate() const;

  plComponentHandle m_hHeld;
  plTransform m_HeldOffset = plTransform::MakeIdentity(); ///< the held object relative to the hand
  bool m_bGripDown = false;

  plComponentHandle m_hPullTarget; ///< marked by the ring
  bool m_bPullLocked = false;      ///< the grip closed on the pull target; a flick launches it

  plComponentHandle m_hIncoming;   ///< launched by this hand and flying towards it
  plTime m_IncomingUntil;          ///< after this the hand stops waiting to catch it

  // A thrown or launched object stays kinematic for one more frame, moving at the velocity it is to leave with, and is made dynamic in the
  // next. Its body keeps that velocity, so the object leaves at the right speed whatever its mass.
  plComponentHandle m_hFling;
  bool m_bFlingIsRelease = false;

  static constexpr plUInt32 MaxSamples = 16;
  plStaticArray<Sample, MaxSamples> m_Samples; ///< the hand's last positions, oldest first
};
