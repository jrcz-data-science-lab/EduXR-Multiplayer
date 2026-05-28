# TODO — OpenXrMultiplayer Plugin

Tracked development items for current and future releases. Plugin is stable and functional; items below represent planned enhancements and bug fixes.

---

## COMPLETED - Dedicated Server Flow (On-Prem Linux)

All items completed as of 2026-05-26.

* [x] Added `EXrNetworkMode::Dedicated` in `XrMpGameInstance`
* [x] Added dedicated discovery/host API hooks in `XrMpGameInstance`
* [x] Added dedicated-mode `HostSession` and `FindSessions` paths
* [x] Added dedicated-mode `JoinSession` support with per-row connect string
* [x] Added `OpenXrMpServer.Target.cs` for Linux dedicated server builds
* [x] Define and freeze backend API contract (GET /sessions, POST /sessions)
* [x] Implement Linux-side session registry service with authentication
* [x] Add server health heartbeat and stale session cleanup policy
* [x] Build and package OpenXrMpServer for Linux with runtime validation
* [x] Validate end-to-end runtime chain: Server -> Heartbeat -> Registry -> Client Join

**Status:** End-to-end flow tested and working. Full API documentation in SessionRegistryApi/README.md

---

## COMPLETED - Fixed & Improved

### Code Quality & Optimisation

* [x] Review all widget Blueprint graphs
* [x] Reroute and clean messy node graphs
* [x] Add comments/comment boxes to non-obvious sections
* [x] Remove dead nodes and unused variables

### Build Configuration

* [x] Audit OpenXrMultiplayer.Build.cs module dependencies
* [x] Remove unused module references
* [x] Move private-only modules from Public to Private dependencies
* [x] Verify clean compilation after trimming

### Legacy Bug Fixes

* [x] LAN session discovery optimisation via NetServerMaxTickRate
* [x] Auto net driver configuration rework
* [x] EOS login early-exit in Local/LAN mode
* [x] Network mode UI flow with explicit None default state
* [x] Missing EnhancedInput plugin dependency
* [x] JoinSession hidden overload warnings
* [x] Added client `LeaveDedicatedServer()` helper for dedicated flow exit

---

## DEPRIORITIZED - Network & Connectivity

### LAN Discovery Investigation
Not pursuing at this time. Reason: Network-sided restrictions (firewall/NAT traversal).

Already tested on different physical devices with Null LAN match. Peer-to-peer UDP/broadcast blocked by network infrastructure on most corporate/school networks. Decision: Stay focused on dedicated server model (far more reliable and manageable).

Reference: Existing code paths remain available in `JoinSessionByIP()` for manual IP-based fallback if needed.

### Fallback Path - LAN P2P
Deprioritized. Dedicated server approach eliminates need for complex peer discovery. `JoinSessionByIP()` kept as fallback for diagnostics only.

---

## COMPLETED - Player Count Accuracy

**Issue (fixed 2026-05-28):** Player count did not always reflect connected clients due to a timing race between server session registration and player join/leave updates.
* Root cause: Server sometimes failed to register (SessionId empty) before PostLogin/Logout attempted registry updates, causing skipped updates and stale counts.
* Fix: Deployed API-side delta endpoint (`POST /sessions/{sessionId}/player-events`), idempotent event handling (`eventId`), and ensured dedicated server pulls the latest API before sending player updates. Server now queues/flushed events until `SessionId` is present and retries registration when needed.
* Verification: New server build using updated registry API was pulled and validated on 2026-05-28; player counts now update reliably in runtime tests.

**Code:** Source/OpenXrMp/XrGameMode.cpp (PostLogin, Logout, UpdatePlayersOnRegistry)

**Priority:** Completed

**Test checklist (verified):**
- [x] Single player join -> registry shows 1/N
- [x] Multiple sequential joins -> registry increments correctly
- [x] Player disconnect -> registry decrements correctly
- [x] Rapid join/leave -> count remains accurate
- [x] Server graceful shutdown -> registry shows 0/N or session deleted
- [x] Server hard-killed -> heartbeat timeout eventually clears session
- [x] Reconnect after disconnect -> count updates correctly

---

## IN PROGRESS - Collision System Integration

### Capsule Collision and Physics (active testing)

Current status (2026-05-28 final push): collision launch issue is reproducible when dynamic grabbable actors overlap player collision. Static world collision (ground/walls) is stable.

* [ ] Physics launch bug: Objects colliding with capsule cause unintended player movement
  * Focus testing on dynamic physics/grabbable actors (default gun, grab blocks, grab ball)
  * Investigate remaining ECR_Block responses and overlap impulse paths
  * Root cause analysis on current collision configuration
  
* [ ] Remove PhysicsPushForce / impulse-on-overlap
  * Current manual impulse in OnCapsuleOverlap is redundant
  * Let engine handle separation naturally
  * Should reduce physics instability
  
* [ ] Capsule follows HMD (real-world head tracking)
  * Capsule currently fixed at pawn location (VR origin)
  * Should track Camera (HMD) component world position each tick
  * Keeps collision volume aligned with actual player body position

**Code locations to review:**
* Source/OpenXrMp/Pawn (Capsule and collision setup)
* Source/OpenXrMp/XrGameMode (Overlap event handlers)
* Config/DefaultEngine.ini (Physics and collision settings)

**Testing focus for final push (today):**
- [ ] Hold/move dynamic grabbables into capsule at different velocities
- [ ] Validate no launch behavior during overlap and release cycles
- [ ] Re-verify static world collisions (ground/walls/slopes)
- [ ] Re-test in dedicated server runtime with at least 2 clients

---

## FUTURE - Code Quality & Optimisation

### C++ Code Pass

* [ ] General readability pass on all plugin `.h` and `.cpp` files
* [ ] Audit all `UPROPERTY` / `UFUNCTION` macros for correct specifiers
* [ ] Replace raw pointers with `TWeakObjectPtr` or `TObjectPtr` where appropriate
* [ ] Remove redundant `#include` statements
* [ ] Mark state-reading functions as `const`

---

## FUTURE - Nice-to-Have Features

* [ ] Add WBP_VrKeyboard Blueprint wrapper widget preview support
* [ ] Create Content/UI/Virtual/WBP_VrKeyboard.uasset reference implementation
* [ ] Document full keyboard integration flow with screenshots
* [ ] Add Niagara-based VR cursor at each controller aim point

---

*Last updated: 2026-05-28 (collision final-push testing in progress)*
*See ROADMAP.md for project-wide status and CHANGELOG.md for version history*

