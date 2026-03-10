# TODO — EduXR Multiplayer Plugin

Tracked items that need attention in future versions. These are non-blocking — the plugin works as-is — but should be addressed before a stable release.

---

## 🔵 Code Quality & Optimisation *(planned for next update)*

### Blueprint Cleanup
- [ ] Review all widget Blueprint graphs (WBP_HostMenu, WBP_FindMenu, WBP_MainMaster, WBP_Multiplayer, WBP_OnlineMode, WBP_XrMainMenu)
- [ ] Reroute messy node graphs so they read left-to-right cleanly
- [ ] Add comments/comment boxes to every non-obvious graph section
- [ ] Remove any dead nodes or unused variables left over from iteration

### Build.cs Optimisation
- [ ] Audit `OpenXrMultiplayer.Build.cs` — list every module in `PublicDependencyModuleNames` and `PrivateDependencyModuleNames`
- [ ] Remove any module that is not directly referenced in plugin C++ headers or source files
- [ ] Move modules only used in `.cpp` files from `Public` → `Private` dependencies
- [ ] Verify the build still compiles and no new linker errors appear after trimming

### C++ Code Optimisation
- [ ] General readability pass on all plugin `.h` and `.cpp` files
- [ ] Ensure all `UPROPERTY` / `UFUNCTION` macros have correct specifiers (no extras, no missing)
- [ ] Replace any raw pointers that should be `TWeakObjectPtr` or `TObjectPtr`
- [ ] Check for any redundant `#include`s and remove them
- [ ] Verify every function that only reads state is marked `const`

---

## 🔴 Bug Fixes — Net Driver / Session *(needs investigation)*

### EOS Login Triggered in Local/LAN Mode
- [ ] **Issue:** When the player selects Local mode (Null subsystem) and has not signed in with Epic, hosting or finding a session still opens the EOS login overlay / triggers an EOS login attempt
- [ ] **Expected:** Null subsystem path should never touch EOS login — the overlay should never appear in local mode
- [ ] **Investigate:** Check `XrMpGameInstance` — confirm `bIsLoggedIntoEOS` gate is evaluated before any EOS SDK call in `HostSession` and `FindSessions`
- [ ] **Fix:** Add an early-out guard: if `bIsLan == true` or `bIsLoggedIntoEOS == false`, skip all EOS subsystem calls entirely

### LAN Session Not Discoverable (Null Subsystem)
- [ ] **Issue:** After hosting a session with the Null subsystem (local mode), `FindSessions` does not return the hosted session — needs more testing to confirm scope
- [ ] **Steps to reproduce:** Host (Null, LAN=true) on Instance 1 → FindSessions (Null, LAN=true) on Instance 2 on the same machine/network → results array is empty
- [ ] **Investigate:** Confirm `bIsLan` flag is passed correctly all the way through to `SessionSettings.bIsLANMatch = true` in both host and search
- [ ] **Investigate:** Check that `FindSessions` search settings also set `bIsLanQuery = true` when in local mode
- [ ] **Investigate:** Confirm both instances are using `OnlineSubsystemNull` and not accidentally falling back to EOS

### Capsule Collision Rework
- [ ] **Physics launch bug** — Objects colliding with the player capsule still cause the player to fly/move at high speed in certain situations. Investigate remaining `ECR_Block` responses and any residual impulse paths that bypass the overlap-only fix from v0.4.0
- [ ] **Remove `PhysicsPushForce` / impulse-on-overlap** — The capsule's normal physics behaviour should be sufficient to push objects out of the way. The manual impulse applied in `OnCapsuleOverlap` is redundant and may be contributing to the launch bug — remove it and let the engine handle separation naturally
- [ ] **Capsule follows HMD (real-world head tracking)** — Currently the capsule stays fixed at the pawn's world location and does not follow the player's physical head movement within the play space. The capsule's XY position should track the Camera (HMD) component's world XY each tick so the collision volume stays true to where the physical player is standing, not just the VR origin point

### Auto Net Driver Method (General Rework)
- [ ] Review the full `ConfigureNetDriverForSubsystem` flow — make sure it only reconfigures when actually switching subsystems, not on every call
- [ ] Ensure net driver config does not bleed EOS settings into a Null session
- [ ] Consider making subsystem selection fully explicit (passed in as a parameter) rather than inferred from internal state flags

---

## 🟡 Minor / Nice-to-Have

- [ ] Add a `WBP_VrKeyboard` Blueprint wrapper widget for the `UVrKeyboardWidget` C++ class so designers can see a preview in the UMG designer (even if the Slate content is built in C++)
- [ ] Add a dedicated `Content/UI/Virtual/WBP_VrKeyboard.uasset` with the select-button layout pre-wired as a reference implementation
- [ ] Document the full keyboard integration flow with screenshots in `README.md` once the widget layout is finalised

---

*Last updated: 2026-03-10*

