# TODO — EduXR Multiplayer Plugin

Tracked items that need attention in future versions. These are non-blocking — the plugin works as-is — but should be addressed before a stable release.

---

## 🟢 Fixed & Completed
(Recent updates)

### Code Quality & Optimisation

#### Blueprint Cleanup
- [x] Review all widget Blueprint graphs (`WBP_HostMenu`, `WBP_FindMenu`, `WBP_MainMaster`, `WBP_Multiplayer`, `WBP_OnlineMode`, `WBP_XrMainMenu`)
- [x] Reroute messy node graphs so they read left-to-right cleanly
- [x] Add comments/comment boxes to every non-obvious graph section
- [x] Remove any dead nodes or unused variables left over from iteration

#### Build.cs Optimisation
- [x] Audit `OpenXrMultiplayer.Build.cs` — list every module in `PublicDependencyModuleNames` and `PrivateDependencyModuleNames`
- [x] Remove any module that is not directly referenced in plugin C++ headers or source files
- [x] Move modules only used in `.cpp` files from `Public` → `Private` dependencies
- [x] Verify the build still compiles and no new linker errors appear after trimming

### Bug Fixes — Net Driver / Session

#### LAN Session Not Discoverable (Null Subsystem)
- [x] **Investigate:** Analyzed `OpenXrMp.log`. Discovery fails with 0 results despite `bIsLANMatch=true`.
- [x] **Fix:** Updated `DefaultEngine.ini` with `NetServerMaxTickRate=60`.

### Auto Net Driver Method (General Rework)
- [x] Review the full `ConfigureNetDriverForSubsystem` flow — make sure it only reconfigures when actually switching subsystems, not on every call
- [x] Ensure net driver config does not bleed EOS settings into a Null session
- [x] Subsystem selection is now explicitly handled via `ActiveNetworkMode` and `bUsingEOS` flags in session methods

### EOS Login Triggered in Local/LAN Mode
- [x] **Issue:** EOS login was triggering even in Local mode
- [x] **Fix:** Added explicit `ActiveNetworkMode` checks. `LoginOnlineService` now early-outs if mode is `Local`. `HostSession` and `FindSessions` now skip EOS-specific identity calls when in `Local` mode.

### Network Mode UI Flow
- [x] Added `EXrNetworkMode::None` as the default startup state
- [x] Updated `ActiveNetworkMode` default to `None` so UI flow starts neutral and only commits to `Local`/`Online` after explicit player choice

### Build/Compile Warning Cleanup
- [x] Added missing `EnhancedInput` plugin dependency in `OpenXrMultiplayer.uplugin`
- [x] Fixed `UGameInstance::JoinSession` hidden overload warnings by exposing base overloads in `XrMpGameInstance.h`

---

## 🔵 Code Quality & Optimisation

### C++ Code Optimisation
- [ ] General readability pass on all plugin `.h` and `.cpp` files
- [ ] Ensure all `UPROPERTY` / `UFUNCTION` macros have correct specifiers (no extras, no missing)
- [ ] Replace any raw pointers that should be `TWeakObjectPtr` or `TObjectPtr`
- [ ] Check for any redundant `#include`s and remove them
- [ ] Verify every function that only reads state is marked `const`

---

## 🔴 Bug Fixes — Net Driver / Session *(needs investigation)*

### LAN Session Not Discoverable (Null Subsystem)
- [ ] **Currently Testing:** Refined `FOnlineSessionSettings` and `FOnlineSessionSearch` in `XrMpGameInstance.cpp` for more standard LAN behavior (`bIsDedicated`, `bUsesStats`, `PingBucketSize`).
- [ ] **Steps to reproduce:** Host (Null, LAN=true) on Instance 1 → FindSessions (Null, LAN=true) on Instance 2 on different devices.

### Capsule Collision Rework
- [ ] **Physics launch bug** — Objects colliding with the player capsule still cause the player to fly/move at high speed in certain situations. Investigate remaining `ECR_Block` responses and any residual impulse paths that bypass the overlap-only fix from v0.4.0
- [ ] **Remove `PhysicsPushForce` / impulse-on-overlap** — The capsule's normal physics behaviour should be sufficient to push objects out of the way. The manual impulse applied in `OnCapsuleOverlap` is redundant and may be contributing to the launch bug — remove it and let the engine handle separation naturally
- [ ] **Capsule follows HMD (real-world head tracking)** — Currently the capsule stays fixed at the pawn's world location and does not follow the player's physical head movement within the play space. The capsule's XY position should track the Camera (HMD) component's world XY each tick so the collision volume stays true to where the physical player is standing, not just the VR origin point

---

## 🟡 Minor / Nice-to-Have

- [ ] Add a `WBP_VrKeyboard` Blueprint wrapper widget for the `UVrKeyboardWidget` C++ class so designers can see a preview in the UMG designer (even if the Slate content is built in C++)
- [ ] Add a dedicated `Content/UI/Virtual/WBP_VrKeyboard.uasset` with the select-button layout pre-wired as a reference implementation
- [ ] Document the full keyboard integration flow with screenshots in `README.md` once the widget layout is finalised
- [ ] Add a Niagara-based VR cursor at each controller aim point so players can see where the `WidgetInteractionComponent` is pointing when interacting with world-space UI

---

*Last updated: 2026-03-20 (v0.6.2 prep)*

