#include <XRToolkitPlugin/XRToolkitPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <Foundation/Configuration/Singleton.h>
#include <GameEngine/XR/StageSpaceComponent.h>
#include <GameEngine/XR/XRInputDevice.h>
#include <GameEngine/XR/XRInterface.h>
#include <XRToolkitPlugin/Components/XRGrabbableComponent.h>
#include <XRToolkitPlugin/Components/XRHandComponent.h>

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plXRHandComponent, 3, plComponentMode::Static)
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
    PL_MEMBER_PROPERTY("PullRange", m_fPullRange)->AddAttributes(new plDefaultValueAttribute(6.0f), new plClampValueAttribute(0.0f, 50.0f), new plSuffixAttribute(" m")),
    PL_MEMBER_PROPERTY("PullAngle", m_PullAngle)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(12.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(1.0f), plAngle::MakeFromDegree(45.0f))),
    PL_MEMBER_PROPERTY("PullFlickRate", m_PullFlickRate)->AddAttributes(new plDefaultValueAttribute(plAngle::MakeFromDegree(250.0f)), new plClampValueAttribute(plAngle::MakeFromDegree(30.0f), plAngle::MakeFromDegree(2000.0f)), new plSuffixAttribute("/s")),
    PL_MEMBER_PROPERTY("PullFlightTime", m_PullFlightTime)->AddAttributes(new plDefaultValueAttribute(plTime::MakeFromSeconds(0.7)), new plClampValueAttribute(plTime::MakeFromSeconds(0.2), plTime::MakeFromSeconds(3.0))),
    PL_MEMBER_PROPERTY("PushStrength", m_fPushStrength)->AddAttributes(new plDefaultValueAttribute(40.0f), new plClampValueAttribute(0.0f, 10000.0f)),
  }
  PL_END_PROPERTIES;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(Grab),
    PL_SCRIPT_FUNCTION_PROPERTY(Release, In, "Throw"),
    PL_SCRIPT_FUNCTION_PROPERTY(IsHolding),
    PL_SCRIPT_FUNCTION_PROPERTY(IsPulling),
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

  // version 3
  s << m_fPullRange;
  s << m_PullAngle;
  s << m_PullFlickRate;
  s << m_PullFlightTime;
  s << m_fPushStrength;
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

  const plUInt32 uiVersion = inout_stream.GetComponentTypeVersion(GetStaticRTTI());

  if (uiVersion >= 3)
  {
    s >> m_fPullRange;
    s >> m_PullAngle;
    s >> m_PullFlickRate;
    s >> m_PullFlightTime;
    s >> m_fPushStrength;
  }
  else if (uiVersion == 2)
  {
    // version 2 pulled the object straight into the hand over a duration
    plTime pullDuration;
    s >> m_fPullRange;
    s >> m_PullAngle;
    s >> pullDuration;
    s >> m_fPushStrength;
  }
}

void plXRHandComponent::OnDeactivated()
{
  FinishFling();
  Release(false);
  m_bGripDown = false;
  m_hPullTarget.Invalidate();
  m_bPullLocked = false;
  m_hIncoming.Invalidate();
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

plGameObjectHandle plXRHandComponent::GetPullTarget() const
{
  const plXRGrabbableComponent* pGrabbable = nullptr;
  if (GetWorld()->TryGetComponent(m_hPullTarget, pGrabbable))
    return pGrabbable->GetOwner()->GetHandle();

  return plGameObjectHandle();
}

plXRDeviceType::Enum plXRHandComponent::GetController() const
{
  return plXRToolkit::ResolveController(GetOwner(), m_Hand, plXRDeviceType::RightController);
}

void plXRHandComponent::AddSample(const plVec3& vPosition, const plQuat& qStageRotation)
{
  // a jump of a metre within one frame is a teleport or a turn of the play area, not hand movement
  if (!m_Samples.IsEmpty() && (vPosition - m_Samples.PeekBack().m_vPosition).GetLengthSquared() > 1.0f)
  {
    m_Samples.Clear();
  }

  while (m_Samples.GetCount() >= MaxSamples)
  {
    m_Samples.RemoveAtAndCopy(0);
  }

  Sample& sample = m_Samples.ExpandAndGetRef();
  sample.m_vPosition = vPosition;
  sample.m_qStageRotation = qStageRotation;
  sample.m_Time = GetWorld()->GetClock().GetAccumulatedTime();
}

plVec3 plXRHandComponent::GetHandVelocity() const
{
  const plUInt32 uiCount = m_Samples.GetCount();
  const plUInt32 uiUsed = plMath::Min(uiCount, plMath::Clamp<plUInt32>(m_uiThrowSamples, 2, MaxSamples));
  if (uiUsed < 2)
    return plVec3::MakeZero();

  const Sample& oldest = m_Samples[uiCount - uiUsed];
  const Sample& newest = m_Samples[uiCount - 1];
  const double fSeconds = (newest.m_Time - oldest.m_Time).GetSeconds();

  if (fSeconds < 0.0001)
    return plVec3::MakeZero();

  return (newest.m_vPosition - oldest.m_vPosition) / static_cast<float>(fSeconds);
}

float plXRHandComponent::GetWristRate() const
{
  // the turn over the last three frames, in radians per second; short enough to catch a flick, long enough to smooth tracking noise
  const plUInt32 uiCount = m_Samples.GetCount();
  if (uiCount < 3)
    return 0.0f;

  const Sample& oldest = m_Samples[uiCount - 3];
  const Sample& newest = m_Samples[uiCount - 1];
  const double fSeconds = (newest.m_Time - oldest.m_Time).GetSeconds();

  if (fSeconds < 0.0001)
    return 0.0f;

  const plQuat qDelta = newest.m_qStageRotation * oldest.m_qStageRotation.GetInverse();
  plVec3 vAxis;
  plAngle angle;
  qDelta.GetRotationAxisAndAngle(vAxis, angle);

  // a quaternion turn above 180 degrees is the same turn the short way round
  float fRadians = plMath::Abs(angle.GetRadian());
  if (fRadians > plMath::Pi<float>())
    fRadians = 2.0f * plMath::Pi<float>() - fRadians;

  return fRadians / static_cast<float>(fSeconds);
}

plTransform plXRHandComponent::GetStageTransform()
{
  if (plStageSpaceComponent* pStage = plXRToolkit::FindStageSpace(GetWorld()))
  {
    const plTransform stage = plXRToolkit::GetCurrentGlobalTransform(pStage->GetOwner());
    return plTransform(stage.m_vPosition, stage.m_qRotation);
  }

  return plTransform::MakeIdentity();
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

plTransform plXRHandComponent::GetPointer(const plTransform& handTransform, plXRDeviceType::Enum controller)
{
  // The controller's aim pose is where a user expects a pointing hand to point. It is in stage space; the stage object places it in
  // the world. Without it the grip's forward axis stands in.
  plXRInterface* pXR = plSingletonRegistry::GetSingletonInstance<plXRInterface>();
  if (pXR != nullptr && pXR->IsInitialized())
  {
    const plXRDeviceID deviceID = pXR->GetXRInput().GetDeviceIDByType(controller);
    if (deviceID != -1)
    {
      const plXRDeviceState& state = pXR->GetXRInput().GetDeviceState(deviceID);
      if (state.m_bAimPoseIsValid)
      {
        return plTransform::MakeGlobalTransform(GetStageTransform(), plTransform(state.m_vAimPosition, state.m_qAimRotation));
      }
    }
  }

  return plTransform(handTransform.m_vPosition, handTransform.m_qRotation);
}

plXRGrabbableComponent* plXRHandComponent::FindPullTarget(const plTransform& pointer, plXRDeviceType::Enum controller)
{
  if (m_fPullRange <= 0.0f)
    return nullptr;

  plXRGrabbableComponentManager* pManager = GetWorld()->GetComponentManager<plXRGrabbableComponentManager>();
  if (pManager == nullptr)
    return nullptr;

  plVec3 vDir = pointer.m_qRotation * plVec3::MakeAxisX();
  vDir.NormalizeIfNotZero(plVec3::MakeAxisX()).IgnoreResult();

  const float fMinCos = plMath::Cos(m_PullAngle);
  const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>();

  plXRGrabbableComponent* pBest = nullptr;
  float fBestCos = fMinCos;

  for (auto it = pManager->GetComponents(); it.IsValid(); ++it)
  {
    plXRGrabbableComponent* pGrabbable = it;
    if (!pGrabbable->CanBeGrabbedBy(controller) || !pGrabbable->HasDynamicActor())
      continue;

    const plVec3 vToObject = pGrabbable->GetOwner()->GetGlobalPosition() - pointer.m_vPosition;
    const float fDistance = vToObject.GetLength();

    // within reach a normal grab takes it
    if (fDistance > m_fPullRange || fDistance <= pGrabbable->m_fGrabRadius + m_fReach)
      continue;

    const float fCos = vDir.Dot(vToObject / fDistance);
    if (fCos <= fBestCos)
      continue;

    // only what the hand can see: the first thing the ray hits has to belong to the grabbable
    if (pPhysics != nullptr)
    {
      plPhysicsCastResult hit;
      if (pPhysics->Raycast(hit, pointer.m_vPosition, vToObject / fDistance, fDistance, plPhysicsQueryParameters(m_uiCollisionLayer, plPhysicsShapeType::Static | plPhysicsShapeType::Dynamic)))
      {
        bool bHitsTarget = false;
        plGameObject* pHitObject = nullptr;
        if (GetWorld()->TryGetObject(hit.m_hShapeObject, pHitObject) || GetWorld()->TryGetObject(hit.m_hActorObject, pHitObject))
        {
          for (; pHitObject != nullptr && !bHitsTarget; pHitObject = pHitObject->GetParent())
          {
            bHitsTarget = pHitObject == pGrabbable->GetOwner();
          }
        }

        if (!bHitsTarget)
          continue;
      }
    }

    pBest = pGrabbable;
    fBestCos = fCos;
  }

  return pBest;
}

void plXRHandComponent::UpdatePull(const plTransform& handTransform, plXRDeviceType::Enum controller)
{
  if (m_fPullRange <= 0.0f || IsHolding())
  {
    m_hPullTarget.Invalidate();
    m_bPullLocked = false;
    return;
  }

  plXRGrabbableComponent* pTarget = nullptr;

  if (m_bPullLocked)
  {
    // the grip closed on it: wait for the flick while the hand stays closed
    if (!m_bGripDown || !GetWorld()->TryGetComponent(m_hPullTarget, pTarget) || !pTarget->CanBeGrabbedBy(controller))
    {
      m_bPullLocked = false;
      m_hPullTarget.Invalidate();
      return;
    }

    if (GetWristRate() >= m_PullFlickRate.GetRadian())
    {
      Launch(pTarget, handTransform.m_vPosition);
      m_bPullLocked = false;
      m_hPullTarget.Invalidate();
      return;
    }
  }
  else if (!m_bGripDown)
  {
    pTarget = FindPullTarget(GetPointer(handTransform, controller), controller);

    const plComponentHandle hTarget = pTarget != nullptr ? pTarget->GetHandle() : plComponentHandle();
    if (hTarget != m_hPullTarget && pTarget != nullptr)
    {
      // a light tick when the hand settles on something new to pull
      plXRToolkit::HapticPulse(controller, plTime::MakeFromMilliseconds(15), 0.2f);
    }

    m_hPullTarget = hTarget;
  }

  if (pTarget != nullptr)
  {
    const plVec3 vCenter = pTarget->GetOwner()->GetGlobalPosition();
    plVec3 vNormal = handTransform.m_vPosition - vCenter;
    vNormal.NormalizeIfNotZero(plVec3::MakeAxisZ()).IgnoreResult();

    const float fRadius = plMath::Clamp(pTarget->m_fGrabRadius, 0.05f, 0.5f);
    const plColor color = m_bPullLocked ? plColor::OrangeRed : plColor::Gold;
    plXRToolkit::DrawCircle(GetWorld(), vCenter, vNormal, fRadius, color);

    if (m_bPullLocked)
    {
      plXRToolkit::DrawCircle(GetWorld(), vCenter, vNormal, fRadius * 0.6f, color);
    }
  }
}

void plXRHandComponent::Launch(plXRGrabbableComponent* pGrabbable, const plVec3& vHandPosition)
{
  if (!pGrabbable->BeginFlight())
    return;

  // a throw that ends just above the hand after the flight time: p(t) = p0 + v*t + g*t^2/2
  plVec3 vGravity(0.0f, 0.0f, -9.81f);
  if (const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>())
  {
    vGravity = pPhysics->GetGravity();
  }

  const float fT = static_cast<float>(m_PullFlightTime.GetSeconds());
  const plVec3 vTarget = vHandPosition + plVec3(0.0f, 0.0f, 0.05f);
  const plVec3 vStart = pGrabbable->GetOwner()->GetGlobalPosition();
  const plVec3 vVelocity = (vTarget - vStart) / fT - vGravity * (0.5f * fT);

  m_hIncoming = pGrabbable->GetHandle();
  m_IncomingUntil = GetWorld()->GetClock().GetAccumulatedTime() + m_PullFlightTime + plTime::MakeFromSeconds(0.75);

  plXRToolkit::HapticPulse(GetController(), plTime::MakeFromMilliseconds(40), 0.6f);

  StartFling(pGrabbable, vVelocity, false);
}

void plXRHandComponent::TryCatch(const plTransform& handTransform)
{
  if (m_hIncoming.IsInvalidated())
    return;

  plXRGrabbableComponent* pGrabbable = nullptr;
  if (!GetWorld()->TryGetComponent(m_hIncoming, pGrabbable) || GetWorld()->GetClock().GetAccumulatedTime() > m_IncomingUntil)
  {
    m_hIncoming.Invalidate();
    return;
  }

  // still in its first kinematic frame, or picked up by the other hand
  if (m_hFling == m_hIncoming || !pGrabbable->CanBeGrabbedBy(GetController()))
    return;

  if (!m_bGripDown || IsHolding())
    return;

  // a closed hand catches what arrives within a generous radius; the arc ends at the hand, but hands move
  const float fCatchRadius = plMath::Max(pGrabbable->m_fGrabRadius, m_fReach) + 0.1f;
  if ((pGrabbable->GetOwner()->GetGlobalPosition() - handTransform.m_vPosition).GetLength() <= fCatchRadius)
  {
    m_hIncoming.Invalidate();
    GrabObject(pGrabbable, handTransform);
  }
}

void plXRHandComponent::StartFling(plXRGrabbableComponent* pGrabbable, const plVec3& vVelocity, bool bRelease)
{
  FinishFling();

  // This frame the actor is still kinematic: moving it by one frame of the velocity is what its body takes as its velocity.
  const float fDeltaSeconds = GetWorld()->GetClock().GetTimeDiff().AsFloatInSeconds();
  if (fDeltaSeconds > 0.0f && vVelocity.IsValid())
  {
    plGameObject* pObject = pGrabbable->GetOwner();
    pObject->SetGlobalPosition(plXRToolkit::GetCurrentGlobalTransform(pObject).m_vPosition + vVelocity * fDeltaSeconds);
  }

  m_hFling = pGrabbable->GetHandle();
  m_bFlingIsRelease = bRelease;
}

void plXRHandComponent::FinishFling()
{
  if (m_hFling.IsInvalidated())
    return;

  plXRGrabbableComponent* pGrabbable = nullptr;
  if (GetWorld()->TryGetComponent(m_hFling, pGrabbable))
  {
    if (m_bFlingIsRelease)
      pGrabbable->OnReleased(this, GetController());
    else
      pGrabbable->EndFlight();
  }

  m_hFling.Invalidate();
}

void plXRHandComponent::Push(const plVec3& vHandPosition)
{
  if (m_fPushStrength <= 0.0f || m_fReach <= 0.0f || IsHolding())
    return;

  const plVec3 vVelocity = GetHandVelocity();
  if (vVelocity.GetLengthSquared() < 0.05f * 0.05f)
    return;

  const plPhysicsWorldModuleInterface* pPhysics = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>();
  if (pPhysics == nullptr)
    return;

  plPhysicsOverlapResultArray results;
  pPhysics->QueryShapesInSphere(results, m_fReach, vHandPosition, plPhysicsQueryParameters(m_uiCollisionLayer, plPhysicsShapeType::Dynamic));

  // the impulse of one frame of a force that grows with the hand's speed
  const float fDeltaSeconds = GetWorld()->GetClock().GetTimeDiff().AsFloatInSeconds();
  const plVec3 vImpulse = vVelocity * (m_fPushStrength * fDeltaSeconds);

  plHybridArray<plGameObjectHandle, 8> pushed;
  for (const plPhysicsOverlapResult& result : results.m_Results)
  {
    plGameObject* pActorObject = nullptr;
    if (!GetWorld()->TryGetObject(result.m_hActorObject, pActorObject) || pushed.Contains(result.m_hActorObject))
      continue;

    pushed.PushBack(result.m_hActorObject);

    // grabbables are picked up, not shoved out of the hand that reaches for them
    bool bGrabbable = false;
    for (const plGameObject* pCur = pActorObject; pCur != nullptr && !bGrabbable; pCur = pCur->GetParent())
    {
      const plXRGrabbableComponent* pGrabbable = nullptr;
      bGrabbable = pCur->TryGetComponentOfBaseType(pGrabbable);
    }

    if (bGrabbable)
      continue;

    plMsgPhysicsAddImpulse msg;
    msg.m_vGlobalPosition = vHandPosition;
    msg.m_vImpulse = vImpulse;
    msg.m_uiImpulseType = 0; // the impulse is given as a value, not looked up by type
    pActorObject->SendMessage(msg);
  }
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
  m_hPullTarget.Invalidate();
  m_bPullLocked = false;
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

  if (IsActiveAndSimulating() && pGrabbable->HasDynamicActor())
  {
    // one more kinematic frame at the release velocity, then dynamic: see StartFling
    const plVec3 vVelocity = bThrow ? GetHandVelocity() * pGrabbable->m_fThrowVelocityScale : plVec3::MakeZero();
    StartFling(pGrabbable, vVelocity, true);
    return;
  }

  pGrabbable->OnReleased(this, GetController());
}

void plXRHandComponent::Update()
{
  // the object thrown or launched last frame has moved one frame at its velocity and becomes dynamic now
  FinishFling();

  const plTransform hand = plXRToolkit::GetCurrentGlobalTransform(GetOwner());
  const plTransform stage = GetStageTransform();
  AddSample(hand.m_vPosition, stage.m_qRotation.GetInverse() * hand.m_qRotation);

  const plXRDeviceType::Enum controller = GetController();
  const float fGrip = plXRToolkit::ReadInput(GetOwner(), m_sGripAction, m_sGripSlot, controller, "grip_value");

  if (!m_bGripDown && fGrip >= m_fGripThreshold)
  {
    m_bGripDown = true;

    // in reach it is a grab; otherwise the hand locks onto what the ring marks, and a flick launches it
    if (!Grab() && !m_hPullTarget.IsInvalidated())
    {
      m_bPullLocked = true;
      plXRToolkit::HapticPulse(controller, plTime::MakeFromMilliseconds(25), 0.35f);
    }
  }
  else if (m_bGripDown && fGrip < m_fGripThreshold * 0.5f)
  {
    m_bGripDown = false;
    Release(true);
  }

  UpdatePull(hand, controller);
  TryCatch(hand);
  Push(hand.m_vPosition);

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
