/**
 * EduXR Custom XR Pawn - VR Player Character
 * 
 * Custom pawn class for XR experiences that manages VR-specific components
 * and replication for multiplayer environments.
 * 
 * Features:
 *  - VR head (camera) tracking
 *  - Left and right hand tracking
 *  - Network replication for head and hand transforms
 *  - Server-authoritative pose updates
 *  - RepNotify callbacks for transform changes
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CustomXrPawn.generated.h"

UCLASS()
class OPENXRMULTIPLAYER_API ACustomXrPawn : public APawn
{
	GENERATED_BODY()

public:
	/**
	 * Constructor
	 * Sets default values for this pawn's properties
	 */
	ACustomXrPawn();
	
	/**
	 * VR Head Camera Component
	 * Represents the player's head position and rotation in VR space
	 */
	UPROPERTY(BlueprintReadWrite, Category = "VR Components")
	class UCameraComponent* VRHeadCamera;

	/**
	 * Left Hand Root Component
	 * Base scene component for left hand tracking
	 */
	UPROPERTY(BlueprintReadWrite, Category = "VR Components")
	class USceneComponent* LeftHandRoot;

	/**
	 * Right Hand Root Component
	 * Base scene component for right hand tracking
	 */
	UPROPERTY(BlueprintReadWrite, Category = "VR Components")
	class USceneComponent* RightHandRoot;

	/**
	 * Left Hand Skeletal Mesh
	 * Visual representation of the left hand
	 */
	UPROPERTY(BlueprintReadWrite, Category = "VR Components")
	class USkeletalMeshComponent* LeftHandMesh;

	/**
	 * Right Hand Skeletal Mesh
	 * Visual representation of the right hand
	 */
	UPROPERTY(BlueprintReadWrite, Category = "VR Components")
	class USkeletalMeshComponent* RightHandMesh;

	/**
	 * Replicated Head Transform
	 * Position and rotation of the player's head
	 * Replicated to all clients and triggers OnRep_HeadTransform when changed
	 */
	UPROPERTY(ReplicatedUsing=OnRep_HeadTransform, BlueprintReadOnly, Category = "VR Replication")
	FTransform HeadTransform;

	/**
	 * Replicated Left Hand Transform
	 * Position and rotation of the player's left hand
	 * Replicated to all clients and triggers OnRep_LeftHandTransform when changed
	 */
	UPROPERTY(ReplicatedUsing=OnRep_LeftHandTransform, BlueprintReadOnly, Category = "VR Replication")
	FTransform LeftHandTransform;

	/**
	 * Replicated Right Hand Transform
	 * Position and rotation of the player's right hand
	 * Replicated to all clients and triggers OnRep_RightHandTransform when changed
	 */
	UPROPERTY(ReplicatedUsing=OnRep_RightHandTransform, BlueprintReadOnly, Category = "VR Replication")
	FTransform RightHandTransform;

protected:
	/**
	 * Called when the game starts or when spawned
	 * Initializes VR components and tracking
	 */
	virtual void BeginPlay() override;
	
	/**
	 * Get properties to replicate
	 * Registers which properties should be replicated across the network
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/**
	 * Server RPC: Update head pose
	 * Called by client to send head position/rotation to server
	 * Server validates and replicates to all clients
	 * 
	 * @param NewHeadTransform The new head position and rotation
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpdateHeadPose(FTransform NewHeadTransform);

	/**
	 * Server RPC: Update hand poses
	 * Called by client to send both hand positions/rotations to server
	 * Server validates and replicates to all clients
	 * 
	 * @param NewLeftHand The new left hand position and rotation
	 * @param NewRightHand The new right hand position and rotation
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_UpdateHandPoses(FTransform NewLeftHand, FTransform NewRightHand);

	/**
	 * RepNotify: Head Transform Changed
	 * Called on all clients when the replicated head transform changes
	 * Updates the VR head camera component position
	 */
	UFUNCTION()
	void OnRep_HeadTransform();

	/**
	 * RepNotify: Left Hand Transform Changed
	 * Called on all clients when the replicated left hand transform changes
	 * Updates the left hand mesh position and rotation
	 */
	UFUNCTION()
	void OnRep_LeftHandTransform();

	/**
	 * RepNotify: Right Hand Transform Changed
	 * Called on all clients when the replicated right hand transform changes
	 * Updates the right hand mesh position and rotation
	 */
	UFUNCTION()
	void OnRep_RightHandTransform();

	/**
	 * Update head pose from local OpenXR input
	 * Sends the new head transform to the server for replication
	 * Called locally by the player's VR device
	 * 
	 * @param NewHeadTransform The new head position and rotation from OpenXR
	 */
	UFUNCTION(BlueprintCallable, Category = "VR")
	void UpdateHeadPose(const FTransform& NewHeadTransform);

	/**
	 * Update hand poses from local OpenXR input
	 * Sends both hand transforms to the server for replication
	 * Called locally by the player's VR device
	 * 
	 * @param NewLeftHand The new left hand position and rotation from OpenXR
	 * @param NewRightHand The new right hand position and rotation from OpenXR
	 */
	UFUNCTION(BlueprintCallable, Category = "VR")
	void UpdateHandPoses(const FTransform& NewLeftHand, const FTransform& NewRightHand);

public:	
	/**
	 * Tick - Called every frame
	 * Used for continuous updates to VR tracking and synchronization
	 * 
	 * @param DeltaTime Time elapsed since last frame in seconds
	 */
	virtual void Tick(float DeltaTime) override;

	/**
	 * Setup player input component
	 * Binds input actions to functions for VR interaction
	 * 
	 * @param PlayerInputComponent The input component to bind actions to
	 */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
