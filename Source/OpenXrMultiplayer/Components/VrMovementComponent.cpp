/**
 * VrMovementComponent.cpp — VR Locomotion Implementation
 *
 * Implements smooth locomotion and snap turning for VR pawns.
 * All movement uses AddActorWorldOffset / SetActorRotation which
 * works with Unreal's built-in bReplicateMovement for network sync.
 */

#include "VrMovementComponent.h"
#include "CustomXrPawn.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"

/** Constructor — enables ticking so TickComponent runs every frame */
UVrMovementComponent::UVrMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

/** Cache a typed pointer to the owning CustomXrPawn to avoid casting every frame */
void UVrMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ownerPawn = Cast<ACustomXrPawn>(GetOwner());
}

/** Reserved for future per-frame movement logic (e.g. gravity, acceleration curves) */
void UVrMovementComponent::TickComponent(float DeltaTime, ELevelTick TickTypez,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickTypez, ThisTickFunction);
}

/**
 * Move the pawn forward/backward along the actor's forward vector.
 * Movement is projected onto the horizontal plane (Z=0) so looking up/down
 * doesn't affect walk direction. Sweep is enabled to respect collision.
 */
float UVrMovementComponent::MoveForward(float value)
{
	if (!ownerPawn || FMath::IsNearlyZero(value)) return 0.f;

	/** Get the pawn's forward direction, flatten to horizontal, normalize */
	FVector forward = ownerPawn->GetActorForwardVector();
	forward.Z = 0.f;
	forward.Normalize();

	/** Calculate frame-rate-independent movement offset */
	FVector movement = forward * value * moveSpeed * GetWorld()->GetDeltaSeconds();

	/** Apply movement with sweep (collision detection) */
	ownerPawn->AddActorWorldOffset(movement, true);

	return value;
}

/**
 * Strafe the pawn left/right along the actor's right vector.
 * Same horizontal-only projection as MoveForward.
 */
float UVrMovementComponent::MoveRight(float value)
{
	if (!ownerPawn || FMath::IsNearlyZero(value)) return 0.f;

	/** Get the pawn's right direction, flatten to horizontal, normalize */
	FVector right = ownerPawn->GetActorRightVector();
	right.Z = 0.f;
	right.Normalize();

	/** Calculate frame-rate-independent movement offset */
	FVector movement = right * value * moveSpeed * GetWorld()->GetDeltaSeconds();

	/** Apply movement with sweep (collision detection) */
	ownerPawn->AddActorWorldOffset(movement, true);

	return value;
}

/**
 * Snap turn the pawn by a fixed angle when thumbstick exceeds the deadzone.
 *
 * The bCanSnapTurn flag prevents repeated turns while the thumbstick stays pushed.
 * When the thumbstick returns to the deadzone center, bCanSnapTurn resets,
 * allowing the next snap turn. This gives a clean "click" rotation feel.
 */
float UVrMovementComponent::SnapTurn(float value)
{
	/** Reset the snap turn lock when thumbstick returns to neutral */
	if (FMath::Abs(value) < snapTurnDeadzone)
	{
		bCanSnapTurn = true;
		return 0.f;
	}

	/** Only allow one snap turn per thumbstick push */
	if (!bCanSnapTurn || !ownerPawn) return 0.f;

	/** Determine direction: positive = turn right, negative = turn left */
	float turnAngle = (value > 0.f) ? snapTurnDegree : -snapTurnDegree;

	/** Apply the rotation to the pawn's yaw */
	FRotator currentRotation = ownerPawn->GetActorRotation();
	currentRotation.Yaw += turnAngle;
	ownerPawn->SetActorRotation(currentRotation);

	/** Lock snap turn until thumbstick returns to neutral */
	bCanSnapTurn = false;

	return turnAngle;
}

/** Jump — placeholder for future implementation */
float UVrMovementComponent::Jump(float value)
{
	return 0.f;
}
