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

---

## DEPRIORITIZED - Network & Connectivity

### LAN Discovery Investigation
Not pursuing at this time. Reason: Network-sided restrictions (firewall/NAT traversal).

Already tested on different physical devices with Null LAN match. Peer-to-peer UDP/broadcast blocked by network infrastructure on most corporate/school networks. Decision: Stay focused on dedicated server model (far more reliable and manageable).

Reference: Existing code paths remain available in `JoinSessionByIP()` for manual IP-based fallback if needed.

### Fallback Path - LAN P2P
Deprioritized. Dedicated server approach eliminates need for complex peer discovery. `JoinSessionByIP()` kept as fallback for diagnostics only.

---

## IN PROGRESS - Player Count Accuracy

**Issue:** Player count may not reflect actual connected clients in all scenarios
* Root cause: Timing misalignment between join/leave events and registry updates
* Symptom: Registry shows stale count e.g., always shows 1 instead of actual count
* Mitigation: Server uses deferred tick for join/leave; heartbeats include count for eventual consistency

**Code:** Source/OpenXrMp/XrGameMode.cpp (PostLogin, Logout, UpdatePlayersOnRegistry)

**Priority:** Medium - Will fix in next iteration

**Test checklist when working on this:**
- [ ] Single player join -> registry shows 1/N
- [ ] Multiple sequential joins -> registry increments correctly
- [ ] Player disconnect -> registry decrements correctly
- [ ] Rapid join/leave -> count remains accurate
- [ ] Server graceful shutdown -> registry shows 0/N or session deleted
- [ ] Server hard-killed -> heartbeat timeout eventually clears session
- [ ] Reconnect after disconnect -> count updates correctly

---

## PLANNED - Collision System Integration

### Capsule Collision and Physics

* [ ] Physics launch bug: Objects colliding with capsule cause unintended player movement
  * Investigate remaining ECR_Block responses and impulse paths
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

*Last updated: 2026-05-26*
*See ROADMAP.md for project-wide status and CHANGELOG.md for version history*

