/**
 * CustomXrPawn.cpp — VR Pawn with full multiplayer replication
 *
 * See CustomXrPawn.h for the replication design overview.
 */

#include "CustomXrPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "EnhancedInputComponent.h"
#include "MotionControllerComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "OpenXrMultiplayer/Components/VrMovementComponent.h"


// ═══════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════

ACustomXrPawn::ACustomXrPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	/**
	 * Enable network replication for this actor
	 * bReplicates: the actor exists on all connected machines
	 * bReplicateMovement: Unreal auto-replicates the actor's world position/rotation
	 *   so we don't need custom RPCs for actor-level movement (AddActorWorldOffset etc.)
	 */
	bReplicates = true;
	SetReplicateMovement(true);

	/**
	 * Higher net update frequency for smoother tracking replication
	 * Default is 100, but VR tracking data changes every frame
	 */
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);

	/**
	 * Component hierarchy for collision:
	 * CapsuleCollider (ROOT) — handles all collision for AddActorWorldOffset sweep
	 *   └─ VrOrigin — the VR play-space origin, everything else attaches here
	 *       ├─ Camera (tracks HMD)
	 *       ├─ MotionControllerLeft/Right
	 *       └─ PlayerMesh, etc.
	 *
	 * The capsule MUST be root because AddActorWorldOffset with sweep
	 * only sweeps the root component's collision shape.
	 */
	CapsuleCollider = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleCollider"));
	/**
	 * Capsule sizing:
	 *   Radius = 20cm — slim profile so the player doesn't clip walls they're not near.
	 *     (Default 34cm was far too wide for a VR player who can see exactly where they are.)
	 *   HalfHeight = 88cm — standard standing height (~176cm total).
	 */
	CapsuleCollider->InitCapsuleSize(20.f, 88.f);
	CapsuleCollider->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleCollider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleCollider->SetSimulatePhysics(false);
	CapsuleCollider->SetEnableGravity(false); // We handle gravity in VrMovementComponent

	/**
	 * Overlap (not block) dynamic physics objects (PhysicsBody channel).
	 *
	 * ECR_Block (default for "Pawn" profile) causes two problems:
	 *   1. Walking into physics cubes/balls STOPS the player dead
	 *      because the sweep hits the object and returns a blocking hit.
	 *   2. Grabbing an object and moving it near the body creates
	 *      collision forces that launch the player at insane speeds.
	 *
	 * ECR_Overlap solves both: the capsule sweep passes through physics objects
	 * (no blocking hit), but we still get overlap events so we can push
	 * objects away with an impulse in the overlap callback.
	 *
	 * ECR_Ignore would also prevent blocking, but then we can't detect
	 * overlaps at all — the player would ghost through everything.
	 */
	CapsuleCollider->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	CapsuleCollider->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
	CapsuleCollider->SetGenerateOverlapEvents(true);

	SetRootComponent(CapsuleCollider);

	VrOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VrOrigin"));
	VrOrigin->SetupAttachment(CapsuleCollider);
	/**
	 * Position VrOrigin at the BOTTOM of the capsule (floor level).
	 *
	 * Without this, VrOrigin sits at capsule center (88cm above floor).
	 * The HMD tracking already includes the user's real-world height above the floor,
	 * so if VrOrigin is at capsule center, the camera ends up at:
	 *   FloorZ + 88cm (capsule half-height) + ~170cm (user's real height) = ~258cm
	 * which makes the player appear way too tall.
	 *
	 * By placing VrOrigin at capsule bottom (floor level), the HMD tracking
	 * correctly represents the user's actual height: FloorZ + ~170cm.
	 */
	VrOrigin->SetRelativeLocation(FVector(0.f, 0.f, -88.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VrOrigin);

	MotionControllerLeft = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeft"));
	MotionControllerLeft->SetupAttachment(VrOrigin);
	MotionControllerLeft->SetTrackingSource(EControllerHand::Left);

	MotionControllerRight = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRight"));
	MotionControllerRight->SetupAttachment(VrOrigin);
	MotionControllerRight->SetTrackingSource(EControllerHand::Right);

	WidgetInteractionLeft = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("LeftAim"));
	WidgetInteractionLeft->SetupAttachment(MotionControllerLeft);

	WidgetInteractionRight = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("RightAim"));
	WidgetInteractionRight->SetupAttachment(MotionControllerRight);

	HeadMountedDisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HMD Mesh"));
	HeadMountedDisplayMesh->SetupAttachment(Camera);

	HandLeft = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandLeft"));
	HandLeft->SetupAttachment(MotionControllerLeft);

	HandRight = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandRight"));
	HandRight->SetupAttachment(MotionControllerRight);

	PlayerMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PlayerMesh"));
	PlayerMesh->SetupAttachment(VrOrigin);

	vrMovementComponent = CreateDefaultSubobject<UVrMovementComponent>(TEXT("VrMovementComponent"));
	
	WidgetInteractionLeft->TraceChannel = ECollisionChannel::ECC_Visibility;
	WidgetInteractionLeft->InteractionDistance = 500.f;
	WidgetInteractionLeft->bShowDebug = true;   // debug line while setting up

	WidgetInteractionRight->TraceChannel = ECollisionChannel::ECC_Visibility;
	WidgetInteractionRight->InteractionDistance = 500.f;
	WidgetInteractionRight->bShowDebug = true;

}


// ═══════════════════════════════════════════════════
// BeginPlay
// ═══════════════════════════════════════════════════

void ACustomXrPawn::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Warning, TEXT("Pawn %s Controller: %s"), *GetName(), 
	Controller ? *Controller->GetName() : TEXT("NULL"));

	/**
	 * Reinforce VrOrigin floor-level offset.
	 *
	 * The constructor sets VrOrigin to -HalfHeight relative to the capsule center,
	 * but in some cases the engine or Blueprint CDO can reset relative transforms
	 * during actor initialization. Setting it here in BeginPlay guarantees it sticks.
	 *
	 * Without this offset, VrOrigin sits at capsule CENTER (88cm above floor).
	 * The HMD tracking then adds the user's real height on top → player appears ~2.5m tall.
	 */
	if (CapsuleCollider && VrOrigin)
	{
		/**
		 * Reinforce capsule size + collision responses here in BeginPlay.
		 * The Blueprint CDO may have saved the old 34cm radius from before
		 * we slimmed it down, and may not have the PhysicsBody ignore set.
		 * This ensures the C++ values always win.
		 */
		CapsuleCollider->SetCapsuleSize(20.f, 88.f);
		CapsuleCollider->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
		CapsuleCollider->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
		CapsuleCollider->SetGenerateOverlapEvents(true);

		// Bind overlap callback to push physics objects when the player walks into them
		CapsuleCollider->OnComponentBeginOverlap.AddDynamic(this, &ACustomXrPawn::OnCapsuleOverlap);

		float HalfHeight = CapsuleCollider->GetScaledCapsuleHalfHeight();

		/**
		// Log BEFORE setting — did the constructor offset survive?
		UE_LOG(LogTemp, Warning, TEXT("VR_HEIGHT_DEBUG | %s | BEFORE reinforcement:"), *GetName());
		UE_LOG(LogTemp, Warning, TEXT("  ActorLocation     = %s"), *GetActorLocation().ToString());
		UE_LOG(LogTemp, Warning, TEXT("  CapsuleHalfHeight = %.1f"), HalfHeight);
		UE_LOG(LogTemp, Warning, TEXT("  CapsuleWorldZ     = %.1f"), CapsuleCollider->GetComponentLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  VrOrigin RelativeZ= %.1f"), VrOrigin->GetRelativeLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  VrOrigin WorldZ   = %.1f"), VrOrigin->GetComponentLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  Camera RelativeZ  = %.1f"), Camera->GetRelativeLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  Camera WorldZ     = %.1f"), Camera->GetComponentLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  Camera bLockToHmd = %s"), Camera->bLockToHmd ? TEXT("true") : TEXT("false"));
		*/

		/**
		VrOrigin->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));

		// Log AFTER setting
		UE_LOG(LogTemp, Warning, TEXT("VR_HEIGHT_DEBUG | %s | AFTER reinforcement:"), *GetName());
		UE_LOG(LogTemp, Warning, TEXT("  VrOrigin RelativeZ= %.1f"), VrOrigin->GetRelativeLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  VrOrigin WorldZ   = %.1f"), VrOrigin->GetComponentLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  Camera WorldZ     = %.1f"), Camera->GetComponentLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("  Expected: VrOrigin should be at ActorZ - %.1f = %.1f"),
			HalfHeight, GetActorLocation().Z - HalfHeight);
		*/
		VrOrigin->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	}


	/**
	 * On non-local pawns, disable all XR tracking so the engine doesn't
	 * override the replicated transforms we apply every Tick.
	 *
	 * Without this:
	 *  - Camera would snap to HMD origin (bLockToHmd)
	 *  - MotionControllers would reset to world origin every frame
	 *    because their tracking tick finds no connected controllers,
	 *    and resets the transform — overriding our SetWorldTransform calls
	 *
	 * This is why the hands appeared "grouped at world center" — the
	 * MotionControllerComponent was fighting our replication every frame.
	 */
	if (!IsLocallyControlled())
	{
		Camera->bLockToHmd = false;
		MotionControllerLeft->SetTrackingMotionSource(NAME_None);
		MotionControllerRight->SetTrackingMotionSource(NAME_None);
		MotionControllerLeft->SetComponentTickEnabled(false);
		MotionControllerRight->SetComponentTickEnabled(false);
	}

	if (vrMovementComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("VrMovementComponent found on %s"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("VrMovementComponent NOT found on %s"), *GetName());
	}
}


// ═══════════════════════════════════════════════════
// Tick — the core replication loop
// ═══════════════════════════════════════════════════

void ACustomXrPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/**
	 * ── Enforce VrOrigin floor-level offset every frame ──
	 *
	 * VrOrigin MUST be at the capsule bottom (floor level), not capsule center.
	 * Without this, the HMD tracking height stacks on top of the capsule half-height,
	 * making the player appear ~2.5m tall instead of their real height.
	 *
	 * We enforce this in Tick because:
	 *   - The C++ constructor sets it to -88, but the Blueprint CDO overwrites it to 0
	 *   - BeginPlay reinforcement also gets overridden by Blueprint construction scripts
	 *   - The Blueprint sets tracking origin to "Local Floor" in its own BeginPlay,
	 *     which may run AFTER ours and reset transforms
	 *
	 * This is cheap (one float comparison + conditional set per frame).
	 */
	if (CapsuleCollider && VrOrigin)
	{
		float DesiredZ = -CapsuleCollider->GetScaledCapsuleHalfHeight();
		if (!FMath::IsNearlyEqual(VrOrigin->GetRelativeLocation().Z, DesiredZ, 0.1f))
		{
			VrOrigin->SetRelativeLocation(FVector(0.f, 0.f, DesiredZ));
		}
	}

	if (IsLocallyControlled())
	{
		/**
		 * LOCAL PAWN: VR tracking is active. Capture the current head/hand
		 * transforms and send them to the server for replication.
		 */
		CaptureAndSendVRTransforms();
	}
	else
	{
		/**
		 * NON-LOCAL PAWN (on ANY machine — server or client):
		 * Apply the replicated transforms so this player's avatar
		 * visually matches the real VR player's head/hand positions.
		 */
		ApplyReplicatedVRTransforms();
	}
}

// ═══════════════════════════════════════════════════
// VR Transform Capture (local pawn only)
// ═══════════════════════════════════════════════════

void ACustomXrPawn::CaptureAndSendVRTransforms()
{
	/**
	 * Compute each tracked component's transform RELATIVE TO VrOrigin.
	 *
	 * Why relative? Because world transforms include the actor's world position
	 * and rotation. If the pawn is rotated 90° in the world, the hand's world
	 * rotation would be 90° off when applied to a differently-rotated pawn.
	 * Relative transforms are actor-rotation-independent.
	 *
	 * GetComponentTransform() = world transform
	 * VrOrigin->GetComponentTransform() = the actor root's world transform
	 * GetRelativeTransform() = child relative to parent in world space
	 */
	const FTransform VrOriginWorld = VrOrigin->GetComponentTransform();

	const FTransform HeadRelative = Camera->GetComponentTransform().GetRelativeTransform(VrOriginWorld);
	const FTransform LeftHandRelative = MotionControllerLeft->GetComponentTransform().GetRelativeTransform(VrOriginWorld);
	const FTransform RightHandRelative = MotionControllerRight->GetComponentTransform().GetRelativeTransform(VrOriginWorld);

	/** Send to server — server stores in replicated props → auto-sent to all clients */
	ServerUpdateVRTransforms(HeadRelative, LeftHandRelative, RightHandRelative);
}


// ═══════════════════════════════════════════════════
// VR Transform Apply (non-local pawns)
// ═══════════════════════════════════════════════════

void ACustomXrPawn::ApplyReplicatedVRTransforms()
{
	/**
	 * Convert the replicated relative transforms back to world space
	 * by multiplying with VrOrigin's current world transform.
	 *
	 * RelativeTransform * ParentWorldTransform = ChildWorldTransform
	 *
	 * We set the world transform on Camera and MotionControllers directly.
	 * Their attached children (HeadMountedDisplayMesh, HandLeft, HandRight,
	 * WidgetInteraction) will follow automatically because of the attachment.
	 */
	const FTransform VrOriginWorld = VrOrigin->GetComponentTransform();

	/** Apply head transform — moves Camera + HeadMountedDisplayMesh */
	const FTransform HeadWorld = Rep_HeadTransform * VrOriginWorld;
	Camera->SetWorldTransform(HeadWorld);

	/** Apply left hand — moves MotionControllerLeft + HandLeft + WidgetInteractionLeft */
	const FTransform LeftHandWorld = Rep_LeftHandTransform * VrOriginWorld;
	MotionControllerLeft->SetWorldTransform(LeftHandWorld);

	/** Apply right hand — moves MotionControllerRight + HandRight + WidgetInteractionRight */
	const FTransform RightHandWorld = Rep_RightHandTransform * VrOriginWorld;
	MotionControllerRight->SetWorldTransform(RightHandWorld);
}


// ═══════════════════════════════════════════════════
// Input Binding
// ═══════════════════════════════════════════════════

void ACustomXrPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input) return;

	Input->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ACustomXrPawn::HandleMoveForward);
	Input->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ACustomXrPawn::HandleMoveRight);
	Input->BindAction(SnapTurnAction, ETriggerEvent::Triggered, this, &ACustomXrPawn::HandleSnapTurn);
	
	Input->BindAction(LeftTriggerAction,  ETriggerEvent::Started,  this, &ACustomXrPawn::OnLeftTriggerPressed);
	Input->BindAction(LeftTriggerAction,  ETriggerEvent::Completed, this, &ACustomXrPawn::OnLeftTriggerReleased);
	Input->BindAction(RightTriggerAction, ETriggerEvent::Started,  this, &ACustomXrPawn::OnRightTriggerPressed);
	Input->BindAction(RightTriggerAction, ETriggerEvent::Completed, this, &ACustomXrPawn::OnRightTriggerReleased);
	/**
	 * Also bind Completed — this fires when the thumbstick returns to neutral.
	 * ETriggerEvent::Triggered only fires while the value is above the action's
	 * trigger threshold, so the deadzone-reset code in SnapTurn() never runs
	 * from Triggered alone. Completed sends a zero value which resets bCanSnapTurn.
	 */
	Input->BindAction(SnapTurnAction, ETriggerEvent::Completed, this, &ACustomXrPawn::HandleSnapTurn);

	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACustomXrPawn::HandleJump);
	}
}


// ═══════════════════════════════════════════════════
// Input Handlers — run locally + send to server
// ═══════════════════════════════════════════════════

/**
 * Movement input handler pattern:
 *   1. Execute locally for instant responsiveness (client-side prediction)
 *   2. Send a Server RPC so the server also executes the movement
 *   3. The server's actor position is the authoritative one
 *   4. bReplicateMovement syncs the server's position to all clients
 *
 * If we ARE the server (listen server), the Server RPC executes immediately
 * on the same machine, so there's no double-movement — the local call
 * and the RPC are the same execution path.
 */

void ACustomXrPawn::HandleMoveForward(const FInputActionValue& Value)
{
	const float Val = Value.Get<float>();

	/**
	 * Execute locally for responsiveness — MoveForward now uses the HMD (camera)
	 * forward direction and returns the computed world-space delta vector.
	 * We send that delta to the server so it can apply the exact same movement
	 * without needing VR tracking data.
	 */
	FVector MoveDelta = FVector::ZeroVector;
	if (vrMovementComponent)
	{
		MoveDelta = vrMovementComponent->MoveForward(Val);
	}

	/** If we're a client (not the server), also tell the server to move us */
	if (!HasAuthority() && !MoveDelta.IsNearlyZero())
	{
		ServerMoveForward(MoveDelta);
	}
}

void ACustomXrPawn::HandleMoveRight(const FInputActionValue& Value)
{
	const float Val = Value.Get<float>();

	FVector MoveDelta = FVector::ZeroVector;
	if (vrMovementComponent)
	{
		MoveDelta = vrMovementComponent->MoveRight(Val);
	}

	if (!HasAuthority() && !MoveDelta.IsNearlyZero())
	{
		ServerMoveRight(MoveDelta);
	}
}

void ACustomXrPawn::HandleSnapTurn(const FInputActionValue& Value)
{
	const float Val = Value.Get<float>();

	/**
	 * Snap turn is handled differently from continuous movement:
	 * - Locally: SnapTurn() applies deadzone + cooldown logic and returns the actual angle
	 * - Server RPC: receives the computed turn angle (not raw input) and applies directly
	 * 
	 * This is necessary because the server's copy of bCanSnapTurn never gets reset —
	 * the "thumbstick returned to neutral" input (below deadzone) doesn't fire
	 * ETriggerEvent::Triggered, so the server never sees it. If we sent raw input
	 * to the server's SnapTurn(), it would work once then be permanently locked.
	 */
	float TurnAngle = 0.f;
	if (vrMovementComponent)
	{
		TurnAngle = vrMovementComponent->SnapTurn(Val);
	}

	/** Only send to server if a turn actually happened */
	if (!HasAuthority() && !FMath::IsNearlyZero(TurnAngle))
	{
		ServerSnapTurn(TurnAngle);
	}
}


// ═══════════════════════════════════════════════════
// Server RPC Implementations
// ═══════════════════════════════════════════════════

/**
 * Receives VR tracking data from the owning client.
 * Stores into replicated properties → auto-sent to all other clients.
 */
void ACustomXrPawn::ServerUpdateVRTransforms_Implementation(
	FTransform InHead,
	FTransform InLeftHand,
	FTransform InRightHand)
{
	/**
	UE_LOG(LogTemp, Warning, TEXT("SERVER RPC | Pawn=%s | From=%s"), *GetName(), 
	   GetNetOwningPlayer() ? *GetNetOwningPlayer()->GetName() : TEXT("NULL"));
	*/
	
	Rep_HeadTransform = InHead;
	Rep_LeftHandTransform = InLeftHand;
	Rep_RightHandTransform = InRightHand;
}

/**
 * Executes forward movement on the server using the pre-computed delta.
 * The client computed this delta using HMD forward direction, so the server
 * applies the same world-space offset without needing VR tracking data.
 */
void ACustomXrPawn::ServerMoveForward_Implementation(FVector MoveDelta)
{
	if (vrMovementComponent)
	{
		vrMovementComponent->ApplyMoveDelta(MoveDelta);
	}
}

/** Executes strafe movement on the server using the pre-computed delta */
void ACustomXrPawn::ServerMoveRight_Implementation(FVector MoveDelta)
{
	if (vrMovementComponent)
	{
		vrMovementComponent->ApplyMoveDelta(MoveDelta);
	}
}

/** Executes snap turn on the server using the pre-computed angle */
void ACustomXrPawn::ServerSnapTurn_Implementation(float Value)
{
	if (vrMovementComponent)
	{
		/**
		 * Use ApplySnapTurn instead of SnapTurn — the Value here is already
		 * the computed turn angle (e.g. 45 or -45), not raw thumbstick input.
		 * ApplySnapTurn bypasses deadzone/cooldown since the client already handled that.
		 */
		vrMovementComponent->ApplySnapTurn(Value);
	}
}

void ACustomXrPawn::HandleJump(const FInputActionValue& Value)
{
	if (vrMovementComponent)
	{
		vrMovementComponent->Jump(1.f);
	}

	if (!HasAuthority())
	{
		ServerJump();
	}
}

/** Executes jump on the server */
void ACustomXrPawn::ServerJump_Implementation()
{
	if (vrMovementComponent)
	{
		vrMovementComponent->Jump(1.f);
	}
}

void ACustomXrPawn::OnLeftTriggerPressed(const FInputActionValue& Value)
{
	if (WidgetInteractionLeft)
	{
		WidgetInteractionLeft->PressPointerKey(EKeys::LeftMouseButton);
	}
}

void ACustomXrPawn::OnLeftTriggerReleased(const FInputActionValue& Value)
{
	if (WidgetInteractionLeft)
	{
		WidgetInteractionLeft->ReleasePointerKey(EKeys::LeftMouseButton);
	}
}

void ACustomXrPawn::OnRightTriggerPressed(const FInputActionValue& Value)
{
	if (WidgetInteractionRight)
	{
		WidgetInteractionRight->PressPointerKey(EKeys::LeftMouseButton);
	}
}

void ACustomXrPawn::OnRightTriggerReleased(const FInputActionValue& Value)
{
	if (WidgetInteractionRight)
	{
		WidgetInteractionRight->ReleasePointerKey(EKeys::LeftMouseButton);
	}
}



// ═══════════════════════════════════════════════════
// Capsule Overlap — push physics objects
// ═══════════════════════════════════════════════════

/**
 * Called when the capsule overlaps a physics-simulated object.
 *
 * Since we use ECR_Overlap for PhysicsBody (instead of ECR_Block),
 * the capsule sweep passes right through physics objects — the player
 * doesn't stop. But we still want the "bump away" behavior where
 * walking into a cube/ball pushes it.
 *
 * We apply an impulse in the direction from the player to the object,
 * proportional to PhysicsPushForce. This feels natural and doesn't
 * affect the player's own movement at all.
 */
void ACustomXrPawn::OnCapsuleOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || !OtherComp) return;

	// Only push objects that are simulating physics
	if (!OtherComp->IsSimulatingPhysics()) return;

	// Compute push direction: from our center toward the other object
	FVector PushDir = OtherActor->GetActorLocation() - GetActorLocation();
	PushDir.Z = 0.f; // Keep the push horizontal so we don't launch objects skyward
	
	if (PushDir.IsNearlyZero())
	{
		// Object is exactly on top of us — push in our forward direction
		PushDir = GetActorForwardVector();
	}
	else
	{
		PushDir.Normalize();
	}

	OtherComp->AddImpulse(PushDir * PhysicsPushForce, NAME_None, true);
}


// ═══════════════════════════════════════════════════
// Replication Registration
// ═══════════════════════════════════════════════════

void ACustomXrPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/**
	 * Register VR tracking transforms for replication.
	 * COND_SkipOwner: don't send these back to the owning client —
	 * they already have the real tracking data from their HMD/controllers.
	 * This saves bandwidth and prevents replicated data from fighting
	 * with real tracking on the local pawn.
	 */
	DOREPLIFETIME_CONDITION(ACustomXrPawn, Rep_HeadTransform, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ACustomXrPawn, Rep_LeftHandTransform, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ACustomXrPawn, Rep_RightHandTransform, COND_SkipOwner);
}
