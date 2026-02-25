# Changelog

All notable changes to the EduXR Multiplayer Plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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

