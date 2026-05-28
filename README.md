# EduXR Multiplayer Plugin

A robust multiplayer networking solution for Unreal Engine 5, specifically designed for educational XR experiences. This plugin provides seamless integration between Epic Online Services (EOS) and local LAN networking, featuring full VR tracking replication and optimized movement.

---

## Key Features

### Versatile Networking
- **Dual Mode Support**: Seamlessly switch between Epic Online Services (EOS) for global P2P and the Null subsystem for local LAN play.
- **Dedicated Server Support**: Built-in support for dedicated server discovery via custom HTTP registry APIs.
- **Explicit Network Flow**: Clear state management with `None`, `Local`, `Online`, and `Dedicated` modes.

### Advanced VR Replication
- **Full Tracking Sync**: High-frequency replication of HMD and motion controller transforms.
- **Origin-Relative Logic**: Tracking is synced relative to the VR Origin, ensuring stability regardless of pawn orientation or movement.
- **Optimized Performance**: Uses unreliable Server RPCs for smooth, low-latency tracking updates.

### Smooth Locomotion and Physics
- **VR Movement Component**: Custom movement component handling locomotion, snap turning, and gravity.
- **Client-Side Prediction**: Instant responsiveness for the local player with server-reconciled movement.
- **Refined Collision**: Slim capsule collider design to minimize wall clipping and prevent physics-based "launch" bugs.

Current status (2026-05-28): collision launch behavior is still under active final-push testing for dynamic grabbable physics actors; static world collision (ground/walls) is currently stable.

### Virtual Reality Interaction
- **C++ VR Keyboard**: A high-performance QWERTY keyboard built in Slate, accessible via world-space UI.
- **Smart Focus System**: Button-driven text box targeting, allowing players to select exactly where they want to type.
- **Widget Interaction**: Pre-configured `WidgetInteractionComponent` logic for seamless UMG interaction in VR.

---

## Requirements

- **Unreal Engine**: 5.7.0+ (Tested on 5.7.2 and 5.7.3)
- **Target Platform**: Windows (Primary), Linux (Server)
- **Plugins**: Online Subsystem EOS (Optional, for online play)

---

## Installation

1. Navigate to your Unreal Engine project's `Plugins` folder (create it if it doesn't exist).
2. Clone or copy the OpenXrMultiplayer plugin into that folder:
   ```bash
   git clone https://github.com/jrcz-data-science-lab/EduXR-Multiplayer.git OpenXrMultiplayer
   ```
3. Restart the Unreal Editor.
4. Enable the **OpenXR Multiplayer** plugin in `Edit -> Plugins` (search for "OpenXR Multiplayer").

### Building for Linux (Dedicated Server)

For detailed instructions on cross-compiling your Unreal Engine project for Linux, refer to this guide:
- **[Unreal Engine Linux Cross-Compilation Guide](https://www.youtube.com/watch?v=kzVzy87qELQ)**

This covers setting up the toolchain and packaging dedicated server builds.

---

## Quick Start

### 1. Game Instance Configuration
Set your project's Game Instance to inherit from `XrMpGameInstance`.
- **Blueprint**: Create a new Blueprint based on `XrMpGameInstance` and set it in `Project Settings -> Maps & Modes`.

### 2. Pawn Setup
Use `CustomXrPawn` as the base class for your VR player. This pawn includes all necessary components for replicated VR movement and tracking.

### 3. Network Configuration
Add the following to your `Config/DefaultEngine.ini` to ensure proper net driver routing:

```ini
[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/SocketSubsystemEOS.NetDriverEOSBase")

[OnlineSubsystem]
DefaultPlatformService=Null

[OnlineSubsystemEOS]
bEnabled=true

[OnlineSubsystemNull]
bEnabled=true
```

### 4. Basic Blueprint Usage

- **Host a Session**: Use the `Host Session` node. Specify `Max Players`, `Is LAN`, and `Server Name`.
- **Find Sessions**: Use the `Find Sessions` node. Bind to `OnFindSessionsComplete_BP` to receive results.
- **Join Session**: Pass a session result to the `Join Session` node.

### 5. Dedicated Registry Configuration
Use your `XrMpGameInstance`-derived Blueprint defaults to set dedicated registry values:

- `SessionRegistryBaseUrl`: Example `http://<registry-ip>:8080`
- `SessionRegistryToken`: Your registry bearer token
- Optional: `ConnectAddress`, `ConnectPort`, `MaxPlayers`

If your dedicated server receives `SESSION_ID` at launch, `StartDedicatedRegistryHeartbeat` can send heartbeat and player count updates to the registry.

Note: Player-count timing issues were addressed on 2026-05-28 by deploying API-side player-events and improving server session registration/flush logic. If you are running older server builds, please pull the latest server build to ensure reliable player count reporting.

### 6. Blueprint Screenshots

- `gameinstance-dedicated-settings.png`
- `gamemode-playercount-flow.png`
- `gamemode-heartbeat-flow.png`

![GameInstance Dedicated Settings](Docs/Images/gameinstance-dedicated-settings.png)
![GameMode Player Count Flow](Docs/Images/gamemode-playercount-flow.png)
![GameMode Heartbeat Flow](Docs/Images/gamemode-heartbeat-flow.png)

---

## Architecture and How It Works

### Network Mode Flow
The plugin initializes in `EXrNetworkMode::None`. The player must explicitly choose a mode (Local, Online, or Dedicated) before hosting or searching. This prevents unwanted EOS login prompts during LAN-only sessions.

### Tracking Replication
The `CustomXrPawn` captures local HMD and controller transforms relative to the `VrOrigin` and sends them to the server via `ServerUpdateVRTransforms`. Other clients then interpolate these values onto their proxy pawns, ensuring smooth visual representation.

---

## API Reference Summary

### XrMpGameInstance
Networking Functions:
- `SetNetworkMode(EXrNetworkMode NewMode)` - Switches between `Local` (LAN), `Online` (EOS P2P), or `Dedicated` server mode
- `GetNetworkMode()` - Returns the currently active network mode
- `IsLoggedIntoEOS()` - Returns true if successfully authenticated with Epic Online Services
- `LoginOnlineService()` - Initiates EOS login (Online mode only)

Session Management:
- `HostSession(MaxPlayers, bIsLan, ServerName)` - Creates and hosts a multiplayer session
- `FindSessions(MaxSearchResults, bIsLan)` - Searches for available sessions; results provided via `OnFindSessionsComplete_BP` delegate
- `JoinSession(SessionIndex)` - Joins a session by index from the last search results
- `JoinSessionByIP(HostIPAddress, Port)` - Joins by direct IP address (fallback method)
- `LeaveDedicatedServer()` - Leaves a dedicated session on the client and returns to menu flow
- `DestroyCurrentSession()` - Destroys the active session
- `GetSessionSearchResults()` - Returns cached array of `FXrMpSessionResult` from the last search
- `IsSearchingForSessions()` - Returns true while a search is in progress (useful for UI spinners)
- `GetSessionId()` - Returns the current session ID

Dedicated Server Support:
- `SetDedicatedServerApiConfig(BaseUrl, ApiToken)` - Configure registry API connection at runtime
- `GetSessionRegistryToken()` - Returns the configured bearer token
- `GetDedicatedSessionId()` - Returns the session ID assigned by the registry
- `StartDedicatedRegistryHeartbeat()` - Begins periodic heartbeat updates to the registry (safe to call multiple times)
- `StopDedicatedRegistryHeartbeat()` - Stops the heartbeat loop
- `NotifyDedicatedPlayerCountChanged(CurrentPlayers)` - Call when players join/leave (updates registry and triggers heartbeat refresh)
- `SendDedicatedHeartbeatUpdate()` - Sends an immediate heartbeat (regular cadence still controlled by timer)

### VR Keyboard (UVrKeyboardWidget)
- `SetTargetTextBox(UEditableTextBox*)` - Routes keyboard input to a specific text box
- `GetTargetTextBox()` - Returns the currently targeted text box
- `HasTargetTextBox()` - Returns true if a target text box is set
- `SetKeyboardText(FString)` - Sets the keyboard buffer to the given text
- `GetKeyboardText()` - Returns the current keyboard buffer contents
- `ClearKeyboardText()` - Clears the keyboard buffer
- `OnKeyboardTextChanged` - Event fired every time a key is pressed
- `OnKeyboardTextCommitted` - Event fired when Enter is pressed

### FXrMpSessionResult (Blueprint Session Data)
Structure containing:
- `ServerName` - Display name of the server
- `OwnerName` - Name of the player who hosts/owns the session
- `CurrentPlayers` - Current number of players in the session
- `MaxPlayers` - Maximum players allowed
- `PingInMs` - Latency to the server (-1 if unknown)
- `SessionIndex` - Pass this to `JoinSession()` to join this server
- `ConnectAddress` - Resolved connection address (dedicated servers)
- `SessionId` - Dedicated server session ID for registry heartbeats

---

## License
This project is licensed under the terms provided in the [LICENSE](LICENSE) file.

---
*Developed for the EduXR Ecosystem by the JRCZ Data Science Lab.*

