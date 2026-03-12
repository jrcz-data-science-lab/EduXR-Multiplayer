/**
 * EduXR Multiplayer Plugin - Game Instance Implementation
 * 
 * Networking is fully explicit — the player chooses Local or Online from the
 * mode-selection screen. EOS is NEVER loaded unless the player picks Online
 * and calls LoginOnlineService(). In Local mode, only OnlineSubsystemNull
 * is used — zero EOS contact.
 */

#include "XrMpGameInstance.h"

#include "OnlineSubsystemUtils.h"
#include "Engine/GameEngine.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"

// ═══════════════════════════════════════════════════
// Construction & Lifecycle
// ═══════════════════════════════════════════════════

UXrMpGameInstance::UXrMpGameInstance()
{
}

/**
 * Init — start with Null subsystem only.
 * EOS is never touched here. DefaultPlatformService in the ini is "Null".
 */
void UXrMpGameInstance::Init()
{
	Super::Init();

	ActiveNetworkMode = EXrNetworkMode::Local;
	bIsLoggedIntoEOS = false;

	ActivateSubsystem(FName(TEXT("Null")));

	UE_LOG(LogTemp, Warning, TEXT("XrMpGameInstance::Init — Mode: Local, Subsystem: Null"));
}

void UXrMpGameInstance::Shutdown()
{
	DestroyCurrentSession();
	Super::Shutdown();
}

// ═══════════════════════════════════════════════════
// Network Mode Selection
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::SetNetworkMode(EXrNetworkMode NewMode)
{
	if (NewMode == ActiveNetworkMode)
	{
		UE_LOG(LogTemp, Log, TEXT("SetNetworkMode: Already in %s mode, no change needed"),
			NewMode == EXrNetworkMode::Local ? TEXT("Local") : TEXT("Online"));
		return;
	}

	// Destroy any existing session before switching subsystems
	DestroyCurrentSession();

	ActiveNetworkMode = NewMode;

	if (NewMode == EXrNetworkMode::Local)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNetworkMode → Local (Null subsystem)"));
		bIsLoggedIntoEOS = false;
		ActivateSubsystem(FName(TEXT("Null")));
	}
	else // Online
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNetworkMode → Online (EOS subsystem)"));
		UE_LOG(LogTemp, Warning, TEXT("  Call LoginOnlineService() to authenticate before hosting/finding sessions"));
		ActivateSubsystem(FName(TEXT("EOS")));
	}
}

// ═══════════════════════════════════════════════════
// Subsystem Activation (internal)
// ═══════════════════════════════════════════════════

/**
 * Switch the active subsystem. Gets the session interface, configures the
 * net driver, and rebinds all delegates.
 */
void UXrMpGameInstance::ActivateSubsystem(const FName& SubsystemName)
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(SubsystemName);
	if (!OSS)
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateSubsystem: %s subsystem not available!"), *SubsystemName.ToString());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ActivateSubsystem: %s"), *OSS->GetSubsystemName().ToString());

	SessionInterface = OSS->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateSubsystem: SessionInterface is invalid for %s"), *SubsystemName.ToString());
		return;
	}

	bool bIsNull = (SubsystemName == FName(TEXT("Null")));
	ConfigureNetDriverForSubsystem(bIsNull);
	BindSessionDelegates();
}

void UXrMpGameInstance::BindSessionDelegates()
{
	if (!SessionInterface.IsValid()) return;

	SessionInterface->ClearOnCreateSessionCompleteDelegates(this);
	SessionInterface->ClearOnDestroySessionCompleteDelegates(this);
	SessionInterface->ClearOnFindSessionsCompleteDelegates(this);
	SessionInterface->ClearOnJoinSessionCompleteDelegates(this);

	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnCreateSessionComplete);
	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnDestroySessionComplete);
	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnFindSessionsComplete);
	SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnJoinSessionComplete);
	SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &UXrMpGameInstance::OnSessionUserInviteAccepted);
}

// ═══════════════════════════════════════════════════
// Net Driver Configuration
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::ConfigureNetDriverForSubsystem(bool bUsingNullSubsystem)
{
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (!GameEngine)
	{
		UE_LOG(LogTemp, Error, TEXT("ConfigureNetDriverForSubsystem: Could not get GameEngine"));
		return;
	}

	for (FNetDriverDefinition& NetDriverDef : GameEngine->NetDriverDefinitions)
	{
		if (NetDriverDef.DefName == NAME_GameNetDriver)
		{
			if (bUsingNullSubsystem)
			{
				NetDriverDef.DriverClassName = FName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
				UE_LOG(LogTemp, Warning, TEXT("Net driver → IpNetDriver (LAN)"));
			}
			else
			{
				NetDriverDef.DriverClassName = FName(TEXT("/Script/SocketSubsystemEOS.NetDriverEOSBase"));
				UE_LOG(LogTemp, Warning, TEXT("Net driver → NetDriverEOSBase (EOS P2P)"));
			}
			break;
		}
	}
}

// ═══════════════════════════════════════════════════
// EOS Login
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::LoginOnlineService()
{
	if (ActiveNetworkMode != EXrNetworkMode::Online)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoginOnlineService: Ignored — current mode is Local. Call SetNetworkMode(Online) first."));
		return;
	}

	LoginToEOS();
}

void UXrMpGameInstance::LoginToEOS()
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
	if (!OSS)
	{
		UE_LOG(LogTemp, Error, TEXT("LoginToEOS: EOS Online Subsystem not available"));
		return;
	}

	IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("LoginToEOS: EOS Identity interface not available"));
		return;
	}

	// Bind the login-complete delegate
	Identity->AddOnLoginCompleteDelegate_Handle(
		0, FOnLoginCompleteDelegate::CreateUObject(this, &UXrMpGameInstance::OnLoginComplete));

	// Check if already logged in (persistent credentials from a previous session)
	ELoginStatus::Type LoginStatus = Identity->GetLoginStatus(0);
	if (LoginStatus == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogTemp, Warning, TEXT("Already logged into EOS"));
		bIsLoggedIntoEOS = true;
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("LoginToEOS: Attempting login..."));

	// Try persistent/auto login first (uses saved credentials, no UI)
	if (!Identity->AutoLogin(0))
	{
		// Fall back to account portal (opens EOS login UI — intentional in Online mode)
		FOnlineAccountCredentials Creds;
		Creds.Type = TEXT("accountportal");
		Creds.Id = TEXT("");
		Creds.Token = TEXT("");
		Identity->Login(0, Creds);
	}
}

void UXrMpGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
                                        const FString& Error)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS Login Success!"));
		bIsLoggedIntoEOS = true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("EOS Login Failed: %s"), *Error);
		bIsLoggedIntoEOS = false;
	}
}

// ═══════════════════════════════════════════════════
// Host Session
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::HostSession(int32 MaxPlayers, bool bIsLan, FString ServerName)
{
	bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);

	UE_LOG(LogTemp, Warning, TEXT("HostSession: Mode=%s, MaxPlayers=%d, ServerName=%s"),
		bUsingEOS ? TEXT("Online(EOS)") : TEXT("Local(Null)"),
		MaxPlayers, *ServerName);

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: SessionInterface is invalid"));
		return;
	}

	// Prepare Session Settings
	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->NumPublicConnections = MaxPlayers;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bIsLANMatch = !bUsingEOS;
	SessionSettings->bUsesPresence = bUsingEOS;
	SessionSettings->bAllowInvites = bUsingEOS;
	SessionSettings->bAllowJoinViaPresence = bUsingEOS;
	SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings->bUseLobbiesIfAvailable = bUsingEOS;
	SessionSettings->bUseLobbiesVoiceChatIfAvailable = bUsingEOS;
	SessionSettings->bIsDedicated = false;
	SessionSettings->bUsesStats = false;

	if (bUsingEOS && MaxPlayers > 16 && SessionSettings->bUseLobbiesVoiceChatIfAvailable)
	{
		UE_LOG(LogTemp, Warning, TEXT("MaxPlayers (%d) > 16, disabling lobby voice chat"), MaxPlayers);
		SessionSettings->bUseLobbiesVoiceChatIfAvailable = false;
	}

	SessionSettings->Set(FName("SERVER_NAME"), ServerName, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings->Set(SETTING_MAPNAME, MapUrl, EOnlineDataAdvertisementType::ViaOnlineService);

	if (!CustomUsername.IsEmpty())
	{
		SessionSettings->Set(FName("PLAYER_NAME"), CustomUsername, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	UE_LOG(LogTemp, Warning, TEXT("  bIsLANMatch=%s, bShouldAdvertise=%s, bUsesPresence=%s"),
		SessionSettings->bIsLANMatch ? TEXT("true") : TEXT("false"),
		SessionSettings->bShouldAdvertise ? TEXT("true") : TEXT("false"),
		SessionSettings->bUsesPresence ? TEXT("true") : TEXT("false"));

	// Destroy existing session if it exists
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession)
	{
		PendingSessionSettings = SessionSettings;
		PendingSessionName = ServerName;
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}

	// Get Local Player and UserId
	const ULocalPlayer* LocalPlayer = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: LocalPlayer is null"));
		return;
	}

	FUniqueNetIdRepl UserId;

	if (bUsingEOS)
	{
		IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
		if (OSS)
		{
			IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
			if (Identity.IsValid())
			{
				UserId = Identity->GetUniquePlayerId(0);
			}
		}

		if (!UserId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("HostSession Failed: Not logged into EOS or UserId invalid. Call LoginOnlineService() first."));
			return;
		}
	}
	else
	{
		UserId = LocalPlayer->GetPreferredUniqueNetId();
		if (!UserId.IsValid())
		{
			UserId = LocalPlayer->GetCachedUniqueNetId();
		}

		if (!UserId.IsValid())
		{
			IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("Null"));
			if (OSS)
			{
				IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
				if (Identity.IsValid())
				{
					UserId = Identity->CreateUniquePlayerId(TEXT("LocalPlayer_0"));
					UE_LOG(LogTemp, Warning, TEXT("Created dummy user ID for Null subsystem: %s"), *UserId->ToString());
				}
			}
		}
	}

	if (!UserId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: UserId is still invalid"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Creating session with UserId: %s"), *UserId->ToString());
	SessionInterface->CreateSession(*UserId, NAME_GameSession, *SessionSettings);
}

// ═══════════════════════════════════════════════════
// OnCreateSessionComplete
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Session Created Successfully! Mode: %s"),
			ActiveNetworkMode == EXrNetworkMode::Online ? TEXT("Online") : TEXT("Local"));

		// Verify session state and start it for LAN beacon discovery
		if (SessionInterface.IsValid())
		{
			FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
			if (Session)
			{
				UE_LOG(LogTemp, Warning, TEXT("  State: %s, bIsLANMatch: %s, Connections: %d/%d"),
					EOnlineSessionState::ToString(Session->SessionState),
					Session->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
					Session->SessionSettings.NumPublicConnections - Session->NumOpenPublicConnections,
					Session->SessionSettings.NumPublicConnections);

				// Start session to transition Pending → InProgress (enables LAN beacon)
				if (Session->SessionState == EOnlineSessionState::Pending)
				{
					UE_LOG(LogTemp, Warning, TEXT("  Starting session (Pending → InProgress)"));
					SessionInterface->StartSession(SessionName);
				}
			}
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				FString::Printf(TEXT("Session Created! Mode: %s"),
					ActiveNetworkMode == EXrNetworkMode::Online ? TEXT("Online") : TEXT("Local")));
		}

		// Travel to listen server map
		FString TravelURL = MapUrl;
		if (!TravelURL.Contains(TEXT("?listen")))
		{
			TravelURL += TEXT("?listen");
		}

		UE_LOG(LogTemp, Warning, TEXT("ServerTravel → %s"), *TravelURL);

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

// ═══════════════════════════════════════════════════
// OnDestroySessionComplete
// ═══════════════════════════════════════════════════

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
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC)
			{
				PC->ClientTravel(ReturnToMainMenuUrl, ETravelType::TRAVEL_Absolute);
			}
		}
	}
}

// ═══════════════════════════════════════════════════
// Find Sessions
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::FindSessions(int32 MaxSearchResults, bool bIsLan)
{
	bool bUsingNull = (ActiveNetworkMode == EXrNetworkMode::Local);

	UE_LOG(LogTemp, Warning, TEXT("FindSessions: Mode=%s, MaxResults=%d"),
		bUsingNull ? TEXT("Local(Null)") : TEXT("Online(EOS)"), MaxSearchResults);

	if (!SessionInterface.IsValid() || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: SessionInterface is invalid or World is null"));
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = MaxSearchResults;
	SessionSearch->bIsLanQuery = bIsLan || bUsingNull;
	SessionSearch->PingBucketSize = 100; // Standard for LAN searches

	if (!bUsingNull)
	{
		// EOS-specific query settings
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		SessionSearch->QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE, 1, EOnlineComparisonOp::GreaterThanEquals);
	}
	// Null subsystem: no extra query settings — LAN beacon doesn't support them

	UE_LOG(LogTemp, Warning, TEXT("  bIsLanQuery=%s"),
		SessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"));

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: LocalPlayer is null"));
		return;
	}

	FUniqueNetIdRepl UserId;
	bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);

	if (bUsingEOS)
	{
		IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
		if (OSS)
		{
			IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
			if (Identity.IsValid())
			{
				UserId = Identity->GetUniquePlayerId(0);
			}
		}

		if (!UserId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: Not logged into EOS or UserId invalid. Call LoginOnlineService() first."));
			return;
		}
	}
	else
	{
		UserId = LocalPlayer->GetPreferredUniqueNetId();
		if (!UserId.IsValid())
		{
			UserId = LocalPlayer->GetCachedUniqueNetId();
		}

		if (!UserId.IsValid())
		{
			IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("Null"));
			if (OSS)
			{
				IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
				if (Identity.IsValid())
				{
					UserId = Identity->CreateUniquePlayerId(TEXT("LocalPlayer_0"));
					UE_LOG(LogTemp, Warning, TEXT("Created dummy user ID for Null subsystem: %s"), *UserId->ToString());
				}
			}
		}
	}

	if (!UserId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: UserId is still invalid"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Calling FindSessions with UserId: %s"), *UserId->ToString());
	bIsSearching = true;
	SessionInterface->FindSessions(*UserId, SessionSearch.ToSharedRef());
}

// ═══════════════════════════════════════════════════
// OnFindSessionsComplete
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	bIsSearching = false;
	CachedSearchResults.Empty();

	UE_LOG(LogTemp, Warning, TEXT("OnFindSessionsComplete: Mode=%s, bWasSuccessful=%s"),
		ActiveNetworkMode == EXrNetworkMode::Local ? TEXT("Local") : TEXT("Online"),
		bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Found %d sessions"), SessionSearch->SearchResults.Num());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				FString::Printf(TEXT("Found %d sessions"), SessionSearch->SearchResults.Num()));
		}

		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
			FString ServerName;
			Result.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName);

			FXrMpSessionResult BPResult;
			BPResult.ServerName = ServerName;
			BPResult.OwnerName = Result.Session.OwningUserName;
			BPResult.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			BPResult.CurrentPlayers = BPResult.MaxPlayers - Result.Session.NumOpenPublicConnections;
			BPResult.PingInMs = Result.PingInMs;
			BPResult.SessionIndex = i;
			CachedSearchResults.Add(BPResult);

			UE_LOG(LogTemp, Warning, TEXT("  [%d] %s (%s) — %d/%d, %dms"),
				i, *ServerName, *Result.Session.OwningUserName,
				BPResult.CurrentPlayers, BPResult.MaxPlayers, Result.PingInMs);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed!"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("FindSessions Failed!"));
		}
	}

	OnFindSessionsComplete_BP.Broadcast(CachedSearchResults, bWasSuccessful);
}

// ═══════════════════════════════════════════════════
// Join Session
// ═══════════════════════════════════════════════════

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

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: LocalPlayer is null"));
		return;
	}

	bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);

	FUniqueNetIdRepl UserId;

	if (bUsingEOS)
	{
		IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("EOS"));
		if (OSS)
		{
			IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
			if (Identity.IsValid())
			{
				UserId = Identity->GetUniquePlayerId(0);
			}
		}

		if (!UserId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: Not logged into EOS or UserId invalid. Call LoginOnlineService() first."));
			return;
		}
	}
	else
	{
		UserId = LocalPlayer->GetPreferredUniqueNetId();
		if (!UserId.IsValid())
		{
			UserId = LocalPlayer->GetCachedUniqueNetId();
		}

		if (!UserId.IsValid())
		{
			IOnlineSubsystem* OSS = IOnlineSubsystem::Get(TEXT("Null"));
			if (OSS)
			{
				IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
				if (Identity.IsValid())
				{
					UserId = Identity->CreateUniquePlayerId(TEXT("LocalPlayer_0"));
				}
			}
		}
	}

	if (!UserId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSession Failed: Unable to get valid User ID"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("JoinSession: Index=%d, Mode=%s"),
		SessionIndex, bUsingEOS ? TEXT("Online") : TEXT("Local"));
	SessionInterface->JoinSession(*UserId, NAME_GameSession, SessionSearch->SearchResults[SessionIndex]);
}

// ═══════════════════════════════════════════════════
// OnJoinSessionComplete
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString ConnectInfo;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Join Success! ConnectInfo: %s"), *ConnectInfo);

			if (ConnectInfo.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("ConnectInfo is EMPTY — cannot travel!"));
				return;
			}

			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC)
			{
				UE_LOG(LogTemp, Warning, TEXT("ClientTravel → %s"), *ConnectInfo);
				PC->ClientTravel(ConnectInfo, ETravelType::TRAVEL_Absolute);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("No PlayerController for travel!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to GetResolvedConnectString for %s"), *SessionName.ToString());
		}
	}
	else
	{
		FString ResultStr;
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:            ResultStr = TEXT("Session Is Full"); break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:      ResultStr = TEXT("Session Does Not Exist"); break;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:  ResultStr = TEXT("Could Not Retrieve Address"); break;
		case EOnJoinSessionCompleteResult::AlreadyInSession:         ResultStr = TEXT("Already In Session"); break;
		case EOnJoinSessionCompleteResult::UnknownError:             ResultStr = TEXT("Unknown Error"); break;
		default: ResultStr = FString::Printf(TEXT("Code: %d"), (int32)Result); break;
		}
		UE_LOG(LogTemp, Error, TEXT("Join Failed: %s"), *ResultStr);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			FString::Printf(TEXT("Join Failed: %s"), *ResultStr));
	}
}

// ═══════════════════════════════════════════════════
// Session Invite (EOS only)
// ═══════════════════════════════════════════════════

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
		UE_LOG(LogTemp, Error, TEXT("EOS Invite was unsuccessful or result is invalid."));
	}
}

// ═══════════════════════════════════════════════════
// Destroy Session
// ═══════════════════════════════════════════════════

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
			if (UserId.IsValid() && ExistingSession->OwningUserId.IsValid() && *UserId == *ExistingSession->OwningUserId)
			{
				bIsOwner = true;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("%s session"), bIsOwner ? TEXT("Destroying") : TEXT("Leaving"));
		SessionInterface->DestroySession(NAME_GameSession);
	}
}

