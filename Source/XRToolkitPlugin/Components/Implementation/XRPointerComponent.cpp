#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Messages/CommonMessages.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <XRToolkitPlugin/Components/XRPointerComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plXRPointerComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ENUM_MEMBER_PROPERTY("Hand", plXRHandSide, m_Hand),
    PL_MEMBER_PROPERTY("MaxDistance", m_fMaxDistance)->AddAttributes(new plDefaultValueAttribute(10.0f), new plClampValueAttribute(0.1f, 1000.0f), new plSuffixAttribute(" m")),
    PL_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer")),
    PL_MEMBER_PROPERTY("ShowBeam", m_bShowBeam)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("ShowBeamWithoutHit", m_bShowBeamWithoutHit)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("BeamColor", m_BeamColor)->AddAttributes(new plDefaultValueAttribute(plColor(0.3f, 0.8f, 1.0f))),
    PL_MEMBER_PROPERTY("HitColor", m_HitColor)->AddAttributes(new plDefaultValueAttribute(plColor::White)),
    PL_MEMBER_PROPERTY("HitDotSize", m_fHitDotSize)->AddAttributes(new plDefaultValueAttribute(0.015f), new plClampValueAttribute(0.0f, 1.0f), new plSuffixAttribute(" m")),
    PL_MEMBER_PROPERTY("ClickAction", m_sClickAction),
    PL_MEMBER_PROPERTY("ClickSlot", m_sClickSlot),
    PL_MEMBER_PROPERTY("ClickThreshold", m_fClickThreshold)->AddAttributes(new plDefaultValueAttribute(0.7f), new plClampValueAttribute(0.05f, 1.0f)),
    PL_MEMBER_PROPERTY("HapticOnHover", m_bHapticOnHover)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("HoverHapticAmplitude", m_fHoverHapticAmplitude)->AddAttributes(new plDefaultValueAttribute(0.2f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_MEMBER_PROPERTY("HoverHapticDuration", m_HoverHapticDuration)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromMilliseconds(20)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(2.0))),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(HasHit),
    PL_SCRIPT_FUNCTION_PROPERTY(GetHitPoint),
    PL_SCRIPT_FUNCTION_PROPERTY(GetHitNormal),
    PL_SCRIPT_FUNCTION_PROPERTY(GetHitDistance),
    PL_SCRIPT_FUNCTION_PROPERTY(IsClickDown),
  }
  PL_END_FUNCTIONS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("XR"),
    new plDirectionVisualizerAttribute(plBasisAxis::PositiveX, 0.5f, plColor::LightSkyBlue),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plXRPointerComponent::plXRPointerComponent() = default;
plXRPointerComponent::~plXRPointerComponent() = default;

void plXRPointerComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_Hand;
  s << m_fMaxDistance;
  s << m_uiCollisionLayer;
  s << m_bShowBeam;
  s << m_bShowBeamWithoutHit;
  s << m_BeamColor;
  s << m_HitColor;
  s << m_fHitDotSize;
  s << m_sClickAction;
  s << m_sClickSlot;
  s << m_fClickThreshold;
  s << m_bHapticOnHover;
  s << m_fHoverHapticAmplitude;
  s << m_HoverHapticDuration;
}

void plXRPointerComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_Hand;
  s >> m_fMaxDistance;
  s >> m_uiCollisionLayer;
  s >> m_bShowBeam;
  s >> m_bShowBeamWithoutHit;
  s >> m_BeamColor;
  s >> m_HitColor;
  s >> m_fHitDotSize;
  s >> m_sClickAction;
  s >> m_sClickSlot;
  s >> m_fClickThreshold;
  s >> m_bHapticOnHover;
  s >> m_fHoverHapticAmplitude;
  s >> m_HoverHapticDuration;
}

void plXRPointerComponent::OnDeactivated()
{
  ClearHit();
  m_bClickDown = false;

  SUPER::OnDeactivated();
}

void plXRPointerComponent::SendPointerEvent(plGameObjectHandle hTarget, const plHashedString& sEvent)
{
  plGameObject* pTarget = nullptr;
  if (!GetWorld()->TryGetObject(hTarget, pTarget))
    return;

  plMsgGenericEvent msg;
  msg.m_sMessage = sEvent;
  msg.m_Value = m_vHitPoint;

  pTarget->SendEventMessage(msg, this);
}

void plXRPointerComponent::ClearHit()
{
  if (!m_hHitObject.IsInvalidated())
  {
    SendPointerEvent(m_hHitObject, plMakeHashedString("XRPointerExit"));
  }

  m_hHitObject.Invalidate();
  m_bHasHit = false;
}

void plXRPointerComponent::Update()
{
  const plTransform aim = plXRToolkit::GetCurrentGlobalTransform(GetOwner());
  const plVec3 vStart = aim.m_vPosition;
  plVec3 vDir = aim.m_qRotation * plVec3::MakeAxisX();
  vDir.NormalizeIfNotZero(plVec3::MakeAxisX()).IgnoreResult();

  const plXRDeviceType::Enum controller = plXRToolkit::ResolveController(GetOwner(), m_Hand, plXRDeviceType::RightController);

  // ray cast
  plPhysicsCastResult hit;
  bool bHit = false;
  if (const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>())
  {
    plBitflags<plPhysicsShapeType> shapeTypes = plPhysicsShapeType::Default;
    shapeTypes.Remove(plPhysicsShapeType::Trigger);

    bHit = pPhysics->Raycast(hit, vStart, vDir, m_fMaxDistance, plPhysicsQueryParameters(m_uiCollisionLayer, shapeTypes));
  }

  const plGameObjectHandle hNewObject = bHit ? hit.m_hShapeObject : plGameObjectHandle();

  const bool bObjectChanged = hNewObject != m_hHitObject;

  // the exit event still reports the last point on the old object
  if (bObjectChanged)
  {
    ClearHit();
  }

  if (bHit)
  {
    m_vHitPoint = hit.m_vPosition;
    m_vHitNormal = hit.m_vNormal;
    m_fHitDistance = hit.m_fDistance;
  }

  if (bObjectChanged)
  {
    if (bHit)
    {
      m_hHitObject = hNewObject;
      SendPointerEvent(m_hHitObject, plMakeHashedString("XRPointerEnter"));

      if (m_bHapticOnHover)
      {
        plXRToolkit::HapticPulse(controller, m_HoverHapticDuration, m_fHoverHapticAmplitude);
      }
    }
  }

  m_bHasHit = bHit;

  // click
  const float fClick = plXRToolkit::ReadInput(GetOwner(), m_sClickAction, m_sClickSlot, controller, "trigger_value");
  if (!m_bClickDown && fClick >= m_fClickThreshold)
  {
    m_bClickDown = true;

    if (m_bHasHit)
    {
      SendPointerEvent(m_hHitObject, plMakeHashedString("XRPointerClick"));
    }
  }
  else if (m_bClickDown && fClick < m_fClickThreshold * 0.5f)
  {
    m_bClickDown = false;
  }

  // beam, drawn in the world so that both eyes see it
  if (m_bShowBeam && (m_bHasHit || m_bShowBeamWithoutHit))
  {
    const plVec3 vEnd = m_bHasHit ? m_vHitPoint : vStart + vDir * m_fMaxDistance;

    plDebugRenderer::Line line(vStart, vEnd);
    line.m_startColor = m_BeamColor;
    line.m_endColor = m_bHasHit ? m_BeamColor : plColor(m_BeamColor.r, m_BeamColor.g, m_BeamColor.b, 0.0f);
    plDebugRenderer::DrawLines(GetWorld(), plArrayPtr<const plDebugRenderer::Line>(&line, 1), plColor::White);

    if (m_bHasHit && m_fHitDotSize > 0.0f)
    {
      plDebugRenderer::DrawLineSphere(GetWorld(), plBoundingSphere::MakeFromCenterAndRadius(m_vHitPoint, m_fHitDotSize), m_HitColor);
      plXRToolkit::DrawCircle(GetWorld(), m_vHitPoint + m_vHitNormal * 0.002f, m_vHitNormal, m_fHitDotSize * 2.0f, m_HitColor);
    }
  }
}
