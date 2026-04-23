/**
 * Example Dedicated Server Game Mode
 * 
 * Hooks PostLogin/Logout to notify the GameInstance of player count changes.
 * The GameInstance will automatically send those counts to the registry's
 * /sessions/{sessionId}/players endpoint.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "XrMpServerGameMode.generated.h"

UCLASS()
class OPENXRMULTIPLAYER_API AXrMpServerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	/** Called whenever player count changes (PostLogin or Logout). */
	void NotifyPlayerCountChanged();

	/** Count the actual current player count from the world. */
	int32 GetCurrentPlayerCount() const;
};

