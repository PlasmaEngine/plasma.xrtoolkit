#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Messages/CommonMessages.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <GameEngine/XR/StageSpaceComponent.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <RendererCore/Pipeline/XRScreenEffects.h>
#include <XRToolkitPlugin/Components/XRTeleportComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plXRTeleportComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ENUM_MEMBER_PROPERTY("Hand", plXRHandSide, m_Hand),
    PL_MEMBER_PROPERTY("ActivateAction", m_sActivateAction),
    PL_MEMBER_PROPERTY("ActivateSlot", m_sActivateSlot),
    PL_MEMBER_PROPERTY("ActivateThreshold", m_fActivateThreshold)->AddAttributes(new plDefaultValueAttribute(0.6f), new plClampValueAttribute(0.05f, 1.0f)),
    PL_MEMBER_PROPERTY("LaunchVelocity", m_fLaunchVelocity)->AddAttributes(new plDefaultValueAttribute(7.0f), new plClampValueAttribute(0.5f, 100.0f), new plSuffixAttribute(" m/s")),
    PL_MEMBER_PROPERTY("Gravity", m_fGravity)->AddAttributes(new plDefaultValueAttribute(9.81f), new plClampValueAttribute(0.0f, 100.0f), new plSuffixAttribute(" m/s^2")),
    PL_MEMBER_PROPERTY("MaxSteps", m_uiMaxSteps)->AddAttributes(new plDefaultValueAttribute(40), new plClampValueAttribute(2, 256)),
    PL_MEMBER_PROPERTY("StepTime", m_fStepTime)->AddAttributes(new plDefaultValueAttribute(0.03f), new plClampValueAttribute(0.005f, 0.5f), new plSuffixAttribute(" s")),
    PL_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer")),
    PL_MEMBER_PROPERTY("MaxSlope", m_MaxSlope)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(30.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(0.0f), plAngle::MakeFromDegree(89.0f))),
    PL_MEMBER_PROPERTY("ShowArc", m_bShowArc)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("ValidColor", m_ValidColor)->AddAttributes(new plDefaultValueAttribute(plColor::LimeGreen)),
    PL_MEMBER_PROPERTY("InvalidColor", m_InvalidColor)->AddAttributes(new plDefaultValueAttribute(plColor::Crimson)),
    PL_MEMBER_PROPERTY("TargetRadius", m_fTargetRadius)->AddAttributes(new plDefaultValueAttribute(0.3f), new plClampValueAttribute(0.0f, 5.0f), new plSuffixAttribute(" m")),
    PL_MEMBER_PROPERTY("BlinkDuration", m_BlinkDuration)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.2)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(2.0))),
    PL_MEMBER_PROPERTY("BlinkColor", m_BlinkColor)->AddAttributes(new plDefaultValueAttribute(plColor::Black)),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(IsAiming),
    PL_SCRIPT_FUNCTION_PROPERTY(HasValidTarget),
    PL_SCRIPT_FUNCTION_PROPERTY(GetTargetPosition),
    PL_SCRIPT_FUNCTION_PROPERTY(TeleportTo, In, "Target"),
  }
  PL_END_FUNCTIONS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("XR"),
    new plDirectionVisualizerAttribute(plBasisAxis::PositiveX, 0.5f, plColor::LimeGreen),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plXRTeleportComponent::plXRTeleportComponent() = default;
plXRTeleportComponent::~plXRTeleportComponent() = default;

void plXRTeleportComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_Hand;
  s << m_sActivateAction;
  s << m_sActivateSlot;
  s << m_fActivateThreshold;
  s << m_fLaunchVelocity;
  s << m_fGravity;
  s << m_uiMaxSteps;
  s << m_fStepTime;
  s << m_uiCollisionLayer;
  s << m_MaxSlope;
  s << m_bShowArc;
  s << m_ValidColor;
  s << m_InvalidColor;
  s << m_fTargetRadius;
  s << m_BlinkDuration;
  s << m_BlinkColor;
}

void plXRTeleportComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_Hand;
  s >> m_sActivateAction;
  s >> m_sActivateSlot;
  s >> m_fActivateThreshold;
  s >> m_fLaunchVelocity;
  s >> m_fGravity;
  s >> m_uiMaxSteps;
  s >> m_fStepTime;
  s >> m_uiCollisionLayer;
  s >> m_MaxSlope;
  s >> m_bShowArc;
  s >> m_ValidColor;
  s >> m_InvalidColor;
  s >> m_fTargetRadius;
  s >> m_BlinkDuration;
  s >> m_BlinkColor;
}

void plXRTeleportComponent::OnDeactivated()
{
  m_bAiming = false;
  m_bValidTarget = false;
  m_ArcPoints.Clear();

  // a blink cut short still delivers the move it was hiding
  if (m_bBlinking && !m_bBlinkMoved)
  {
    MoveStageTo(m_vBlinkTarget);
  }
  EndBlink();

  SUPER::OnDeactivated();
}

void plXRTeleportComponent::TraceArc(const plTransform& aim)
{
  m_ArcPoints.Clear();
  m_bArcHit = false;
  m_bValidTarget = false;

  const plVec3 vUp = plVec3::MakeAxisZ();
  const plVec3 vGravity = -vUp * m_fGravity;
  const float dt = m_fStepTime;

  plVec3 vDir = aim.m_qRotation * plVec3::MakeAxisX();
  vDir.NormalizeIfNotZero(plVec3::MakeAxisX()).IgnoreResult();

  plVec3 vPos = aim.m_vPosition;
  plVec3 vVel = vDir * m_fLaunchVelocity;
  m_ArcPoints.PushBack(vPos);

  const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>();

  plBitflags<plPhysicsShapeType> shapeTypes = plPhysicsShapeType::Default;
  shapeTypes.Remove(plPhysicsShapeType::Trigger);
  const plPhysicsQueryParameters params(m_uiCollisionLayer, shapeTypes);

  for (plUInt32 i = 0; i < m_uiMaxSteps; ++i)
  {
    const plVec3 vNext = vPos + vVel * dt + vGravity * (0.5f * dt * dt);
    vVel += vGravity * dt;

    plVec3 vSegment = vNext - vPos;
    const float fLength = vSegment.GetLength();

    if (pPhysics != nullptr && fLength > 0.0001f)
    {
      vSegment /= fLength;

      plPhysicsCastResult hit;
      if (pPhysics->Raycast(hit, vPos, vSegment, fLength, params))
      {
        m_ArcPoints.PushBack(hit.m_vPosition);
        m_bArcHit = true;
        m_vTarget = hit.m_vPosition;
        m_vTargetNormal = hit.m_vNormal;
        m_bValidTarget = hit.m_vNormal.GetAngleBetween(vUp) <= m_MaxSlope;
        return;
      }
    }

    m_ArcPoints.PushBack(vNext);
    vPos = vNext;
  }

  m_vTarget = vPos;
  m_vTargetNormal = vUp;
}

void plXRTeleportComponent::DrawArc() const
{
  if (!m_bShowArc || m_ArcPoints.GetCount() < 2)
    return;

  const plColor color = m_bValidTarget ? m_ValidColor : m_InvalidColor;

  plHybridArray<plDebugRenderer::Line, 64> lines;
  for (plUInt32 i = 1; i < m_ArcPoints.GetCount(); ++i)
  {
    lines.PushBack(plDebugRenderer::Line(m_ArcPoints[i - 1], m_ArcPoints[i]));
  }

  plDebugRenderer::DrawLines(GetWorld(), lines, color);

  if (m_bArcHit && m_fTargetRadius > 0.0f)
  {
    const plVec3 vRingCenter = m_vTarget + m_vTargetNormal * 0.01f;
    plXRToolkit::DrawCircle(GetWorld(), vRingCenter, m_vTargetNormal, m_fTargetRadius, color);
    plXRToolkit::DrawCircle(GetWorld(), vRingCenter, m_vTargetNormal, m_fTargetRadius * 0.6f, color);
  }
}

void plXRTeleportComponent::TeleportTo(const plVec3& vTarget)
{
  if (m_BlinkDuration <= plTime::MakeZero() || !IsActiveAndSimulating())
  {
    MoveStageTo(vTarget);
    return;
  }

  // a second teleport during a blink retargets it; one past the darkest point starts a new blink
  if (m_bBlinking && m_bBlinkMoved)
  {
    EndBlink();
  }

  if (!m_bBlinking)
  {
    m_bBlinking = true;
    m_bBlinkMoved = false;
    m_BlinkStart = GetWorld()->GetClock().GetAccumulatedTime();
  }

  m_vBlinkTarget = vTarget;
}

bool plXRTeleportComponent::UpdateBlink()
{
  const double fProgress = (GetWorld()->GetClock().GetAccumulatedTime() - m_BlinkStart).GetSeconds() / m_BlinkDuration.GetSeconds();

  if (fProgress >= 0.5 && !m_bBlinkMoved)
  {
    m_bBlinkMoved = true;
    MoveStageTo(m_vBlinkTarget);
  }

  if (fProgress >= 1.0)
  {
    EndBlink();
    return false;
  }

  // up to fully covered at the halfway point, then back
  const float fFade = static_cast<float>(fProgress < 0.5 ? fProgress * 2.0 : (1.0 - fProgress) * 2.0);
  plXRScreenEffects::SetFade(m_BlinkColor, fFade);
  return true;
}

void plXRTeleportComponent::EndBlink()
{
  if (m_bBlinking)
  {
    plXRScreenEffects::SetFade(m_BlinkColor, 0.0f);
  }

  m_bBlinking = false;
  m_bBlinkMoved = false;
}

void plXRTeleportComponent::MoveStageTo(const plVec3& vTarget)
{
  plStageSpaceComponent* pStage = plXRToolkit::FindStageSpace(GetWorld());
  if (pStage == nullptr)
  {
    plLog::Warning("plXRTeleportComponent: the world has no plStageSpaceComponent to move.");
    return;
  }

  plGameObject* pStageObject = pStage->GetOwner();
  const plTransform stage = plXRToolkit::GetCurrentGlobalTransform(pStageObject);

  // the floor point under the head, in the world
  plTransform head;
  plXRToolkit::GetHeadPoseInStage(head);
  const plVec3 vHeadFloorWorld = stage.TransformPosition(plVec3(head.m_vPosition.x, head.m_vPosition.y, 0.0f));

  plTransform newStage = stage;
  newStage.m_vPosition += vTarget - vHeadFloorWorld;
  pStageObject->SetGlobalTransform(newStage);

  plMsgGenericEvent msg;
  msg.m_sMessage = plMakeHashedString("XRTeleport");
  msg.m_Value = vTarget;
  GetOwner()->SendEventMessage(msg, this);
}

void plXRTeleportComponent::Update()
{
  // no aiming while the view is blinked out
  if (m_bBlinking && UpdateBlink())
    return;

  const plXRDeviceType::Enum controller = plXRToolkit::ResolveController(GetOwner(), m_Hand, plXRDeviceType::LeftController);
  const float fValue = plXRToolkit::ReadInput(GetOwner(), m_sActivateAction, m_sActivateSlot, controller, "primary_analog_stick_posy");

  if (!m_bAiming)
  {
    if (fValue < m_fActivateThreshold)
      return;

    m_bAiming = true;
  }
  else if (fValue < m_fActivateThreshold * 0.5f)
  {
    m_bAiming = false;
    m_ArcPoints.Clear();

    if (m_bValidTarget)
    {
      m_bValidTarget = false;
      TeleportTo(m_vTarget);
    }

    return;
  }

  TraceArc(plXRToolkit::GetCurrentGlobalTransform(GetOwner()));
  DrawArc();
}
