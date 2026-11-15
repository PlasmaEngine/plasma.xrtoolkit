#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <XRToolkitPlugin/Components/XRGrabbableComponent.h>
#include <XRToolkitPlugin/Components/XRHandComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plXRHandComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ENUM_MEMBER_PROPERTY("Hand", plXRHandSide, m_Hand),
    PL_MEMBER_PROPERTY("Reach", m_fReach)->AddAttributes(new plDefaultValueAttribute(0.08f), new plClampValueAttribute(0.0f, 2.0f), new plSuffixAttribute(" m")),
    PL_MEMBER_PROPERTY("CollisionLayer", m_uiCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer")),
    PL_MEMBER_PROPERTY("GripAction", m_sGripAction),
    PL_MEMBER_PROPERTY("GripSlot", m_sGripSlot),
    PL_MEMBER_PROPERTY("GripThreshold", m_fGripThreshold)->AddAttributes(new plDefaultValueAttribute(0.5f), new plClampValueAttribute(0.05f, 1.0f)),
    PL_MEMBER_PROPERTY("ThrowSamples", m_uiThrowSamples)->AddAttributes(new plDefaultValueAttribute(5), new plClampValueAttribute(2, 16)),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(Grab),
    PL_SCRIPT_FUNCTION_PROPERTY(Release, In, "Throw"),
    PL_SCRIPT_FUNCTION_PROPERTY(IsHolding),
  }
  PL_END_FUNCTIONS;
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("XR"),
    new plSphereVisualizerAttribute("Reach", plColor::LightSkyBlue),
    new plSphereManipulatorAttribute("Reach"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_COMPONENT_TYPE;
// clang-format on

plXRHandComponent::plXRHandComponent() = default;
plXRHandComponent::~plXRHandComponent() = default;

void plXRHandComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s << m_Hand;
  s << m_fReach;
  s << m_uiCollisionLayer;
  s << m_sGripAction;
  s << m_sGripSlot;
  s << m_fGripThreshold;
  s << m_uiThrowSamples;
}

void plXRHandComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  auto& s = inout_stream.GetStream();

  s >> m_Hand;
  s >> m_fReach;
  s >> m_uiCollisionLayer;
  s >> m_sGripAction;
  s >> m_sGripSlot;
  s >> m_fGripThreshold;
  s >> m_uiThrowSamples;
}

void plXRHandComponent::OnDeactivated()
{
  Release(false);
  m_bGripDown = false;
  m_Samples.Clear();

  SUPER::OnDeactivated();
}

plGameObjectHandle plXRHandComponent::GetHeldObject() const
{
  const plXRGrabbableComponent* pGrabbable = nullptr;
  if (GetWorld()->TryGetComponent(m_hHeld, pGrabbable))
    return pGrabbable->GetOwner()->GetHandle();

  return plGameObjectHandle();
}

plXRDeviceType::Enum plXRHandComponent::GetController() const
{
  return plXRToolkit::ResolveController(GetOwner(), m_Hand, plXRDeviceType::RightController);
}

void plXRHandComponent::AddSample(const plVec3& vPosition)
{
  const plUInt32 uiMax = plMath::Clamp<plUInt32>(m_uiThrowSamples, 2, MaxSamples);

  // a jump of a metre within one frame is a teleport or a turn of the play area, not hand movement
  if (!m_Samples.IsEmpty() && (vPosition - m_Samples.PeekBack().m_vPosition).GetLengthSquared() > 1.0f)
  {
    m_Samples.Clear();
  }

  while (m_Samples.GetCount() >= uiMax)
  {
    m_Samples.RemoveAtAndCopy(0);
  }

  Sample& sample = m_Samples.ExpandAndGetRef();
  sample.m_vPosition = vPosition;
  sample.m_Time = GetWorld()->GetClock().GetAccumulatedTime();
}

plVec3 plXRHandComponent::GetHandVelocity() const
{
  if (m_Samples.GetCount() < 2)
    return plVec3::MakeZero();

  const Sample& oldest = m_Samples[0];
  const Sample& newest = m_Samples.PeekBack();
  const double fSeconds = (newest.m_Time - oldest.m_Time).GetSeconds();

  if (fSeconds < 0.0001)
    return plVec3::MakeZero();

  return (newest.m_vPosition - oldest.m_vPosition) / static_cast<float>(fSeconds);
}

plVec3 plXRHandComponent::GetLastFrameVelocity() const
{
  const plUInt32 uiCount = m_Samples.GetCount();
  if (uiCount < 2)
    return plVec3::MakeZero();

  const Sample& previous = m_Samples[uiCount - 2];
  const Sample& newest = m_Samples[uiCount - 1];
  const double fSeconds = (newest.m_Time - previous.m_Time).GetSeconds();

  if (fSeconds < 0.0001)
    return plVec3::MakeZero();

  return (newest.m_vPosition - previous.m_vPosition) / static_cast<float>(fSeconds);
}

plXRGrabbableComponent* plXRHandComponent::FindNearestGrabbable(const plVec3& vHandPosition, plXRDeviceType::Enum controller)
{
  plWorld* pWorld = GetWorld();
  plXRGrabbableComponent* pBest = nullptr;
  float fBestDistance = plMath::MaxValue<float>();

  auto consider = [&](plXRGrabbableComponent* pGrabbable, float fDistance)
  {
    if (fDistance < fBestDistance && pGrabbable->CanBeGrabbedBy(controller))
    {
      pBest = pGrabbable;
      fBestDistance = fDistance;
    }
  };

  // grabbables whose radius contains the hand
  if (plXRGrabbableComponentManager* pManager = pWorld->GetComponentManager<plXRGrabbableComponentManager>())
  {
    for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
    {
      plXRGrabbableComponent* pGrabbable = it;
      if (!pGrabbable->IsActiveAndInitialized())
        continue;

      const float fDistance = (pGrabbable->GetOwner()->GetGlobalPosition() - vHandPosition).GetLength();
      if (fDistance <= pGrabbable->m_fGrabRadius)
      {
        consider(pGrabbable, fDistance);
      }
    }
  }

  // grabbables whose physics shapes touch the reach sphere
  if (m_fReach > 0.0f)
  {
    if (const plPhysicsWorldModuleInterface* pPhysics = pWorld->GetModuleReadOnly<plPhysicsWorldModuleInterface>())
    {
      plBitflags<plPhysicsShapeType> shapeTypes = plPhysicsShapeType::Default;
      shapeTypes.Remove(plPhysicsShapeType::Trigger);

      plPhysicsOverlapResultArray results;
      pPhysics->QueryShapesInSphere(results, m_fReach, vHandPosition, plPhysicsQueryParameters(m_uiCollisionLayer, shapeTypes));

      for (const plPhysicsOverlapResult& result : results.m_Results)
      {
        plGameObject* pObject = nullptr;
        if (!pWorld->TryGetObject(result.m_hShapeObject, pObject) && !pWorld->TryGetObject(result.m_hActorObject, pObject))
          continue;

        for (; pObject != nullptr; pObject = pObject->GetParent())
        {
          plXRGrabbableComponent* pGrabbable = nullptr;
          if (pObject->TryGetComponentOfBaseType(pGrabbable))
          {
            consider(pGrabbable, (result.m_vCenterPosition - vHandPosition).GetLength());
            break;
          }
        }
      }
    }
  }

  return pBest;
}

bool plXRHandComponent::Grab()
{
  if (IsHolding() || !IsActiveAndSimulating())
    return false;

  const plTransform hand = plXRToolkit::GetCurrentGlobalTransform(GetOwner());

  plXRGrabbableComponent* pGrabbable = FindNearestGrabbable(hand.m_vPosition, GetController());
  if (pGrabbable == nullptr)
    return false;

  GrabObject(pGrabbable, hand);
  return true;
}

void plXRHandComponent::GrabObject(plXRGrabbableComponent* pGrabbable, const plTransform& handTransform)
{
  plGameObject* pObject = pGrabbable->GetOwner();
  const plTransform object = plXRToolkit::GetCurrentGlobalTransform(pObject);

  if (pGrabbable->m_GrabMode == plXRGrabMode::SnapToHand)
  {
    // the snap pose is in metres in the hand's frame, so the hand's scale does not stretch it
    m_HeldOffset.m_vPosition = pGrabbable->m_vSnapPosition;
    m_HeldOffset.m_qRotation = pGrabbable->m_qSnapRotation;
    m_HeldOffset.m_vScale = object.m_vScale;
  }
  else
  {
    const plTransform hand(handTransform.m_vPosition, handTransform.m_qRotation);
    m_HeldOffset = plTransform::MakeLocalTransform(hand, object);
  }

  m_hHeld = pGrabbable->GetHandle();
  pObject->MakeDynamic();
  pGrabbable->OnGrabbed(this, GetController());
}

void plXRHandComponent::Release(bool bThrow)
{
  plXRGrabbableComponent* pGrabbable = nullptr;
  if (!GetWorld()->TryGetComponent(m_hHeld, pGrabbable))
  {
    m_hHeld.Invalidate();
    return;
  }

  m_hHeld.Invalidate();
  pGrabbable->OnReleased(this, GetController(), GetHandVelocity(), GetLastFrameVelocity(), bThrow);
}

void plXRHandComponent::Update()
{
  const plTransform hand = plXRToolkit::GetCurrentGlobalTransform(GetOwner());
  AddSample(hand.m_vPosition);

  const plXRDeviceType::Enum controller = GetController();
  const float fGrip = plXRToolkit::ReadInput(GetOwner(), m_sGripAction, m_sGripSlot, controller, "grip_value");

  if (!m_bGripDown && fGrip >= m_fGripThreshold)
  {
    m_bGripDown = true;
    Grab();
  }
  else if (m_bGripDown && fGrip < m_fGripThreshold * 0.5f)
  {
    m_bGripDown = false;
    Release(true);
  }

  plXRGrabbableComponent* pGrabbable = nullptr;
  if (!GetWorld()->TryGetComponent(m_hHeld, pGrabbable))
  {
    m_hHeld.Invalidate();
    return;
  }

  // the held object follows the hand; a kinematic actor on it picks the new pose up when the physics step starts
  const plTransform unscaledHand(hand.m_vPosition, hand.m_qRotation);
  plTransform target = plTransform::MakeGlobalTransform(unscaledHand, m_HeldOffset);
  target.m_vScale = m_HeldOffset.m_vScale;

  pGrabbable->GetOwner()->SetGlobalTransform(target);
}
