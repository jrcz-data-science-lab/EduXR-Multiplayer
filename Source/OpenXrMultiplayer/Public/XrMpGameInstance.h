/**
 * EduXR Multiplayer Plugin - Game Instance Header
 * 
 * Defines the core multiplayer networking class for educational XR experiences.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "XrMpGameInstance.generated.h"

// Forward declarations for internal use
class IOnlineIdentityInterface;
class FOnlineSessionSettings;
class FOnlineSessionSearch;

/**
 * UXrMpGameInstance - Educational XR Multiplayer Game Instance
 * 
 * This class manages all multiplayer networking functionality for the EduXR plugin.
 * It provides a unified interface for:
 *  - Session management (Create, Find, Join, Destroy)
 *  - Network subsystem switching (EOS vs Null/LAN)
 *  - Automatic net driver configuration
 *  - User authentication (EOS login)
 * 
 * Key Features:
 *  - Dual-mode networking: EOS (online P2P) and Null subsystem (LAN/IP)
 *  - Automatic subsystem selection: Uses LAN by default, EOS if logged in
 *  - Seamless network mode switching without code changes
 *  - Support for both standalone and packaged builds
 * 
 * Usage:
 *  1. Create a Blueprint based on this class or use directly
 *  2. Call HostSession() to create a multiplayer session
 *  3. Call FindSessions() to search for available sessions
 *  4. Call JoinSession() to connect to a found session
 *  5. Optional: Call LoginOnlineService() to switch to EOS networking
 */
UCLASS()
class OPENXRMULTIPLAYER_API UXrMpGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/**
	 * Constructor - Minimal initialization
	 * Most setup happens in Init()
	 */
	UXrMpGameInstance();

	/**
	 * Initialize the game instance
	 * Sets up subsystems and delegates for multiplayer networking
	 */
	virtual void Init() override;
	
	/**
	 * Clean up on shutdown
	 * Destroys the current session before shutting down
	 */
	virtual void Shutdown() override;

	/**
	 * Host a new multiplayer session
	 * 
	 * @param MaxPlayers Maximum number of players allowed in the session
	 * @param bIsLan Whether this is a LAN match (only for Null subsystem)
	 * @param ServerName Display name for the server
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void HostSession(int32 MaxPlayers, bool bIsLan, FString ServerName);

	/**
	 * Find available multiplayer sessions
	 * 
	 * @param MaxSearchResults Maximum number of sessions to return
	 * @param bIsLan Whether to search for LAN matches
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void FindSessions(int32 MaxSearchResults, bool bIsLan);

	/**
	 * Join a previously found session
	 * 
	 * @param SessionIndex Index of the session from the search results
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void JoinSession(int32 SessionIndex);

	/**
	 * Destroy the current active session
	 * Used to clean up before returning to main menu or when leaving multiplayer
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void DestroyCurrentSession();

	/**
	 * Explicitly login to Epic Online Services
	 * 
	 * Switches the multiplayer subsystem to EOS for online P2P networking.
	 * If already logged in via auto-login, immediately switches to EOS networking.
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void LoginOnlineService();

	/**
	 * Optional custom username to display in multiplayer sessions
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XR Multiplayer")
	FString CustomUsername;

protected:
	/**
	 * Internal EOS login implementation
	 * Authenticates the user with EOS using either persistent/auto-login or account portal
	 */
	void LoginToEOS();
	
	/**
	 * Called when EOS login attempt completes
	 * 
	 * Updates bIsLoggedIntoEOS flag based on whether login succeeded and was explicitly requested
	 * @param LocalUserNum The local player index
	 * @param bWasSuccessful Whether authentication succeeded
	 * @param UserId The authenticated user's ID
	 * @param Error Error message if authentication failed
	 */
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	/**
	 * Map URL to load when hosting a session (includes ?listen parameter for listen server)
	 * Default: /Game/VRTemplate/VRTemplateMap?listen
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XR Multiplayer")
	FString MapUrl = TEXT("/Game/VRTemplate/VRTemplateMap?listen");

	/**
	 * Map URL to return to when leaving multiplayer (no ?listen parameter)
	 * Default: /Game/VRTemplate/VRTemplateMap
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XR Multiplayer")
	FString ReturnToMainMenuUrl = TEXT("/Game/VRTemplate/VRTemplateMap");

	/**
	 * Called when session creation completes asynchronously
	 * If successful, travels to the listen map to start hosting
	 */
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	/**
	 * Called when session destruction completes asynchronously
	 * Handles cleanup or recreation of sessions
	 */
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	
	/**
	 * Called when session search completes asynchronously
	 * Stores results in SessionSearch member variable for use with JoinSession()
	 */
	void OnFindSessionsComplete(bool bWasSuccessful);
	
	/**
	 * Called when session join attempt completes asynchronously
	 * If successful, travels the player to the server
	 */
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	/**
	 * Called when player accepts a session invite (EOS only feature)
	 * Automatically joins the session the inviter is in
	 */
	void OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 LocalUserNum, FUniqueNetIdPtr UserId,
	                                 const FOnlineSessionSearchResult& InviteResult);

private:
	/**
	 * Interface to the online session system
	 * Can be EOS subsystem (online P2P) or Null subsystem (LAN/local networking)
	 */
	IOnlineSessionPtr SessionInterface;
	
	/**
	 * Results from the last session search query
	 * Populated by FindSessions(), used by JoinSession()
	 */
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	/**
	 * Delegate handles for session operations
	 * Stored to allow unbinding delegates when needed
	 */
	FDelegateHandle CreateSessionHandle;
	FDelegateHandle DestroySessionHandle;
	FDelegateHandle FindSessionsHandle;
	FDelegateHandle JoinSessionHandle;

	/**
	 * Session settings to apply when recreating a destroyed session
	 * Used for transitioning between different session configurations
	 */
	TSharedPtr<FOnlineSessionSettings> PendingSessionSettings;
	
	/**
	 * Name of the session being recreated
	 */
	FString PendingSessionName;

	/**
	 * True if currently logged into EOS and networking should use EOS
	 * False if using LAN/Null subsystem or not logged in
	 */
	bool bIsLoggedIntoEOS = false;
	
	/**
	 * True if user explicitly called LoginOnlineService()
	 * False if EOS auto-logged in or no login attempted
	 * Used to distinguish between explicit and automatic login
	 */
	bool bLoginExplicitlyRequested = false;

	/**
	 * Get the active online subsystem based on current login state
	 * Returns EOS if logged in, otherwise Null subsystem for LAN/local networking
	 * 
	 * @return The active online subsystem (EOS or Null)
	 */
	IOnlineSubsystem* GetActiveOnlineSubsystem();
	
	/**
	 * Configure the net driver for the active online subsystem
	 * Switches between IpNetDriver (Null/LAN) and NetDriverEOSBase (EOS P2P)
	 * 
	 * @param bUsingNullSubsystem True to configure for Null subsystem, false for EOS
	 */
	void ConfigureNetDriverForSubsystem(bool bUsingNullSubsystem);
};
