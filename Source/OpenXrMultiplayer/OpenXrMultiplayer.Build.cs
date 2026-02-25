// OpenXrMultiplayer.Build.cs — Build Configuration for the EduXR Multiplayer Plugin
//
// Defines module dependencies required by the plugin:
//  - Networking: Sockets, Networking, NetCore for multiplayer transport
//  - Online: OnlineSubsystem, OnlineSubsystemEOS for session management
//  - XR: OpenXR, HeadMountedDisplay, XRBase for VR tracking
//  - Input: EnhancedInput for VR controller input
//  - UI: UMG for WidgetInteractionComponent (in-world UI pointing)

using UnrealBuildTool;

public class OpenXrMultiplayer : ModuleRules
{
	public OpenXrMultiplayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);

		// Public dependencies — modules that consumers of this plugin also need.
		// These are exposed in public headers so dependents must link against them.
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"Sockets",              // Low-level socket API
				"Networking",           // Network transport layer
				"NetCore",              // Core networking types and utilities
				"OnlineSubsystem",      // Abstract online subsystem interface
				"OnlineSubsystemEOS",   // Epic Online Services subsystem
				"OnlineSubsystemUtils", // Helper utilities for online subsystems
				"OpenXR",               // OpenXR runtime integration
				"HeadMountedDisplay",   // HMD tracking and rendering API
				"XRBase",               // Base XR types (MotionControllerComponent etc.)
				"EnhancedInput",        // Enhanced Input System for VR controllers
				"UMG",                  // Widget Interaction (UWidgetInteractionComponent)
			}
		);

		// Private dependencies — only used internally by the plugin.
		// Not exposed to consumers of this module.
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",  // UObject system and reflection
				"Engine",       // Core engine classes (AActor, UWorld etc.)
				"Slate",        // Low-level UI framework
				"SlateCore",    // Slate rendering and widgets
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}