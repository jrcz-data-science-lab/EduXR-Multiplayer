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
#include "SocketSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"

// ═══════════════════════════════════════════════════
// Construction & Lifecycle
// ═══════════════════════════════════════════════════

UXrMpGameInstance::UXrMpGameInstance()
{
}

void UXrMpGameInstance::UpdateLANDiagnosticsSummary(const FString& Summary)
{
	LastLANDiagnosticsSummary = Summary;
	if (bEnableLANDiagnostics)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] SUMMARY: %s"), *Summary);
	}
}

/**
 * Init — start with Null subsystem only.
 * EOS is never touched here. DefaultPlatformService in the ini is "Null".
 */
void UXrMpGameInstance::Init()
{
	Super::Init();

	ActiveNetworkMode = EXrNetworkMode::None;
	bIsLoggedIntoEOS = false;
	UpdateLANDiagnosticsSummary(TEXT("Init: ActiveNetworkMode=None, subsystem=Null"));

	UE_LOG(LogTemp, Warning, TEXT("Init: ActiveNetworkMode=None, bEnableLANDiagnostics=%s"), bEnableLANDiagnostics ? TEXT("true") : TEXT("false"));
	ActivateSubsystem(FName(TEXT("Null")));

	PostLoadMapWithWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UXrMpGameInstance::OnPostLoadMapWithWorld);

	if (bEnableLANDiagnostics)
	{
		LogNetworkInterfaces(TEXT("Init"));
	}

	UE_LOG(LogTemp, Warning, TEXT("XrMpGameInstance::Init — Mode: Local, Subsystem: Null"));
}

void UXrMpGameInstance::Shutdown()
{
	if (PostLoadMapWithWorldHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWithWorldHandle);
		PostLoadMapWithWorldHandle = FDelegateHandle();
	}

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
		const TCHAR* ModeText =
			NewMode == EXrNetworkMode::Local ? TEXT("Local") :
			NewMode == EXrNetworkMode::Online ? TEXT("Online") :
			NewMode == EXrNetworkMode::Dedicated ? TEXT("Dedicated") :
			TEXT("None");
		UE_LOG(LogTemp, Log, TEXT("SetNetworkMode: Already in %s mode, no change needed"), ModeText);
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
	else if (NewMode == EXrNetworkMode::Online)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNetworkMode → Online (EOS subsystem)"));
		UE_LOG(LogTemp, Warning, TEXT("  Call LoginOnlineService() to authenticate before hosting/finding sessions"));
		ActivateSubsystem(FName(TEXT("EOS")));
	}
	else if (NewMode == EXrNetworkMode::Dedicated)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNetworkMode → Dedicated (HTTP registry + direct IP connect)"));
		bIsLoggedIntoEOS = false;
		// Dedicated flow uses HTTP discovery + direct IP travel; keep IpNetDriver via Null.
		ActivateSubsystem(FName(TEXT("Null")));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetNetworkMode → None (idle)"));
		bIsLoggedIntoEOS = false;
		ActivateSubsystem(FName(TEXT("Null")));
	}
}

void UXrMpGameInstance::SetDedicatedServerApiConfig(const FString& InBaseUrl, const FString& InApiToken)
{
	DedicatedApiBaseUrl = InBaseUrl;
	DedicatedApiToken = InApiToken;
	UE_LOG(LogTemp, Warning, TEXT("SetDedicatedServerApiConfig: BaseUrl=%s, TokenSet=%s"),
		*DedicatedApiBaseUrl,
		DedicatedApiToken.IsEmpty() ? TEXT("false") : TEXT("true"));
}

FString UXrMpGameInstance::BuildDedicatedApiUrl(const FString& Route) const
{
	if (DedicatedApiBaseUrl.IsEmpty())
	{
		return FString();
	}

	FString Base = DedicatedApiBaseUrl;
	while (Base.EndsWith(TEXT("/")))
	{
		Base.RemoveFromEnd(TEXT("/"));
	}

	FString NormalizedRoute = Route;
	if (!NormalizedRoute.StartsWith(TEXT("/")))
	{
		NormalizedRoute = TEXT("/") + NormalizedRoute;
	}

	return Base + NormalizedRoute;
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
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld(), SubsystemName);
	if (!OSS)
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateSubsystem: %s subsystem not available!"), *SubsystemName.ToString());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ActivateSubsystem: %s"), *OSS->GetSubsystemName().ToString());
	UE_LOG(LogTemp, Warning, TEXT("ActivateSubsystem: OSS ptr=%p, current ActiveNetworkMode=%d"), OSS, static_cast<uint8>(ActiveNetworkMode));

	SessionInterface = OSS->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ActivateSubsystem: SessionInterface is invalid for %s"), *SubsystemName.ToString());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ActivateSubsystem: SessionInterface valid, configuring net driver for %s"), *SubsystemName.ToString());

	const bool bIsNull = SubsystemName.ToString().Equals(TEXT("Null"), ESearchCase::IgnoreCase);
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
	SessionInterface->ClearOnStartSessionCompleteDelegates(this);

	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnCreateSessionComplete);
	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnDestroySessionComplete);
	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnFindSessionsComplete);
	SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnJoinSessionComplete);
	SessionInterface->OnStartSessionCompleteDelegates.AddUObject(this, &UXrMpGameInstance::OnStartSessionComplete);
	SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &UXrMpGameInstance::OnSessionUserInviteAccepted);
}

bool UXrMpGameInstance::ResolveLocalSessionUserId(FUniqueNetIdRepl& OutUserId, bool bUsingEOS, const TCHAR* Context)
{
	OutUserId = FUniqueNetIdRepl();

	const UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("%s: Resolving user id (bUsingEOS=%s, World=%s, LocalPlayer=%s)"),
		Context,
		bUsingEOS ? TEXT("true") : TEXT("false"),
		World ? *World->GetName() : TEXT("<null>"),
		LocalPlayer ? TEXT("valid") : TEXT("<null>"));
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("%s Failed: LocalPlayer is null"), Context);
		return false;
	}

	if (bUsingEOS)
	{
		IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS")));
		if (OSS)
		{
			IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
			if (Identity.IsValid())
			{
				OutUserId = Identity->GetUniquePlayerId(0);
			}
		}

		if (!OutUserId.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("%s Failed: Not logged into EOS or UserId invalid. Call LoginOnlineService() first."), Context);
			return false;
		}

		UE_LOG(LogTemp, Warning, TEXT("%s: Resolved EOS user id = %s"), Context, *OutUserId->ToString());
		return true;
	}

	OutUserId = LocalPlayer->GetPreferredUniqueNetId();
	UE_LOG(LogTemp, Warning, TEXT("%s: Preferred NetId valid=%s"), Context, OutUserId.IsValid() ? TEXT("true") : TEXT("false"));
	if (!OutUserId.IsValid())
	{
		OutUserId = LocalPlayer->GetCachedUniqueNetId();
		UE_LOG(LogTemp, Warning, TEXT("%s: Cached NetId valid=%s"), Context, OutUserId.IsValid() ? TEXT("true") : TEXT("false"));
	}

	if (!OutUserId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s Failed: Local UserId is invalid in Local(Null) mode"), Context);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("%s: Resolved Local(Null) user id = %s"), Context, *OutUserId->ToString());
	return true;
}

void UXrMpGameInstance::TryRegisterLocalPlayer(FName SessionName, const TCHAR* Context)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Skipping RegisterPlayer because SessionInterface is invalid"), Context);
		return;
	}

	const bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);
	FUniqueNetIdRepl UserId;
	if (!ResolveLocalSessionUserId(UserId, bUsingEOS, Context) || !UserId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Skipping RegisterPlayer because user id could not be resolved"), Context);
		return;
	}

	if (!UserId.GetUniqueNetId().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Cannot register local player because UniqueNetId is invalid"), Context);
		return;
	}

	const bool bRegisterRequested = SessionInterface->RegisterPlayer(SessionName, *UserId.GetUniqueNetId(), false);
	UE_LOG(LogTemp, Warning, TEXT("%s: RegisterPlayer(%s) request result: %s"),
		Context,
		*SessionName.ToString(),
		bRegisterRequested ? TEXT("true") : TEXT("false"));
}

void UXrMpGameInstance::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
	UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: LoadedWorld=%s, pendingStart=%s, pendingSession=%s"),
		LoadedWorld ? *LoadedWorld->GetName() : TEXT("<null>"),
		bPendingStartSessionAfterTravel ? TEXT("true") : TEXT("false"),
		*PendingStartSessionName.ToString());

	if (!bPendingStartSessionAfterTravel || !SessionInterface.IsValid() || PendingStartSessionName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: Ignoring (SessionInterfaceValid=%s, PendingSessionNameNone=%s)"),
			SessionInterface.IsValid() ? TEXT("true") : TEXT("false"),
			PendingStartSessionName.IsNone() ? TEXT("true") : TEXT("false"));
		return;
	}

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(PendingStartSessionName);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: Pending start ignored because session %s no longer exists"),
			*PendingStartSessionName.ToString());
		bPendingStartSessionAfterTravel = false;
		PendingStartSessionName = NAME_None;
		return;
	}

	if (Session->SessionState == EOnlineSessionState::Pending)
	{
		TryRegisterLocalPlayer(PendingStartSessionName, TEXT("OnPostLoadMapWithWorld"));

		UWorld* World = GetWorld();
		if (World)
		{
			UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: Scheduling StartSession for %s in %.2fs (World=%s, SessionState=%s)"),
				*PendingStartSessionName.ToString(),
				NullStartSessionDelaySeconds,
				*World->GetName(),
				EOnlineSessionState::ToString(Session->SessionState));
			World->GetTimerManager().SetTimer(
				PendingStartSessionTimerHandle,
				this,
				&UXrMpGameInstance::StartPendingSessionAfterTravel,
				NullStartSessionDelaySeconds,
				false);
			UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: Timer armed, handle valid=%s"), PendingStartSessionTimerHandle.IsValid() ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: No world for timer, starting session %s immediately"),
				*PendingStartSessionName.ToString());
			const bool bStartRequested = SessionInterface->StartSession(PendingStartSessionName);
			UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: StartSession request result: %s"),
				bStartRequested ? TEXT("true") : TEXT("false"));
			bPendingStartSessionAfterTravel = false;
			PendingStartSessionName = NAME_None;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OnPostLoadMapWithWorld: Session %s already in state %s"),
			*PendingStartSessionName.ToString(), EOnlineSessionState::ToString(Session->SessionState));
		bPendingStartSessionAfterTravel = false;
		PendingStartSessionName = NAME_None;
	}
}

void UXrMpGameInstance::StartPendingSessionAfterTravel()
{
	UE_LOG(LogTemp, Warning, TEXT("StartPendingSessionAfterTravel: pendingStart=%s, pendingSession=%s"),
		bPendingStartSessionAfterTravel ? TEXT("true") : TEXT("false"),
		*PendingStartSessionName.ToString());

	if (!bPendingStartSessionAfterTravel || !SessionInterface.IsValid() || PendingStartSessionName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartPendingSessionAfterTravel: Ignored (SessionInterfaceValid=%s, PendingSessionNameNone=%s)"),
			SessionInterface.IsValid() ? TEXT("true") : TEXT("false"),
			PendingStartSessionName.IsNone() ? TEXT("true") : TEXT("false"));
		return;
	}

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(PendingStartSessionName);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartPendingSessionAfterTravel: Session %s no longer exists"),
			*PendingStartSessionName.ToString());
		bPendingStartSessionAfterTravel = false;
		PendingStartSessionName = NAME_None;
		return;
	}

	if (Session->SessionState == EOnlineSessionState::Pending)
	{
		TryRegisterLocalPlayer(PendingStartSessionName, TEXT("StartPendingSessionAfterTravel"));

		UE_LOG(LogTemp, Warning, TEXT("StartPendingSessionAfterTravel: Starting session %s (State=%s, LAN=%s, BuildUniqueId=%d, OpenPublic=%d/%d)"),
			*PendingStartSessionName.ToString(),
			EOnlineSessionState::ToString(Session->SessionState),
			Session->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
			Session->SessionSettings.BuildUniqueId,
			Session->NumOpenPublicConnections,
			Session->SessionSettings.NumPublicConnections);
		const bool bStartRequested = SessionInterface->StartSession(PendingStartSessionName);
		UE_LOG(LogTemp, Warning, TEXT("StartPendingSessionAfterTravel: StartSession request result: %s"),
			bStartRequested ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StartPendingSessionAfterTravel: Session %s already in state %s"),
			*PendingStartSessionName.ToString(), EOnlineSessionState::ToString(Session->SessionState));
	}

	bPendingStartSessionAfterTravel = false;
	PendingStartSessionName = NAME_None;
	PendingStartSessionTimerHandle.Invalidate();
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
		if (NetDriverDef.DefName.ToString().Equals(TEXT("GameNetDriver"), ESearchCase::IgnoreCase))
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
			UE_LOG(LogTemp, Warning, TEXT("ConfigureNetDriverForSubsystem: GameNetDriver=%s, DriverClassName=%s"),
				*NetDriverDef.DefName.ToString(),
				*NetDriverDef.DriverClassName.ToString());
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
		UE_LOG(LogTemp, Warning, TEXT("LoginOnlineService: Ignored — current mode is not Online. Call SetNetworkMode(Online) first."));
		return;
	}

	LoginToEOS();
}

void UXrMpGameInstance::LoginToEOS()
{
	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld(), TEXT("EOS"));
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
	UE_LOG(LogTemp, Warning, TEXT("LoginToEOS: bound login-complete delegate, local user 0 status=%d"), static_cast<int32>(Identity->GetLoginStatus(0)));

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
	UE_LOG(LogTemp, Warning, TEXT("OnLoginComplete: LocalUserNum=%d, bWasSuccessful=%s, UserId=%s, Error=%s"),
		LocalUserNum,
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		*UserId.ToString(),
		Error.IsEmpty() ? TEXT("<none>") : *Error);

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

void UXrMpGameInstance::HostSession(int32 InMaxPlayers, bool bIsLan, FString ServerName)
{
	UE_LOG(LogTemp, Warning, TEXT("HostSession: Hosting is disabled in this build. Use the Python API / launcher to create dedicated servers; Unreal can only find and join existing servers."));
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow,
			TEXT("Hosting is disabled. Use the launcher/API to create servers."));
	}

	/*
	// Previous host implementation preserved for future re-enable:
	bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);
	const bool bUseLanMatch = !bUsingEOS;
	UWorld* World = GetWorld();
	if (bEnableLANDiagnostics)
	{
		LogDiscoveryReadiness(TEXT("HostSession"), bUseLanMatch);
	}
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("HostSession: Mode=%s, RequestedLan=%s, EffectiveLan=%s, MaxPlayers=%d, ServerName=%s"),
		bUsingEOS ? TEXT("Online(EOS)") : TEXT("Local(Null)"),
		bIsLan ? TEXT("true") : TEXT("false"),
		bUseLanMatch ? TEXT("true") : TEXT("false"),
		InMaxPlayers,
		*ServerName));

	if (bEnableLANDiagnostics)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] HostSession starting - network diagnostics enabled"));
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] This machine should advertise on beacon port 15000"));
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] World=%s, NetMode=%d, MapUrl=%s"),
			World ? *World->GetName() : TEXT("<null>"),
			World ? static_cast<int32>(World->GetNetMode()) : -1,
			*MapUrl);
	}

	if (bEnableLANDiagnostics && bUseLanMatch)
	{
		LogNetworkInterfaces(TEXT("HostSession"));
	}

	UE_LOG(LogTemp, Warning, TEXT("HostSession: Mode=%s, RequestedLan=%s, EffectiveLan=%s, MaxPlayers=%d, ServerName=%s"),
		bUsingEOS ? TEXT("Online(EOS)") : TEXT("Local(Null)"),
		bIsLan ? TEXT("true") : TEXT("false"),
		bUseLanMatch ? TEXT("true") : TEXT("false"),
		InMaxPlayers, *ServerName);
	UE_LOG(LogTemp, Warning, TEXT("HostSession: ActiveNetworkMode=%d, SessionInterfaceValid=%s, World=%s"),
		static_cast<uint8>(ActiveNetworkMode),
		SessionInterface.IsValid() ? TEXT("true") : TEXT("false"),
		World ? *World->GetName() : TEXT("<null>"));

	if (bUsingEOS && bIsLan)
	{
		UE_LOG(LogTemp, Warning, TEXT("HostSession: bIsLan=true was requested in Online mode and will be ignored."));
	}

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("HostSession Failed: SessionInterface is invalid"));
		return;
	}

	// Prepare Session Settings
	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->NumPublicConnections = InMaxPlayers;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bIsLANMatch = bUseLanMatch;
	SessionSettings->bUsesPresence = bUsingEOS;
	SessionSettings->bAllowInvites = bUsingEOS;
	SessionSettings->bAllowJoinViaPresence = bUsingEOS;
	SessionSettings->bAllowJoinViaPresenceFriendsOnly = false;
	SessionSettings->bUseLobbiesIfAvailable = bUsingEOS;
	SessionSettings->bUseLobbiesVoiceChatIfAvailable = bUsingEOS;
	SessionSettings->bIsDedicated = false;
	SessionSettings->bUsesStats = false;
	if (bUseLanMatch)
	{
		// Keep LAN discovery cross-build/platform friendly while using OSS Null.
		SessionSettings->BuildUniqueId = 1;
	}

	if (bUsingEOS && InMaxPlayers > 16 && SessionSettings->bUseLobbiesVoiceChatIfAvailable)
	{
		UE_LOG(LogTemp, Warning, TEXT("MaxPlayers (%d) > 16, disabling lobby voice chat"), InMaxPlayers);
		SessionSettings->bUseLobbiesVoiceChatIfAvailable = false;
	}

	SessionSettings->Set(FName("SERVER_NAME"), ServerName, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings->Set(SETTING_MAPNAME, MapUrl, EOnlineDataAdvertisementType::ViaOnlineService);

	if (!CustomUsername.IsEmpty())
	{
		SessionSettings->Set(FName("PLAYER_NAME"), CustomUsername, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	UE_LOG(LogTemp, Warning, TEXT("  bIsLANMatch=%s, bShouldAdvertise=%s, bUsesPresence=%s, BuildUniqueId=%d"),
		SessionSettings->bIsLANMatch ? TEXT("true") : TEXT("false"),
		SessionSettings->bShouldAdvertise ? TEXT("true") : TEXT("false"),
		SessionSettings->bUsesPresence ? TEXT("true") : TEXT("false"),
		SessionSettings->BuildUniqueId);
	UE_LOG(LogTemp, Warning, TEXT("  bAllowJoinInProgress=%s, bAllowInvites=%s, bAllowJoinViaPresence=%s, bUseLobbiesIfAvailable=%s, bUseLobbiesVoiceChatIfAvailable=%s, bIsDedicated=%s, bUsesStats=%s"),
		SessionSettings->bAllowJoinInProgress ? TEXT("true") : TEXT("false"),
		SessionSettings->bAllowInvites ? TEXT("true") : TEXT("false"),
		SessionSettings->bAllowJoinViaPresence ? TEXT("true") : TEXT("false"),
		SessionSettings->bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"),
		SessionSettings->bUseLobbiesVoiceChatIfAvailable ? TEXT("true") : TEXT("false"),
		SessionSettings->bIsDedicated ? TEXT("true") : TEXT("false"),
		SessionSettings->bUsesStats ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("  MapUrl=%s, ReturnToMainMenuUrl=%s"), *MapUrl, *ReturnToMainMenuUrl);

	// Destroy existing session if it exists
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession)
	{
		UE_LOG(LogTemp, Warning, TEXT("HostSession: Existing session found, destroying before recreate. State=%s, LAN=%s, BuildUniqueId=%d, OpenPublic=%d/%d"),
			EOnlineSessionState::ToString(ExistingSession->SessionState),
			ExistingSession->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
			ExistingSession->SessionSettings.BuildUniqueId,
			ExistingSession->NumOpenPublicConnections,
			ExistingSession->SessionSettings.NumPublicConnections);
		PendingSessionSettings = SessionSettings;
		PendingSessionName = ServerName;
		UE_LOG(LogTemp, Warning, TEXT("HostSession: Pending recreate stored (ServerName=%s)"), *PendingSessionName);
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}

	// Get Local Player and UserId
	FUniqueNetIdRepl UserId;
	if (!ResolveLocalSessionUserId(UserId, bUsingEOS, TEXT("HostSession")))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Creating session with UserId: %s"), *UserId->ToString());
	UE_LOG(LogTemp, Warning, TEXT("HostSession: CreateSession request -> SessionName=GameSession, Search settings advertised key count may follow in OnCreateSessionComplete"));
	SessionInterface->CreateSession(*UserId, NAME_GameSession, *SessionSettings);
}

void UXrMpGameInstance::HostDedicatedSession(int32 InMaxPlayers, const FString& ServerName)
{
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("HostDedicatedSession: disabled in Unreal (ServerName=%s, MaxPlayers=%d)"), *ServerName, InMaxPlayers));
	UE_LOG(LogTemp, Warning, TEXT("HostDedicatedSession: Dedicated server creation is disabled in Unreal. Use the Python API / launcher flow instead. ServerName=%s, MaxPlayers=%d"), *ServerName, InMaxPlayers);

	/*
	// Previous dedicated-host implementation preserved for future re-enable:
	const FString CreateUrl = BuildDedicatedApiUrl(DedicatedApiCreateRoute);
	if (CreateUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("HostDedicatedSession: DedicatedApiBaseUrl is empty, using fallback host if configured"));
		if (!DedicatedFallbackHost.IsEmpty())
		{
			JoinSessionByIP(DedicatedFallbackHost, DedicatedFallbackPort);
		}
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(CreateUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!DedicatedApiToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *DedicatedApiToken));
	}
	Request->SetTimeout(DedicatedApiTimeoutSeconds);

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("serverName"), ServerName);
	Payload->SetNumberField(TEXT("maxPlayers"), InMaxPlayers);
	Payload->SetStringField(TEXT("map"), MapUrl);
	Payload->SetNumberField(TEXT("buildUniqueId"), 1);
	Payload->SetStringField(TEXT("mode"), TEXT("dedicated"));

	FString PayloadJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Payload, Writer);
	Request->SetContentAsString(PayloadJson);

	TWeakObjectPtr<UXrMpGameInstance> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			UXrMpGameInstance* Self = WeakThis.Get();
			const int32 StatusCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : -1;
			UE_LOG(LogTemp, Warning, TEXT("HostDedicatedSession: API response bSucceeded=%s, Status=%d"),
				bSucceeded ? TEXT("true") : TEXT("false"),
				StatusCode);

			if (!bSucceeded || !HttpResponse.IsValid() || !EHttpResponseCodes::IsOk(StatusCode))
			{
				UE_LOG(LogTemp, Error, TEXT("HostDedicatedSession: Create request failed. Body=%s"),
					HttpResponse.IsValid() ? *HttpResponse->GetContentAsString() : TEXT("<no response>"));
				return;
			}

			FString ConnectString;
			TSharedPtr<FJsonObject> JsonResponse;
			const FString ResponseBody = HttpResponse->GetContentAsString();
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
			if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
			{
				if (!JsonResponse->TryGetStringField(TEXT("connectString"), ConnectString))
				{
					FString Address;
					int32 Port = Self->DedicatedFallbackPort;
					JsonResponse->TryGetStringField(TEXT("connectAddress"), Address);
					if (Address.IsEmpty())
					{
						JsonResponse->TryGetStringField(TEXT("address"), Address);
					}
					JsonResponse->TryGetNumberField(TEXT("connectPort"), Port);
					if (!Address.IsEmpty())
					{
						ConnectString = FString::Printf(TEXT("%s:%d"), *Address, Port);
					}
				}
			}

			if (ConnectString.IsEmpty() && !Self->DedicatedFallbackHost.IsEmpty())
			{
				ConnectString = FString::Printf(TEXT("%s:%d"), *Self->DedicatedFallbackHost, Self->DedicatedFallbackPort);
			}

			if (ConnectString.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("HostDedicatedSession: API succeeded but no connect string was returned."));
				return;
			}

			UE_LOG(LogTemp, Warning, TEXT("HostDedicatedSession: Traveling host to dedicated endpoint %s"), *ConnectString);
			if (APlayerController* PC = Self->GetWorld() ? Self->GetWorld()->GetFirstPlayerController() : nullptr)
			{
				PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
			}
		});

	UE_LOG(LogTemp, Warning, TEXT("HostDedicatedSession: POST %s"), *CreateUrl);
	Request->ProcessRequest();
	*/
}

// ═══════════════════════════════════════════════════
// Find Dedicated Sessions
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::FindDedicatedSessions(int32 MaxSearchResults)
{
	bIsSearching = true;
	CachedSearchResults.Empty();
	CachedDedicatedConnectStrings.Empty();

	const FString ListUrl = BuildDedicatedApiUrl(DedicatedApiListRoute);
	if (ListUrl.IsEmpty())
	{
		if (!DedicatedFallbackHost.IsEmpty())
		{
			FXrMpSessionResult FallbackRow;
			FallbackRow.ServerName = TEXT("Dedicated Server");
			FallbackRow.OwnerName = TEXT("On-Prem Server");
			FallbackRow.CurrentPlayers = 0;
			FallbackRow.MaxPlayers = 64;
			FallbackRow.PingInMs = -1;
			FallbackRow.SessionIndex = 0;
			FallbackRow.ConnectAddress = FString::Printf(TEXT("%s:%d"), *DedicatedFallbackHost, DedicatedFallbackPort);
			CachedSearchResults.Add(FallbackRow);
			CachedDedicatedConnectStrings.Add(FallbackRow.ConnectAddress);
			UE_LOG(LogTemp, Warning, TEXT("FindDedicatedSessions: API URL missing, exposing fallback row %s"), *FallbackRow.ConnectAddress);
		}

		bIsSearching = false;
		OnFindSessionsComplete_BP.Broadcast(CachedSearchResults, CachedSearchResults.Num() > 0);
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ListUrl);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!DedicatedApiToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *DedicatedApiToken));
	}
	Request->SetTimeout(DedicatedApiTimeoutSeconds);

	TWeakObjectPtr<UXrMpGameInstance> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, MaxSearchResults](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			UXrMpGameInstance* Self = WeakThis.Get();
			Self->bIsSearching = false;

			const int32 StatusCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : -1;
			UE_LOG(LogTemp, Warning, TEXT("FindDedicatedSessions: API response bSucceeded=%s, Status=%d"),
				bSucceeded ? TEXT("true") : TEXT("false"),
				StatusCode);

			if (!bSucceeded || !HttpResponse.IsValid() || !EHttpResponseCodes::IsOk(StatusCode))
			{
				UE_LOG(LogTemp, Error, TEXT("FindDedicatedSessions: Request failed. Body=%s"),
					HttpResponse.IsValid() ? *HttpResponse->GetContentAsString() : TEXT("<no response>"));
				Self->OnFindSessionsComplete_BP.Broadcast(Self->CachedSearchResults, false);
				return;
			}

			Self->CachedSearchResults.Empty();
			Self->CachedDedicatedConnectStrings.Empty();

			const FString ResponseBody = HttpResponse->GetContentAsString();
			TArray<TSharedPtr<FJsonValue>> SessionRows;
			TSharedPtr<FJsonObject> RootObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
			if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* SessionsField = nullptr;
				if (RootObject->TryGetArrayField(TEXT("sessions"), SessionsField) && SessionsField)
				{
					SessionRows = *SessionsField;
				}
			}
			else
			{
				Reader = TJsonReaderFactory<>::Create(ResponseBody);
				FJsonSerializer::Deserialize(Reader, SessionRows);
			}

			auto ResolveIntField = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, int32 DefaultValue)
			{
				int32 IntValue = DefaultValue;
				if (!Obj.IsValid())
				{
					return IntValue;
				}

				Obj->TryGetNumberField(FieldName, IntValue);
				FString AsString;
				if (Obj->TryGetStringField(FieldName, AsString) && !AsString.IsEmpty())
				{
					IntValue = FCString::Atoi(*AsString);
				}
				return IntValue;
			};

			int32 AddedCount = 0;
			for (const TSharedPtr<FJsonValue>& RowValue : SessionRows)
			{
				if (!RowValue.IsValid() || AddedCount >= MaxSearchResults)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> RowObject = RowValue->AsObject();
				if (!RowObject.IsValid())
				{
					continue;
				}

				FString ServerName;
				if (!RowObject->TryGetStringField(TEXT("serverName"), ServerName))
				{
					RowObject->TryGetStringField(TEXT("name"), ServerName);
				}
				if (ServerName.IsEmpty())
				{
					ServerName = TEXT("Dedicated Server");
				}

				FString OwnerName;
				if (!RowObject->TryGetStringField(TEXT("ownerName"), OwnerName))
				{
					OwnerName = TEXT("On-Prem Server");
				}

				FString ConnectString;
				if (!RowObject->TryGetStringField(TEXT("connectString"), ConnectString))
				{
					FString Address;
					int32 Port = ResolveIntField(RowObject, TEXT("port"), Self->DedicatedFallbackPort);
					if (!RowObject->TryGetStringField(TEXT("connectAddress"), Address))
					{
						if (!RowObject->TryGetStringField(TEXT("address"), Address))
						{
							RowObject->TryGetStringField(TEXT("host"), Address);
						}
					}
					Port = ResolveIntField(RowObject, TEXT("connectPort"), Port);
					if (!Address.IsEmpty())
					{
						ConnectString = FString::Printf(TEXT("%s:%d"), *Address, Port);
					}
				}

				if (ConnectString.IsEmpty())
				{
					continue;
				}

				FXrMpSessionResult ResultRow;
				ResultRow.ServerName = ServerName;
				ResultRow.OwnerName = OwnerName;
				ResultRow.CurrentPlayers = ResolveIntField(RowObject, TEXT("currentPlayers"), 0);
				ResultRow.MaxPlayers = ResolveIntField(RowObject, TEXT("maxPlayers"), 64);
				ResultRow.PingInMs = ResolveIntField(RowObject, TEXT("pingMs"), -1);
				ResultRow.SessionIndex = Self->CachedSearchResults.Num();
				ResultRow.ConnectAddress = ConnectString;

				Self->CachedDedicatedConnectStrings.Add(ConnectString);
				Self->CachedSearchResults.Add(ResultRow);
				++AddedCount;
			}

			UE_LOG(LogTemp, Warning, TEXT("FindDedicatedSessions: Parsed %d session rows"), Self->CachedSearchResults.Num());
			Self->OnFindSessionsComplete_BP.Broadcast(Self->CachedSearchResults, true);
		});

	UE_LOG(LogTemp, Warning, TEXT("FindDedicatedSessions: GET %s"), *ListUrl);
	Request->ProcessRequest();
}

// ═══════════════════════════════════════════════════
// OnCreateSessionComplete
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("OnCreateSessionComplete: Session=%s, Success=%s, PendingStartAfterTravel=%s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		bPendingStartSessionAfterTravel ? TEXT("true") : TEXT("false")));

	UE_LOG(LogTemp, Warning, TEXT("OnCreateSessionComplete: Session=%s, Success=%s, Mode=%s, PendingStartAfterTravel=%s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		ActiveNetworkMode == EXrNetworkMode::Online ? TEXT("Online") : TEXT("Local"),
		bPendingStartSessionAfterTravel ? TEXT("true") : TEXT("false"));

	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Session Created Successfully! Mode: %s"),
			ActiveNetworkMode == EXrNetworkMode::Online ? TEXT("Online") : TEXT("Local"));
		TryRegisterLocalPlayer(SessionName, TEXT("OnCreateSessionComplete"));

		// Verify session state and start it for LAN beacon discovery
		if (SessionInterface.IsValid())
		{
			FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
			if (Session)
			{
				UE_LOG(LogTemp, Warning, TEXT("  State: %s, bIsLANMatch: %s, BuildUniqueId: %d, Connections: %d/%d"),
					EOnlineSessionState::ToString(Session->SessionState),
					Session->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
					Session->SessionSettings.BuildUniqueId,
					Session->SessionSettings.NumPublicConnections - Session->NumOpenPublicConnections,
					Session->SessionSettings.NumPublicConnections);
				UE_LOG(LogTemp, Warning, TEXT("  Advertise=%s, Presence=%s, Lobbies=%s, VoiceChat=%s, OpenPublic=%d, OwningUserName=%s"),
					Session->SessionSettings.bShouldAdvertise ? TEXT("true") : TEXT("false"),
					Session->SessionSettings.bUsesPresence ? TEXT("true") : TEXT("false"),
					Session->SessionSettings.bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"),
					Session->SessionSettings.bUseLobbiesVoiceChatIfAvailable ? TEXT("true") : TEXT("false"),
					Session->NumOpenPublicConnections,
					*Session->OwningUserName);

				if (Session->SessionState == EOnlineSessionState::Pending)
				{
					UE_LOG(LogTemp, Warning, TEXT("  Deferring StartSession until map load completes"));
					bPendingStartSessionAfterTravel = true;
					PendingStartSessionName = SessionName;
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
		UE_LOG(LogTemp, Warning, TEXT("OnCreateSessionComplete: Travel source world=%s, SessionName=%s"),
			GetWorld() ? *GetWorld()->GetName() : TEXT("<null>"),
			*SessionName.ToString());

		UWorld* World = GetWorld();
		if (World)
		{
			UE_LOG(LogTemp, Warning, TEXT("OnCreateSessionComplete: Executing ServerTravel on world %s"), *World->GetName());
			World->ServerTravel(TravelURL);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("OnCreateSessionComplete: No valid world for ServerTravel"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to Create Session"));
	}
}

void UXrMpGameInstance::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("OnStartSessionComplete: Session=%s, Success=%s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false")));

	UE_LOG(LogTemp, Warning, TEXT("OnStartSessionComplete: Session=%s, Success=%s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (bEnableLANDiagnostics)
	{
		LogDiscoveryReadiness(TEXT("OnStartSessionComplete"), ActiveNetworkMode == EXrNetworkMode::Local);
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Session %s now in state:"), *SessionName.ToString());
		if (SessionInterface.IsValid())
		{
			if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName))
			{
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   State: %s"), EOnlineSessionState::ToString(Session->SessionState));
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   bIsLANMatch: %s"), Session->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"));
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   BuildUniqueId: %d"), Session->SessionSettings.BuildUniqueId);
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   bShouldAdvertise: %s"), Session->SessionSettings.bShouldAdvertise ? TEXT("true") : TEXT("false"));
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   OpenPublicConnections: %d"), Session->NumOpenPublicConnections);
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   Beacon should now be advertising on port 15000"));
			}
		}
	}

	if (bEnableLANDiagnostics)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Beacon port 15000 should now be active (if state is InProgress)"));
	}

	if (!SessionInterface.IsValid())
	{
		return;
	}

	if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName))
	{
		UE_LOG(LogTemp, Warning, TEXT("  SessionState=%s, bIsLANMatch=%s, BuildUniqueId=%d"),
			EOnlineSessionState::ToString(Session->SessionState),
			Session->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
			Session->SessionSettings.BuildUniqueId);
		UE_LOG(LogTemp, Warning, TEXT("  NumOpenPublicConnections=%d, bShouldAdvertise=%s, bUsesPresence=%s, bUseLobbiesIfAvailable=%s"),
			Session->NumOpenPublicConnections,
			Session->SessionSettings.bShouldAdvertise ? TEXT("true") : TEXT("false"),
			Session->SessionSettings.bUsesPresence ? TEXT("true") : TEXT("false"),
			Session->SessionSettings.bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"));
	}

	if (bEnableLANDiagnostics)
	{
		LogSessionSnapshot(TEXT("OnStartSessionComplete"), SessionName);
	}
}

// ═══════════════════════════════════════════════════
// OnDestroySessionComplete
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("OnDestroySessionComplete: Session=%s, Success=%s, PendingRecreate=%s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		PendingSessionSettings.IsValid() ? TEXT("true") : TEXT("false")));

	UE_LOG(LogTemp, Warning, TEXT("OnDestroySessionComplete: Session=%s, Success=%s, PendingRecreate=%s"),
		*SessionName.ToString(),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		PendingSessionSettings.IsValid() ? TEXT("true") : TEXT("false"));

	if (bWasSuccessful)
	{
		if (PendingSessionSettings.IsValid())
		{
			const bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);
			FUniqueNetIdRepl UserId;
			if (!ResolveLocalSessionUserId(UserId, bUsingEOS, TEXT("OnDestroySessionComplete(CreatePending)")))
			{
				return;
			}

			UE_LOG(LogTemp, Warning, TEXT("OnDestroySessionComplete: Recreating pending session '%s'"), *PendingSessionName);
			SessionInterface->CreateSession(*UserId, NAME_GameSession, *PendingSessionSettings);
			PendingSessionSettings.Reset();
		}
		else
		{
			APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
			if (PC)
			{
				UE_LOG(LogTemp, Warning, TEXT("OnDestroySessionComplete: Returning to main menu via %s"), *ReturnToMainMenuUrl);
				PC->ClientTravel(ReturnToMainMenuUrl, ETravelType::TRAVEL_Absolute);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("OnDestroySessionComplete: No PlayerController available for menu travel"));
			}
		}
	}
}

// ═══════════════════════════════════════════════════
// Find Sessions
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::FindSessions(int32 MaxSearchResults, bool bIsLan)
{
	if (ActiveNetworkMode == EXrNetworkMode::Dedicated)
	{
		FindDedicatedSessions(MaxSearchResults);
		return;
	}

	CachedDedicatedConnectStrings.Empty();

	bool bUsingNull = (ActiveNetworkMode == EXrNetworkMode::Local);
	const bool bUseLanQuery = bUsingNull || bIsLan;
	UWorld* World = GetWorld();
	if (bEnableLANDiagnostics)
	{
		LogDiscoveryReadiness(TEXT("FindSessions"), bUseLanQuery);
	}
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("FindSessions: Mode=%s, RequestedLan=%s, EffectiveLan=%s, MaxResults=%d"),
		bUsingNull ? TEXT("Local(Null)") : TEXT("Online(EOS)"),
		bIsLan ? TEXT("true") : TEXT("false"),
		bUseLanQuery ? TEXT("true") : TEXT("false"),
		MaxSearchResults));

	if (bEnableLANDiagnostics)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] FindSessions starting - searching for LAN beacons on port 15000"));
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] World=%s, NetMode=%d, SessionInterfaceValid=%s"),
			World ? *World->GetName() : TEXT("<null>"),
			World ? static_cast<int32>(World->GetNetMode()) : -1,
			SessionInterface.IsValid() ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Warning, TEXT("FindSessions: Mode=%s, RequestedLan=%s, EffectiveLan=%s, MaxResults=%d"),
		bUsingNull ? TEXT("Local(Null)") : TEXT("Online(EOS)"),
		bIsLan ? TEXT("true") : TEXT("false"),
		bUseLanQuery ? TEXT("true") : TEXT("false"),
		MaxSearchResults);

	if (!SessionInterface.IsValid() || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed: SessionInterface is invalid or World is null"));
		return;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = MaxSearchResults;
	SessionSearch->bIsLanQuery = bUseLanQuery;
	SessionSearch->PingBucketSize = 100; // Standard for LAN searches
	UE_LOG(LogTemp, Warning, TEXT("  Search object created: MaxSearchResults=%d, bIsLanQuery=%s, PingBucketSize=%d"),
		SessionSearch->MaxSearchResults,
		SessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
		SessionSearch->PingBucketSize);

	if (bEnableLANDiagnostics && bUseLanQuery)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Null LAN query configured: bIsLanQuery=true, will broadcast on UDP 15000"));
	}

	if (!bUsingNull)
	{
		// EOS-specific query settings
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		SessionSearch->QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE, 1, EOnlineComparisonOp::GreaterThanEquals);
		UE_LOG(LogTemp, Warning, TEXT("  EOS query settings applied: SEARCH_LOBBIES=true, SEARCH_MINSLOTSAVAILABLE>=1"));
	}
	// Null subsystem: no extra query settings — LAN beacon doesn't support them

	UE_LOG(LogTemp, Warning, TEXT("  bIsLanQuery=%s, PingBucketSize=%d"),
		SessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
		SessionSearch->PingBucketSize);

	FUniqueNetIdRepl UserId;
	bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);
	if (!ResolveLocalSessionUserId(UserId, bUsingEOS, TEXT("FindSessions")))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Calling FindSessions with UserId: %s"), *UserId->ToString());
	UE_LOG(LogTemp, Warning, TEXT("FindSessions: Requesting search on subsystem %s"), bUsingNull ? TEXT("Null") : TEXT("EOS"));
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
	const int32 NumResults = SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("OnFindSessionsComplete: Success=%s, Results=%d, SearchState=%d"),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		NumResults,
		SessionSearch.IsValid() ? static_cast<int32>(SessionSearch->SearchState) : -1));

	if (bEnableLANDiagnostics)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] FindSessions completed - bWasSuccessful=%s, NumResults=%d, SearchState=%d"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			NumResults,
			SessionSearch.IsValid() ? static_cast<int32>(SessionSearch->SearchState) : -1);
	}

	UE_LOG(LogTemp, Warning, TEXT("OnFindSessionsComplete: Mode=%s, bWasSuccessful=%s"),
		ActiveNetworkMode == EXrNetworkMode::Local ? TEXT("Local") : TEXT("Online"),
		bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Found %d sessions"), SessionSearch->SearchResults.Num());
		UE_LOG(LogTemp, Warning, TEXT("OnFindSessionsComplete: SearchState=%d, bIsLanQuery=%s, PingBucketSize=%d"),
			static_cast<int32>(SessionSearch->SearchState),
			SessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
			SessionSearch->PingBucketSize);

		if (bEnableLANDiagnostics)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Null beacon search returned %d results"), SessionSearch->SearchResults.Num());
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				FString::Printf(TEXT("Found %d sessions"), SessionSearch->SearchResults.Num()));
		}

		if (bEnableLANDiagnostics && NumResults == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Found 0 sessions - possible causes:"));
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   1. Host session NOT transitioning to InProgress state"));
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   2. Host and client on different subnets (check IP ranges)"));
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   3. Null beacon not binding to correct network adapter"));
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   4. BuildUniqueId mismatch between host and search"));
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   5. UDP broadcast blocked at driver/OS level"));
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Workaround: Use JoinSessionByIP() with host IP address"));
		}

		for (int32 i = 0; i < NumResults; i++)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
			FString ServerName;
			Result.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName);
			FString PlayerName;
			Result.Session.SessionSettings.Get(FName("PLAYER_NAME"), PlayerName);
			FString MapName;
			Result.Session.SessionSettings.Get(SETTING_MAPNAME, MapName);

			FXrMpSessionResult BPResult;
			BPResult.ServerName = ServerName;
			BPResult.OwnerName = Result.Session.OwningUserName;
			BPResult.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			BPResult.CurrentPlayers = BPResult.MaxPlayers - Result.Session.NumOpenPublicConnections;
			BPResult.PingInMs = Result.PingInMs;
			BPResult.SessionIndex = i;
			CachedSearchResults.Add(BPResult);

			UE_LOG(LogTemp, Warning, TEXT("  [%d] SessionId=%s, ServerName=%s, Owner=%s, OwnerIdValid=%s, Players=%d/%d, Ping=%dms, LAN=%s, BuildUniqueId=%d"),
				i,
				*Result.GetSessionIdStr(),
				*ServerName,
				*Result.Session.OwningUserName,
				Result.Session.OwningUserId.IsValid() ? TEXT("true") : TEXT("false"),
				BPResult.CurrentPlayers,
				BPResult.MaxPlayers,
				Result.PingInMs,
				Result.Session.SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
				Result.Session.SessionSettings.BuildUniqueId);
			UE_LOG(LogTemp, Warning, TEXT("      PLAYER_NAME=%s, MAPNAME=%s, Advertise=%s, Presence=%s, Lobbies=%s"),
				*PlayerName,
				*MapName,
				Result.Session.SessionSettings.bShouldAdvertise ? TEXT("true") : TEXT("false"),
				Result.Session.SessionSettings.bUsesPresence ? TEXT("true") : TEXT("false"),
				Result.Session.SessionSettings.bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"));

			if (bEnableLANDiagnostics)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Found session: %s from %s (ping %dms)"), 
					*ServerName, *Result.Session.OwningUserName, Result.PingInMs);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FindSessions Failed!"));
		UE_LOG(LogTemp, Error, TEXT("OnFindSessionsComplete: SessionSearchValid=%s, SearchState=%d, Results=%d"),
			SessionSearch.IsValid() ? TEXT("true") : TEXT("false"),
			SessionSearch.IsValid() ? static_cast<int32>(SessionSearch->SearchState) : -1,
			NumResults);
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
	if (ActiveNetworkMode == EXrNetworkMode::Dedicated)
	{
		if (!CachedDedicatedConnectStrings.IsValidIndex(SessionIndex))
		{
			UE_LOG(LogTemp, Error, TEXT("JoinSession (Dedicated) Failed: Invalid SessionIndex %d"), SessionIndex);
			return;
		}

		const FString ConnectString = CachedDedicatedConnectStrings[SessionIndex];
		UE_LOG(LogTemp, Warning, TEXT("JoinSession (Dedicated): SessionIndex=%d, Connect=%s"), SessionIndex, *ConnectString);
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("JoinSession (Dedicated) Failed: No PlayerController for travel"));
		}
		return;
	}

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

	bool bUsingEOS = (ActiveNetworkMode == EXrNetworkMode::Online);

	FUniqueNetIdRepl UserId;
	if (!ResolveLocalSessionUserId(UserId, bUsingEOS, TEXT("JoinSession")))
	{
		return;
	}

	const FOnlineSessionSearchResult& SelectedResult = SessionSearch->SearchResults[SessionIndex];
	FString ServerName;
	SelectedResult.Session.SessionSettings.Get(FName("SERVER_NAME"), ServerName);
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("JoinSession: Index=%d, ServerName=%s, Ping=%d, LAN=%s, BuildUniqueId=%d"),
		SessionIndex,
		*ServerName,
		SelectedResult.PingInMs,
		SelectedResult.Session.SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
		SelectedResult.Session.SessionSettings.BuildUniqueId));
	UE_LOG(LogTemp, Warning, TEXT("JoinSession: Index=%d, Mode=%s, SessionId=%s, ServerName=%s, Owner=%s, Ping=%d, LAN=%s, BuildUniqueId=%d"),
		SessionIndex,
		bUsingEOS ? TEXT("Online") : TEXT("Local"),
		*SelectedResult.GetSessionIdStr(),
		*ServerName,
		*SelectedResult.Session.OwningUserName,
		SelectedResult.PingInMs,
		SelectedResult.Session.SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
		SelectedResult.Session.SessionSettings.BuildUniqueId);
	UE_LOG(LogTemp, Warning, TEXT("JoinSession: Resolved user id = %s"), *UserId->ToString());
	SessionInterface->JoinSession(*UserId, NAME_GameSession, SessionSearch->SearchResults[SessionIndex]);
}

// ═══════════════════════════════════════════════════
// OnJoinSessionComplete
// ═══════════════════════════════════════════════════

void UXrMpGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UpdateLANDiagnosticsSummary(FString::Printf(TEXT("OnJoinSessionComplete: Session=%s, Result=%d"), *SessionName.ToString(), static_cast<int32>(Result)));

	UE_LOG(LogTemp, Warning, TEXT("OnJoinSessionComplete: Session=%s, Result=%d"), *SessionName.ToString(), static_cast<int32>(Result));

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString ConnectInfo;
		if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
		{
			UE_LOG(LogTemp, Warning, TEXT("Join Success! ConnectInfo: %s"), *ConnectInfo);
			UE_LOG(LogTemp, Warning, TEXT("OnJoinSessionComplete: Resolved connect string length=%d"), ConnectInfo.Len());

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
				UE_LOG(LogTemp, Warning, TEXT("OnJoinSessionComplete: ClientTravel issued successfully"));
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
		UE_LOG(LogTemp, Error, TEXT("OnJoinSessionComplete: No travel issued because join failed"));
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

void UXrMpGameInstance::JoinSessionByIP(const FString& HostIPAddress, int32 Port)
{
	if (HostIPAddress.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSessionByIP Failed: HostIPAddress is empty"));
		return;
	}

	if (Port <= 0 || Port > 65535)
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSessionByIP Failed: Port %d is invalid"), Port);
		return;
	}

	FString ConnectInfo = FString::Printf(TEXT("%s:%d"), *HostIPAddress, Port);
	UE_LOG(LogTemp, Warning, TEXT("JoinSessionByIP: Traveling to %s (bypassing LAN discovery)"), *ConnectInfo);
	UE_LOG(LogTemp, Warning, TEXT("JoinSessionByIP: ActiveNetworkMode=%d, SessionInterfaceValid=%s"),
		static_cast<uint8>(ActiveNetworkMode),
		SessionInterface.IsValid() ? TEXT("true") : TEXT("false"));

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		PC->ClientTravel(ConnectInfo, ETravelType::TRAVEL_Absolute);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("JoinSessionByIP Failed: No PlayerController for travel"));
	}
}

void UXrMpGameInstance::LogNetworkInterfaces(const TCHAR* Context)
{
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] === Network Interfaces at %s ==="), Context);
	
	FString LocalHostName = FPlatformProcess::ComputerName();
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Computer Name: %s"), *LocalHostName);
	FString Summary = FString::Printf(TEXT("%s: Computer=%s"), Context, *LocalHostName);

	int32 AdapterCount = 0;
	int32 IPv4Count = 0;
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem)
	{
		TArray<TSharedPtr<FInternetAddr>> AdapterAddresses;
		SocketSubsystem->GetLocalAdapterAddresses(AdapterAddresses);
		AdapterCount = AdapterAddresses.Num();
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Socket subsystem adapter count: %d"), AdapterCount);

		for (int32 Index = 0; Index < AdapterAddresses.Num(); ++Index)
		{
			const TSharedPtr<FInternetAddr>& Address = AdapterAddresses[Index];
			if (!Address.IsValid())
			{
				continue;
			}
			const FString AddressText = Address->ToString(false);
			++IPv4Count;
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   Adapter[%d] IPv4=%s"), Index, *AddressText);
		}

		bool bCanBindAll = false;
		TSharedRef<FInternetAddr> HostAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
		const FString HostAddrText = HostAddr->ToString(false);
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   LocalHostAddr=%s"), *HostAddrText);
		Summary.Appendf(TEXT(", HostIP=%s"), *HostAddrText);
		Summary.Appendf(TEXT(", Adapters=%d, IPv4=%d"), AdapterCount, IPv4Count);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Socket subsystem unavailable; cannot enumerate adapters"));
		Summary += TEXT(", SocketSubsystem=<null>");
	}
	
	// Also keep the human-readable check for manual verification.
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] To see all IPs on this machine, run: ipconfig /all"));
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Make sure host and client IPv4 addresses are on SAME SUBNET"));
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Example: 192.168.1.x talks to 192.168.1.y (same /24 network)"));
	UpdateLANDiagnosticsSummary(Summary);
}

void UXrMpGameInstance::LogDiscoveryReadiness(const TCHAR* Context, bool bExpectLanBeacon) const
{
	const TCHAR* ModeText =
		ActiveNetworkMode == EXrNetworkMode::Local ? TEXT("Local") :
		ActiveNetworkMode == EXrNetworkMode::Online ? TEXT("Online") :
		ActiveNetworkMode == EXrNetworkMode::Dedicated ? TEXT("Dedicated") :
		TEXT("None");

	IOnlineSubsystem* DefaultOSS = Online::GetSubsystem(GetWorld());
	IOnlineSubsystem* NullOSS = Online::GetSubsystem(GetWorld(), FName(TEXT("Null")));
	IOnlineSubsystem* EosOSS = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS")));

	FString MultiHomeArg;
	const bool bHasMultiHome = FParse::Value(FCommandLine::Get(), TEXT("MULTIHOME="), MultiHomeArg);

	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] === DiscoveryReadiness @ %s ==="), Context);
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Mode=%s, ExpectLanBeacon=%s, SessionInterfaceValid=%s"),
		ModeText,
		bExpectLanBeacon ? TEXT("true") : TEXT("false"),
		SessionInterface.IsValid() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] OSS default=%s, Null=%s, EOS=%s"),
		DefaultOSS ? *DefaultOSS->GetSubsystemName().ToString() : TEXT("<null>"),
		NullOSS ? *NullOSS->GetSubsystemName().ToString() : TEXT("<null>"),
		EosOSS ? *EosOSS->GetSubsystemName().ToString() : TEXT("<null>"));

	if (const UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		bool bFoundGameNetDriver = false;
		for (const FNetDriverDefinition& NetDriverDef : GameEngine->NetDriverDefinitions)
		{
			if (NetDriverDef.DefName.ToString().Equals(TEXT("GameNetDriver"), ESearchCase::IgnoreCase))
			{
				bFoundGameNetDriver = true;
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] GameNetDriver DriverClass=%s, Fallback=%s"),
					*NetDriverDef.DriverClassName.ToString(),
					*NetDriverDef.DriverClassNameFallback.ToString());
				break;
			}
		}

		if (!bFoundGameNetDriver)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] GameNetDriver definition not found in UGameEngine::NetDriverDefinitions"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] GEngine is not a UGameEngine; cannot inspect net driver definitions"));
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem)
	{
		TArray<TSharedPtr<FInternetAddr>> AdapterAddresses;
		SocketSubsystem->GetLocalAdapterAddresses(AdapterAddresses);
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] AdapterCount=%d"), AdapterAddresses.Num());

		for (int32 Index = 0; Index < AdapterAddresses.Num(); ++Index)
		{
			if (AdapterAddresses[Index].IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG]   Adapter[%d]=%s"), Index, *AdapterAddresses[Index]->ToString(false));
			}
		}

		if (bExpectLanBeacon && AdapterAddresses.Num() > 1 && !bHasMultiHome)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Multiple adapters detected and -MULTIHOME is missing; LAN beacon may bind to a non-LAN adapter."));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] CommandLine MULTIHOME=%s"), bHasMultiHome ? *MultiHomeArg : TEXT("<not set>"));

	if (bExpectLanBeacon)
	{
		if (ActiveNetworkMode != EXrNetworkMode::Local)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] ExpectLanBeacon=true but mode is not Local/Null."));
		}

		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] Null LAN discovery path uses UDP beacon broadcast on port 15000 and game traffic on 7777."));
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] If direct IP works but browser is empty, discovery broadcast routing/firewall/adapter selection is still the likely issue."));
	}
}

void UXrMpGameInstance::LogSessionSnapshot(const TCHAR* Context, FName SessionName) const
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] %s: SessionInterface invalid; snapshot skipped"), Context);
		return;
	}

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] %s: Session %s not found"), Context, *SessionName.ToString());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LAN_DIAG] %s: Session=%s, State=%s, LAN=%s, Advertise=%s, Presence=%s, BuildUniqueId=%d, OpenPublic=%d/%d"),
		Context,
		*SessionName.ToString(),
		EOnlineSessionState::ToString(Session->SessionState),
		Session->SessionSettings.bIsLANMatch ? TEXT("true") : TEXT("false"),
		Session->SessionSettings.bShouldAdvertise ? TEXT("true") : TEXT("false"),
		Session->SessionSettings.bUsesPresence ? TEXT("true") : TEXT("false"),
		Session->SessionSettings.BuildUniqueId,
		Session->NumOpenPublicConnections,
		Session->SessionSettings.NumPublicConnections);
}

// ═══════════════════════════════════════════════════
// Dedicated Server Registry — Player Count Tracking
// ═══════════════════════════════════════════════════

bool UXrMpGameInstance::ShouldRunDedicatedRegistryReporting() const
{
	// Only run dedicated registry reporting on dedicated server instances
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Check if this is a dedicated server (net role is authority, no listening clients in PIE, etc.)
	bool bIsDedicatedServer = World->IsNetMode(NM_DedicatedServer);
	
	// Also check for ListenServer mode if needed (though the user said dedicated-only)
	// For now, we'll only report on dedicated servers (not listen servers)
	
	return bIsDedicatedServer && !SessionRegistryBaseUrl.IsEmpty() && !SessionId.IsEmpty();
}

void UXrMpGameInstance::RefreshDedicatedRegistryRuntimeConfig()
{
	// Best-effort to populate config from command line or environment vars
	// This is called before heartbeat/reporting starts
	
	if (SessionRegistryBaseUrl.IsEmpty())
	{
		// Could also check FPlatformMisc::GetEnvironmentVariable(TEXT("SESSION_REGISTRY_BASE_URL"))
		// For now, rely on Blueprint/config defaults
	}

	if (SessionRegistryToken.IsEmpty())
	{
		// Could check FPlatformMisc::GetEnvironmentVariable(TEXT("SESSION_REGISTRY_TOKEN"))
	}

	if (SessionId.IsEmpty())
	{
		// Try to extract from command line or use a default
		FString CmdSessionId;
		if (FParse::Value(FCommandLine::Get(), TEXT("SESSION_ID="), CmdSessionId))
		{
			SessionId = CmdSessionId;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("RefreshDedicatedRegistryRuntimeConfig: BaseUrl=%s, Token=%s, SessionId=%s, MaxPlayers=%d, Address=%s:%d"),
		*SessionRegistryBaseUrl,
		SessionRegistryToken.IsEmpty() ? TEXT("<not set>") : TEXT("<set>"),
		*SessionId,
		MaxPlayers,
		*ConnectAddress,
		ConnectPort);
}

FString UXrMpGameInstance::BuildSessionRegistryRoute(const FString& Suffix) const
{
	if (SessionId.IsEmpty())
	{
		return FString();
	}

	// URL encode the session id to be safe
	FString EncodedSessionId = FGenericPlatformHttp::UrlEncode(*SessionId);
	return FString::Printf(TEXT("/sessions/%s%s"), *EncodedSessionId, *Suffix);
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UXrMpGameInstance::CreateDedicatedRegistryJsonRequest(const FString& Verb, const FString& Route) const
{
	FString FullUrl = BuildDedicatedApiUrl(Route);
	if (FullUrl.IsEmpty())
	{
		checkf(!FullUrl.IsEmpty(), TEXT("CreateDedicatedRegistryJsonRequest: Could not build URL for route %s"), *Route);
	}

	auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FullUrl);
	Request->SetVerb(*Verb);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

	if (!SessionRegistryToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *SessionRegistryToken));
	}

	Request->SetTimeout(DedicatedApiTimeoutSeconds);

	return Request;
}

void UXrMpGameInstance::NotifyDedicatedPlayerCountChanged(int32 CurrentPlayers)
{
	if (!ShouldRunDedicatedRegistryReporting())
	{
		return;
	}

	// Store the pending player count update
	PendingRegistryCurrentPlayers = FMath::Max(0, CurrentPlayers);
	PendingRegistryMaxPlayers = MaxPlayers;
	bPendingRegistryPlayersUpdate = true;

	UE_LOG(LogTemp, Warning, TEXT("NotifyDedicatedPlayerCountChanged: CurrentPlayers=%d, MaxPlayers=%d, SessionId=%s"),
		PendingRegistryCurrentPlayers,
		PendingRegistryMaxPlayers,
		*SessionId);

	// Send the update immediately (do not wait for heartbeat interval)
	SendDedicatedPlayerCountUpdate();
}

void UXrMpGameInstance::SendDedicatedPlayerCountUpdate()
{
	if (!ShouldRunDedicatedRegistryReporting())
	{
		return;
	}

	if (!bPendingRegistryPlayersUpdate)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SendDedicatedPlayerCountUpdate: No pending update"));
		return;
	}

	if (bRegistryPlayersRequestInFlight)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SendDedicatedPlayerCountUpdate: Request already in flight, skipping"));
		return;
	}

	FString Route = BuildSessionRegistryRoute(TEXT("/players"));
	if (Route.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("SendDedicatedPlayerCountUpdate: Could not build route (SessionId may be empty)"));
		bPendingRegistryPlayersUpdate = false;
		return;
	}

	auto Request = CreateDedicatedRegistryJsonRequest(TEXT("POST"), Route);

	// Build JSON payload
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("currentPlayers"), PendingRegistryCurrentPlayers);
	Payload->SetNumberField(TEXT("maxPlayers"), PendingRegistryMaxPlayers);

	FString PayloadJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Payload, Writer);

	Request->SetContentAsString(PayloadJson);

	bRegistryPlayersRequestInFlight = true;

	UXrMpGameInstance* Self = this;
	Request->OnProcessRequestComplete().BindLambda(
		[Self](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!IsValid(Self))
			{
				return;
			}

			Self->bRegistryPlayersRequestInFlight = false;

			const int32 StatusCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : -1;
			const bool bWasSuccessful = bSucceeded && HttpResponse.IsValid() && EHttpResponseCodes::IsOk(StatusCode);

			if (bWasSuccessful)
			{
				UE_LOG(LogTemp, Log, TEXT("SendDedicatedPlayerCountUpdate: Success (status %d), players update sent to registry"), StatusCode);
				Self->bPendingRegistryPlayersUpdate = false;

				// Clear any pending retry timer
				if (Self->SessionRegistryPlayersRetryTimerHandle.IsValid())
				{
					if (UWorld* World = Self->GetWorld())
					{
						World->GetTimerManager().ClearTimer(Self->SessionRegistryPlayersRetryTimerHandle);
						Self->SessionRegistryPlayersRetryTimerHandle.Invalidate();
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("SendDedicatedPlayerCountUpdate: Failed (bSucceeded=%s, status=%d), will retry in %.1f seconds"),
					bSucceeded ? TEXT("true") : TEXT("false"),
					StatusCode,
					Self->SessionRegistryPlayersRetryDelaySeconds);
				UE_LOG(LogTemp, Warning, TEXT("  Response: %s"), HttpResponse.IsValid() ? *HttpResponse->GetContentAsString() : TEXT("<null>"));

				// Schedule retry
				Self->RetryDedicatedPlayerCountUpdate();
			}
		});

	UE_LOG(LogTemp, Log, TEXT("SendDedicatedPlayerCountUpdate: POST %s, payload=%s"), *Request->GetURL(), *PayloadJson);
	Request->ProcessRequest();
}

void UXrMpGameInstance::SendDedicatedHeartbeatTimerTick()
{
	if (!ShouldRunDedicatedRegistryReporting())
	{
		return;
	}

	FString Route = BuildSessionRegistryRoute(TEXT("/heartbeat"));
	if (Route.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SendDedicatedHeartbeatTimerTick: Could not build route (SessionId may be empty)"));
		return;
	}

	auto Request = CreateDedicatedRegistryJsonRequest(TEXT("POST"), Route);

	// Heartbeat can be empty or minimal payload
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("alive"), TEXT("true"));

	FString PayloadJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadJson);
	FJsonSerializer::Serialize(Payload, Writer);

	Request->SetContentAsString(PayloadJson);

	UXrMpGameInstance* Self = this;
	Request->OnProcessRequestComplete().BindLambda(
		[Self](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
		{
			if (!IsValid(Self))
			{
				return;
			}

			const int32 StatusCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : -1;
			if (bSucceeded && HttpResponse.IsValid() && EHttpResponseCodes::IsOk(StatusCode))
			{
				UE_LOG(LogTemp, Verbose, TEXT("SendDedicatedHeartbeatTimerTick: Heartbeat sent successfully (status %d)"), StatusCode);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("SendDedicatedHeartbeatTimerTick: Heartbeat failed (bSucceeded=%s, status=%d)"),
					bSucceeded ? TEXT("true") : TEXT("false"),
					StatusCode);
			}
		});

	UE_LOG(LogTemp, Verbose, TEXT("SendDedicatedHeartbeatTimerTick: POST %s"), *Request->GetURL());
	Request->ProcessRequest();
}

void UXrMpGameInstance::StartDedicatedRegistryHeartbeat()
{
	if (!ShouldRunDedicatedRegistryReporting())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDedicatedRegistryHeartbeat: Not running on dedicated server or registry config incomplete"));
		return;
	}

	RefreshDedicatedRegistryRuntimeConfig();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("StartDedicatedRegistryHeartbeat: No world available"));
		return;
	}

	// Cancel any existing heartbeat timer
	if (SessionRegistryHeartbeatTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(SessionRegistryHeartbeatTimerHandle);
	}

	// Start the heartbeat loop
	World->GetTimerManager().SetTimer(
		SessionRegistryHeartbeatTimerHandle,
		this,
		&UXrMpGameInstance::SendDedicatedHeartbeatTimerTick,
		SessionRegistryHeartbeatIntervalSeconds,
		true);  // Loop

	UE_LOG(LogTemp, Warning, TEXT("StartDedicatedRegistryHeartbeat: Started with interval %.1f seconds, SessionId=%s, BaseUrl=%s"),
		SessionRegistryHeartbeatIntervalSeconds,
		*SessionId,
		*SessionRegistryBaseUrl);
}

void UXrMpGameInstance::StopDedicatedRegistryHeartbeat()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Stop heartbeat timer
	if (SessionRegistryHeartbeatTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(SessionRegistryHeartbeatTimerHandle);
		SessionRegistryHeartbeatTimerHandle.Invalidate();
	}

	// Stop retry timer
	if (SessionRegistryPlayersRetryTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(SessionRegistryPlayersRetryTimerHandle);
		SessionRegistryPlayersRetryTimerHandle.Invalidate();
	}

	UE_LOG(LogTemp, Warning, TEXT("StopDedicatedRegistryHeartbeat: Stopped, SessionId=%s"), *SessionId);
}

void UXrMpGameInstance::SendDedicatedHeartbeatUpdate()
{
	// Alias for direct heartbeat send (not the timer tick)
	SendDedicatedHeartbeatTimerTick();
}

