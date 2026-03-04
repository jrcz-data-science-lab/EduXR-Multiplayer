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
 * Blueprint-friendly struct representing a single session search result.
 * Exposes all the important session info so Blueprints can build custom
 * server browser widgets without touching C++.
 */
USTRUCT(BlueprintType)
struct FXrMpSessionResult
{
	GENERATED_BODY()

	/** Display name of the server (set by host via SERVER_NAME key) */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString ServerName;

	/** Name of the player who owns/hosts the session */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString OwnerName;

	/** Current number of players in the session */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	/** Maximum number of players the session supports */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;

	/** Ping to the server in milliseconds (-1 if unknown) */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 PingInMs = -1;

	/** Index into the internal search results array — pass to JoinSession() */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 SessionIndex = -1;
};

/** Multicast delegate broadcast when FindSessions completes — bind in Blueprints to update your UI */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnXrMpFindSessionsComplete, const TArray<FXrMpSessionResult>&, Results, bool, bWasSuccessful);

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

	/**
	 * Broadcast when FindSessions completes (success or failure).
	 * Bind this in Blueprints (e.g. your server-browser widget) to receive results.
	 *
	 * Example (Blueprint):
	 *   1. Get a reference to the Game Instance, cast to XrMpGameInstance
	 *   2. Bind Event to "On Find Sessions Complete"
	 *   3. In the event, iterate "Results" array to populate your UI list
	 *   4. Call JoinSession(Result.SessionIndex) when the player picks one
	 */
	UPROPERTY(BlueprintAssignable, Category = "XR Multiplayer")
	FOnXrMpFindSessionsComplete OnFindSessionsComplete_BP;

	/**
	 * Get the last set of session search results.
	 * Returns an empty array if no search has been performed or the last search failed.
	 *
	 * @return Array of Blueprint-friendly session result structs
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "XR Multiplayer")
	TArray<FXrMpSessionResult> GetSessionSearchResults() const { return CachedSearchResults; }

	/**
	 * Returns true if a session search is currently in progress.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "XR Multiplayer")
	bool IsSearchingForSessions() const { return bIsSearching; }

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

	/**
	 * Cached Blueprint-friendly session results from the last search.
	 * Populated in OnFindSessionsComplete, returned by GetSessionSearchResults().
	 */
	TArray<FXrMpSessionResult> CachedSearchResults;

	/**
	 * True while a FindSessions request is in flight.
	 * Prevents overlapping searches and lets the UI show a spinner.
	 */
	bool bIsSearching = false;
};
