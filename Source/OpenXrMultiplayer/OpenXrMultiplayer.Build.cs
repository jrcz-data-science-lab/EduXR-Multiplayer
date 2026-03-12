// OpenXrMultiplayer.Build.cs — Build Configuration for the EduXR Multiplayer Plugin
//
// Defines module dependencies required by the plugin:
//  - Online: OnlineSubsystem + OnlineSubsystemUtils for session management
//  - XR: XRBase for MotionControllerComponent
//  - Input: EnhancedInput for VR controller input
//  - UI: UMG for WidgetInteractionComponent & UMG widgets (in-world UI)

using UnrealBuildTool;

public class OpenXrMultiplayer : ModuleRules
{
	public OpenXrMultiplayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public dependencies — exposed in public headers so dependents must link against them.
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",                 // Core types used everywhere
				"OnlineSubsystem",      // IOnlineSubsystem, IOnlineSessionPtr (XrMpGameInstance.h)
				"OnlineSubsystemUtils", // OnlineSubsystemUtils.h helpers (XrMpGameInstance.h)
				"XRBase",               // UMotionControllerComponent (CustomXrPawn.h)
				"EnhancedInput",        // UInputAction, FInputActionValue (CustomXrPawn.h)
				"UMG",					// UWidgetInteractionComponent, UUserWidget, UEditableTextBox (public headers)
				"HeadMountedDisplay"	// UMotionControllerComponent (public headers)
			}
		);

		// Private dependencies — only used internally in .cpp files.
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",  // UObject system and reflection
				"Engine",       // Core engine classes (AActor, UWorld, UGameEngine etc.)
				"Slate",        // Slate widgets (SButton, SBorder etc. in VrKeyboardWidget.cpp)
				"SlateCore",    // Slate rendering primitives
				"NetCore",      // DOREPLIFETIME macros (Net/UnrealNetwork.h in CustomXrPawn.cpp)
				"InputCore",    // EKeys definitions (EKeys::LeftMouseButton in CustomXrPawn.cpp)
			}
		);
	}
}