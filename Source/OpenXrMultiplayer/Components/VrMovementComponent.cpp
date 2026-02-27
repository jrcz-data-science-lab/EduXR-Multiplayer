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
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"

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

	if (ownerPawn)
	{
		/**
		 * Safety: resolve any initial capsule-floor overlap.
		 *
		 * In VR, the pawn often spawns with actor origin at floor level (Z=0).
		 * But the capsule root means actor origin = capsule CENTER, so the bottom
		 * half of the capsule (88cm) extends below the floor. This causes the
		 * capsule to be embedded in the floor from frame 1, and AddActorWorldOffset
		 * with sweep=true can't move a fully-embedded primitive.
		 *
		 * Fix: trace down from high above to find the floor, then position the
		 * actor so the capsule bottom sits exactly ON the floor.
		 */
		UCapsuleComponent* Capsule = ownerPawn->FindComponentByClass<UCapsuleComponent>();
		float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;

		FVector SpawnLoc = ownerPawn->GetActorLocation();

		UE_LOG(LogTemp, Warning, TEXT("VR_MOVE | BeginPlay | Pawn=%s | SpawnPos=%s | HalfHeight=%.1f"),
			*ownerPawn->GetName(), *SpawnLoc.ToString(), HalfHeight);

		// Trace from well above the spawn pos to find the actual floor
		FVector TraceStart = SpawnLoc + FVector(0.f, 0.f, 500.f);
		FVector TraceEnd = SpawnLoc - FVector(0.f, 0.f, 500.f);
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(ownerPawn);

		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			/**
			 * Place capsule center at FloorZ + HalfHeight + small epsilon.
			 * The epsilon (2cm) prevents float-precision overlap where the capsule
			 * bottom barely clips the floor. Without this, AddActorWorldOffset
			 * with sweep=true detects bStartPenetrating and blocks ALL movement
			 * until the player jumps (which lifts the capsule out of the floor).
			 */
			float CorrectZ = Hit.ImpactPoint.Z + HalfHeight + 2.f;
			FVector CorrectedPos = SpawnLoc;
			CorrectedPos.Z = CorrectZ;

			UE_LOG(LogTemp, Warning, TEXT("VR_MOVE | BeginPlay | FloorZ=%.1f | CorrectZ=%.1f | Adjusting from %.1f to %.1f"),
				Hit.ImpactPoint.Z, CorrectZ, SpawnLoc.Z, CorrectedPos.Z);

			// Use SetActorLocation WITHOUT sweep — we're fixing the initial position
			ownerPawn->SetActorLocation(CorrectedPos, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("VR_MOVE | BeginPlay | No floor found below spawn!"));
		}
	}
}

/** 
 * Per-frame movement logic: apply gravity and handle ground detection.
 * 
 * Ground detection strategy:
 *   - Line trace downward from the actor center to below the capsule bottom
 *   - If the trace hits and we're falling/stationary → grounded
 *   - We do NOT use SetActorLocation to snap Z every frame because that
 *     causes the capsule to overlap floor geometry, which blocks ALL
 *     subsequent AddActorWorldOffset calls with sweep=true
 *   - Instead, gravity naturally pulls the pawn down and the sweep stops
 *     the pawn at the floor surface
 *
 * The previous implementation broke movement because:
 *   1. SetActorLocation(snappedPos) bypasses collision (no sweep)
 *   2. The snapped position could push the capsule 0.1cm into the floor
 *   3. Any subsequent AddActorWorldOffset(..., true) with sweep=true
 *      immediately detects the overlap and returns zero movement
 *   4. Result: pawn appears frozen — can't move at all
 */
void UVrMovementComponent::TickComponent(float DeltaTime, ELevelTick TickTypez,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickTypez, ThisTickFunction);

	if (!ownerPawn) return;

	// Only run gravity/ground on the authoritative or locally-controlled copy
	if (!ownerPawn->IsLocallyControlled() && !ownerPawn->HasAuthority()) return;

	// ── Depenetration Resolution ──
	// If the capsule is overlapping world geometry (e.g. landed on an edge,
	// walked into a corner, or spawned embedded), sweep-based movement is
	// completely blocked (bStartPenetrating). We must push the pawn out first.
	UCapsuleComponent* Capsule = ownerPawn->FindComponentByClass<UCapsuleComponent>();
	float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 20.f;

	if (Capsule)
	{
		ResolvePenetration(Capsule, HalfHeight, CapsuleRadius);
	}

	// ── Ground Detection ──
	// Trace from actor center straight down. The trace distance covers
	// from center to just past the capsule bottom.
	FVector ActorLoc = ownerPawn->GetActorLocation();
	// Start trace from center of capsule
	FVector TraceStart = ActorLoc;
	// End trace a bit past the capsule bottom (HalfHeight + small margin)
	FVector TraceEnd = ActorLoc - FVector(0.f, 0.f, HalfHeight + GroundTraceDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(ownerPawn);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd,
		ECC_WorldStatic, Params);

	// ── Debug: log ground state every ~1 second ──
#if !UE_BUILD_SHIPPING
	GroundDebugTimer += DeltaTime;
	if (GroundDebugTimer >= 1.f)
	{
		GroundDebugTimer = 0.f;
		UE_LOG(LogTemp, Warning,
			TEXT("GROUND | Pos=%s | HalfH=%.1f | TraceHit=%s | HitZ=%.1f | Grounded=%s | VVel=%.1f"),
			*ActorLoc.ToString(), HalfHeight,
			bHit ? TEXT("Y") : TEXT("N"),
			bHit ? Hit.ImpactPoint.Z : 0.f,
			bIsGrounded ? TEXT("Y") : TEXT("N"),
			VerticalVelocity);
			
		if (Capsule)
		{
			// Check if capsule is overlapping anything right now
			TArray<FOverlapResult> Overlaps;
			FCollisionQueryParams OverlapParams;
			OverlapParams.AddIgnoredActor(ownerPawn);
			bool bOverlapping = GetWorld()->OverlapMultiByChannel(
				Overlaps, ActorLoc, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight),
				OverlapParams);
			UE_LOG(LogTemp, Warning, TEXT("GROUND | Capsule overlapping %d actors"), Overlaps.Num());
		}
	}
#endif

	if (bHit && VerticalVelocity <= 0.f)
	{
		// Ground detected and we're not moving upward (jumping)
		bIsGrounded = true;
		VerticalVelocity = 0.f;

		// Check if the pawn is floating above the ground (e.g. walked off an edge last frame)
		// The "correct" Z for the actor center is ImpactPoint.Z + HalfHeight
		float CorrectZ = Hit.ImpactPoint.Z + HalfHeight;
		float CurrentZ = ActorLoc.Z;

		// If we're more than 2cm above where we should be (floating), nudge down with sweep
		// This handles small steps and ledges without using SetActorLocation
		if (CurrentZ > CorrectZ + 2.f)
		{
			float DropDistance = CurrentZ - CorrectZ;
			FVector DropDelta(0.f, 0.f, -DropDistance);
			ownerPawn->AddActorWorldOffset(DropDelta, true);
		}
	}
	else
	{
		bIsGrounded = false;
	}

	// ── Apply Gravity ──
	if (!bIsGrounded)
	{
		VerticalVelocity += GravityZ * DeltaTime;
		VerticalVelocity = FMath::Max(VerticalVelocity, -2000.f); // terminal velocity
		FVector GravityDelta(0.f, 0.f, VerticalVelocity * DeltaTime);
		ownerPawn->AddActorWorldOffset(GravityDelta, true); // sweep to stop at floor
	}
}

/**
 * Move the pawn forward/backward relative to the HMD (camera) facing direction.
 * Movement is projected onto the horizontal plane (Z=0) so looking up/down
 * doesn't affect walk direction. Sweep is enabled to respect collision.
 *
 * Returns the computed delta so the caller can send it to the server.
 */
FVector UVrMovementComponent::MoveForward(float value)
{
	if (!ownerPawn || FMath::IsNearlyZero(value)) return FVector::ZeroVector;

	/**
	 * Get the HMD (camera) forward direction, not the actor's forward.
	 * This makes movement HMD-oriented — you walk where you're looking.
	 * Flatten to horizontal and normalize to remove pitch influence.
	 */
	FVector forward;
	UCameraComponent* Cam = ownerPawn->GetVRCamera();
	if (Cam)
	{
		forward = Cam->GetForwardVector();
	}
	else
	{
		forward = ownerPawn->GetActorForwardVector();
	}
	forward.Z = 0.f;
	forward.Normalize();

	/** Calculate frame-rate-independent movement offset */
	FVector movement = forward * value * moveSpeed * GetWorld()->GetDeltaSeconds();

	/** Apply movement with sweep (collision detection) */
	FVector PosBefore = ownerPawn->GetActorLocation();
	FHitResult SweepHit;
	ownerPawn->AddActorWorldOffset(movement, true, &SweepHit);
	FVector PosAfter = ownerPawn->GetActorLocation();

#if !UE_BUILD_SHIPPING
	MoveDebugTimer += GetWorld()->GetDeltaSeconds();
	if (MoveDebugTimer >= 0.5f)
	{
		MoveDebugTimer = 0.f;
		FVector ActualDelta = PosAfter - PosBefore;
		UE_LOG(LogTemp, Warning,
			TEXT("MOVE_FWD | val=%.2f | delta=%s | actual=%s | blocked=%s | hit=%s | startInPen=%s"),
			value, *movement.ToString(), *ActualDelta.ToString(),
			SweepHit.bBlockingHit ? TEXT("Y") : TEXT("N"),
			(SweepHit.bBlockingHit && SweepHit.GetActor()) ? *SweepHit.GetActor()->GetName() : TEXT("none"),
			SweepHit.bStartPenetrating ? TEXT("Y") : TEXT("N"));
	}
#endif

	return movement;
}

/**
 * Strafe the pawn left/right relative to the HMD (camera) right vector.
 * Same horizontal-only projection as MoveForward.
 */
FVector UVrMovementComponent::MoveRight(float value)
{
	if (!ownerPawn || FMath::IsNearlyZero(value)) return FVector::ZeroVector;

	/**
	 * Get the HMD (camera) right direction for HMD-oriented strafing.
	 * Flatten to horizontal and normalize.
	 */
	FVector right;
	UCameraComponent* Cam = ownerPawn->GetVRCamera();
	if (Cam)
	{
		right = Cam->GetRightVector();
	}
	else
	{
		right = ownerPawn->GetActorRightVector();
	}
	right.Z = 0.f;
	right.Normalize();

	/** Calculate frame-rate-independent movement offset */
	FVector movement = right * value * moveSpeed * GetWorld()->GetDeltaSeconds();

	/** Apply movement with sweep (collision detection) */
	ownerPawn->AddActorWorldOffset(movement, true);

	return movement;
}

/**
 * Apply a pre-computed movement delta directly.
 * Used by the server — the server doesn't have VR tracking, so the client
 * computes the HMD-oriented direction and sends the resulting delta.
 */
void UVrMovementComponent::ApplyMoveDelta(FVector Delta)
{
	if (!ownerPawn) return;
	ownerPawn->AddActorWorldOffset(Delta, true);
}

/**
 * Snap turn the pawn by a fixed angle when thumbstick exceeds the deadzone.
 *
 * Uses a DUAL reset mechanism to prevent the snap-turn-only-once bug:
 *   1. Deadzone reset: when thumbstick returns to neutral (below deadzone)
 *   2. Cooldown timer: automatically re-enables after SnapTurnCooldown seconds
 *
 * The old approach relied ONLY on the Completed event sending value < deadzone,
 * which failed because some VR input configurations don't send zero on release,
 * or the Completed event fires with the last non-zero value.
 *
 * NOTE: This method is ONLY used locally. The server uses ApplySnapTurn()
 * because the server never receives the "thumbstick returned to neutral" input.
 */
float UVrMovementComponent::SnapTurn(float value)
{
	/** Reset the snap turn lock when thumbstick returns to neutral */
	if (FMath::Abs(value) < snapTurnDeadzone)
	{
		bCanSnapTurn = true;
		SnapTurnCooldownTimer = 0.f;
		return 0.f;
	}

	/**
	 * Cooldown-based reset: if the flag is locked but enough time has passed,
	 * force-reset it. This handles the case where the Completed/neutral input
	 * never arrives (e.g. the user flicks the stick quickly).
	 */
	if (!bCanSnapTurn)
	{
		SnapTurnCooldownTimer += GetWorld()->GetDeltaSeconds();
		if (SnapTurnCooldownTimer >= SnapTurnCooldown)
		{
			bCanSnapTurn = true;
			SnapTurnCooldownTimer = 0.f;
		}
	}

	/** Only allow one snap turn per thumbstick push (or per cooldown period) */
	if (!bCanSnapTurn || !ownerPawn) return 0.f;

	/** Determine direction: positive = turn right, negative = turn left */
	float turnAngle = (value > 0.f) ? snapTurnDegree : -snapTurnDegree;

	/** Apply the rotation */
	ApplySnapTurn(turnAngle);

	/** Lock snap turn until thumbstick returns to neutral or cooldown expires */
	bCanSnapTurn = false;
	SnapTurnCooldownTimer = 0.f;

	return turnAngle;
}

/**
 * Apply a snap turn rotation directly without deadzone/cooldown logic.
 * Used by the server RPC — the server never sees the "thumbstick released"
 * input that resets bCanSnapTurn, so it must bypass the cooldown entirely.
 */
void UVrMovementComponent::ApplySnapTurn(float turnAngle)
{
	if (!ownerPawn) return;

	FRotator currentRotation = ownerPawn->GetActorRotation();
	currentRotation.Yaw += turnAngle;
	ownerPawn->SetActorRotation(currentRotation);
}

/**
 * Jump — launch the pawn upward if currently grounded.
 * Sets VerticalVelocity to JumpZVelocity; gravity in TickComponent
 * will decelerate and eventually pull the pawn back down.
 */
float UVrMovementComponent::Jump(float value)
{
	if (!ownerPawn || !bIsGrounded) return 0.f;

	VerticalVelocity = JumpZVelocity;
	bIsGrounded = false;

	return JumpZVelocity;
}


// ═══════════════════════════════════════════════════
// Depenetration Resolution
// ═══════════════════════════════════════════════════

/**
 * Detect and resolve capsule overlap with world geometry.
 *
 * When the capsule is embedded in geometry (e.g., landed on a ledge edge,
 * pushed into a wall, or spawned clipping into the floor), ALL sweep-based
 * movement is blocked because AddActorWorldOffset detects bStartPenetrating
 * and returns zero. The player appears frozen and can only escape by jumping.
 *
 * Strategy: try a tiny upward sweep. If bStartPenetrating is true, the capsule
 * is embedded. Push the pawn out using the hit Normal and PenetrationDepth.
 * If the upward push isn't enough (e.g. wall penetration), try horizontal sweeps too.
 *
 * IMPORTANT: We sweep against ECC_WorldStatic (not ECC_Pawn) so that only
 * static world geometry (walls, floors, platforms) triggers depenetration.
 * Using ECC_Pawn would also detect physics objects (cubes, guns, balls),
 * and when the player holds a grabbed object near their body, the depenetration
 * would violently push the player away at insane speeds.
 */
void UVrMovementComponent::ResolvePenetration(UCapsuleComponent* Capsule, float HalfHeight, float Radius)
{
	if (!ownerPawn || !Capsule) return;

	// Try a tiny sweep to detect if we're currently penetrating
	FHitResult Hit;
	FVector Start = ownerPawn->GetActorLocation();
	// Sweep a tiny amount upward — if we're embedded, this will report bStartPenetrating
	FVector End = Start + FVector(0.f, 0.f, 0.1f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(ownerPawn);

	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	// Use ECC_WorldStatic — only depenetrate against static world geometry,
	// NOT physics objects (ECC_PhysicsBody) which the player may be holding
	bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity,
		ECC_WorldStatic, CapsuleShape, QueryParams);

	if (bHit && Hit.bStartPenetrating)
	{
		/**
		 * We're embedded! The hit result gives us:
		 *   - Normal: the direction to push OUT of the penetrating surface
		 *   - PenetrationDepth: how deep we're embedded
		 *
		 * Apply the push-out adjustment without sweep (since we're already overlapping,
		 * a swept move would just return zero again).
		 */
		float PushDist = Hit.PenetrationDepth + 2.f; // extra margin to fully clear
		FVector Adjustment = Hit.Normal * PushDist;

		UE_LOG(LogTemp, Warning, TEXT("DEPENETRATE | %s | Normal=%s | Depth=%.2f | Adj=%s"),
			*ownerPawn->GetName(), *Hit.Normal.ToString(), Hit.PenetrationDepth, *Adjustment.ToString());

		ownerPawn->SetActorLocation(Start + Adjustment, false);
	}
}


