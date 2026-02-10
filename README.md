# EduXR Multiplayer Plugin

A multiplayer plugin for Unreal Engine 5 designed for educational XR experiences. Provides seamless integration with both Epic Online Services (EOS) and local/LAN networking using the Null subsystem.

> **⚠️ Status:** Beta (v0.2.0) - Still in active development. Core functionality working, additional features coming soon.

## Features

- 🎮 **Dual Network Mode Support**
  - Epic Online Services (EOS) for online P2P multiplayer
  - Null subsystem for local/LAN multiplayer
  - Automatic fallback based on login status

- 🔄 **Smart Subsystem Detection**
  - Automatically uses Null subsystem (LAN) if not logged into EOS
  - Switches to EOS networking when user explicitly logs in
  - Seamless switching between network modes

- 🛠️ **Developer Friendly**
  - Blueprint-accessible functions for session management
  - Automatic net driver configuration
  - Comprehensive logging for debugging

- 🌐 **Full Session Management**
  - Create and host sessions
  - Find and browse sessions
  - Join sessions with proper user ID handling
  - Handle session invites (EOS only)

## Requirements

- **Unreal Engine:** 5.7.2 (tested and working)
- **Other Versions:** May work on UE 5.5+, but only tested on 5.7.2
- **Platform:** Windows (primary platform, other platforms untested)
- **Epic Online Services (EOS) SDK:** Optional - for online multiplayer features

## Installation

### Method 1: Clone to Plugins Folder

1. Navigate to your project's `Plugins` folder (create it if it doesn't exist)
2. Clone this repository:
   ```bash
   git clone https://github.com/jrcz-data-science-lab/EduXR-Multiplayer.git
   ```
3. Restart Unreal Editor
4. Enable the plugin in Edit → Plugins → Project → Networking

### Method 2: Download Release

1. Download the latest release from [Releases](https://github.com/jrcz-data-science-lab/EduXR-Multiplayer/releases)
2. Extract to `YourProject/Plugins/EduXR-Multiplayer`
3. Restart Unreal Editor
4. Enable the plugin
3. Restart Unreal Editor
4. Enable the plugin

## Included Content

The plugin includes blueprints, UI components, and test levels **(NOT FINAL)**:

### Content Directory Structure
- **Blueprints/** - Blueprint examples and utility actors for multiplayer
- **Levels/** - Test levels demonstrating multiplayer functionality
- **UI/** - UI components for multiplayer menus and lobby screens

These assets are optional - you can use the plugin without them.

## Quick Start

### 1. Set Up Your Game Instance

In your project settings, set the Game Instance Class to use the plugin's game instance:

**Blueprint:**
- Create a Blueprint based on `XrMpGameInstance`
- Set it in Project Settings → Maps & Modes → Game Instance Class

**C++:**
```ini
[/Script/EngineSettings.GameMapsSettings]
GameInstanceClass=/EduXR/Blueprints/BP_YourGameInstance.BP_YourGameInstance_C
```

### 2. Configure Network Driver (Important!)

Add this to your `DefaultEngine.ini`:

```ini
[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/SocketSubsystemEOS.NetDriverEOSBase")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[OnlineSubsystem]
DefaultPlatformService=EOS

[OnlineSubsystemEOS]
bEnabled=true

[OnlineSubsystemNull]
bEnabled=true
```

### 3. Using in Blueprints

#### Host a Session
```
Call Function: Host Session
- Max Players: 20
- Is Lan: false
- Server Name: "My Server"
```

#### Find Sessions
```
Call Function: Find Sessions
- Max Search Results: 50
- Is Lan: false
```

#### Join Session
```
Call Function: Join Session
- Session Index: 0 (from search results)
```

#### Optional: Login to EOS
```
Call Function: Login Online Service
(Only needed if you want to use EOS networking instead of LAN)
```

## How It Works

### Automatic Subsystem Selection

The plugin intelligently selects the appropriate online subsystem:

```
┌─────────────────────────────────────────┐
│  Game Starts                            │
│  ✓ EOS auto-login (for social features)│
│  ✓ Multiplayer defaults to LAN         │
└─────────────────────────────────────────┘
                  ↓
         User's Choice:
                  ↓
    ┌─────────────┴─────────────┐
    │                           │
    ▼                           ▼
Don't Call Login          Call LoginOnlineService()
    │                           │
    ▼                           ▼
Use Null Subsystem      Use EOS Subsystem
    │                           │
    ▼                           ▼
IpNetDriver               NetDriverEOSBase
LAN/IP Networking         P2P Networking
Port 7777                 EOS Lobbies
```

### EOS Integration (Optional)

If you want online multiplayer with EOS:

1. Set up your EOS application in the Epic Developer Portal
2. Configure your `DefaultEngine.ini` with EOS credentials
3. Call `Login Online Service` before hosting/finding sessions
4. The plugin will automatically switch to EOS networking

## API Reference

### Blueprint Functions

#### `Host Session`
Creates and hosts a new multiplayer session.
- **Parameters:**
  - `Max Players` (int32): Maximum number of players allowed
  - `Is Lan` (bool): Whether this is a LAN match
  - `Server Name` (FString): Display name for the server

#### `Find Sessions`
Searches for available sessions.
- **Parameters:**
  - `Max Search Results` (int32): Maximum number of sessions to find
  - `Is Lan` (bool): Whether to search for LAN matches

#### `Join Session`
Joins a session from the search results.
- **Parameters:**
  - `Session Index` (int32): Index of the session in search results

#### `Destroy Current Session`
Destroys the current active session.

#### `Login Online Service`
Explicitly logs into EOS for online multiplayer.

### Configuration Properties

#### `Map Url` (FString)
The map to load when hosting a session.
- Default: `"/Game/VRTemplate/VRTemplateMap?listen"`

#### `Return To Main Menu Url` (FString)
The map to return to when leaving a session.
- Default: `"/Game/VRTemplate/VRTemplateMap"`

#### `Custom Username` (FString)
Optional custom username to display in sessions.

## Troubleshooting

### "No local user set in LocalBindAddr" Error
- This means EOS net driver is being used without EOS login
- Verify your net driver configuration in DefaultEngine.ini
- Call `Login Online Service` if you want to use EOS

### Connection Timeout When Joining
- Check firewall settings (port 7777 for LAN)
- Ensure host has `?listen` in the map URL
- Verify both host and client are using the same subsystem

### "User is not logged into Online Subsystem"
- This is expected when not using EOS
- The plugin will automatically create a dummy user ID for Null subsystem
- Ignore this message if you want LAN-only gameplay

## Development

### Building from Source

1. Clone the repository to your project's Plugins folder
2. Open your `.uproject` file
3. Unreal will prompt to rebuild - click Yes
4. The plugin will compile automatically

### Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## Known Limitations

**⚠️ Beta Status:**
- Plugin is still in active development
- Not all features are complete
- Some Content assets are still being created
- Only tested on UE 5.7.2 (other versions may have compatibility issues)

**Functional Limitations:**
- EOS voice chat requires ≤16 players (automatic voice chat disabled for larger lobbies)
- Seamless travel not fully supported - uses absolute travel for session creation
- Primary testing on Windows - other platforms may need adjustments
- Content directory assets still under development


## License

**Proprietary License** - See [LICENSE](LICENSE) file for details

This software is the exclusive property of JRCZ Data Science Lab and KamiDanji.
Unauthorized access, copying, modification, or distribution is strictly prohibited.

## Credits

Developed by [KamiDanji](https://github.com/KamiDanji) and the [JRCZ Data Science Lab](https://github.com/jrcz-data-science-lab) for educational XR multiplayer experiences on Unreal Engine 5.

---

**Part of the EduXR project for educational and research purposes.**

