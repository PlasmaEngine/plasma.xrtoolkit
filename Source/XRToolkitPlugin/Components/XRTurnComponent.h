#pragma once

#include <Core/World/Component.h>
#include <Core/World/World.h>
#include <XRToolkitPlugin/Core/XRToolkitTypes.h>

class plXRTurnComponent;

using plXRTurnComponentManager = plXRToolkitComponentManager<plXRTurnComponent>;

/// \brief Turns the player with a controller's thumbstick by rotating the world's plStageSpaceComponent around the headset.
///
/// Snap turns by Snap Angle each time the stick is pushed past Snap Threshold; the stick has to come back towards the centre before the
/// next turn. Smooth turns at Smooth Speed, scaled by how far the stick is pushed beyond the dead zone. The rotation pivots around the
/// headset's position, so the player's view turns on the spot. While turning smoothly, a vignette narrows the view (plXRScreenEffects)
/// in proportion to the turn rate.
class PL_XRTOOLKIT_DLL plXRTurnComponent : public plComponent
{
  PL_DECLARE_COMPONENT_TYPE(plXRTurnComponent, plComponent, plXRTurnComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnDeactivated() override;

  //////////////////////////////////////////////////////////////////////////
  // plXRTurnComponent

public:
  plXRTurnComponent();
  ~plXRTurnComponent();

  static constexpr float UpdatePriority = plXRToolkit::UpdatePriorityStage;

  plEnum<plXRTurnMode> m_Mode;                                // [ property ]

  /// Which controller's stick turns. Auto: the tracked controller, else the right one.
  plEnum<plXRHandSide> m_Hand;                                // [ property ]

  /// Input actions of a plInputComponent on this object or a parent. Empty: the input slots are read directly.
  plString m_sTurnLeftAction;                                 // [ property ]
  plString m_sTurnRightAction;                                // [ property ]
  /// Input slots read when no action is used. Empty: xr_hand_<left|right>_primary_analog_stick_negx / _posx.
  plString m_sTurnLeftSlot;                                   // [ property ]
  plString m_sTurnRightSlot;                                  // [ property ]

  plAngle m_SnapAngle = plAngle::MakeFromDegree(45.0f);       // [ property ]
  /// Stick deflection that triggers a snap turn. The stick has to drop below half of it before the next.
  float m_fSnapThreshold = 0.7f;                              // [ property ]

  /// Turn rate at full deflection, per second.
  plAngle m_SmoothSpeed = plAngle::MakeFromDegree(90.0f);     // [ property ]
  /// Stick deflection below which smooth turning does nothing.
  float m_fDeadZone = 0.15f;                                  // [ property ]
  /// Comfort vignette at full smooth-turn speed (0 to 1), scaled down with the turn rate. 0 turns it off.
  float m_fVignette = 0.5f;                                   // [ property ]
  /// How quickly the vignette follows the turn rate, in vignette strength per second.
  float m_fVignetteSpeed = 4.0f;                              // [ property ]

  /// \brief Turns the player by \a fDegrees around the headset. Positive turns right.
  void TurnBy(float fDegrees); // [ scriptable ]

  /// \brief Called by the component manager every frame while simulating.
  void Update();

private:
  /// Moves the vignette towards fTarget and hands it to plXRScreenEffects.
  void UpdateVignette(float fTarget);

  bool m_bSnapArmed = true;
  float m_fCurrentVignette = 0.0f;
};
