#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/World/CoordinateSystem.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GameEngine/XR/StageSpaceComponent.h>
#include <RendererCore/Pipeline/XRScreenEffects.h>
#include <XRToolkitPlugin/Components/XRTurnComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plXRTurnComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ENUM_MEMBER_PROPERTY("Mode", plXRTurnMode, m_Mode),
    PL_ENUM_MEMBER_PROPERTY("Hand", plXRHandSide, m_Hand),
    PL_MEMBER_PROPERTY("TurnLeftAction", m_sTurnLeftAction),
    PL_MEMBER_PROPERTY("TurnRightAction", m_sTurnRightAction),
    PL_MEMBER_PROPERTY("TurnLeftSlot", m_sTurnLeftSlot),
    PL_MEMBER_PROPERTY("TurnRightSlot", m_sTurnRightSlot),
    PL_MEMBER_PROPERTY("SnapAngle", m_SnapAngle)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(45.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(1.0f), plAngle::MakeFromDegree(180.0f))),
    PL_MEMBER_PROPERTY("SnapThreshold", m_fSnapThreshold)->AddAttributes(new plDefaultValueAttribute(0.7f), new plClampValueAttribute(0.05f, 1.0f)),
    PL_MEMBER_PROPERTY("SmoothSpeed", m_SmoothSpeed)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(90.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(1.0f), plAngle::MakeFromDegree(720.0f)), new plSuffixAttribute("/s")),
    PL_MEMBER_PROPERTY("DeadZone", m_fDeadZone)->AddAttributes(new plDefaultValueAttribute(0.15f), new plClampValueAttribute(0.0f, 0.95f)),
    PL_MEMBER_PROPERTY("Vignette", m_fVignette)->AddAttributes(new plDefaultValueAttribute(0.5f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_MEMBER_PROPERTY("VignetteSpeed", m_fVignetteSpeed)->AddAttributes(new plDefaultValueAttribute(4.0f), new plClampValueAttribute(0.1f, 100.0f), new plSuffixAttribute("/s")),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(TurnBy, In, "Degrees"),
  }
  PL_END_FUNCTIONS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("XR"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plXRTurnComponent::plXRTurnComponent() = default;
plXRTurnComponent::~plXRTurnComponent() = default;

void plXRTurnComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_Mode;
  s << m_Hand;
  s << m_sTurnLeftAction;
  s << m_sTurnRightAction;
  s << m_sTurnLeftSlot;
  s << m_sTurnRightSlot;
  s << m_SnapAngle;
  s << m_fSnapThreshold;
  s << m_SmoothSpeed;
  s << m_fDeadZone;
  s << m_fVignette;
  s << m_fVignetteSpeed;
}

void plXRTurnComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_Mode;
  s >> m_Hand;
  s >> m_sTurnLeftAction;
  s >> m_sTurnRightAction;
  s >> m_sTurnLeftSlot;
  s >> m_sTurnRightSlot;
  s >> m_SnapAngle;
  s >> m_fSnapThreshold;
  s >> m_SmoothSpeed;
  s >> m_fDeadZone;
  s >> m_fVignette;
  s >> m_fVignetteSpeed;
}

void plXRTurnComponent::OnDeactivated()
{
  m_bSnapArmed = true;

  if (m_fCurrentVignette > 0.0f)
  {
    m_fCurrentVignette = 0.0f;
    plXRScreenEffects::SetVignette(0.0f);
  }

  SUPER::OnDeactivated();
}

void plXRTurnComponent::TurnBy(float fDegrees)
{
  if (plMath::IsZero(fDegrees, 0.0001f))
    return;

  plStageSpaceComponent* pStage = plXRToolkit::FindStageSpace(GetWorld());
  if (pStage == nullptr)
    return;

  plGameObject* pStageObject = pStage->GetOwner();
  const plTransform stage = plXRToolkit::GetCurrentGlobalTransform(pStageObject);

  plTransform head;
  plXRToolkit::GetHeadPoseInStage(head);
  const plVec3 vPivot = stage.TransformPosition(head.m_vPosition);

  // rotating forward towards right is a positive or a negative angle around up, depending on the world's handedness
  plCoordinateSystem cs;
  GetWorld()->GetCoordinateSystem(vPivot, cs);
  const float fSign = cs.m_vForwardDir.CrossRH(cs.m_vRightDir).Dot(cs.m_vUpDir) >= 0.0f ? 1.0f : -1.0f;

  const plQuat qTurn = plQuat::MakeFromAxisAndAngle(cs.m_vUpDir, plAngle::MakeFromDegree(fDegrees * fSign));

  plTransform newStage = stage;
  newStage.m_vPosition = vPivot + qTurn * (stage.m_vPosition - vPivot);
  newStage.m_qRotation = qTurn * stage.m_qRotation;
  newStage.m_qRotation.Normalize();

  pStageObject->SetGlobalTransform(newStage);
}

void plXRTurnComponent::Update()
{
  const plXRDeviceType::Enum controller = plXRToolkit::ResolveController(GetOwner(), m_Hand, plXRDeviceType::RightController);
  const float fRight = plXRToolkit::ReadInput(GetOwner(), m_sTurnRightAction, m_sTurnRightSlot, controller, "primary_analog_stick_posx");
  const float fLeft = plXRToolkit::ReadInput(GetOwner(), m_sTurnLeftAction, m_sTurnLeftSlot, controller, "primary_analog_stick_negx");
  const float fStick = fRight - fLeft;

  if (m_Mode == plXRTurnMode::Snap)
  {
    UpdateVignette(0.0f);

    if (!m_bSnapArmed)
    {
      m_bSnapArmed = plMath::Abs(fStick) < m_fSnapThreshold * 0.5f;
      return;
    }

    if (plMath::Abs(fStick) >= m_fSnapThreshold)
    {
      m_bSnapArmed = false;
      TurnBy(plMath::Sign(fStick) * m_SnapAngle.GetDegree());
    }

    return;
  }

  // smooth: the deflection beyond the dead zone is rescaled to 0..1
  const float fAbs = plMath::Abs(fStick);
  if (fAbs <= m_fDeadZone)
  {
    UpdateVignette(0.0f);
    return;
  }

  const float fAmount = plMath::Min(1.0f, (fAbs - m_fDeadZone) / (1.0f - m_fDeadZone));
  const float fSeconds = static_cast<float>(GetWorld()->GetClock().GetTimeDiff().GetSeconds());
  TurnBy(plMath::Sign(fStick) * fAmount * m_SmoothSpeed.GetDegree() * fSeconds);
  UpdateVignette(fAmount * m_fVignette);
}

void plXRTurnComponent::UpdateVignette(float fTarget)
{
  if (m_fCurrentVignette == fTarget)
    return;

  const float fStep = m_fVignetteSpeed * static_cast<float>(GetWorld()->GetClock().GetTimeDiff().GetSeconds());
  if (m_fCurrentVignette < fTarget)
    m_fCurrentVignette = plMath::Min(fTarget, m_fCurrentVignette + fStep);
  else
    m_fCurrentVignette = plMath::Max(fTarget, m_fCurrentVignette - fStep);

  plXRScreenEffects::SetVignette(m_fCurrentVignette);
}
