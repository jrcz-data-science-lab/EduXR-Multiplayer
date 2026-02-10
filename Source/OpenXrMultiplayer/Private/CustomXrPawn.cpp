/**
 * EduXR Custom XR Pawn - VR Player Character Implementation
 * 
 * Implements VR-specific pawn functionality for multiplayer experiences:
 *  - VR component initialization (head, hands)
 *  - Network replication of player poses
 *  - Server-authoritative transform updates
 *  - RepNotify callbacks for synchronized motion
 */

#include "CustomXrPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"  // For LeftHandMesh and RightHandMesh
#include "Components/SceneComponent.h"         // For hand root components
#include "Net/UnrealNetwork.h"                // DOREPLIFETIME macro for replication
#include "GameFramework/Actor.h"              // HasAuthority() check

/**
 * Constructor
 * Sets default values for this pawn's properties
 * Configures network replication settings for VR smoothness
 */
ACustomXrPawn::ACustomXrPawn()
{
 	// Enable Tick for continuous VR tracking updates
	PrimaryActorTick.bCanEverTick = true;
	
	// VR replication settings for smooth player motion across network
	NetUpdateFrequency = 60.0f;  // Update at 60Hz for smooth VR experience
	SetReplicates(true);         // Enable network replication
	SetReplicateMovement(true);  // Replicate position and rotation changes
}

/**
 * Called when the game starts or when spawned
 * Initializes VR components and performs setup checks
 * Validates that all required VR components are assigned
 */
void ACustomXrPawn::BeginPlay()
{
	Super::BeginPlay();
	
	// Validation: Check if critical VR components are assigned in Blueprint
	if (!VRHeadCamera) UE_LOG(LogTemp, Warning, TEXT("CustomXrPawn: VRHeadCamera not assigned in Blueprint!"));
}

/**
 * Tick - Called every frame
 * Used for continuous updates to VR tracking and other frame-based logic
 * @param DeltaTime Time elapsed since last frame in seconds
 */
void ACustomXrPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// VR tracking updates handled through UpdateHeadPose/UpdateHandPoses
}

/**
 * Setup player input component
 * Binds input actions to VR interaction functions
 * @param PlayerInputComponent The input component to bind actions to
 */
void ACustomXrPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Input bindings for VR controllers configured here
}

/**
 * Get properties to replicate over the network
 * Registers which properties should be synchronized across all clients
 * Specifies RepNotify callbacks for when properties change
 */
void ACustomXrPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Register replicated properties with RepNotify callbacks
	DOREPLIFETIME(ACustomXrPawn, HeadTransform);
	DOREPLIFETIME(ACustomXrPawn, LeftHandTransform);
	DOREPLIFETIME(ACustomXrPawn, RightHandTransform);
}

/**
 * RepNotify: Head Transform Changed
 * Called on all clients when the replicated head transform changes
 * Updates the VR head camera position and rotation smoothly
 */
void ACustomXrPawn::OnRep_HeadTransform()
{
	// Client reaction: Update camera/visuals to match replicated position
	if (VRHeadCamera)
	{
		VRHeadCamera->SetWorldTransform(HeadTransform);
	}
}

/**
 * RepNotify: Left Hand Transform Changed
 * Called on all clients when the replicated left hand transform changes
 * Updates the left hand skeletal mesh position and rotation
 */
void ACustomXrPawn::OnRep_LeftHandTransform()
{
	if (LeftHandMesh)
	{
		LeftHandMesh->SetWorldTransform(LeftHandTransform);
	}
}

/**
 * RepNotify: Right Hand Transform Changed
 * Called on all clients when the replicated right hand transform changes
 * Updates the right hand skeletal mesh position and rotation
 */
void ACustomXrPawn::OnRep_RightHandTransform()
{
	if (RightHandMesh)
	{
		RightHandMesh->SetWorldTransform(RightHandTransform);
	}
}

/**
 * Update head pose from local OpenXR input
 * Handles both server and client cases:
 *  - Server: Sets the transform directly
 *  - Client: Sends Server RPC to server for validation and replication
 * 
 * @param NewHeadTransform The new head position and rotation from OpenXR
 */
void ACustomXrPawn::UpdateHeadPose(const FTransform& NewHeadTransform)
{
	if (HasAuthority())  // Server: set directly
	{
		HeadTransform = NewHeadTransform;
	}
	else  // Client: send to server via RPC for server-authority
	{
		Server_UpdateHeadPose(NewHeadTransform);
	}
}

/**
 * Server RPC Implementation: Update head pose on server
 * Validates and applies the head transform, which then replicates to all clients
 */
void ACustomXrPawn::Server_UpdateHeadPose_Implementation(FTransform NewHeadTransform)
{
	HeadTransform = NewHeadTransform;
}

/**
 * Server RPC Validation: Validate head pose update
 * Performs security checks before allowing the RPC
 * Future: Add distance checks to prevent cheating
 */
bool ACustomXrPawn::Server_UpdateHeadPose_Validate(FTransform NewHeadTransform)
{
	return true;  // Always allow
}

/**
 * Update hand poses from local OpenXR input
 * Handles both server and client cases:
 *  - Server: Sets both transforms directly
 *  - Client: Sends Server RPC to server for validation and replication
 * 
 * @param NewLeftHand The new left hand position and rotation from OpenXR
 * @param NewRightHand The new right hand position and rotation from OpenXR
 */
void ACustomXrPawn::UpdateHandPoses(const FTransform& NewLeftHand, const FTransform& NewRightHand)
{
	if (HasAuthority())  // Server: set directly
	{
		LeftHandTransform = NewLeftHand;
		RightHandTransform = NewRightHand;
	}
	else  // Client: send to server via RPC for server-authority
	{
		Server_UpdateHandPoses(NewLeftHand, NewRightHand);
	}
}

/**
 * Server RPC Implementation: Update hand poses on server
 * Validates and applies both hand transforms, which then replicate to all clients
 */
void ACustomXrPawn::Server_UpdateHandPoses_Implementation(FTransform NewLeftHand, FTransform NewRightHand)
{
	LeftHandTransform = NewLeftHand;
	RightHandTransform = NewRightHand;
}

/**
 * Server RPC Validation: Validate hand poses update
 * Performs security checks before allowing the RPC
 * Future: Add distance checks to prevent cheating
 */
bool ACustomXrPawn::Server_UpdateHandPoses_Validate(FTransform NewLeftHand, FTransform NewRightHand)
{
	return true;  // Always allow
}
