# Changelog

All notable changes to the EduXR Multiplayer Plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
- **PIE Testing**: Seamless local multiplayer testing in editor

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

