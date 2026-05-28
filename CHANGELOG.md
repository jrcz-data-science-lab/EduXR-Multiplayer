# OpenXrMp Changelog

## Unreleased - 2026-05-28

### Confirmed

* Dedicated server runtime is now using the updated registry API build
  * Player-count issue is verified fixed after pulling the latest API version to server

### Added

* Client-side `LeaveDedicatedServer()` helper in `XrMpGameInstance`
  * Allows clients to leave dedicated sessions cleanly and return to menu flow

### In Progress

* Player collision launch bug validation is actively in test
  * Repro is currently tied to dynamic grabbable physics objects (gun/blocks/ball)
  * Static world collision (floor/walls) remains stable
  * Final tuning and verification are ongoing before end-of-day push

---

## 0.7.1 - 2026-05-28

### Bug Fixes

* Fixed player count accuracy in dedicated-server flow
  * Deployed API-side player-events endpoint and idempotent delta handling
  * Ensured dedicated server registers SessionId before sending player updates and flushes queued events
  * Resolved runtime scenario where SessionId was empty and UpdatePlayersOnRegistry skipped updates

---

## 0.7.0 - 2026-05-26

### Major Features

* Complete dedicated server implementation for on-prem Linux deployment
  * Session registry API with bearer token authentication
  * Heartbeat-based session lifecycle management
  * Player count tracking and updates via authoritative server state
  * Linux server packaging and startup validation

* End-to-end runtime validation
  * Server successfully registers with registry on startup
  * Player join/leave events trigger registry updates
  * Client discovery and session joining via registry
  * Full flow tested and operational

### Infrastructure

* Added SessionRegistryApi service with Python backend
  * Implements core endpoints: POST /sessions, GET /sessions, DELETE /sessions
  * Heartbeat endpoint: POST /sessions/{sessionId}/heartbeat
  * Player count endpoint: POST /sessions/{sessionId}/players
  * Admin panel: GET /admin with browser UI
  * Comprehensive API documentation with response examples

* Linux server build configuration
  * OpenXrMpServer.Target.cs for headless server builds
  * start_server.sh wrapper script for session registration
  * SESSION_ID command-line argument handling
  * Binary output: Binaries/Linux/OpenXrMpServer

* Game mode integration
  * PostLogin() hook triggers player count updates to registry
  * Logout() hook decrements player count on disconnect
  * UpdatePlayersOnRegistry() uses authoritative GameState player count
  * Deferred updates by one tick for PlayerState settlement

### Documentation

* SessionRegistryApi/README.md - Complete registry API reference
  * Endpoint definitions and response formats
  * Session lifecycle policies (manual, auto_close_when_empty)
  * Placeholder expansion for server launch
  * Admin panel and heartbeat client documentation

* Plugins/OpenXrMultiplayer/README.md - Updated plugin reference
  * Engine version: 5.7.0+ (tested on 5.7.2 and 5.7.3)
  * Comprehensive API documentation
  * Dedicated server setup instructions
  * VR keyboard widget reference

* Project-wide roadmap and status tracking
  * ROADMAP.md - High-level development status
  * TODO.md - Plugin-specific development items
  * CHANGELOG.md - This file

### Known Issues

* None critical for 0.7.1 — player-count accuracy fixed in 0.7.1; remaining items tracked in ROADMAP.md and Plugins/OpenXrMultiplayer/TODO.md

### Deprecated

* LAN peer-to-peer discovery deprioritized
  * Network infrastructure restrictions prevent reliable LAN discovery
  * Dedicated server model provides superior reliability
  * JoinSessionByIP() remains available for fallback/diagnostics

---

## 0.6.3 - 2026-04-07

### Bug Fixes & Improvements

* Fixed Blueprint graph organisation and documentation
* Optimized Build.cs module dependencies
* Resolved EOS login early-exit in Local/LAN modes
* Added EnhancedInput plugin dependency
* Fixed JoinSession hidden overload warnings
* Network mode UI flow now starts with explicit None state

### Networking

* LAN session discovery investigation completed
  * Identified network-side restrictions blocking peer discovery
  * Cross-device testing showed infrastructure-level isolation
  * Decision: Focus on dedicated server flow

---

## 0.6.2 and earlier

* Initial plugin development
* VR template integration
* OpenXR and EOS support
* LAN and Online session modes
* Virtual keyboard implementation
* Blueprint menu system

---

*Project started: 2026*
*Latest update: 2026-05-28 (final push in progress)*

