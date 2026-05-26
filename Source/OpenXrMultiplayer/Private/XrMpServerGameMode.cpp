#include "XrMpServerGameMode.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "XrMpGameInstance.h"

void AXrMpServerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	NotifyPlayerCountChangedNextTick();
}

void AXrMpServerGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	NotifyPlayerCountChangedNextTick();
}

void AXrMpServerGameMode::NotifyPlayerCountChangedNextTick()
{
	if (UWorld* World = GetWorld())
	{
		FTimerDelegate Delegate;
		Delegate.BindUObject(this, &AXrMpServerGameMode::NotifyPlayerCountChanged);
		World->GetTimerManager().SetTimerForNextTick(Delegate);
		return;
	}

	NotifyPlayerCountChanged();
}

void AXrMpServerGameMode::NotifyPlayerCountChanged()
{
	if (!GetWorld() || !GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	UXrMpGameInstance* XrMpGameInstance = Cast<UXrMpGameInstance>(GetGameInstance());
	if (!XrMpGameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AXrMpServerGameMode::NotifyPlayerCountChanged: GameInstance is not UXrMpGameInstance"));
		return;
	}

	const int32 CurrentPlayers = GetCurrentPlayerCount();
	UE_LOG(LogTemp, Log, TEXT("AXrMpServerGameMode::NotifyPlayerCountChanged: CurrentPlayers=%d"), CurrentPlayers);
	XrMpGameInstance->NotifyDedicatedPlayerCountChanged(CurrentPlayers);
}

int32 AXrMpServerGameMode::GetCurrentPlayerCount() const
{
	if (GameState)
	{
		int32 Count = 0;
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (IsValid(PlayerState) && !PlayerState->IsOnlyASpectator())
			{
				++Count;
			}
		}
		return Count;
	}

	int32 Count = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get())
		{
			++Count;
		}
	}

	return Count;
}


