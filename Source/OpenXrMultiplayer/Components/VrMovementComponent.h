/**
 * VrMovementComponent.h — VR Locomotion for CustomXrPawn
 *
 * Handles smooth locomotion and snap turning for VR pawns.
 * Attach this component to a CustomXrPawn to enable thumbstick-based movement.
 *
 * Movement is camera-relative: forward/back follows the HMD look direction,
 * strafe follows the perpendicular axis. Snap turn rotates the entire pawn
 * in discrete steps to reduce VR motion sickness.
 */

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "VrMovementComponent.generated.h"

/** Forward declaration — the pawn that owns this movement component */
class ACustomXrPawn;

/**
 * UVrMovementComponent — VR-specific movement component
 *
 * Provides:
 *  - Smooth forward/backward locomotion (MoveForward)
 *  - Smooth left/right strafing (MoveRight)
 *  - Snap turning in fixed degree increments (SnapTurn)
 *  - Jump placeholder for future implementation
 *
 * All movement is applied via AddActorWorldOffset / SetActorRotation
 * on the owning pawn, which works with Unreal's bReplicateMovement
 * for automatic network replication.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OPENXRMULTIPLAYER_API UVrMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	/** Constructor — sets default tick and movement values */
	UVrMovementComponent();

	/** Called every frame — reserved for future per-frame movement logic */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/** Movement speed in cm/s — controls how fast the pawn moves with thumbstick input */
	UPROPERTY(EditAnywhere, Category="Movement")
	float moveSpeed = 150.f;

	/** Degrees to rotate per snap turn — typical VR values are 30, 45, or 90 */
	UPROPERTY(EditAnywhere, Category="Movement")
	float snapTurnDegree = 45;

	/** Internal flag — prevents repeated snap turns while thumbstick is held */
	bool bCanSnapTurn = true;

	/** Thumbstick deadzone for snap turn — input below this threshold is ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float snapTurnDeadzone = 0.5f;

	/**
	 * Move the pawn forward/backward relative to actor facing direction.
	 * @param value Thumbstick Y axis value (-1 to 1)
	 * @return The input value passed in (for chaining)
	 */
	UFUNCTION(BlueprintCallable)
	float MoveForward(float value);

	/**
	 * Strafe the pawn left/right relative to actor facing direction.
	 * @param value Thumbstick X axis value (-1 to 1)
	 * @return The input value passed in (for chaining)
	 */
	UFUNCTION(BlueprintCallable)
	float MoveRight(float value);

	/**
	 * Snap turn the pawn by a fixed angle.
	 * Uses a deadzone to prevent repeated turns while thumbstick is held.
	 * @param value Thumbstick X axis value — direction determines turn direction
	 * @return The actual turn angle applied (0 if blocked by deadzone or cooldown)
	 */
	UFUNCTION(BlueprintCallable)
	float SnapTurn(float value);

	/**
	 * Jump — placeholder for future implementation.
	 * @param value Input value (unused currently)
	 * @return Always 0 (not yet implemented)
	 */
	UFUNCTION(BlueprintCallable)
	float Jump(float value);

protected:
	/** Called on game start — caches a reference to the owning CustomXrPawn */
	virtual void BeginPlay() override;

	/** Cached pointer to the owning pawn — avoids repeated casts every frame */
	ACustomXrPawn* ownerPawn;

private:
};
