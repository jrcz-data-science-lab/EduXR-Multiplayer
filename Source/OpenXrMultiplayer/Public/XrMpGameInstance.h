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
 * Explicit network mode — set by the player from the mode-selection screen.
 *
 * Local:  Uses OnlineSubsystemNull (LAN / same-network IP).
 *         EOS is never loaded, no login overlay, no internet required.
 *
 * Online: Uses OnlineSubsystemEOS (Epic Online Services P2P).
 *         Requires an explicit LoginOnlineService() call to authenticate.
 */
UENUM(BlueprintType)
enum class EXrNetworkMode : uint8
{
	/** LAN / local networking via OnlineSubsystemNull — no EOS */
	Local   UMETA(DisplayName = "Local (LAN)"),

	/** Online P2P via OnlineSubsystemEOS — requires EOS login */
	Online  UMETA(DisplayName = "Online (EOS)"),
	
	/** Default uninitialized state */
	None	UMETA(DisplayName = "None (default)")
};

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
 * Networking is fully explicit:
 *  - Call SetNetworkMode(Local)  when the player picks "Local"
 *  - Call SetNetworkMode(Online) when the player picks "Online"
 *  - In Online mode, call LoginOnlineService() to authenticate with EOS
 *  - HostSession / FindSessions / JoinSession use whichever mode is active
 *
 * EOS is NEVER loaded unless the player explicitly selects Online mode
 * and calls LoginOnlineService(). No auto-login, no overlay in Local mode.
 */
UCLASS()
class OPENXRMULTIPLAYER_API UXrMpGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// Keep base overloads visible so our Blueprint JoinSession(int32) helper does not hide UGameInstance virtual overloads.
	using UGameInstance::JoinSession;

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

	// ─────────────────────────────────────────────
	// Network Mode — call from the mode-selection screen
	// ─────────────────────────────────────────────

	/**
	 * Set the networking mode. Call this from Blueprint when the player
	 * picks "Local" or "Online" on the mode-selection screen.
	 * 
	 * None: Default so we don't handle anything on startup
	 *
	 * Local:  Switches SessionInterface to OnlineSubsystemNull.
	 *         Configures IpNetDriver. EOS is not touched at all.
	 *
	 * Online: Switches SessionInterface to OnlineSubsystemEOS.
	 *         Configures NetDriverEOSBase. You must still call
	 *         LoginOnlineService() before hosting / finding sessions.
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void SetNetworkMode(EXrNetworkMode NewMode);

	/** Returns the current network mode */
	UFUNCTION(BlueprintPure, Category = "XR Multiplayer")
	EXrNetworkMode GetNetworkMode() const { return ActiveNetworkMode; }

	/** Returns true if the player has successfully logged into EOS */
	UFUNCTION(BlueprintPure, Category = "XR Multiplayer")
	bool IsLoggedIntoEOS() const { return bIsLoggedIntoEOS; }

	// ─────────────────────────────────────────────
	// Session Management
	// ─────────────────────────────────────────────

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
	 * Join a session by direct IP address (workaround for LAN discovery issues)
	 * @param HostIPAddress IP address of the host (e.g., "192.168.1.100")
	 * @param Port Game port (default 7777)
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void JoinSessionByIP(const FString& HostIPAddress, int32 Port = 7777);

	/**
	 * Destroy the current active session
	 * Used to clean up before returning to main menu or when leaving multiplayer
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void DestroyCurrentSession();

	// ─────────────────────────────────────────────
	// EOS Login — only works in Online mode
	// ─────────────────────────────────────────────

	/**
	 * Login to Epic Online Services.
	 * Only call this after SetNetworkMode(Online).
	 * In Local mode this is a no-op.
	 */
	UFUNCTION(BlueprintCallable, Category = "XR Multiplayer")
	void LoginOnlineService();

	// ─────────────────────────────────────────────
	// Properties
	// ─────────────────────────────────────────────

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

	/** Called when StartSession completes. Useful for LAN beacon diagnostics. */
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	
	/**
	 * Called when player accepts a session invite (EOS only feature)
	 * Automatically joins the session the inviter is in
	 */
	void OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 LocalUserNum, FUniqueNetIdPtr UserId,
	                                 const FOnlineSessionSearchResult& InviteResult);

private:
	/** The explicitly chosen network mode — defaults to None for INI */
	EXrNetworkMode ActiveNetworkMode = EXrNetworkMode::None;

	/** True after a successful EOS login in Online mode */
	bool bIsLoggedIntoEOS = false;

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
	 * Switch SessionInterface + net driver to the given subsystem.
	 * Clears and rebinds all session delegates.
	 * @param SubsystemName  "Null" or "EOS"
	 */
	void ActivateSubsystem(const FName& SubsystemName);

	/**
	 * Configure the net driver for the given subsystem.
	 * @param bUsingNullSubsystem True → IpNetDriver, False → NetDriverEOSBase
	 */
	void ConfigureNetDriverForSubsystem(bool bUsingNullSubsystem);

	/** Bind all session delegates to the current SessionInterface */
	void BindSessionDelegates();

	/** Resolve a stable local user id for session create/find/join operations. */
	bool ResolveLocalSessionUserId(FUniqueNetIdRepl& OutUserId, bool bUsingEOS, const TCHAR* Context);

	/** Register the local player in a created session so OSS Null keeps membership consistent across travel. */
	void TryRegisterLocalPlayer(FName SessionName, const TCHAR* Context);

	/** Called after a map load so host sessions can be started after ServerTravel completes. */
	void OnPostLoadMapWithWorld(UWorld* LoadedWorld);

	/** Timer callback that starts the pending host session after a small Null-only delay. */
	void StartPendingSessionAfterTravel();

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

	/** Handle for the post-map-load delegate used to defer StartSession until after travel. */
	FDelegateHandle PostLoadMapWithWorldHandle;

	/** True when host flow should call StartSession after ServerTravel finishes loading. */
	bool bPendingStartSessionAfterTravel = false;

	/** Session name to start once post-travel map load completes. */
	FName PendingStartSessionName = NAME_None;

	/** Timer used for delayed StartSession after post-travel map load. */
	FTimerHandle PendingStartSessionTimerHandle;

	/** Log available network interfaces for debugging cross-device discovery. */
	void LogNetworkInterfaces(const TCHAR* Context);

	/** Delay before StartSession in Local/Null mode to avoid post-travel beacon timing races. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XR Multiplayer|LAN", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float NullStartSessionDelaySeconds = 0.35f;

	/** If set, enables verbose Null LAN beacon diagnostics in logs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XR Multiplayer|LAN|Debug", meta = (AllowPrivateAccess = "true"))
	bool bEnableLANDiagnostics = false;

	/** Latest condensed LAN diagnostics summary, useful for Blueprint UI/debug overlays. */
	UPROPERTY(BlueprintReadOnly, Category = "XR Multiplayer|LAN|Debug", meta = (AllowPrivateAccess = "true"))
	FString LastLANDiagnosticsSummary;

	/** Returns the latest condensed LAN diagnostics summary. */
	UFUNCTION(BlueprintPure, Category = "XR Multiplayer|LAN|Debug")
	FString GetLastLANDiagnosticsSummary() const { return LastLANDiagnosticsSummary; }

	/** Update the cached LAN diagnostics summary string. */
	void UpdateLANDiagnosticsSummary(const FString& Summary);
};
