    # EduXR Multiplayer Plugin
    
    A multiplayer plugin for Unreal Engine 5 designed for educational XR experiences. Provides seamless integration with both Epic Online Services (EOS) and local/LAN networking using the Null subsystem, with full VR head and hand tracking replication.
    
    > **⚠️ Status:** Beta (v0.4.0) - Still in active development. Physics collision rework, world-space UI interaction, gravity/jump, and depenetration resolution added.
    
    ## Features
    
    - 🎮 **Dual Network Mode Support**
      - Epic Online Services (EOS) for online P2P multiplayer
      - Null subsystem for local/LAN multiplayer
      - Automatic fallback based on login status
    
    - 🔄 **Smart Subsystem Detection**
      - Automatically uses Null subsystem (LAN) if not logged into EOS
      - Switches to EOS networking when user explicitly logs in
      - Seamless switching between network modes
    
    - 🕶️ **VR Tracking Replication**
      - Head (HMD) position and rotation synced across all players
      - Left and right hand (motion controller) transforms synced
      - Relative-to-VrOrigin transform space — rotation-independent, works regardless of pawn facing
      - Non-local pawns disable HMD lock and controller tracking to prevent engine override
      - High-frequency unreliable Server RPCs for smooth tracking updates
    
    - 🏃 **Multiplayer Movement**
      - Smooth forward/backward and strafe locomotion via VrMovementComponent
      - Snap turn with deadzone and cooldown
      - Movement replicated via Server RPCs + Unreal's built-in `bReplicateMovement`
      - Client-side prediction for instant local responsiveness
    
    - 🪂 **Gravity & Jump**
      - Full gravity simulation with configurable acceleration (-980 cm/s² default)
      - Ground detection via capsule-bottom line trace
      - Jump support with network replication (reliable Server RPC)
      - Terminal velocity cap and automatic floor-snapping on landing
    
    - 🧱 **Physics Collision Rework**
      - Slim capsule collider (20cm radius) — no more clipping walls at a distance
      - Physics objects (cubes, guns, balls) overlap instead of block — no more being launched at insane speeds
      - Impulse-based push on overlap — walking into physics objects nudges them away naturally
      - Automatic depenetration resolution — escapes embedded geometry without freezing movement
    
    - 🖐️ **World-Space UI Interaction**
      - WidgetInteractionComponent on each motion controller for pointing at world-space UMG widgets
      - Trigger input bindings simulate mouse press/release on UI elements
      - Configurable interaction distance and debug visualization
      - World-placed interactive menus via Blueprint widgets
    
    - 🛠️ **Developer Friendly**
      - Blueprint-accessible functions for session management
      - Automatic net driver configuration
      - Comprehensive `/** */` comments on every function and property
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
    - **Epic Online Services (EOS) SDK:** Optional — for online multiplayer features
    
    ## Installation
    
    ### Clone to Plugins Folder
    
    1. Navigate to your project's `Plugins` folder (create it if it doesn't exist)
    2. Clone this repository:
       ```bash
       git clone https://github.com/jrcz-data-science-lab/EduXR-Multiplayer.git
       ```
    3. Restart Unreal Editor
    4. Enable the plugin in Edit → Plugins → Project → Networking
    
    ## Included Content
    
    The plugin includes blueprints, UI components, and test levels **(NOT FINAL)**:
    
    ### Content Directory Structure
    - **Blueprints/** — Blueprint examples and utility actors for multiplayer
    - **Levels/** — Test levels demonstrating multiplayer functionality
    - **UI/** — UI components for multiplayer menus and lobby screens
    
    These assets are optional — you can use the plugin's C++ classes directly without them.
    
    ## Quick Start~~~~
    
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
    
    ### 2. Set Up Your VR Pawn
    
    Use `CustomXrPawn` as the base class for your VR player pawn:
    
    - **Blueprint:** Create a Blueprint based on `CustomXrPawn`
    - The pawn comes with: VrOrigin, Camera, MotionControllerLeft/Right, HandLeft/Right meshes, HMD mesh, CapsuleCollider, PlayerMesh, VrMovementComponent
    - Assign meshes and input actions in the Blueprint defaults
    - All VR tracking and movement replication is handled automatically
    
    ### 3. Configure Network Driver (Important!)
    
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
    
    ### 4. Using in Blueprints
    
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
    
    ### VR Tracking Replication
    
    ```
    ┌─────────────────────────────────────────────────────┐
    │  LOCAL PAWN (IsLocallyControlled)                   │
    │  HMD + Controllers → Camera + MotionControllers    │
    │  Every Tick:                                        │
    │    1. Capture relative transforms (to VrOrigin)     │
    │    2. Send via ServerUpdateVRTransforms (unreliable) │
    │    3. Server stores in Rep_ properties              │
    │    4. Unreal replicates to all other clients        │
    └─────────────────────────────────────────────────────┘
    
    ┌─────────────────────────────────────────────────────┐
    │  NON-LOCAL PAWN (!IsLocallyControlled)              │
    │  Tracking disabled (no HMD lock, no controller src) │
    │  Every Tick:                                        │
    │    1. Read Rep_ transforms                          │
    │    2. Multiply by VrOrigin world transform          │
    │    3. Set Camera + MotionController world transforms│
    │    4. Attached meshes follow automatically          │
    └─────────────────────────────────────────────────────┘
    ```
    
    ### EOS Integration (Optional)
    
    If you want online multiplayer with EOS:
    
    1. Set up your EOS application in the Epic Developer Portal
    2. Configure your `DefaultEngine.ini` with EOS credentials
    3. Call `Login Online Service` before hosting/finding sessions
    4. The plugin will automatically switch to EOS networking
    
    ## API Reference
    
    ### Session Management (XrMpGameInstance)
    
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
    
    ### VR Pawn (CustomXrPawn)
    
    #### Components (created in C++)
    | Component | Type | Description |
    |-----------|------|-------------|
    | `VrOrigin` | SceneComponent | Root — VR play-space origin |
    | `Camera` | CameraComponent | Tracks HMD on local pawn |
    | `MotionControllerLeft` | MotionControllerComponent | Left hand tracking |
    | `MotionControllerRight` | MotionControllerComponent | Right hand tracking |
    | `HeadMountedDisplayMesh` | StaticMeshComponent | Visual HMD mesh (attached to Camera) |
    | `HandLeft` | SkeletalMeshComponent | Left hand mesh (attached to MotionControllerLeft) |
    | `HandRight` | SkeletalMeshComponent | Right hand mesh (attached to MotionControllerRight) |
    | `WidgetInteractionLeft` | WidgetInteractionComponent | Left hand UI interaction |
    | `WidgetInteractionRight` | WidgetInteractionComponent | Right hand UI interaction |
    | `CapsuleCollider` | CapsuleComponent | Collision capsule |
    | `PlayerMesh` | SkeletalMeshComponent | Body mesh |
    | `VrMovementComponent` | VrMovementComponent | Smooth locomotion + snap turn |
    
    #### Input Actions (set in Blueprint)
    | Property | Description |
    |----------|-------------|
    | `MoveForwardAction` | Enhanced Input action for forward/backward |
    | `MoveRightAction` | Enhanced Input action for strafe left/right |
    | `SnapTurnAction` | Enhanced Input action for snap turn |
    | `JumpAction` | Enhanced Input action for jumping |
    | `LeftTriggerAction` | Enhanced Input action for left trigger (UI press) |
    | `RightTriggerAction` | Enhanced Input action for right trigger (UI press) |
    
    ### VR Movement (VrMovementComponent)
    
    | Property | Default | Description |
    |----------|---------|-------------|
    | `moveSpeed` | 150 cm/s | Smooth locomotion speed |
    | `snapTurnDegree` | 45° | Degrees per snap turn |
    | `snapTurnDeadzone` | 0.5 | Thumbstick deadzone for snap turn |
    | `SnapTurnCooldown` | 0.3s | Cooldown before snap turn re-enables automatically |
    | `GravityZ` | -980 cm/s² | Gravity acceleration |
    | `JumpZVelocity` | 420 cm/s | Initial upward velocity when jumping |
    | `GroundTraceDistance` | 15 cm | How far past capsule bottom to trace for ground |
    
    ### Capsule Collider
    
    | Property | Value | Description |
    |----------|-------|-------------|
    | Radius | 20 cm | Slim profile to avoid clipping walls at a distance |
    | HalfHeight | 88 cm | Standard standing height (~176 cm total) |
    | PhysicsBody Response | Overlap | Physics objects pass through — pushed via impulse on overlap |
    | `PhysicsPushForce` | 500 N | Force applied to physics objects on overlap |
    
    ### Configuration Properties (XrMpGameInstance)
    
    #### `Map Url` (FString)
    The map to load when hosting a session.
    - Default: `"/Game/VRTemplate/VRTemplateMap?listen"`
    
    #### `Return To Main Menu Url` (FString)
    The map to return to when leaving a session.
    - Default: `"/Game/VRTemplate/VRTemplateMap"`
    
    #### `Custom Username` (FString)
    Optional custom username to display in sessions.
    
    ## Testing Multiplayer Locally
    
    To test VR multiplayer with two instances on the same machine:
    
    1. **Package the project** or use standalone builds
    2. Launch **Instance 1** normally (with VR headset connected)
    3. Launch **Instance 2** with the `-nohmd` flag for desktop spectator mode
    4. Host on one instance, find + join on the other
    5. VR tracking from the headset user should replicate to the other instance
    
    > **Note:** PIE (Play-in-Editor) multiplayer is not supported. Use standalone builds for testing.
    
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
    
    ### Hands Appear at World Origin on Remote Players
    - Ensure `BeginPlay` disables tracking on non-local pawns (this is done automatically in CustomXrPawn)
    - If using a Blueprint child, make sure you call `Super::BeginPlay`
    
    ### Player Launched at Extreme Speed When Picking Up Objects
    - This happens when the capsule uses `ECR_Block` on the `PhysicsBody` channel
    - The fix (applied in v0.4.0): capsule uses `ECR_Overlap` for `PhysicsBody` and `Destructible`
    - If using a Blueprint child, ensure it does not override collision settings back to blocking
    
    ### Player Clips Walls Despite Being Far Away
    - The default Pawn capsule radius (34cm) is too wide for VR — reduced to 20cm in v0.4.0
    - If the Blueprint CDO saved the old 34cm value, the BeginPlay reinforcement will override it
    
    ### LNK2019 for EKeys::LeftMouseButton
    - Add `"InputCore"` to `PublicDependencyModuleNames` in your `.Build.cs`
    - This module provides the `EKeys` struct definitions
    
    ### Replication Only Works When VR is Host
    - Ensure the pawn has `bReplicates = true` and `SetReplicateMovement(true)`
    - Ensure Server RPCs are declared with `UFUNCTION(Server, ...)` in the header
    - Check that the pawn is possessed by a PlayerController (needed for RPC ownership)
    
    ## Development
    
    ### Building from Source
    
    1. Clone the repository to your project's Plugins folder
    2. Open your `.uproject` file
    3. Unreal will prompt to rebuild — click Yes
    4. The plugin will compile automatically
    
    ## Known Limitations
    
    **⚠️ Beta Status:**
    - Plugin is still in active development
    - Not all features are complete
    - Some Content assets are still being created
    - Only tested on UE 5.7.2 (other versions may have compatibility issues)
    - PIE multiplayer not functional — use standalone builds
    
    **Functional Limitations:**
    - EOS voice chat requires ≤16 players (automatic voice chat disabled for larger lobbies)
    - Seamless travel not fully supported — uses absolute travel for session creation
    - Primary testing on Windows — other platforms may need adjustments
    - Content directory assets still under development
    
    ## License
    
    **Proprietary License** — See [LICENSE](LICENSE) file for details
    
    This software is the exclusive property of JRCZ Data Science Lab and KamiDanji.
    Unauthorized access, copying, modification, or distribution is strictly prohibited.
    
    ## Credits
    
    Developed by [KamiDanji](https://github.com/KamiDanji) and the [JRCZ Data Science Lab](https://github.com/jrcz-data-science-lab) for educational XR multiplayer experiences on Unreal Engine 5.
    
    ---
    
    **Part of the EduXR project for educational and research purposes.**
    
