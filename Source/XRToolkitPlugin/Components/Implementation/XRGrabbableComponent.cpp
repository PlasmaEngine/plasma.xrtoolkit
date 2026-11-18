#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Messages/CommonMessages.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <XRToolkitPlugin/Components/XRGrabbableComponent.h>
#include <XRToolkitPlugin/Components/XRHandComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plXRGrabbableComponent, 1, plComponentMode::Dynamic)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ENUM_MEMBER_PROPERTY("GrabMode", plXRGrabMode, m_GrabMode),
    PL_ENUM_MEMBER_PROPERTY("Hand", plXRGrabHand, m_Hand),
    PL_MEMBER_PROPERTY("GrabRadius", m_fGrabRadius)->AddAttributes(new plDefaultValueAttribute(0.15f), new plClampValueAttribute(0.0f, 5.0f), new plSuffixAttribute(" m")),
    PL_MEMBER_PROPERTY("SnapPosition", m_vSnapPosition),
    PL_MEMBER_PROPERTY("SnapRotation", m_qSnapRotation),
    PL_MEMBER_PROPERTY("ThrowVelocityScale", m_fThrowVelocityScale)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.0f, 10.0f)),
    PL_MEMBER_PROPERTY("HapticOnGrab", m_bHapticOnGrab)->AddAttributes(new plDefaultValueAttribute(true)),
    PL_MEMBER_PROPERTY("HapticAmplitude", m_fHapticAmplitude)->AddAttributes(new plDefaultValueAttribute(0.5f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_MEMBER_PROPERTY("HapticDuration", m_HapticDuration)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromMilliseconds(50)), new plClampValueAttribute(plTime::MakeZero(), plTime::MakeFromSeconds(2.0))),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(IsGrabbed),
    PL_SCRIPT_FUNCTION_PROPERTY(Drop),
  }
  PL_END_FUNCTIONS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("XR"),
    new plSphereVisualizerAttribute("GrabRadius", plColor::LightSkyBlue),
    new plSphereManipulatorAttribute("GrabRadius"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plXRGrabbableComponent::plXRGrabbableComponent() = default;
plXRGrabbableComponent::~plXRGrabbableComponent() = default;

void plXRGrabbableComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_GrabMode;
  s << m_Hand;
  s << m_fGrabRadius;
  s << m_vSnapPosition;
  s << m_qSnapRotation;
  s << m_fThrowVelocityScale;
  s << m_bHapticOnGrab;
  s << m_fHapticAmplitude;
  s << m_HapticDuration;
}

void plXRGrabbableComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_GrabMode;
  s >> m_Hand;
  s >> m_fGrabRadius;
  s >> m_vSnapPosition;
  s >> m_qSnapRotation;
  s >> m_fThrowVelocityScale;
  s >> m_bHapticOnGrab;
  s >> m_fHapticAmplitude;
  s >> m_HapticDuration;
}

void plXRGrabbableComponent::OnDeactivated()
{
  Drop();

  SUPER::OnDeactivated();
}

plGameObjectHandle plXRGrabbableComponent::GetGrabbingHandObject() const
{
  const plXRHandComponent* pHand = nullptr;
  if (GetWorld()->TryGetComponent(m_hHand, pHand))
    return pHand->GetOwner()->GetHandle();

  return plGameObjectHandle();
}

void plXRGrabbableComponent::Drop()
{
  plXRHandComponent* pHand = nullptr;
  if (GetWorld()->TryGetComponent(m_hHand, pHand))
  {
    pHand->Release(false);
  }

  m_hHand.Invalidate();
}

bool plXRGrabbableComponent::CanBeGrabbedBy(plXRDeviceType::Enum controller) const
{
  if (!IsActiveAndInitialized() || IsGrabbed())
    return false;

  if (m_Hand == plXRGrabHand::Left)
    return controller == plXRDeviceType::LeftController;

  if (m_Hand == plXRGrabHand::Right)
    return controller == plXRDeviceType::RightController;

  return true;
}

plComponent* plXRGrabbableComponent::FindPhysicsActor()
{
  for (plComponent* pComponent : GetOwner()->GetComponents())
  {
    const plAbstractProperty* pProp = pComponent->GetDynamicRTTI()->FindPropertyByName("Kinematic");
    if (pProp != nullptr && pProp->GetCategory() == plPropertyCategory::Member && pProp->GetSpecificType() == plGetStaticRTTI<bool>())
      return pComponent;
  }

  return nullptr;
}

void plXRGrabbableComponent::SendEvent(const plHashedString& sEvent, plXRHandComponent* pHand, plXRDeviceType::Enum controller)
{
  plMsgGenericEvent msg;
  msg.m_sMessage = sEvent;
  msg.m_Value = controller == plXRDeviceType::LeftController ? "Left" : "Right";

  GetOwner()->SendEventMessage(msg, pHand);
}

void plXRGrabbableComponent::MakeKinematic()
{
  m_hPhysicsActor.Invalidate();
  if (plComponent* pActor = FindPhysicsActor())
  {
    const auto* pProp = static_cast<const plAbstractMemberProperty*>(pActor->GetDynamicRTTI()->FindPropertyByName("Kinematic"));
    m_bActorWasKinematic = plReflectionUtils::GetMemberPropertyValue(pProp, pActor).ConvertTo<bool>();
    plReflectionUtils::SetMemberPropertyValue(pProp, pActor, true);
    m_hPhysicsActor = pActor->GetHandle();
  }
}

void plXRGrabbableComponent::RestorePhysics()
{
  plComponent* pActor = nullptr;
  if (GetWorld()->TryGetComponent(m_hPhysicsActor, pActor))
  {
    const auto* pProp = static_cast<const plAbstractMemberProperty*>(pActor->GetDynamicRTTI()->FindPropertyByName("Kinematic"));
    plReflectionUtils::SetMemberPropertyValue(pProp, pActor, m_bActorWasKinematic);
  }

  m_hPhysicsActor.Invalidate();
}

bool plXRGrabbableComponent::HasDynamicActor()
{
  plComponent* pActor = nullptr;
  if (GetWorld()->TryGetComponent(m_hPhysicsActor, pActor))
    return !m_bActorWasKinematic;

  pActor = FindPhysicsActor();
  if (pActor == nullptr)
    return false;

  const auto* pProp = static_cast<const plAbstractMemberProperty*>(pActor->GetDynamicRTTI()->FindPropertyByName("Kinematic"));
  return !plReflectionUtils::GetMemberPropertyValue(pProp, pActor).ConvertTo<bool>();
}

bool plXRGrabbableComponent::BeginFlight()
{
  if (IsGrabbed() || !HasDynamicActor())
    return false;

  MakeKinematic();
  return true;
}

void plXRGrabbableComponent::EndFlight()
{
  RestorePhysics();
}

void plXRGrabbableComponent::OnGrabbed(plXRHandComponent* pHand, plXRDeviceType::Enum controller)
{
  m_hHand = pHand->GetHandle();

  // a held physics actor follows the hand kinematically; a launched object is already kinematic and keeps its own setting to restore
  if (m_hPhysicsActor.IsInvalidated())
  {
    MakeKinematic();
  }

  if (m_bHapticOnGrab)
  {
    plXRToolkit::HapticPulse(controller, m_HapticDuration, m_fHapticAmplitude);
  }

  SendEvent(plMakeHashedString("XRGrab"), pHand, controller);
}

void plXRGrabbableComponent::OnReleased(plXRHandComponent* pHand, plXRDeviceType::Enum controller)
{
  m_hHand.Invalidate();

  RestorePhysics();

  SendEvent(plMakeHashedString("XRRelease"), pHand, controller);
}
