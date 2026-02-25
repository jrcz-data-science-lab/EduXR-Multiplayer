/**
 * OpenXrMultiplayer.cpp — Plugin Module Implementation
 *
 * Implements the module startup/shutdown hooks for the EduXR Multiplayer plugin.
 * Currently minimal — the real logic lives in UXrMpGameInstance and ACustomXrPawn.
 */

#include "OpenXrMultiplayer.h"

#define LOCTEXT_NAMESPACE "FOpenXrMultiplayerModule"

/** Called when the module is loaded into memory by the engine */
void FOpenXrMultiplayerModule::StartupModule()
{
}

/** Called when the module is unloaded — clean up any module-level resources */
void FOpenXrMultiplayerModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOpenXrMultiplayerModule, OpenXrMultiplayer)