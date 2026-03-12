/**
 * EduXR Custom XR Pawn - VR Player Character with Multiplayer Replication
 *
 * Replication design:
 *   LOCAL PAWN (IsLocallyControlled):
 *     - VR tracking drives Camera + MotionControllers as normal
 *     - Every Tick, capture relative transforms and write into replicated properties
 *     - Movement input calls Server RPCs so the server moves the actor authoritatively
 *
 *   NON-LOCAL PAWN (!IsLocallyControlled) on ANY machine (server or client):
 *     - Camera/MotionController tracking is disabled (no HMD/controller connected)
 *     - Every Tick, read replicated transforms and apply them to the components
 *     - Actor world position is handled by Unreal's built-in movement replication
 *
 * Transform space:
 *   All replicated transforms are RELATIVE TO VrOrigin. This avoids world-space
 *   rotation issues — the transforms stay correct regardless of pawn facing direction.
 *   On apply, we multiply by VrOrigin's world transform to get the final world position.
 */
#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "MotionControllerComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "CustomXrPawn.generated.h"

class UVrMovementComponent;

UCLASS()
class OPENXRMULTIPLAYER_API ACustomXrPawn : public APawn
{
	GENERATED_BODY()

public:
	ACustomXrPawn();

	/** Get the VR camera component (for HMD-oriented movement) */
	UFUNCTION(BlueprintCallable, Category="VR")
	UCameraComponent* GetVRCamera() const { return Camera; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ─────────────────────────────────────────────
	// Core VR Components
	// ─────────────────────────────────────────────

	/** Scene root — represents the VR play-space origin */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	USceneComponent* VrOrigin;

	/** VR camera, attached to VrOrigin. Tracks the HMD on the local pawn */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	UCameraComponent* Camera;

	/** Capsule used for collision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	UCapsuleComponent* CapsuleCollider;

	/** Left motion controller — tracks the left hand on the local pawn */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	UMotionControllerComponent* MotionControllerLeft;

	/** Right motion controller — tracks the right hand on the local pawn */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	UMotionControllerComponent* MotionControllerRight;

	/** Widget interaction for left hand UI pointing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	UWidgetInteractionComponent* WidgetInteractionLeft;

	/** Widget interaction for right hand UI pointing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR", meta=(AllowPrivateAccess="true"))
	UWidgetInteractionComponent* WidgetInteractionRight;

	// ─────────────────────────────────────────────
	// Visual Meshes (what other players see)
	// ─────────────────────────────────────────────

	/** Static mesh representing the HMD — attached to Camera so it follows head tracking */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Visuals", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* HeadMountedDisplayMesh;

	/** Left hand skeletal mesh — attached to Mo	tionControllerLeft */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Visuals", meta=(AllowPrivateAccess="true"))
	USkeletalMeshComponent* HandLeft;

	/** Right hand skeletal mesh — attached to MotionControllerRight */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Visuals", meta=(AllowPrivateAccess="true"))
	USkeletalMeshComponent* HandRight;

	/** Body mesh — attached to VrOrigin */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Visuals", meta=(AllowPrivateAccess="true"))
	USkeletalMeshComponent* PlayerMesh;

	/** Custom VR movement component (smooth locomotion + snap turn) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Movement", meta=(AllowPrivateAccess="true"))
	UVrMovementComponent* vrMovementComponent;

	// ─────────────────────────────────────────────
	// Input Actions
	// ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* MoveForwardAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* MoveRightAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* SnapTurnAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* LeftTriggerAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* RightTriggerAction;

	// ─────────────────────────────────────────────
	// Replicated VR Tracking Transforms
	// All transforms are RELATIVE TO VrOrigin
	// ─────────────────────────────────────────────

	/**
	 * Head (camera) transform relative to VrOrigin
	 * Replicated from server → all clients
	 * Written by the owning client via ServerUpdateVRTransforms
	 */
	UPROPERTY(Replicated)
	FTransform Rep_HeadTransform;

	/**
	 * Left hand (motion controller) transform relative to VrOrigin
	 */
	UPROPERTY(Replicated)
	FTransform Rep_LeftHandTransform;

	/**
	 * Right hand (motion controller) transform relative to VrOrigin
	 */
	UPROPERTY(Replicated)
	FTransform Rep_RightHandTransform;

	// ─────────────────────────────────────────────
	// Server RPCs
	// ─────────────────────────────────────────────

	/**
	 * Server RPC — sends local VR tracking data to the server every Tick
	 *
	 * Unreliable because this fires every frame (~90Hz). If a packet is lost,
	 * the next frame immediately sends fresh data, so reliability is unnecessary.
	 *
	 * The server stores the transforms in the replicated properties, which
	 * Unreal automatically sends to all other clients.
	 */
	UFUNCTION(Server, Unreliable)
	void ServerUpdateVRTransforms(
		FTransform InHead,
		FTransform InLeftHand,
		FTransform InRightHand
	);

	/**
	 * Server RPC — executes movement on the server
	 *
	 * Receives a pre-computed movement delta vector from the client.
	 * The client computes this using the HMD forward direction, so the server
	 * doesn't need VR tracking data to determine move direction.
	 *
	 * Unreliable because this is continuous input (thumbstick held down).
	 * The actor's world position is replicated automatically via bReplicateMovement.
	 */
	UFUNCTION(Server, Unreliable)
	void ServerMoveForward(FVector MoveDelta);

	/** Server RPC — executes strafe on the server using pre-computed delta */
	UFUNCTION(Server, Unreliable)
	void ServerMoveRight(FVector MoveDelta);

	/**
	 * Server RPC — executes snap turn on the server
	 *
	 * Reliable because snap turn is a discrete event that must not be lost
	 */
	UFUNCTION(Server, Reliable)
	void ServerSnapTurn(float Value);

	/**
	 * Server RPC — executes jump on the server
	 *
	 * Reliable because jump is a discrete event that must not be lost
	 */
	UFUNCTION(Server, Reliable)
	void ServerJump();
	
	
	/** How much force to apply when pushing physics objects (in Newtons) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Collision")
	float PhysicsPushForce = 500.f;

private:
	// ─────────────────────────────────────────────
	// Internal Helpers
	// ─────────────────────────────────────────────

	/** Input callback wrappers — handle input locally + send to server */
	void HandleMoveForward(const FInputActionValue& Value);
	void HandleMoveRight(const FInputActionValue& Value);
	void HandleSnapTurn(const FInputActionValue& Value);
	void HandleJump(const FInputActionValue& Value);

	/**
	 * Captures the current VR component transforms relative to VrOrigin
	 * and sends them to the server for replication.
	 * Called every Tick on the locally controlled pawn only.
	 */
	void CaptureAndSendVRTransforms();

	/**
	 * Applies the replicated VR transforms to the visual components
	 * so other players see head/hands in the correct positions.
	 *
	 * Called every Tick on non-local pawns.
	 * Works on BOTH server and clients (unlike OnRep which only works on clients).
	 *
	 * Sets Camera and MotionController world transforms by combining
	 * the relative replicated transform with VrOrigin's world transform.
	 * Since HeadMountedDisplayMesh/HandLeft/HandRight are attached as children,
	 * they follow automatically.
	 */
	void ApplyReplicatedVRTransforms();

	void OnLeftTriggerPressed(const FInputActionValue& Value);
	void OnLeftTriggerReleased(const FInputActionValue& Value);
	void OnRightTriggerPressed(const FInputActionValue& Value);
	void OnRightTriggerReleased(const FInputActionValue& Value);
	/**
	 * Called when the capsule overlaps a physics object.
	 * Applies an impulse to push the object away from the player,
	 * simulating the old "bump away" behavior without blocking movement.
	 */
	UFUNCTION()
	void OnCapsuleOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	

#if !UE_BUILD_SHIPPING
	/** Debug timer for throttled network state logging (per-pawn, not static) */
	float DebugTimer = 0.f;
#endif
};
