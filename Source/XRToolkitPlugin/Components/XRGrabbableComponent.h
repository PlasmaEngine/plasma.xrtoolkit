#pragma once

#include <Core/World/Component.h>
#include <Core/World/World.h>
#include <XRToolkitPlugin/Core/XRToolkitTypes.h>

class plXRHandComponent;

using plXRGrabbableComponentManager = plComponentManager<class plXRGrabbableComponent, plBlockStorageType::FreeList>;

/// \brief Marks an object that a plXRHandComponent can pick up, hold and throw.
///
/// A hand picks the object up when its grip point is inside Grab Radius of the object's origin, or when the hand's reach sphere touches one
/// of the object's physics shapes. While it is held, a physics actor on the object (any component with a "Kinematic" property, such as a
/// dynamic actor) is switched to kinematic and follows the hand; on release it is switched back and thrown with the hand's velocity.
///
/// Sends plMsgGenericEvent "XRGrab" and "XRRelease" as event messages from the object, with the hand component as the sender and
/// "Left" or "Right" as the value.
class PL_XRTOOLKIT_DLL plXRGrabbableComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plXRGrabbableComponent, plComponent, plXRGrabbableComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plXRGrabbableComponent

public:
  plXRGrabbableComponent();
  ~plXRGrabbableComponent();

  plEnum<plXRGrabMode> m_GrabMode;                                  // [ property ]
  plEnum<plXRGrabHand> m_Hand;                                      // [ property ]

  /// How close the hand's grip point has to come to the object's origin, in metres.
  float m_fGrabRadius = 0.15f;                                      // [ property ]

  /// Snap To Hand only: where the object's origin sits relative to the grip pose.
  plVec3 m_vSnapPosition = plVec3::MakeZero();                      // [ property ]
  plQuat m_qSnapRotation = plQuat::MakeIdentity();                  // [ property ]

  /// Multiplies the hand's velocity when the object is let go.
  float m_fThrowVelocityScale = 1.0f;                               // [ property ]

  bool m_bHapticOnGrab = true;                                      // [ property ]
  float m_fHapticAmplitude = 0.5f;                                  // [ property ]
  plTime m_HapticDuration = plTime::MakeFromMilliseconds(50);       // [ property ]

  /// \brief Whether a hand holds the object right now.
  bool IsGrabbed() const { return !m_hHand.IsInvalidated(); } // [ scriptable ]

  /// \brief The object of the hand holding this, or an invalid handle.
  plGameObjectHandle GetGrabbingHandObject() const; // [ scriptable ]

  /// \brief Makes the hand holding the object let go of it, without throwing it.
  void Drop(); // [ scriptable ]

  /// \brief Whether the hand tracking \a controller may pick the object up (it is active, not held, and the Hand setting allows it).
  bool CanBeGrabbedBy(plXRDeviceType::Enum controller) const;

  /// \brief Called by plXRHandComponent when it picks the object up.
  void OnGrabbed(plXRHandComponent* pHand, plXRDeviceType::Enum controller);

  /// \brief Called by plXRHandComponent when it lets go.
  ///
  /// \a vHandVelocity is the hand's velocity over its last few frames and \a vLastFrameVelocity the object's velocity over the last
  /// frame, which a kinematic actor keeps when it becomes dynamic again; the difference is applied as an impulse.
  void OnReleased(plXRHandComponent* pHand, plXRDeviceType::Enum controller, const plVec3& vHandVelocity, const plVec3& vLastFrameVelocity, bool bThrow);

private:
  /// \brief Finds the component with a bool "Kinematic" property on the owner (a dynamic physics actor).
  plComponent* FindPhysicsActor();
  void SendEvent(const plHashedString& sEvent, plXRHandComponent* pHand, plXRDeviceType::Enum controller);

  plComponentHandle m_hHand;
  plComponentHandle m_hPhysicsActor;
  bool m_bActorWasKinematic = false;
};
