/**
 * OpenXrMultiplayer.h — Plugin Module Header
 *
 * Declares the module entry point for the EduXR Multiplayer plugin.
 * This is the standard Unreal module interface — StartupModule and
 * ShutdownModule are called by the engine when the plugin loads/unloads.
 *
 * The actual multiplayer logic lives in UXrMpGameInstance and ACustomXrPawn.
 * This module class is just the loader/unloader hook.
 */

#pragma once

#include "Modules/ModuleManager.h"

/**
 * FOpenXrMultiplayerModule — Plugin module entry point
 *
 * Currently a minimal implementation. Future enhancements could add:
 *  - Console commands for debugging multiplayer
 *  - Module-level settings registration
 *  - Subsystem auto-detection on startup
 */
class FOpenXrMultiplayerModule : public IModuleInterface
{
public:
	/** Called when the plugin is loaded — runs any one-time setup */
	virtual void StartupModule() override;

	/** Called when the plugin is unloaded — runs cleanup */
	virtual void ShutdownModule() override;
};
