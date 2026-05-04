# TODO — EduXR Multiplayer Plugin

Tracked items for future versions of the EduXR Multiplayer Plugin.

---

## 🚀 Active Development (High Priority)

### 🌐 Dedicated Server Flow (On-Prem Linux)
- [ ] Define and freeze backend API contract (`GET /sessions`, `POST /sessions`).
- [ ] Implement Linux-side session registry service and wire authentication.
- [ ] Add server health heartbeat + stale session cleanup policy.
- [ ] Build and package `OpenXrMpServer` for Linux and validate process startup on Ubuntu.
- [ ] Validate end-to-end runtime chain: Server -> Heartbeat -> Registry -> Client Join.

### 🧱 Capsule Collision Rework
- [ ] **Physics launch bug** — Investigate remaining `ECR_Block` responses causing unexpected player velocity.
- [ ] **Remove Redundant Impulses** — Remove manual impulse in `OnCapsuleOverlap` and let the engine handle separation.
- [ ] **HMD-Relative Capsule Tracking** — Ensure the capsule XY position tracks the Camera (HMD) component in world space.

### 🔍 Networking & Connectivity
- [ ] **LAN Discovery Investigation** — Test cross-device discovery on trusted private LANs to resolve peer connectivity issues.
- [ ] **Fallback Path** — Ensure `JoinSessionByIP()` remains a robust fallback for all network modes.

---

## 🔵 Code Quality & Refinement (Medium Priority)

### C++ Optimization
- [ ] General readability pass on all `.h` and `.cpp` files.
- [ ] Review `UPROPERTY` / `UFUNCTION` specifiers for accuracy.
- [ ] Replace remaining raw pointers with `TWeakObjectPtr` or `TObjectPtr` where appropriate.
- [ ] Remove redundant `#include`s and ensure `const` correctness.

---

## 🟡 Nice-to-Have (Low Priority)

- [ ] **VR Cursor Visuals** — Add Niagara-based aim point visuals for `WidgetInteractionComponent`.
- [ ] **Keyboard UX** — Add a `WBP_VrKeyboard` Blueprint wrapper for better designer-time previews.
- [ ] **Reference Implementation** — Provide a pre-wired `WBP_VrKeyboard` asset in `Content/UI/Virtual/`.

---

## 🟢 Completed Milestones

### Documentation & Management
- [x] Refactor `README.md` for public use.
- [x] Clean up and professionalize `CHANGELOG.md`.
- [x] Reorganize `TODO.md` for better task tracking.

### Architecture & UI
- [x] **Network Mode Flow** — Added `EXrNetworkMode::None` and explicit mode selection.
- [x] **Blueprint Cleanup** — Refactored and commented all major menu widgets.
- [x] **EOS Stability** — Fixed login triggers and identity casting crashes.

### Optimization & Fixes
- [x] **Build.cs Optimization** — Audited and trimmed unused module dependencies.
- [x] **Warning Cleanup** — Resolved plugin dependency and hidden overload warnings.
- [x] **Net Driver Configuration** — Unified LAN and EOS net driver handling.

---
*Last updated: 2026-05-04*

