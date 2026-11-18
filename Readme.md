# XR Toolkit (`plasma.xrtoolkit`)

Interaction components for VR projects: hands that pick up, hold and throw objects, laser pointers, arc teleport and
snap or smooth turning. They work with any XR runtime the engine supports and read the controllers through the engine's
XR input slots, so a project only places components; it writes no input or tracking code.

## What is in it

| Component | Purpose |
| --- | --- |
| `plXRGrabbableComponent` | Marks an object that a hand can pick up and throw. |
| `plXRHandComponent` | Reads a controller's grip button, holds the nearest grabbable and throws it on release. |
| `plXRPointerComponent` | A laser pointer that sends enter, exit and click events to what it points at. |
| `plXRTeleportComponent` | Aims a ballistic arc and moves the play area to where it lands. |
| `plXRTurnComponent` | Turns the player in steps or continuously with a thumbstick. |

All five are in the **XR** category of the Add Component menu.

## Layout

```
Source/XRToolkitPlugin/           runtime plugin (plXRToolkitPlugin)
  Core/                           shared enums, input and haptics helpers, the update manager
  Components/                     the five components
Source/EditorPluginXRToolkit/     editor plugin: icon folder, hiding properties that do not apply
EditorIcons/TypeIcons/            component icons for the editor UI
Assets/Editor/ShapeIcons/         viewport icons
Utilities/GenerateIcons.py        regenerates both icon sets
```

## Building

```bash
PLASMA_SDK_ROOT=<engine checkout> plasmabuild build XRToolkitPlugin
PLASMA_SDK_ROOT=<engine checkout> plasmabuild build EditorPluginXRToolkit
```

## Setting up a player rig

The components sit on the usual XR rig: a stage space object with one child per tracked device.

1. Create an object **Player** and add a `plStageSpaceComponent`. This object is the play area; teleport and turning
   move it.
2. Add a child **Head** with a `plDeviceTrackingComponent` (Device Type **HMD**) and the camera.
3. Add children **Left Grip** and **Right Grip**, each with a `plDeviceTrackingComponent` (Device Type **Left Controller**
   or **Right Controller**, Pose Location **Grip**). Add a `plXRHandComponent` to each.
4. Add children **Left Aim** and **Right Aim** with `plDeviceTrackingComponent` set to Pose Location **Aim**. Add a
   `plXRTeleportComponent` to **Left Aim** and a `plXRPointerComponent` to **Right Aim**.
5. Add a `plXRTurnComponent` to **Player** (or to any object; it finds the stage space by itself).
6. Give every object that should be picked up a `plXRGrabbableComponent`, and a dynamic physics actor if it should fall
   and be thrown.

Leave the device tracking components at Transform Space **Local**, so the controllers move with the play area when it is
teleported or turned.

Each component that reads a controller has a **Hand** property. **Auto** uses the controller of the
`plDeviceTrackingComponent` on the same object or a parent, so the setup above needs no further changes.

### Input

Each input is read in one of two ways:

- **Action**: when the action property is set and a `plInputComponent` sits on the object or one of its parents, the
  value of that input action is used. Players can then rebind it like any other action.
- **Slot**: otherwise the input slot is read directly. An empty slot property uses the default slot of the component's
  controller, such as `xr_hand_left_grip_value` or `xr_hand_right_trigger_value`. All XR slot names are listed in the
  editor's input slot list under `xr_hand_...`.

On controllers whose only 2D input is a trackpad (the Vive wands), the default stick slots only count while the pad is
pressed, so resting a thumb on it neither turns nor aims a teleport.

### Order of updates

The components update every frame while the world simulates, right after `plDeviceTrackingComponent` has moved the
controllers and before the physics step: teleport and turning first, then hands and pointers. A held object is
therefore where the hand is in the same frame, and physics sees its new position.

### Lines and markers

Pointer beams, the teleport arc and the target ring are drawn as debug lines in the world, so both eyes see them. The
render pipeline used for XR has to draw debug geometry for them to show.

## Grabbable (`plXRGrabbableComponent`)

Marks an object that a `plXRHandComponent` can pick up.

A hand can pick the object up when the hand's position is within **Grab Radius** of the object's origin, or when the
hand's reach sphere touches one of the object's physics shapes. The radius is shown as a sphere in the viewport when the
object is selected.

While the object is held, a physics actor on it (a component with a **Kinematic** property, such as a dynamic actor) is
switched to kinematic and follows the hand. When the hand lets go, the actor gets its earlier Kinematic setting back and
leaves with the hand's velocity times **Throw Velocity Scale**. The actor's **Mass** property is used to turn that velocity
into an impulse. Objects without a physics actor stay where they were let go.

### Setup

1. Add a `plXRGrabbableComponent` to the object.
2. For throwing, add a collision shape and a dynamic physics actor.
3. To hold the object in a set way (a tool by its handle), keep **Grab Mode** at **Snap To Hand** and set **Snap
   Position** and **Snap Rotation** to the pose of the object's origin relative to the grip.

### Properties

| Property | Default | Description |
| --- | --- | --- |
| Grab Mode | Snap To Hand | **Snap To Hand** moves the object's origin to the grip pose, plus the snap offset. **Keep Offset** holds the object where it was relative to the hand when it was picked up. |
| Hand | Any | Which hands may pick the object up: **Any**, **Left** or **Right**. |
| Grab Radius | 0.15 m | How close the hand has to come to the object's origin. 0 relies on physics shapes only. |
| Snap Position | 0, 0, 0 | Snap To Hand only. Position of the object's origin relative to the grip pose, in metres. |
| Snap Rotation | identity | Snap To Hand only. Rotation of the object relative to the grip pose. |
| Throw Velocity Scale | 1 | Multiplies the hand's velocity when the object is let go. 0 drops it. |
| Haptic On Grab | on | Vibrates the controller when the object is picked up. |
| Haptic Amplitude | 0.5 | Strength of the pulse, 0 to 1. |
| Haptic Duration | 0.05 s | Length of the pulse. |

### Events and script functions

- `plMsgGenericEvent` **XRGrab** when picked up and **XRRelease** when let go, sent as event messages from the object.
  The sender is the hand component and the value is "Left" or "Right".
- `IsGrabbed()` returns whether a hand holds the object. `Drop()` makes the hand let go without throwing.

## Hand (`plXRHandComponent`)

Picks up grabbable objects with a controller's grip button, holds them and throws them.

When the grip value reaches **Grip Threshold**, the hand picks up the nearest grabbable in reach. When the value falls
below half of the threshold, the hand lets go and throws the object with its velocity averaged over the last **Throw
Samples** frames. The reach sphere is shown in the viewport when the object is selected.

**Throwing.** On release the object moves one more frame as a kinematic actor at the throw velocity and becomes dynamic in
the next. Its body keeps that velocity, so a throw is as fast as the hand whatever the object's mass.

**Pulling.** Pulling and catching are separate steps:

1. While the hand is open, the grabbable it points at (along the controller's aim pose, within **Pull Angle** and **Pull
   Range**, not hidden behind other geometry, and with a dynamic physics actor) is marked with a gold ring. The controller
   ticks when a new one is picked.
2. Closing the hand locks onto it: the ring turns red and doubles. Keep the hand closed.
3. Flick the wrist, turning it faster than **Pull Flick Rate**. The object is launched on a ballistic arc that ends just
   above where the hand was, **Pull Flight Time** later.
4. Catch it: a closed hand grabs it when it comes close. A hand that is open when it arrives lets it fall past; closing it
   then grabs it like any object in reach.

Grabbables within reach are always grabbed normally.

**Pushing.** The open hand pushes dynamic physics actors that touch its reach sphere and are not grabbables, such as a
door on a hinge. The push is a force along the hand's movement: **Push Strength** newtons per metre per second of hand
speed.

### Setup

1. Add the component to the object that tracks a controller's grip pose, or to a child of it.
2. Set **Collision Layer** to a layer that contains the grabbable objects' shapes.

### Properties

| Property | Default | Description |
| --- | --- | --- |
| Hand | Auto | Which controller's grip button and haptics are used. **Auto** uses the tracked controller, and the right one if none is found. |
| Reach | 0.08 m | Radius of the sphere around the hand that a grabbable's physics shape has to touch. 0 uses Grab Radius only. |
| Collision Layer | Default | Collision layer of the reach query. |
| Grip Action | (empty) | Input action of a `plInputComponent` on this object or a parent. Empty reads the grip slot. |
| Grip Slot | (empty) | Input slot read when no action is used. Empty uses `xr_hand_<left/right>_grip_value`. |
| Grip Threshold | 0.5 | Grip value at which the hand closes. The hand opens again below half of it. |
| Throw Samples | 5 | Number of frames the throw velocity is averaged over, 2 to 16. More frames give smoother, less snappy throws. |
| Pull Range | 6 m | How far away a grabbable can be pulled. 0 turns pulling off. |
| Pull Angle | 12° | How far off the pointing direction a grabbable may be and still be picked for pulling. Shown only with a pull range. |
| Pull Flick Rate | 250°/s | How fast the wrist has to turn to launch the locked grabbable. Shown only with a pull range. |
| Pull Flight Time | 0.7 s | How long a launched grabbable takes to reach the hand. Shown only with a pull range. |
| Push Strength | 40 | Force per metre per second of hand speed with which the open hand pushes dynamic objects. 0 turns pushing off. |

### Script functions

`Grab()` picks up the nearest grabbable in reach, `Release(Throw)` lets go, `IsHolding()` returns whether the hand holds
something and `IsPulling()` whether the held object is still flying into the hand.

## Pointer (`plXRPointerComponent`)

A laser pointer along the object's forward axis (+X). Every frame it casts a ray up to **Max Distance**, draws the beam
and a dot with a ring where it hits.

The object that is hit receives `plMsgGenericEvent` messages as event messages, with the pointer as the sender and the
hit point as the value:

| Event | When |
| --- | --- |
| XRPointerEnter | The beam starts hitting the object. |
| XRPointerExit | The beam stops hitting the object, or the pointer is deactivated. |
| XRPointerClick | The click input is pressed while the beam hits the object. |

The current hit can be read at any time, for example to place a cursor on a UI panel: `HasHit()`, `GetHitPoint()`,
`GetHitNormal()`, `GetHitDistance()` and, from C++, `GetHitObject()`. `IsClickDown()` returns whether the click input is
held.

### Setup

1. Add the component to the object that tracks a controller's aim pose.
2. Set **Collision Layer** to a layer that hits the things to point at but not the player's own hands.
3. React to the events in a script on the target objects.

### Properties

| Property | Default | Description |
| --- | --- | --- |
| Hand | Auto | Which controller's trigger clicks and which one vibrates. **Auto** uses the tracked controller, and the right one if none is found. |
| Max Distance | 10 m | Length of the ray. |
| Collision Layer | Default | Collision layer of the ray. Trigger shapes are never hit. |
| Show Beam | on | Draws the beam and the hit marker. |
| Show Beam Without Hit | on | Draws the full-length beam, fading out, while nothing is hit. |
| Beam Color | light blue | Colour of the beam. |
| Hit Color | white | Colour of the dot and ring at the hit point. |
| Hit Dot Size | 0.015 m | Radius of the dot at the hit point; the ring is twice as wide. |
| Click Action | (empty) | Input action of a `plInputComponent` on this object or a parent. Empty reads the click slot. |
| Click Slot | (empty) | Input slot read when no action is used. Empty uses `xr_hand_<left/right>_trigger_value`. |
| Click Threshold | 0.7 | Trigger value at which a click is sent. The trigger has to drop below half of it before the next click. |
| Haptic On Hover | on | Vibrates the controller when the beam reaches a new object. |
| Hover Haptic Amplitude | 0.2 | Strength of the pulse, 0 to 1. |
| Hover Haptic Duration | 0.02 s | Length of the pulse. |

## Teleport (`plXRTeleportComponent`)

Moves the player to where a ballistic arc lands.

While the activate input is held (the thumbstick pushed forward by default), an arc is cast from the object along its
forward axis (+X) and drawn with a ring where it lands. The arc and ring use **Valid Color** when the surface is no
steeper than **Max Slope**, and **Invalid Color** otherwise. Releasing the input over a valid target moves the play area
so that the floor under the headset lands on the target; the player keeps where they stand within the play area. Releasing
over an invalid target, or over nothing, does not move the player.

A short blink hides the move: the view fades to **Blink Color**, the play area moves at the darkest point, and the view
fades back, all within **Blink Duration**. A Blink Duration of 0 moves at once, as a cut. The fade covers the 3D view
only; game UI and debug drawing stay visible. Once the move happened, the teleport sends `plMsgGenericEvent`
**XRTeleport** as an event message from the object, with the target as the value, so a project can add its own effects.

### Setup

1. Make sure the world has one `plStageSpaceComponent` and that the tracked devices are its children.
2. Add the component to the object that tracks a controller's aim pose.
3. Set **Collision Layer** to a layer that hits walkable ground.

### Properties

| Property | Default | Description |
| --- | --- | --- |
| Hand | Auto | Which controller's stick activates the teleport. **Auto** uses the tracked controller, and the left one if none is found. |
| Activate Action | (empty) | Input action of a `plInputComponent` on this object or a parent. Empty reads the activate slot. |
| Activate Slot | (empty) | Input slot read when no action is used. Empty uses `xr_hand_<left/right>_primary_analog_stick_posy`. |
| Activate Threshold | 0.6 | Input value at which aiming starts. Aiming ends, and the teleport happens, below half of it. |
| Launch Velocity | 7 m/s | Start speed of the arc. Higher values reach further. |
| Gravity | 9.81 m/s² | Downward acceleration that bends the arc. |
| Max Steps | 40 | Number of straight segments the arc is cast in. |
| Step Time | 0.03 s | Simulated time per segment. Max Steps times Step Time is the longest flight time of the arc. |
| Collision Layer | Default | Collision layer of the arc's ray casts. Trigger shapes are never hit. |
| Max Slope | 30° | Steepest surface that counts as a valid landing. |
| Show Arc | on | Draws the arc and the target ring. |
| Valid Color | lime green | Colour of the arc and ring over a valid target. |
| Invalid Color | crimson | Colour of the arc and ring over an invalid target or when nothing is hit. |
| Target Radius | 0.3 m | Radius of the target ring. |
| Blink Duration | 0.2 s | Length of the fade out and back that hides the move. 0 moves at once. |
| Blink Color | black | Colour the view fades to. Shown while Blink Duration is above 0. |

### Script functions

`IsAiming()`, `HasValidTarget()` and `GetTargetPosition()` describe the current aim. `TeleportTo(Target)` moves the
player to any point, for example for scripted transitions, with the same blink. Aiming is ignored during a blink.

## Turn (`plXRTurnComponent`)

Turns the player with a thumbstick by rotating the play area around the headset, so the view turns on the spot.

In **Snap** mode, pushing the stick past **Snap Threshold** turns by **Snap Angle** once; the stick has to come back
towards the centre before the next turn. In **Smooth** mode, the player turns at up to **Smooth Speed** while the stick
is held beyond the dead zone.

While turning smoothly, a comfort vignette darkens the edges of the view, which reduces discomfort for many players. It
grows with the turn rate up to **Vignette** and follows changes at **Vignette Speed**. Set Vignette to 0 to turn it off.
Snap turns have no vignette.

### Setup

1. Make sure the world has one `plStageSpaceComponent`.
2. Add the component to any object, for example the stage space object.

### Properties

| Property | Default | Description |
| --- | --- | --- |
| Mode | Snap | **Snap** turns in steps, **Smooth** turns continuously. |
| Hand | Auto | Which controller's stick turns. **Auto** uses the tracked controller, and the right one if none is found. |
| Turn Left Action | (empty) | Input action for turning left. Empty reads the turn left slot. |
| Turn Right Action | (empty) | Input action for turning right. Empty reads the turn right slot. |
| Turn Left Slot | (empty) | Input slot for turning left. Empty uses `xr_hand_<left/right>_primary_analog_stick_negx`. |
| Turn Right Slot | (empty) | Input slot for turning right. Empty uses `xr_hand_<left/right>_primary_analog_stick_posx`. |
| Snap Angle | 45° | Snap only. Angle of one turn. |
| Snap Threshold | 0.7 | Snap only. Stick deflection that triggers a turn. |
| Smooth Speed | 90°/s | Smooth only. Turn rate at full deflection. |
| Dead Zone | 0.15 | Smooth only. Stick deflection below which the player does not turn. |
| Vignette | 0.5 | Smooth only. Vignette strength at full turn speed, 0 to 1. 0 turns it off. |
| Vignette Speed | 4 /s | Smooth only. How quickly the vignette follows the turn rate. |

### Script functions

`TurnBy(Degrees)` turns the player around the headset; positive values turn right.
