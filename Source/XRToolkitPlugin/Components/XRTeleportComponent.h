#pragma once

#include <Core/World/Component.h>
#include <Core/World/World.h>
#include <Foundation/Math/Color.h>
#include <XRToolkitPlugin/Core/XRToolkitTypes.h>

class plXRTeleportComponent;

using plXRTeleportComponentManager = plXRToolkitComponentManager<plXRTeleportComponent>;

/// \brief Arc teleport: aim with a controller, release to move the play area to the target.
///
/// While the activate input (thumbstick forward by default) is held, a ballistic arc is cast from the owner along its forward axis (+X)
/// and drawn with a ring at its end: valid colour when the arc lands on a surface no steeper than Max Slope, invalid colour otherwise.
/// Releasing the input over a valid target moves the owner of the world's plStageSpaceComponent so that the floor under the headset lands
/// on the target; the head keeps its offset within the play area. A short blink to Blink Color (plXRScreenEffects) hides the move, and
/// plMsgGenericEvent "XRTeleport" is sent as an event message from the owner with the target as the value once the move happened.
class PL_XRTOOLKIT_DLL plXRTeleportComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plXRTeleportComponent, plComponent, plXRTeleportComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plXRTeleportComponent

public:
  plXRTeleportComponent();
  ~plXRTeleportComponent();

  static constexpr float UpdatePriority = plXRToolkit::UpdatePriorityStage;

  /// Which controller's stick activates the teleport. Auto: the tracked controller, else the left one.
  plEnum<plXRHandSide> m_Hand;                                  // [ property ]

  /// Input action of a plInputComponent on this object or a parent. Empty: the input slot is read directly.
  plString m_sActivateAction;                                   // [ property ]
  /// Input slot read when no action is used. Empty: xr_hand_<left|right>_primary_analog_stick_posy.
  plString m_sActivateSlot;                                     // [ property ]
  /// Input value at which aiming starts. Aiming ends, and the teleport happens, below half of it.
  float m_fActivateThreshold = 0.6f;                            // [ property ]

  /// Start speed of the arc, in metres per second. Higher reaches further.
  float m_fLaunchVelocity = 7.0f;                               // [ property ]
  /// Downward acceleration that bends the arc, in metres per second squared.
  float m_fGravity = 9.81f;                                     // [ property ]
  /// Number of segments the arc is cast in.
  plUInt16 m_uiMaxSteps = 40;                                   // [ property ]
  /// Simulated time per segment, in seconds.
  float m_fStepTime = 0.03f;                                    // [ property ]
  plUInt8 m_uiCollisionLayer = 0;                               // [ property ]
  /// Steepest surface that counts as a valid landing.
  plAngle m_MaxSlope = plAngle::MakeFromDegree(30.0f);          // [ property ]

  bool m_bShowArc = true;                                       // [ property ]
  plColor m_ValidColor = plColor::LimeGreen;                    // [ property ]
  plColor m_InvalidColor = plColor::Crimson;                    // [ property ]
  /// Radius of the ring drawn at the end of the arc, in metres.
  float m_fTargetRadius = 0.3f;                                 // [ property ]

  /// Length of the blink that hides the move: the view fades to Blink Color, the move happens at the darkest point, then it fades
  /// back. Zero moves at once, as a cut.
  plTime m_BlinkDuration = plTime::MakeFromSeconds(0.2);        // [ property ]
  plColor m_BlinkColor = plColor::Black;                        // [ property ]

  bool IsAiming() const { return m_bAiming; }                   // [ scriptable ]
  bool HasValidTarget() const { return m_bAiming && m_bValidTarget; } // [ scriptable ]
  const plVec3& GetTargetPosition() const { return m_vTarget; } // [ scriptable ]

  /// \brief Moves the stage space so that the floor under the headset is at \a vTarget, behind a blink when Blink Duration is set.
  /// Does nothing without a plStageSpaceComponent.
  void TeleportTo(const plVec3& vTarget); // [ scriptable ]

  /// \brief Called by the component manager every frame while simulating.
  void Update();

private:
  void TraceArc(const plTransform& aim);
  void DrawArc() const;
  void MoveStageTo(const plVec3& vTarget);
  /// Advances the blink; returns false once it is over.
  bool UpdateBlink();
  void EndBlink();

  bool m_bBlinking = false;
  bool m_bBlinkMoved = false;
  plTime m_BlinkStart;
  plVec3 m_vBlinkTarget = plVec3::MakeZero();

  bool m_bAiming = false;
  bool m_bValidTarget = false;
  bool m_bArcHit = false;
  plVec3 m_vTarget = plVec3::MakeZero();
  plVec3 m_vTargetNormal = plVec3::MakeAxisZ();
  plHybridArray<plVec3, 64> m_ArcPoints;
};
