// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "XMUFoundationCharacter.generated.h"


class UXMUFoundationMovement;
struct FInputActionValue;
/**
 * FXMUReplicatedAcceleration: Compressed representation of acceleration
 */
USTRUCT()
struct FXMUReplicatedAcceleration
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 AccelXYRadians = 0;	// Direction of XY accel component, quantized to represent [0, 2*pi]

	UPROPERTY()
	uint8 AccelXYMagnitude = 0;	//Accel rate of XY component, quantized to represent [0, MaxAcceleration]

	UPROPERTY()
	int8 AccelZ = 0;	// Raw Z accel rate component, quantized to represent [-MaxAcceleration, MaxAcceleration]
};


UCLASS()
class XYLOMOVEMENTUTIL_API AXMUFoundationCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AXMUFoundationCharacter(const FObjectInitializer& ObjectInitializer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * ACharacter Interface
	 */
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

/*--------------------------------------------------------------------------------------------------------------------*/
	/* Jump */

public:
	virtual void CheckJumpInput(float DeltaTime) override;
protected:
	virtual bool CanJumpInternal_Implementation() const override;
	/** Virtual version of JumpIsAllowedInternal */ 
	virtual bool JumpIsAllowed() const;

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
	/* Crouch */

public:
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnRep_IsCrouched() override;

/*--------------------------------------------------------------------------------------------------------------------*/
	

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * AXMUFoundationCharacter
	 */

public:
	UXMUFoundationMovement* GetFoundationMovement() const { return FoundationMovement; }
private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UXMUFoundationMovement> FoundationMovement;

public:
	FCollisionQueryParams GetIgnoreCharacterParams() const;
	
protected:
	UFUNCTION()
	void OnRep_ReplicatedAcceleration();
private:
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FXMUReplicatedAcceleration ReplicatedAcceleration;

/*--------------------------------------------------------------------------------------------------------------------*/
	/* Crouch */

public:
	/** ranges from 0 (when standing) to 1 (when fully crouched) */
	UFUNCTION(BlueprintCallable)
	float GetCrouchPercentage() const;
	
/*--------------------------------------------------------------------------------------------------------------------*/
	
/*--------------------------------------------------------------------------------------------------------------------*/
	/* Input */
	
protected:
	virtual void Move(const FInputActionValue& Value);
	virtual void Look(const FInputActionValue& Value);

/*--------------------------------------------------------------------------------------------------------------------*/
	
};
