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

	VrOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VrOrigin"));
	SetRootComponent(VrOrigin);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VrOrigin);

	CapsuleCollider = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleCollider"));
	CapsuleCollider->SetupAttachment(VrOrigin);

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
	
	// ── ADD THIS DEBUG ──
	if (GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("CLIENT TICK | Pawn=%s | LocalControlled=%s | HasAuthority=%s"), 
			*GetName(), 
			IsLocallyControlled() ? TEXT("YES") : TEXT("NO"),
			HasAuthority() ? TEXT("YES") : TEXT("NO"));
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

	/** Execute locally for responsiveness */
	if (vrMovementComponent)
	{
		vrMovementComponent->MoveForward(Val);
	}

	/** If we're a client (not the server), also tell the server to move us */
	if (!HasAuthority())
	{
		ServerMoveForward(Val);
	}
}

void ACustomXrPawn::HandleMoveRight(const FInputActionValue& Value)
{
	const float Val = Value.Get<float>();

	if (vrMovementComponent)
	{
		vrMovementComponent->MoveRight(Val);
	}

	if (!HasAuthority())
	{
		ServerMoveRight(Val);
	}
}

void ACustomXrPawn::HandleSnapTurn(const FInputActionValue& Value)
{
	const float Val = Value.Get<float>();

	if (vrMovementComponent)
	{
		vrMovementComponent->SnapTurn(Val);
	}

	if (!HasAuthority())
	{
		ServerSnapTurn(Val);
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
	UE_LOG(LogTemp, Warning, TEXT("SERVER RPC | Pawn=%s | From=%s"), *GetName(), 
	   GetNetOwningPlayer() ? *GetNetOwningPlayer()->GetName() : TEXT("NULL"));
	
	Rep_HeadTransform = InHead;
	Rep_LeftHandTransform = InLeftHand;
	Rep_RightHandTransform = InRightHand;
}

/**
 * Executes forward movement on the server.
 * The resulting position change is replicated via bReplicateMovement.
 */
void ACustomXrPawn::ServerMoveForward_Implementation(float Value)
{
	if (vrMovementComponent)
	{
		vrMovementComponent->MoveForward(Value);
	}
}

/** Executes strafe movement on the server */
void ACustomXrPawn::ServerMoveRight_Implementation(float Value)
{
	if (vrMovementComponent)
	{
		vrMovementComponent->MoveRight(Value);
	}
}

/** Executes snap turn on the server */
void ACustomXrPawn::ServerSnapTurn_Implementation(float Value)
{
	if (vrMovementComponent)
	{
		vrMovementComponent->SnapTurn(Value);
	}
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
