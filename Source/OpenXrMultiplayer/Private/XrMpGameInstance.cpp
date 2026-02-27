/**
 * EduXR Multiplayer Plugin - Game Instance Implementation
 * 
 * This file implements the core multiplayer networking functionality for educational XR experiences.
 * It handles session management, subsystem switching, and network driver configuration.
 *
 * Key Responsibilities:
 *  - Initialize online subsystems (EOS and Null)
 *  - Manage multiplayer session lifecycle
 *  - Handle network subsystem switching
 *  - Configure net drivers based on active subsystem
 *  - Track login and session state
 */

#include "XrMpGameInstance.h"

#include <string>

#include "OnlineSubsystemUtils.h"
#include "Engine/GameEngine.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"

/**
 * Constructor - Minimal initialization
 * Most setup happens in Init() when the game instance is initialized
 */
UXrMpGameInstance::UXrMpGameInstance()
{
}

/**
 * Initialize the game instance
 * 
 * Startup sequence:
 *  1. Try EOS Online Subsystem first (for online multiplayer)
 *  2. Fall back to Null subsystem if EOS not available (for LAN/local)
 *  3. Get session interface from the active subsystem
 *  4. Bind delegates for async session callbacks
 *  5. For EOS: bind login event handler to detect auto-login
 *  6. Track if user is explicitly logged in vs auto-logged by EOS
 *
 * Key Design: Start with Null subsystem for LAN, switch to EOS only on explicit LoginOnlineService() call
 * This allows the same codebase to work offline (LAN) and online (EOS) without recompilation
 */
void UXrMpGameInstance::Init()
{
	Super::Init();

	// Try EOS first (online multiplayer), fall back to Null (LAN/local)
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!OnlineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS Online Subsystem NOT Found, falling back to Null"));
		OnlineSubsystem = IOnlineSubsystem::Get(TEXT("Null"));
	}

	if (OnlineSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s Online Subsystem Found!"), *OnlineSubsystem->GetSubsystemName().ToString());
		
		// Get the session interface - this is our main API for session operations
		SessionInterface = OnlineSubsystem->GetSessionInterface();

		if (SessionInterface.IsValid())
		{
			// Bind delegates for async session callbacks
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(
				this, &UXrMpGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(
				this, &UXrMpGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(
				this, &UXrMpGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(
				this, &UXrMpGameInstance::OnJoinSessionComplete);
			SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(
				this, &UXrMpGameInstance::OnSessionUserInviteAccepted);
		}

		// If EOS is available, set up login event monitoring
		// Important: EOS may auto-login the user, so we track that separately from explicit requests
		if (OnlineSubsystem->GetSubsystemName() == FName(TEXT("EOS")))
		{
			IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface();
			if (Identity.IsValid())
			{
				// Bind to login events (handles both auto-login and explicit login)
				Identity->AddOnLoginCompleteDelegate_Handle(
					0, FOnLoginCompleteDelegate::CreateUObject(this, &UXrMpGameInstance::OnLoginComplete));

				// Check if already logged in (auto-login might have happened before delegate was bound)
				// If auto-logged in, we keep bIsLoggedIntoEOS=false until user explicitly calls LoginOnlineService()
				ELoginStatus::Type LoginStatus = Identity->GetLoginStatus(0);
				if (LoginStatus == ELoginStatus::LoggedIn)
				{
					UE_LOG(LogTemp, Warning, TEXT("EOS already logged in via auto-login, but will use Null subsystem unless you call LoginOnlineService()"));
					bIsLoggedIntoEOS = false; // Auto-login doesn't count as explicit request
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No Online Subsystem Found!"));
	}
}

/**
 * Clean up on shutdown
 * Destroys the current session before shutting down
 */
void UXrMpGameInstance::Shutdown()
{
	DestroyCurrentSession();
	Super::Shutdown();
}

/**
 * Explicitly request login to Epic Online Services
 * Sets flag that user explicitly requested login, then initiates EOS authentication
 */
void UXrMpGameInstance::LoginOnlineService()
{
	bLoginExplicitlyRequested = true;
	LoginToEOS();
}

/**
 * Internal EOS login implementation
 * Authenticates the user with EOS using either:
 *  1. Persistent/Auto-login (saved credentials)
 *  2. Account portal login (interactive)
 */
void UXrMpGameInstance::LoginToEOS()
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!OSS) return;
	IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	if (!Identity.IsValid()) return;

	// Check if already logged in from auto-login
	ELoginStatus::Type LoginStatus = Identity->GetLoginStatus(0);
	if (LoginStatus == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogTemp, Warning, TEXT("Already logged into EOS, now using EOS for multiplayer sessions"));
		bIsLoggedIntoEOS = true;
		return;
	}

	// 1) Try persistent/auto login first
	if (!Identity->AutoLogin(0))
	{
		FOnlineAccountCredentials Creds;
		Creds.Type = TEXT("accountportal");
		Creds.Id = TEXT("");
		Creds.Token = TEXT("");
		Identity->Login(0, Creds);
	}
}

/**
 * Called when EOS login attempt completes
 * 
 * Updates login state based on:
 *  - Whether login was successful
 *  - Whether user explicitly requested login or it was auto-login
 *  
 * Sets bIsLoggedIntoEOS=true ONLY if:
 *  1. Login succeeded AND
 *  2. User explicitly called LoginOnlineService()
 *  
 * This ensures we only switch to EOS networking on explicit user action
 */
void UXrMpGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
                                        const FString& Error)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS Login Success!"));
		
		// Only mark as logged in if the user explicitly requested login
		if (bLoginExplicitlyRequested)
		{
			bIsLoggedIntoEOS = true;
			UE_LOG(LogTemp, Warning, TEXT("EOS will be used for multiplayer sessions"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("EOS auto-login succeeded, but will use Null subsystem for multiplayer unless you call LoginOnlineService()"));
			bIsLoggedIntoEOS = false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("EOS Login Failed: %s. Will use Null subsystem for multiplayer."), *Error);
		bIsLoggedIntoEOS = false;
	}
}

/**
 * Get the active online subsystem based on current login state
 * 
 * Returns the appropriate subsystem:
 *  - EOS: If user is explicitly logged in and EOS is available
 *  - Null: If not logged in or EOS not available (fallback for LAN/local networking)
 *
 * Special case for PIE (Play-in-Editor):
 *  - Always returns Null subsystem to avoid socket binding issues with multiple instances
 *  - This allows seamless multiplayer testing in the editor with LAN
 *
 * @return The active online subsystem (EOS or Null)
 */
IOnlineSubsystem* UXrMpGameInstance::GetActiveOnlineSubsystem()
{
	UE_LOG(LogTemp, Warning, TEXT("GetActiveOnlineSubsystem: bIsLoggedIntoEOS=%s, bLoginExplicitlyRequested=%s"), 
		bIsLoggedIntoEOS ? TEXT("true") : TEXT("false"),
		bLoginExplicitlyRequested ? TEXT("true") : TEXT("false"));

	// In PIE mode, always use Null subsystem to avoid EOS socket binding issues
	if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		IOnlineSubsystem* NullSubsystem = IOnlineSubsystem::Get(TEXT("Null"));
		if (NullSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Using Null Online Subsystem (PIE mode detected - using LAN networking for editor multiplayer testing)"));
		}
		return NullSubsystem;
	}
		
	// If logged into EOS, try to use it
	if (bIsLoggedIntoEOS)
	{
		IOnlineSubsystem* EOSSubsystem = IOnlineSubsystem::Get(TEXT("EOS"));
		if (EOSSubsystem)
		{
			UE_LOG(LogTemp, Log, TEXT("Using EOS Online Subsystem"));
			return EOSSubsystem;
		}
	}

	// Fall back to Null subsystem if not logged into EOS or EOS not available
	IOnlineSubsystem* NullSubsystem = IOnlineSubsystem::Get(TEXT("Null"));
	if (NullSubsystem)
	{
		UE_LOG(LogTemp, Log, TEXT("Using Null Online Subsystem (local/LAN multiplayer)"));
	}
	return NullSubsystem;
}

/**
 * Configure the net driver for the active online subsystem
 * 
 * Switches between different net drivers based on the subsystem:
 *  - IpNetDriver: Used for Null subsystem (LAN, local IP networking)
 *  - NetDriverEOSBase: Used for EOS subsystem (P2P networking through EOS)
 *
 * This function modifies the GameNetDriver definition at runtime to use the appropriate driver.
 * When hosting a session, the correct driver will be initialized based on the selected subsystem.
 *
 * @param bUsingNullSubsystem True to configure for Null subsystem (LAN), false for EOS (online P2P)
 */
void UXrMpGameInstance::ConfigureNetDriverForSubsystem(bool bUsingNullSubsystem)
{
	// Get the engine's network driver configuration
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (!GameEngine)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNetDriverForSubsystem: Could not get GameEngine"));
		return;
	}

	// Find the GameNetDriver definition
	for (FNetDriverDefinition& NetDriverDef : GameEngine->NetDriverDefinitions)
	{
		if (NetDriverDef.DefName == NAME_GameNetDriver)
		{
			if (bUsingNullSubsystem)
			{
				// Use standard IP net driver for Null subsystem (LAN)
				NetDriverDef.DriverClassName = FName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
				UE_LOG(LogTemp, Warning, TEXT("Configured GameNetDriver to use IpNetDriver for Null subsystem"));
			}
			else
			{
				// Use EOS net driver for online play
				NetDriverDef.DriverClassName = FName(TEXT("/Script/SocketSubsystemEOS.NetDriverEOSBase"));
				UE_LOG(LogTemp, Warning, TEXT("Configured GameNetDriver to use NetDriverEOSBase for EOS subsystem"));
			}
			break;
		}
	}
}

/**
 * Host a new multiplayer session
 * 
 * Complete session creation workflow:
 *  1. Get the active online subsystem (EOS or Null based on login state)
 *  2. Configure the net driver for the chosen subsystem
 *  3. Create session settings (LAN vs online, voice chat, player count, etc.)
 *  4. Destroy any existing session to avoid conflicts
 *  5. Create the new session asynchronously
 *  6. When complete, OnCreateSessionComplete() is called which loads the listen map
 *
 * The process automatically selects:
 *  - EOS networking if user explicitly logged in
 *  - Null/LAN networking otherwise
 *
 * @param MaxPlayers Maximum number of players allowed in the session
 * @param bIsLan Whether this is explicitly a LAN match (parameter for compatibility)
 * @param ServerName Display name for the session in session lists
 */
void UXrMpGameInstance::HostSession(int32 MaxPlayers, bool bIsLan, FString ServerName)
{
	// Automatically determine which subsystem to use based on login status
	IOnlineSubsystem* OSS = GetActiveOnlineSubsystem();
	
	// Check if we're using EOS subsystem (not Null)
	bool bUsingEOSSubsystem = (OSS && OSS->GetSubsystemName() == FName(TEXT("EOS")));
	
	// Configure net driver for the chosen subsystem
	// EOS needs NetDriverEOSBase, Null/LAN needs IpNetDriver
	// BOTH cases must be configured explicitly because the ini default might
	// have been changed by a previous EOS session in the same game instance
	ConfigureNetDriverForSubsystem(!bUsingEOSSubsystem);
	
	if (OSS)
	{
		SessionInterface = OSS->GetSessionInterface();
		
		// Rebind delegates to the new session interface
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegates(this);
			SessionInterface->ClearOnDestroySessionCompleteDelegates(this);
			SessionInterface->ClearOnFindSessionsCompleteDelegates(this);
			SessionInterface->ClearOnJoinSessionCompleteDelegates(this);
			
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnJoinSessionComplete);
		}
	}

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: SessionInterface is invalid"));
		return;
	}


	// 1. Prepare Session Settings
	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->NumPublicConnections = MaxPlayers;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bIsLANMatch = bIsLan || !bUsingEOSSubsystem; // IP/Null is LAN, EOS is not
	SessionSettings->bUsesPresence = bUsingEOSSubsystem; // Presence is for EOS
	SessionSettings->bAllowInvites = bUsingEOSSubsystem;
	SessionSettings->bAllowJoinViaPresence = bUsingEOSSubsystem;
	SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings->bUseLobbiesIfAvailable = bUsingEOSSubsystem;
	SessionSettings->bUseLobbiesVoiceChatIfAvailable = bUsingEOSSubsystem;

	// EOS constraint: Lobbies that generate RTC conference rooms (voice) must have <= 16 players.
	// If requested MaxPlayers exceeds 16, disable lobby voice chat to allow larger lobbies.
	if (bUsingEOSSubsystem && MaxPlayers > 16 && SessionSettings->bUseLobbiesVoiceChatIfAvailable)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "MaxPlayers (%d) exceeds EOS conference room limit (16). Disabling lobby voice chat for this session."
		       ), MaxPlayers);
		SessionSettings->bUseLobbiesVoiceChatIfAvailable = false;
	}

	SessionSettings->Set(FName("SERVER_NAME"), ServerName, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings->Set(SETTING_MAPNAME, MapUrl, EOnlineDataAdvertisementType::ViaOnlineService);

	if (!CustomUsername.IsEmpty())
	{
		SessionSettings->Set(FName("PLAYER_NAME"), CustomUsername, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	// 2. Destroy existing session if it exists
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession)
	{
		PendingSessionSettings = SessionSettings;
		PendingSessionName = ServerName;
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}

	// 3. Get Local Player and UserId
	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: LocalPlayer is null"));
		return;
	}

	FUniqueNetIdRepl UserId = LocalPlayer->GetPreferredUniqueNetId();
	if (!UserId.IsValid())
	{
		if (!bUsingEOSSubsystem)
		{
			// Null/IP subsystem doesn't require authentication, so we can use the identity interface to create a user ID
			UserId = LocalPlayer->GetCachedUniqueNetId();
			if (!UserId.IsValid())
			{
				// For Null subsystem, use the identity interface to create a valid user ID
				IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
				if (Identity.IsValid())
				{
					UserId = Identity->CreateUniquePlayerId(TEXT("LocalPlayer_0"));
					UE_LOG(LogTemp, Warning, TEXT("Created dummy user ID for Null subsystem: %s"), *UserId->ToString());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("HostSession Failed: Could not get Identity interface for Null subsystem"));
					return;
				}
			}
		}
		else
		{
			// EOS and other real subsystems require authentication
			UE_LOG(LogTemp, Error, TEXT("HostSession Failed: User is not logged into Online Subsystem"));
			return;
		}
	}

	if (!UserId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: UserId is still invalid after attempts to create one"));
		return;
	}

	// 4. Create the Session
	UE_LOG(LogTemp, Warning, TEXT("Calling CreateSession with UserId: %s, ServerName: %s, MaxPlayers: %d"), 
		*UserId->ToString(), *ServerName, MaxPlayers);
	SessionInterface->CreateSession(*UserId, NAME_GameSession, *SessionSettings);
}

/**
 * Called when session creation completes asynchronously
 * 
 * If successful:
 *  - Logs success message
 *  - Travels the server to the listen map with ?listen parameter
 *  - Uses absolute travel to ensure proper net driver initialization
 *  - Players will then be able to connect to this server
 *
 * If failed:
 *  - Logs the error
 *  - Session was not created, can try again
 *
 * @param SessionName The name of the session that was created (NAME_GameSession)
 * @param bWasSuccessful Whether session creation succeeded or failed
 */
void UXrMpGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Session Created Successfully!"));
		
		// Log which subsystem and net driver we're using
		IOnlineSubsystem* OSS = GetActiveOnlineSubsystem();
		FString SubsystemName = OSS ? OSS->GetSubsystemName().ToString() : TEXT("None");
		UE_LOG(LogTemp, Warning, TEXT("Active subsystem: %s"), *SubsystemName);
		
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, 
				FString::Printf(TEXT("Session Created! Subsystem: %s"), *SubsystemName));
		}
		
		// Ensure the URL has ?listen parameter for listen server
		FString TravelURL = MapUrl;
		if (!TravelURL.Contains(TEXT("?listen")))
		{
			TravelURL += TEXT("?listen");
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Starting listen server with URL: %s"), *TravelURL);
		
		/**
		 * IMPORTANT: Use ServerTravel, NOT ClientTravel!
		 * 
		 * ServerTravel opens the map as a listen server — the engine starts the
		 * net driver, binds the listen socket, and begins accepting connections.
		 * 
		 * ClientTravel just does a local map load and does NOT start a listen server.
		 * This was causing the asymmetric join bug: the host's machine loaded the map
		 * but never opened a listen socket, so joining clients could find the EOS session
		 * but couldn't actually connect to the game server.
		 */
		UWorld* World = GetWorld();
		if (World)
		{
			World->ServerTravel(TravelURL);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to Create Session"));
	}
}

/**
 * Called when session destruction completes asynchronously
 * 
 * Handles two scenarios:
 *  1. If pending session settings exist: Recreate the session with those settings
 *     (used for transitioning between session configurations)
 *  2. Otherwise: Travel back to main menu or lobby map
 *
 * @param SessionName The name of the session that was destroyed (NAME_GameSession)
 * @param bWasSuccessful Whether session destruction succeeded or failed
 */
void UXrMpGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (PendingSessionSettings.IsValid())
		{
			const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			if (!LocalPlayer) return;

			FUniqueNetIdRepl UserId = LocalPlayer->GetPreferredUniqueNetId();
			if (!UserId.IsValid()) return;

			SessionInterface->CreateSession(*UserId, NAME_GameSession, *PendingSessionSettings);
			PendingSessionSettings.Reset();
		}
		else
		{
			// Session was destroyed or left, travel back to main menu map
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC)
			{
				PC->ClientTravel(ReturnToMainMenuUrl, ETravelType::TRAVEL_Absolute);
			}
		}
	}
}

/**
 * Search for available multiplayer sessions
 * 
 * Workflow:
 *  1. Get active online subsystem (EOS or Null)
 *  2. Create search parameters (max results, LAN query, slot availability)
 *  3. Get local player's user ID (create dummy ID for Null subsystem if needed)
 *  4. Call SessionInterface->FindSessions() asynchronously
 *  5. When complete, OnFindSessionsComplete() is called with results
 *
 * Results are stored in SessionSearch member variable for later use with JoinSession()
 *
 * @param MaxSearchResults Maximum number of session results to return
 * @param bIsLan Whether to search for LAN-only matches
 */
void UXrMpGameInstance::FindSessions(int32 MaxSearchResults, bool bIsLan)
{
	// Automatically determine which subsystem to use based on login status
	IOnlineSubsystem* OSS = GetActiveOnlineSubsystem();
	if (OSS)
	{
		SessionInterface = OSS->GetSessionInterface();
		
		// Rebind delegates to the new session interface
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegates(this);
			SessionInterface->ClearOnDestroySessionCompleteDelegates(this);
			SessionInterface->ClearOnFindSessionsCompleteDelegates(this);
			SessionInterface->ClearOnJoinSessionCompleteDelegates(this);
			
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnJoinSessionComplete);
		}
	}

	if (!SessionInterface.IsValid() || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: SessionInterface is invalid or World is null"));
		return;
	}

	// Check if we're using Null subsystem
	bool bUsingNullSubsystem = (OSS && OSS->GetSubsystemName() == FName(TEXT("Null")));

	// Configure net driver to match the subsystem we're searching with
	ConfigureNetDriverForSubsystem(bUsingNullSubsystem);

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = MaxSearchResults;
	SessionSearch->bIsLanQuery = bIsLan || bUsingNullSubsystem;

	if (!bUsingNullSubsystem)
	{
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	SessionSearch->QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE, 1, EOnlineComparisonOp::GreaterThanEquals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (LocalPlayer)
	{
		FUniqueNetIdRepl UserId = LocalPlayer->GetPreferredUniqueNetId();
		if (!UserId.IsValid())
		{
			if (bUsingNullSubsystem)
			{
				// Null subsystem doesn't require authentication, so we can use the identity interface to create a user ID
				UserId = LocalPlayer->GetCachedUniqueNetId();
				if (!UserId.IsValid())
				{
					// For Null subsystem, use the identity interface to create a valid user ID
					IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
					if (Identity.IsValid())
					{
						UserId = Identity->CreateUniquePlayerId(TEXT("LocalPlayer_0"));
						UE_LOG(LogTemp, Warning, TEXT("Created dummy user ID for Null subsystem: %s"), *UserId->ToString());
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: Could not get Identity interface for Null subsystem"));
						return;
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: User is not logged into Online Subsystem"));
				return;
			}
		}

		if (!UserId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: UserId is still invalid after attempts to create one"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("Calling FindSessions with UserId: %s, MaxResults: %d, IsLAN: %s"), 
			*UserId->ToString(), MaxSearchResults, bUsingNullSubsystem ? TEXT("true") : TEXT("false"));
		SessionInterface->FindSessions(*UserId, SessionSearch.ToSharedRef());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: LocalPlayer is null"));
	}
}


/**
 * Called when session search completes asynchronously
 * 
 * Stores search results in the SessionSearch member variable.
 * Results can then be displayed to the user or used with JoinSession()
 *
 * @param bWasSuccessful Whether the search succeeded or failed
 */
void UXrMpGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FindSessions Complete — Found %d sessions"), SessionSearch->SearchResults.Num());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			                                 FString::Printf(
				                                 TEXT("Found %d sessions"), SessionSearch->SearchResults.Num()));
		}

		// Log details for each found session (helps debug LAN discovery issues)
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
			FString ServerName;
			Result.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName);

			UE_LOG(LogTemp, Warning, TEXT("  Session[%d]: Owner=%s, ServerName=%s, OpenSlots=%d/%d, Ping=%dms"),
				i,
				*Result.Session.OwningUserName,
				*ServerName,
				Result.Session.NumOpenPublicConnections,
				Result.Session.SessionSettings.NumPublicConnections,
				Result.PingInMs);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan,
					FString::Printf(TEXT("  [%d] %s (%s) — %d/%d slots, %dms ping"),
						i, *ServerName, *Result.Session.OwningUserName,
						Result.Session.SessionSettings.NumPublicConnections - Result.Session.NumOpenPublicConnections,
						Result.Session.SessionSettings.NumPublicConnections,
						Result.PingInMs));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed! bWasSuccessful=%s, SessionSearch Valid=%s"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			SessionSearch.IsValid() ? TEXT("true") : TEXT("false"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("FindSessions Failed!"));
		}
	}
}

/**
 * Join a previously found multiplayer session
 * 
 * Workflow:
 *  1. Validate session index is within search results
 *  2. Get local player's user ID (create dummy ID for Null if needed)
 *  3. Call SessionInterface->JoinSession() with selected session
 *  4. When complete, OnJoinSessionComplete() is called with connection info
 *
 * @param SessionIndex Index of the session in SessionSearch->SearchResults to join
 */
void UXrMpGameInstance::JoinSession(int32 SessionIndex)
{
	if (!SessionInterface.IsValid() || !SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: SessionInterface or SessionSearch is invalid"));
		return;
	}
	
	if (!SessionSearch->SearchResults.IsValidIndex(SessionIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: Invalid SessionIndex %d"), SessionIndex);
		return;
	}

	// Get the active subsystem to determine if we're using Null
	IOnlineSubsystem* OSS = GetActiveOnlineSubsystem();
	bool bUsingNullSubsystem = (OSS && OSS->GetSubsystemName() == FName(TEXT("Null")));

	// Configure net driver for the chosen subsystem before joining
	// The joining client needs the same net driver type as the host
	ConfigureNetDriverForSubsystem(bUsingNullSubsystem);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: LocalPlayer is null"));
		return;
	}

	FUniqueNetIdRepl UserId = LocalPlayer->GetPreferredUniqueNetId();
	if (!UserId.IsValid())
	{
		if (bUsingNullSubsystem)
		{
			// Try cached ID first
			UserId = LocalPlayer->GetCachedUniqueNetId();
			if (!UserId.IsValid())
			{
				// Create a dummy user ID for Null subsystem
				if (OSS)
				{
					IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
					if (Identity.IsValid())
					{
						UserId = Identity->CreateUniquePlayerId(TEXT("LocalPlayer_0"));
						UE_LOG(LogTemp, Warning, TEXT("Created dummy user ID for Null subsystem: %s"), *UserId->ToString());
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: Could not get Identity interface for Null subsystem"));
						return;
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: User is not logged into Online Subsystem"));
			return;
		}
	}

	if (!UserId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: Unable to get valid User ID"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Calling JoinSession with UserId: %s, SessionIndex: %d"), 
		*UserId->ToString(), SessionIndex);
	SessionInterface->JoinSession(*UserId, NAME_GameSession, SessionSearch->SearchResults[SessionIndex]);
}

/**
 * Called when session join attempt completes asynchronously
 * 
 * If successful:
 *  - Gets the server's connection information (IP address and port)
 *  - Travels the player to the server using that connection string
 *  - Example connection string: "145.19.85.24:7777/Game/MyMap"
 *  - Uses absolute travel for clean connection
 *
 * If failed:
 *  - Logs the error
 *  - Player remains in current map, can retry or try another session
 *
 * @param SessionName The name of the session being joined (NAME_GameSession)
 * @param Result Whether the join succeeded, failed, or was cancelled
 */
void UXrMpGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString ConnectInfo;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Join Session Success! ConnectInfo: %s"), *ConnectInfo);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				                                 FString::Printf(TEXT("Joined Session! Traveling to: %s"), *ConnectInfo));
			}

			// Validate the connect string is not empty
			if (ConnectInfo.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("ConnectInfo is EMPTY — cannot travel to server!"));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: ConnectInfo is empty!"));
				return;
			}

			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC)
			{
				UE_LOG(LogTemp, Warning, TEXT("ClientTravel starting to: %s"), *ConnectInfo);
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow,
					FString::Printf(TEXT("ClientTravel to: %s"), *ConnectInfo));
				PC->ClientTravel(ConnectInfo, ETravelType::TRAVEL_Absolute);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("OnJoinSessionComplete: PlayerController is NULL — cannot travel!"));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: No PlayerController for travel!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to GetResolvedConnectString for session: %s"), *SessionName.ToString());
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
				FString::Printf(TEXT("ERROR: No ConnectString for session %s"), *SessionName.ToString()));
		}
	}
	else
	{
		FString ResultStr;
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull: ResultStr = TEXT("Session Is Full"); break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist: ResultStr = TEXT("Session Does Not Exist"); break;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: ResultStr = TEXT("Could Not Retrieve Address"); break;
		case EOnJoinSessionCompleteResult::AlreadyInSession: ResultStr = TEXT("Already In Session"); break;
		case EOnJoinSessionCompleteResult::UnknownError: ResultStr = TEXT("Unknown Error"); break;
		default: ResultStr = FString::Printf(TEXT("Result Code: %d"), (int32)Result); break;
		}
		UE_LOG(LogTemp, Error, TEXT("Join Session Failed: %s"), *ResultStr);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			FString::Printf(TEXT("Join Failed: %s"), *ResultStr));
	}
}

/**
 * Called when player accepts a session invite (EOS feature only)
 * 
 * When a friend sends an invite via EOS, this is called when the player accepts it.
 * Automatically joins the session the inviter is in.
 *
 * Currently a basic implementation - future enhancements could include:
 *  - Custom invite UI
 *  - Confirmation dialogs
 *  - Friend list integration
 *
 * @param bWasSuccessful Whether the invite was successfully accepted
 * @param LocalUserNum The local player index
 * @param UserId The invited player's ID
 * @param InviteResult The session that was invited to (contains connection info)
 */
void UXrMpGameInstance::OnSessionUserInviteAccepted(const bool bWasSuccessful, const int32 LocalUserNum,
                                                    FUniqueNetIdPtr UserId,
                                                    const FOnlineSessionSearchResult& InviteResult)
{
	if (bWasSuccessful && InviteResult.IsValid() && SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS Invite Accepted! Joining session..."));
		SessionInterface->JoinSession(LocalUserNum, NAME_GameSession, InviteResult);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("EOS Invite Accepted but was unsuccessful or result is invalid."));
	}
}

/**
 * Destroy the current active session
 * 
 * Used when player wants to leave multiplayer or shutdown the server.
 * Determines if the player is the session owner:
 *  - Owner: Destroys the session, which should kick all connected players
 *  - Client: Leaves the session
 *
 * After destruction, OnDestroySessionComplete() is called asynchronously
 * which handles returning to the main menu or recreating a new session
 */
void UXrMpGameInstance::DestroyCurrentSession()
{
	if (!SessionInterface.IsValid()) return;

	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession)
	{
		bool bIsOwner = false;
		const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
		if (LocalPlayer)
		{
			FUniqueNetIdRepl UserId = LocalPlayer->GetPreferredUniqueNetId();
			if (UserId.IsValid() && ExistingSession->OwningUserId.IsValid() && *UserId == *ExistingSession->
				OwningUserId)
			{
				bIsOwner = true;
			}
		}

		if (bIsOwner)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("Owner is destroying the session. This should kick all members in an EOS Lobby."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Client is leaving the session."));
		}

		SessionInterface->DestroySession(NAME_GameSession);
	}
}


