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
class UCapsuleComponent;

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

	/** Time accumulator for snap turn cooldown reset */
	float SnapTurnCooldownTimer = 0.f;

	/** Cooldown duration in seconds before snap turn auto-resets even without neutral input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SnapTurnCooldown = 0.02f;

	/** Thumbstick deadzone for snap turn — input below this threshold is ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float snapTurnDeadzone = 0.5f;

	/**
	 * Move the pawn forward/backward relative to HMD facing direction.
	 * @param value Thumbstick Y axis value (-1 to 1)
	 * @return The computed movement delta vector (for server replication)
	 */
	UFUNCTION(BlueprintCallable)
	FVector MoveForward(float value);

	/**
	 * Strafe the pawn left/right relative to HMD facing direction.
	 * @param value Thumbstick X axis value (-1 to 1)
	 * @return The computed movement delta vector (for server replication)
	 */
	UFUNCTION(BlueprintCallable)
	FVector MoveRight(float value);

	/**
	 * Snap turn the pawn by a fixed angle.
	 * Uses a deadzone to prevent repeated turns while thumbstick is held.
	 * @param value Thumbstick X axis value — direction determines turn direction
	 * @return The actual turn angle applied (0 if blocked by deadzone or cooldown)
	 */
	UFUNCTION(BlueprintCallable)
	float SnapTurn(float value);

	/**
	 * Apply a snap turn rotation directly without deadzone/cooldown checks.
	 * Used by the server RPC to apply the rotation authoritatively.
	 * The server never receives the "thumbstick returned to neutral" input,
	 * so it cannot use the cooldown-based SnapTurn method.
	 * @param turnAngle The angle in degrees to rotate (positive = right, negative = left)
	 */
	void ApplySnapTurn(float turnAngle);

	/**
	 * Jump — launches the pawn upward if grounded.
	 * @param value Input value (unused currently, triggers on press)
	 * @return Jump velocity applied (0 if not grounded)
	 */
	UFUNCTION(BlueprintCallable)
	float Jump(float value);

	/**
	 * Apply a pre-computed movement delta directly (used by server RPCs).
	 * The server doesn't have VR tracking, so the client computes the
	 * HMD-oriented movement direction and sends the delta.
	 * @param Delta The world-space movement offset to apply
	 */
	void ApplyMoveDelta(FVector Delta);

	// ─────────────────────────────────────────────
	// Gravity & Jump
	// ─────────────────────────────────────────────

	/** Gravity acceleration in cm/s² (default: ~980 = Earth gravity) */
	UPROPERTY(EditAnywhere, Category="Movement")
	float GravityZ = -980.f;

	/** Initial upward velocity when jumping in cm/s */
	UPROPERTY(EditAnywhere, Category="Movement")
	float JumpZVelocity = 420.f;

	/** Current vertical velocity — driven by gravity and jump */
	float VerticalVelocity = 0.f;

	/** Whether the pawn is currently on the ground */
	bool bIsGrounded = true;

	/** How far past the capsule bottom to trace for ground (cm) */
	UPROPERTY(EditAnywhere, Category="Movement")
	float GroundTraceDistance = 15.f;

	/**
	 * Maximum XY distance (cm) the capsule is allowed to recenter toward the HMD in a single tick.
	 * Clamps tracking spikes so a bad frame cannot launch/teleport the pawn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|VR")
	float MaxCapsuleRecenterPerTick = 25.f;

protected:
	/** Called on game start — caches a reference to the owning CustomXrPawn */
	virtual void BeginPlay() override;

	/** Cached pointer to the owning pawn — avoids repeated casts every frame */
	ACustomXrPawn* ownerPawn;

	/**
	 * Detect and resolve capsule overlap with world geometry.
	 * Prevents the pawn from getting permanently stuck when embedded in geometry.
	 * Called every tick before ground detection.
	 */
	void ResolvePenetration(UCapsuleComponent* Capsule, float HalfHeight, float Radius);

	/**
	 * Recenter the capsule to follow the HMD XY position for room-scale movement.
	 * Keeps camera world position stable by offsetting VrOrigin by the applied actor delta.
	 */
	void SyncCapsuleToHmdXY();

private:
#if !UE_BUILD_SHIPPING
	/** Debug timers for throttled logging */
	float GroundDebugTimer = 0.f;
	float MoveDebugTimer = 0.f;
#endif
};
