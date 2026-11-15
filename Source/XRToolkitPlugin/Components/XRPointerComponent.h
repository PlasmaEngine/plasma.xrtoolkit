#pragma once

#include <Core/World/Component.h>
#include <Core/World/World.h>
#include <Foundation/Math/Color.h>
#include <XRToolkitPlugin/Core/XRToolkitTypes.h>

class plXRPointerComponent;

using plXRPointerComponentManager = plXRToolkitComponentManager<plXRPointerComponent>;

/// \brief A laser pointer along the owner's forward axis (+X), usually on the object tracking a controller's aim pose.
///
/// Casts a ray up to Max Distance every frame and draws the beam and a dot where it hits. The hit object receives plMsgGenericEvent
/// "XRPointerEnter" when the beam reaches it, "XRPointerExit" when the beam leaves it and "XRPointerClick" when the trigger is pressed
/// while pointing at it, as event messages with the pointer as the sender and the hit point as the value. The current hit stays
/// queryable through GetHitObject() and GetHitPoint(), so UI code can map it onto a panel.
class PL_XRTOOLKIT_DLL plXRPointerComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plXRPointerComponent, plComponent, plXRPointerComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plXRPointerComponent

public:
  plXRPointerComponent();
  ~plXRPointerComponent();

  static constexpr float UpdatePriority = plXRToolkit::UpdatePriorityHands;

  /// Which controller's trigger clicks and which one vibrates. Auto: the tracked controller, else the right one.
  plEnum<plXRHandSide> m_Hand;                                 // [ property ]
  float m_fMaxDistance = 10.0f;                                // [ property ]
  plUInt8 m_uiCollisionLayer = 0;                              // [ property ]

  bool m_bShowBeam = true;                                     // [ property ]
  /// Draws the full-length beam while nothing is hit. Otherwise the beam is only drawn onto a hit.
  bool m_bShowBeamWithoutHit = true;                           // [ property ]
  plColor m_BeamColor = plColor(0.3f, 0.8f, 1.0f);             // [ property ]
  plColor m_HitColor = plColor::White;                         // [ property ]
  /// Radius of the dot drawn at the hit point, in metres.
  float m_fHitDotSize = 0.015f;                                // [ property ]

  /// Input action of a plInputComponent on this object or a parent. Empty: the input slot is read directly.
  plString m_sClickAction;                                     // [ property ]
  /// Input slot read when no action is used. Empty: xr_hand_<left|right>_trigger_value.
  plString m_sClickSlot;                                       // [ property ]
  /// Trigger value at which a click is sent. The trigger has to drop below half of it before the next one.
  float m_fClickThreshold = 0.7f;                              // [ property ]

  bool m_bHapticOnHover = true;                                // [ property ]
  float m_fHoverHapticAmplitude = 0.2f;                        // [ property ]
  plTime m_HoverHapticDuration = plTime::MakeFromMilliseconds(20); // [ property ]

  bool HasHit() const { return m_bHasHit; }                    // [ scriptable ]
  plGameObjectHandle GetHitObject() const { return m_hHitObject; }
  const plVec3& GetHitPoint() const { return m_vHitPoint; }    // [ scriptable ]
  const plVec3& GetHitNormal() const { return m_vHitNormal; }  // [ scriptable ]
  float GetHitDistance() const { return m_fHitDistance; }      // [ scriptable ]

  /// \brief Whether the click input is held down.
  bool IsClickDown() const { return m_bClickDown; }            // [ scriptable ]

  /// \brief Called by the component manager every frame while simulating.
  void Update();

private:
  void SendPointerEvent(plGameObjectHandle hTarget, const plHashedString& sEvent);
  void ClearHit();

  plGameObjectHandle m_hHitObject;
  plVec3 m_vHitPoint = plVec3::MakeZero();
  plVec3 m_vHitNormal = plVec3::MakeAxisZ();
  float m_fHitDistance = 0.0f;
  bool m_bHasHit = false;
  bool m_bClickDown = false;
};
