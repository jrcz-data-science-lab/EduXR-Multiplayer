﻿﻿﻿﻿# Changelog

All notable changes to the EduXR Multiplayer Plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.6.0] - 2026-03-10 (Beta)

### Status
⚠️ **Beta Release** - Fully functional C++ VR virtual keyboard with button-driven target text box selection. Active development.

### Engine Support
- ✅ **Unreal Engine 5.7.2** - Tested and working
- ⚠️ **Other UE5 versions** - Untested, may have compatibility issues

### Tested On
- Windows with UE 5.7.2
- Windows standalone mode multiplayer (VR host + desktop client)
- LAN networking (Null subsystem)
- EOS networking (when logged in)

### Added
- **Fully Functional C++ VR Virtual Keyboard (`UVrKeyboardWidget`)** — Complete QWERTY keyboard built entirely in C++ using Slate widgets, fully usable in world-space via a `WidgetComponent`
  - Number row (0–9 + shifted symbols), QWERTY letter rows, Shift/Backspace/Enter/Space/Clear modifier keys
  - Shift toggles case and auto-releases after one character
  - Live preview text box at the top of the keyboard shows current input
  - `OnKeyboardTextChanged` BlueprintAssignable delegate — fires every time a key is pressed
  - `OnKeyboardTextCommitted` BlueprintAssignable delegate — fires when Enter is pressed, passes the committed text
  - `SetKeyboardText` / `GetKeyboardText` / `ClearKeyboardText` BlueprintCallable functions
  - `MaxCharacters` (default 128) and `bStartShiftEnabled` configurable in Blueprint defaults
- **Target Text Box Selection via Buttons** — Each text box in a widget has a dedicated select button placed beside it; pressing it routes all keyboard input to that box
  - `SetTargetTextBox(UEditableTextBox*)` BlueprintCallable — call from the select button's `OnClicked` event next to each text box
  - `GetTargetTextBox()` / `HasTargetTextBox()` BlueprintPure utility functions
  - When Enter is pressed, the committed text is written directly into the targeted `UEditableTextBox` and the keyboard buffer is cleared for the next input
  - When `SetTargetTextBox` is called, the target box's existing text is loaded into the keyboard so the player can edit rather than replace
  - `TWeakObjectPtr` used internally to prevent dangling pointer crashes
  - No focus events or click-to-focus logic required — the player explicitly selects which box to type into
- **`AVrKeyboard` Actor** — World-space actor with a `UWidgetComponent` that hosts the `UVrKeyboardWidget`
  - Drop `BP_VrKeyboard` into any level and assign the widget class to get a floating keyboard in world-space
  - Interact with it using the existing `WidgetInteractionComponent` on each motion controller (trigger simulates mouse click)

### Changed
- VR keyboard no longer deferred to a future Blueprint-only version — the C++ `UVrKeyboardWidget` is the final implementation
- `BroadcastTextCommitted` now writes to `TargetTextBox` before broadcasting the delegate, ensuring the text box is updated before any Blueprint logic runs
- Keyboard buffer is cleared automatically after `Enter` so it is ready for the next text box without manual reset

### Planned for v0.6.1
- **Blueprint code cleanup** — Refactor and add comments to all widget Blueprint graphs for readability
- **Build.cs optimisation** — Audit `OpenXrMultiplayer.Build.cs` and remove any unnecessary module dependencies
- **C++ code optimisation** — General pass over all plugin C++ files for performance, clarity, and consistency

### Known Limitations
- EOS voice chat requires ≤16 players (auto-disabled for larger lobbies)
- Seamless travel not supported — uses absolute travel for session creation
- Primary testing on Windows platform only
- UE 5.7.2 only — untested on other engine versions
- Some Content/ assets still in development
- PIE (Play-in-Editor) multiplayer not functional — use standalone builds for testing
- Blueprint widget graphs not yet commented/cleaned up — coming in v0.6.1

### Technical Details
- `UVrKeyboardWidget` extends `UUserWidget`, overrides `RebuildWidget()` to return a fully Slate-built keyboard (no UMG designer required)
- `ReleaseSlateResources` properly resets `TSharedPtr<SEditableTextBox> PreviewTextBox` to avoid Slate resource leaks
- `FKeyDefinition` struct carries Normal label, Shifted label, `EKeyAction`, and relative width — all rows built from `TArray<FKeyDefinition>` in `BuildKeyboardWidget()`
- `TWeakObjectPtr<UEditableTextBox> TargetTextBox` — safe across GC cycles, checked with `.IsValid()` before every write
- Target selection flow: each text box has a select button beside it → `OnClicked` calls `SetTargetTextBox` on the keyboard → keyboard stores weak ref → Enter press writes committed text back via `SetText` and clears the buffer

---

## [0.5.0] - 2026-03-04 (Beta)

### Status
⚠️ **Beta Release** - Blueprint-exposed session search results, world-space UI widgets for server browsing. VR keyboard C++ removed — will be reimplemented as a Blueprint widget in v0.5.1. Active development.

### Engine Support
- ✅ **Unreal Engine 5.7.2** - Tested and working
- ⚠️ **Other UE5 versions** - Untested, may have compatibility issues

### Tested On
- Windows with UE 5.7.2
- Windows standalone mode multiplayer (VR host + desktop client)
- LAN networking (Null subsystem)
- EOS networking (when logged in)

### Added
- **Blueprint-Exposed Session Search Results** — Full session data accessible from Blueprints for custom server browser widgets
  - `FXrMpSessionResult` BlueprintType struct exposing: ServerName, OwnerName, CurrentPlayers, MaxPlayers, PingInMs, SessionIndex
  - `OnFindSessionsComplete_BP` BlueprintAssignable multicast delegate — bind in any widget to receive results when `FindSessions()` completes
  - `GetSessionSearchResults()` BlueprintCallable pure function — returns the cached array of `FXrMpSessionResult` from the last search
  - `IsSearchingForSessions()` BlueprintCallable pure function — returns true while a search is in flight (use to show loading spinners)
  - `SessionIndex` field in each result maps directly to `JoinSession(Index)` — no C++ needed to build a full server browser
- **World-Space UI Server Browser** — Blueprint widgets placed in the world for browsing and joining sessions using motion controllers
  - Interactive world-placed menus built with UMG widgets
  - Motion controller pointing + trigger press to interact with UI elements
- **WBP_VrKeyboard** — Empty Widget Blueprint placeholder in `Content/UI/Virtual/` for the upcoming Blueprint-based VR keyboard (v0.5.1)

### Removed
- **VR Virtual Keyboard C++ Implementation** — `UVrKeyboardWidget`, `UVrKeyHelper`, and associated header/source files have been removed
  - The C++ approach (building the full keyboard in `NativeConstruct()`) produced an empty designer in Widget Blueprints that inherited from it, making it difficult to extend or customize in the editor
  - Will be reimplemented as a pure Blueprint widget in **v0.5.1** with full designer support and visual editability

### Changed
- `OnFindSessionsComplete` now populates a `TArray<FXrMpSessionResult>` cache and broadcasts `OnFindSessionsComplete_BP` to all bound Blueprint listeners
- `FindSessions()` now sets `bIsSearching = true` before the async request, cleared when the callback fires
- Session result logging now uses the cached `FXrMpSessionResult` for consistent CurrentPlayers/MaxPlayers display

### Known Limitations
- EOS voice chat requires ≤16 players (auto-disabled for larger lobbies)
- Seamless travel not supported — uses absolute travel for session creation
- Primary testing on Windows platform only
- UE 5.7.2 only — untested on other engine versions
- Some Content/ assets still in development
- PIE (Play-in-Editor) multiplayer not functional — use standalone builds for testing
- VR keyboard not yet functional — placeholder only, full implementation coming in v0.5.1

### Planned for v0.5.1
- **Blueprint VR Virtual Keyboard** — Full QWERTY keyboard rebuilt as a Blueprint widget with designer-visible layout
  - Number row, letter rows, Shift/Backspace/Space/Enter/Clear modifier keys
  - Shift toggles case, auto-releases after one character
  - Event dispatchers for `OnTextCommitted` and `OnKeyPressed`
  - Fully editable in the UMG Designer — no more empty designer issue
  - Place on a `WidgetComponent` in the world, interact with motion controller + trigger

### Technical Details
- `FXrMpSessionResult` uses a unique name to avoid collision with engine's `FBlueprintSessionResult` in `FindSessionsCallbackProxy.h`
- `FOnXrMpFindSessionsComplete` dynamic multicast delegate avoids engine typedef collision
- Cached results stored in `TArray<FXrMpSessionResult> CachedSearchResults` private member
- `bIsSearching` flag prevents overlapping searches and enables UI loading states

---

## [0.4.0] - 2026-02-27 (Beta)

### Status
⚠️ **Beta Release** - VR physics collision rework, world-space UI interaction, gravity/jump system, and depenetration resolution. Active development.

### Engine Support
- ✅ **Unreal Engine 5.7.2** - Tested and working
- ⚠️ **Other UE5 versions** - Untested, may have compatibility issues

### Tested On
- Windows with UE 5.7.2
- Windows standalone mode multiplayer (VR host + desktop client)
- LAN networking (Null subsystem)
- EOS networking (when logged in)

### Added
- **World-Space Widget Interaction** — Trigger input bindings for in-world UI interaction
  - `LeftTriggerAction` / `RightTriggerAction` input actions for pressing UI buttons via motion controllers
  - `WidgetInteractionLeft` / `WidgetInteractionRight` configured with trace channel, interaction distance (500cm), and debug line visualization
  - `PressPointerKey` / `ReleasePointerKey` with `EKeys::LeftMouseButton` to simulate mouse clicks on world-space UMG widgets
  - World-space UI Blueprints (`BP_WorldMenu`, `BP_XrMenu`) edited to place interactive widgets in the level
- **Gravity & Jump System** — Full gravity simulation in `VrMovementComponent`
  - Configurable gravity (`GravityZ`, default -980 cm/s²) applied every tick when not grounded
  - Ground detection via line trace from capsule center downward past capsule bottom
  - Jump with `JumpZVelocity` (default 420 cm/s), only allowed when grounded
  - Terminal velocity capped at -2000 cm/s to prevent fall-through
  - `ServerJump` reliable Server RPC for network-replicated jumping
  - Automatic floor-snap on landing — sweeps the pawn down to the detected floor surface
- **Depenetration Resolution** — Escape from embedded geometry automatically
  - Sweep-based penetration detection every tick before ground check
  - Uses `ECC_WorldStatic` channel — only depenetrates against static world geometry, not physics objects
  - Applies push-out along hit Normal with PenetrationDepth + margin
  - Prevents the "frozen player" bug where embedded capsule blocks all sweep-based movement
- **Capsule Overlap Physics Push** — Walking into physics objects pushes them away
  - `OnCapsuleOverlap` callback applies impulse to simulating-physics actors
  - Horizontal-only push direction (Z zeroed) to prevent launching objects skyward
  - Configurable `PhysicsPushForce` (default 500N)
  - Fallback: pushes in actor forward direction if object is directly on top of the player
- **Spawn Position Floor Correction** — `VrMovementComponent::BeginPlay` traces downward to find the floor and positions the capsule correctly above it, preventing the capsule from spawning embedded in the floor
- **VrOrigin Floor-Level Enforcement** — VrOrigin offset (`-HalfHeight`) reinforced in Constructor, BeginPlay, and every Tick to prevent Blueprint CDO from overriding the floor-level positioning
- **Comprehensive Debug Logging** — Height debugging, ground state logging, movement delta tracking, and network state display (all stripped in shipping builds via `#if !UE_BUILD_SHIPPING`)

### Changed
- **Capsule Collision Rework** — Completely overhauled player-physics interaction
  - Capsule radius reduced from 34cm to **20cm** — slimmer profile so players don't clip walls they're not near
  - `ECC_PhysicsBody` and `ECC_Destructible` collision response changed from `ECR_Block` → **`ECR_Overlap`**
  - Eliminates the "picked-up object collides with body and launches player at insane speeds" bug
  - Eliminates the "walking into physics cubes stops the player dead" bug
  - Capsule collision settings reinforced in BeginPlay to override Blueprint CDO values
  - Overlap events enabled (`SetGenerateOverlapEvents(true)`) for physics push callback
- **VrMovementComponent** now extends `UPawnMovementComponent` (was `UActorComponent`)
  - Enables integration with Unreal's movement framework
- **MoveForward / MoveRight** now use HMD (camera) forward/right direction instead of actor forward
  - Movement is camera-relative: you walk where you're looking
  - Returns the computed delta vector for server replication (server doesn't need VR tracking data)
- **SnapTurn** now uses dual reset mechanism — deadzone reset + cooldown timer (`SnapTurnCooldown`, default 0.3s)
  - Fixes the "snap turn only works once" bug where the neutral input never arrived
- **Added `InputCore` module dependency** to `OpenXrMultiplayer.Build.cs` for `EKeys::LeftMouseButton` symbol

### Fixed
- Fixed physics objects (cubes, guns, balls) colliding with player capsule and launching the player at extreme speeds — capsule now overlaps instead of blocking physics objects
- Fixed player capsule being too wide (34cm radius) causing wall collisions when the player wasn't visually near walls — reduced to 20cm
- Fixed player unable to move after spawning — capsule was embedded in the floor; added spawn floor correction and depenetration resolution
- Fixed VrOrigin being at capsule center instead of capsule bottom — player appeared ~2.5m tall; offset now enforced every tick
- Fixed depenetration system pushing player away from held/grabbed physics objects — depenetration now only checks `ECC_WorldStatic`, ignoring physics bodies
- Fixed `LNK2019` linker error for `EKeys::LeftMouseButton` — added `InputCore` module to Build.cs dependencies
- Fixed snap turn only firing once per session — added cooldown-based reset as fallback when neutral input doesn't arrive

### Known Limitations
- EOS voice chat requires ≤16 players (auto-disabled for larger lobbies)
- Seamless travel not supported — uses absolute travel for session creation
- Primary testing on Windows platform only
- UE 5.7.2 only — untested on other engine versions
- Some Content/ assets still in development
- PIE (Play-in-Editor) multiplayer not functional — use standalone builds for testing

### Technical Details
- Capsule overlap response for `ECC_PhysicsBody` with impulse-based push in overlap callback
- `ResolvePenetration` sweep against `ECC_WorldStatic` only — safe for grabbed objects
- Floor correction in `VrMovementComponent::BeginPlay` via downward `ECC_WorldStatic` line trace
- Ground detection via line trace + gravity tick with terminal velocity cap
- VrOrigin Z-offset enforced in Constructor (-88), BeginPlay (-HalfHeight), and Tick (conditional set)
- Widget interaction via `PressPointerKey`/`ReleasePointerKey` on `EKeys::LeftMouseButton`

---

## [0.3.0] - 2026-02-25 (Beta)

### Status
⚠️ **Beta Release** - VR tracking replication and session management working. Still in active development.

### Engine Support
- ✅ **Unreal Engine 5.7.2** - Tested and working
- ⚠️ **Other UE5 versions** - Untested, may have compatibility issues

### Tested On
- Windows with UE 5.7.2
- Windows standalone mode multiplayer (VR host + desktop client, desktop host + VR client)
- LAN networking (Null subsystem)
- EOS networking (when logged in)

### Added
- **CustomXrPawn** — Full VR pawn with multiplayer-replicated head and hand tracking
  - Camera, MotionControllerLeft, MotionControllerRight created in C++ constructor
  - HeadMountedDisplayMesh, HandLeft, HandRight skeletal/static meshes for remote player visuals
  - WidgetInteractionComponent on each hand for UI pointing
  - CapsuleComponent for collision
  - PlayerMesh for body representation
- **VR Tracking Replication** — Head and both hand transforms replicated to all clients
  - Transforms captured relative to VrOrigin (rotation-independent, avoids world-space issues)
  - Server RPC (`ServerUpdateVRTransforms`) sends tracking data every tick (unreliable, high frequency)
  - `COND_SkipOwner` on replicated properties to avoid fighting with local HMD tracking
  - Non-local pawns disable HMD lock and motion controller tracking sources in BeginPlay to prevent the engine from overriding replicated transforms
- **VrMovementComponent** — Smooth locomotion and snap turn for VR
  - `MoveForward` / `MoveRight` — horizontal-plane movement with sweep collision
  - `SnapTurn` — discrete rotation with deadzone and cooldown to prevent repeated turns
  - All movement applied via `AddActorWorldOffset` / `SetActorRotation`, compatible with `bReplicateMovement`
- **Movement Replication** — Input-driven movement works on both server and client pawns
  - Local input executes immediately for responsiveness (client-side prediction)
  - Server RPCs (`ServerMoveForward`, `ServerMoveRight`, `ServerSnapTurn`) execute authoritatively on the server
  - `bReplicateMovement` syncs the server's actor position to all clients automatically
  - Snap turn uses reliable RPC (discrete event), movement uses unreliable RPCs (continuous input)
- **Comprehensive code comments** — All `.h` and `.cpp` files documented with `/** */` format
  - Replication design overview in CustomXrPawn.h header
  - Every function, property, and design decision explained
  - Comments written for the next developer to understand why and what

### Changed
- **Session management** — Net driver now only reconfigured when using EOS (not on every host call)
  - Prevents unnecessary IpNetDriver → IpNetDriver reconfiguration for Null subsystem
  - EOS net driver (`NetDriverEOSBase`) only activated when `bIsLoggedIntoEOS` is true
- **EOS auto-login handling** — EOS auto-login no longer triggers multiplayer subsystem switch
  - `bIsLoggedIntoEOS` only set to `true` when user explicitly calls `LoginOnlineService()`
  - Auto-login by EOS SDK is tracked separately and does not affect subsystem selection
- **Removed PIE support from documentation** — PIE multiplayer testing is not functional and has been de-emphasized

### Fixed
- Fixed hands appearing grouped at world center on remote pawns — caused by MotionControllerComponent resetting transforms every tick when no physical controller is connected. Solution: disable tracking source and tick on non-local pawns.
- Fixed replication only working when VR player is the host — added Server RPCs so client-owned pawns can send tracking data to the server for redistribution.
- Fixed EOS auto-login on host — hosting a session no longer triggers EOS login; only explicit `LoginOnlineService()` call switches to EOS networking.
- Fixed linker error for `ConfigureNetDriverForSubsystem` — ensured implementation matches header declaration.
- Fixed `WidgetInteractionComponent` linker error — added `UMG` module dependency.

### Known Limitations
- EOS voice chat requires ≤16 players (auto-disabled for larger lobbies)
- Seamless travel not supported — uses absolute travel for session creation
- Primary testing on Windows platform only
- UE 5.7.2 only — untested on other engine versions
- Some Content/ assets still in development
- PIE (Play-in-Editor) multiplayer not functional — use standalone builds for testing

### Technical Details
- Built for Unreal Engine 5.7.2
- Uses Online Subsystem architecture (EOS + Null)
- Relative transform replication (VrOrigin-space) for rotation-independent tracking sync
- `SetNetUpdateFrequency(60)` / `SetMinNetUpdateFrequency(30)` for smooth VR tracking
- Unreliable Server RPCs for continuous tracking/movement data, reliable for discrete events (snap turn)
- `COND_SkipOwner` replication condition to avoid bandwidth waste and tracking conflicts

---

## [0.2.0] - 2026-02-10 (Beta)

### Status
⚠️ **Beta Release** - Core multiplayer functionality working, still in active development

### Engine Support
- ✅ **Unreal Engine 5.7.2** - Tested and working
- ⚠️ **Other UE5 versions** - Untested, may have compatibility issues

### Tested On
- Windows with UE 5.7.2
- Windows standalone mode multiplayer
- Windows packaged build
- LAN networking (Null subsystem)
- EOS networking (when logged in)

### Added
- Dual network mode support (EOS and Null subsystem)
- Automatic subsystem detection based on login status
- Session management (Create, Find, Join, Destroy)
- Blueprint-accessible API for multiplayer functions
- Automatic net driver configuration
- Dummy user ID creation for Null subsystem (no login required)
- Smart EOS/LAN switching based on explicit login
- Comprehensive logging for debugging
- Blueprint examples and UI components in Content/ directory
- Level prototypes for testing

### Features
- **Host Session**: Create and host multiplayer sessions
- **Find Sessions**: Browse available sessions with customizable search
- **Join Session**: Connect to sessions from search results
- **Login to EOS**: Optional online multiplayer with Epic Online Services
- **Automatic Fallback**: Uses LAN if EOS not available

### Configuration
- IpNetDriver as primary with EOS fallback for maximum compatibility
- Null subsystem enabled for LAN play
- EOS subsystem enabled for online play
- Configurable map URLs and session settings

### Known Limitations
- EOS voice chat requires ≤16 players (auto-disabled for larger lobbies)
- Seamless travel not supported - uses absolute travel
- Primary testing on Windows platform only
- UE 5.7.2 only - untested on other engine versions
- Some Content/ assets still in development

### Technical Details
- Built for Unreal Engine 5.7.2
- Uses Online Subsystem architecture
- Proper delegate handling for session callbacks
- User ID management for both authenticated and unauthenticated scenarios
- Net driver configuration system for automatic switching

### Content Directory
- **Blueprints/** - Blueprint examples and utility actors
- **Levels/** - Test levels for multiplayer
- **UI/** - UI components for multiplayer menus

