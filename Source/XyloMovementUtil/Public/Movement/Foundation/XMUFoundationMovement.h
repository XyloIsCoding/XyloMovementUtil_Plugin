// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "XMUFoundationMovement.generated.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Move Response Data: What we receive from server
 *	call UCharacterMovementComponent::SetMoveResponseDataContainer in cmc constructor to use this class
 */

class AXMUFoundationCharacter;

struct XYLOMOVEMENTUTIL_API FXMUFoundationMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;

	/**
	 * Copy the FClientAdjustment and set a few flags relevant to that data.
	 */
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;

	float Stamina;
	bool bStaminaDrained;
	float Charge;
	bool bChargeDrained;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Move Data: What we send to server
 */

struct XYLOMOVEMENTUTIL_API FXMUFoundationNetworkMoveData : public FCharacterNetworkMoveData
{
public:
	typedef FCharacterNetworkMoveData Super;
 
	FXMUFoundationNetworkMoveData()
		: FoundationCompressedMoveFlags(0)
		, Stamina(0)
		, Charge(0)
	{
	}

	/**
	 * Given a FSavedMove_Character from UCharacterMovementComponent, fill in data in this struct with relevant movement data.
	 * Note that the instance of the FSavedMove_Character is likely a custom struct of a derived struct of your own, if you have added your own saved move data.
	 * @see UCharacterMovementComponent::AllocateNewMove()
	 */
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	/*
	 * Custom Data
	 */

	uint8 FoundationCompressedMoveFlags; // generated using FXMUSavedMove_Character_Foundation::GetFoundationCompressedFlags
	float Stamina;
	float Charge;
	
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Move Data Container: Wraps Network Move Data to send to server
 *	call UCharacterMovementComponent::SetNetworkMoveDataContainer in cmc constructor to use this class
 */

struct XYLOMOVEMENTUTIL_API FXMUFoundationNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
public:
	typedef FCharacterNetworkMoveDataContainer Super;
 
	FXMUFoundationNetworkMoveDataContainer()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}
 
private:
	FXMUFoundationNetworkMoveData MoveData[3];
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Saved Move: passed to FCharacterNetworkMoveData to be sent to server when necessary
 */

class XYLOMOVEMENTUTIL_API FXMUSavedMove_Character_Foundation : public FSavedMove_Character
{
	using Super = FSavedMove_Character;
	
public:
	FXMUSavedMove_Character_Foundation()
		: Stamina(0)
		, bStaminaDrained(0)
		, Charge(0)
		, bChargeDrained(0)
		, AnimRootMotionTransitionName("")
		, bAnimRootMotionTransitionFinishedLastFrame(0)
		, RootMotionSourceTransitionName("")
		, bRootMotionSourceTransitionFinishedLastFrame(0)
	{
	}

	virtual ~FXMUSavedMove_Character_Foundation() override
	{}
	
	float Stamina;
	uint32 bStaminaDrained : 1;
	float Charge;
	uint32 bChargeDrained : 1;

	FString AnimRootMotionTransitionName;
	uint32 bAnimRootMotionTransitionFinishedLastFrame : 1;
	FString RootMotionSourceTransitionName;
	uint32 bRootMotionSourceTransitionFinishedLastFrame : 1;

	/** Clear saved move properties, so it can be re-used. */
	virtual void Clear() override;

	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
	
	/** Returns true if this move can be combined with NewMove for replication without changing any behavior 
	 * <p> Call Context: Called in ReplicateMoveToServer between SetMoveFor and PerformMovement
	 * <p> Note: Returns true if nothing important changed during last update */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Combine this move with an older move and update relevant state. 
	 * <p> Call Context: Called in ReplicateMoveToServer if CanCombineWith returns true
	 * <p> Note: Since nothing changed since PendingMove, we merge PendingMove and NewMove, and simulate
	 *			 both together, which means reverting the Character / CMC state to the PendingMove */
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;

	/** Called to set up this saved move (when initially created) to make a predictive correction.
	 * <p> Call Context: Called in ReplicateMoveToServer before CanCombineWith and PerformMovement */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;

	/** Set the properties describing the position, etc. of the moved pawn at the start of the move.
	 * <p> Call Context: Called in SetMoveFor and in ReplicateMoveToServer if we called CombineWith
	 * <p> Note: Here we set the values in this SavedMove that will change during PerformMovement. We do it here because
	 *			 it will update this move after CombineWith (thus setting this move values to the PendingMove one) */
	virtual void SetInitialPosition(ACharacter* C) override;

	/** Called before ClientUpdatePosition uses this SavedMove to make a predictive correction
	 * <p> Call Context: Called in ClientUpdatePositionAfterServerUpdate before MoveAutonomous
	 * <p> Note: Does not need to deal with variables that drive compressed flags cause MoveAutonomous already calls
	 *			 UpdateFromCompressedFlags.
	 * <p> Note 2: Should not restore any variable that is sent back with FCharacterMoveResponseDataContainer else it
	 *			   will override the server correction, causing more desync */
	virtual void PrepMoveFor(ACharacter* C) override;

	/** Set the properties describing the final position, etc. of the moved pawn.
	 * <p> Call Context: Called in ReplicateMoveToServer after PerformMovement and
	 *					 in ClientUpdatePositionAfterServerUpdate after MoveAutonomous */
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;

	/** Generate a compressed flags Foundationd on saved moved variables */
	virtual uint8 GetFoundationCompressedFlags() const;

	// Bit masks used by GetCompressedFlags() to encode movement information.
	enum FoundationCompressedFlags
	{
		FLAG_Foundation_Custom_0		= 0x01,
		FLAG_Foundation_Custom_1		= 0x02,
		FLAG_Foundation_Custom_2		= 0x04,
		FLAG_Foundation_Custom_3		= 0x08,
		FLAG_Foundation_Custom_4		= 0x10,
		FLAG_Foundation_Custom_5		= 0x20,
		FLAG_Foundation_Custom_6		= 0x40,
		FLAG_Foundation_Custom_7		= 0x80,
	};
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Prediction Data: manages saved moves
 *	it is returned by UXMUFoundationMovement::GetPredictionData_Client, this is the way we set the SavedMove class
 *	for this component
 */

class XYLOMOVEMENTUTIL_API FXMUNetworkPredictionData_Client_Character_Foundation : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FXMUNetworkPredictionData_Client_Character_Foundation(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return MakeShared<FXMUSavedMove_Character_Foundation>();
	}
};









////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * UXMUFoundationMovement
 */


UENUM()
enum EXMUCustomMovementMode
{
	CMOVE_None UMETA(Hidden),
	
};

USTRUCT()
struct FXMUAnimRootMotion
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Name = "";
	TWeakObjectPtr<UAnimMontage> Montage = nullptr;
	UPROPERTY()
	bool bFinishedLastFrame = false;

	virtual void Reset()
	{
		Name = "";
		Montage = nullptr;
		bFinishedLastFrame = false;
	}
};

USTRUCT()
struct FXMURootMotionSource
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Name = "";
	TSharedPtr<FRootMotionSource_MoveToForce> RMS;
	UPROPERTY()
	int16 ID = 0;
	UPROPERTY()
	bool bFinishedLastFrame = false;

	virtual void Reset()
	{
		Name = "";
		RMS.Reset();
		ID = 0;
		bFinishedLastFrame = false;
	}
};


/**
 * FXMUCharacterGroundInfo
 *
 *	Information about the ground under the character.  It only gets updated as needed.
 */
USTRUCT(BlueprintType)
struct FXMUCharacterGroundInfo
{
	GENERATED_BODY()

	FXMUCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};


/**
 * 
 */
UCLASS()
class XYLOMOVEMENTUTIL_API UXMUFoundationMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UXMUFoundationMovement(const FObjectInitializer& ObjectInitializer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * UCharacterMovementComponent Interface
	 */

public:
	virtual bool HasValidData() const override;
	virtual void PostLoad() override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves) override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual void Crouch(bool bClientSimulation) override;
	virtual void UnCrouch(bool bClientSimulation) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
protected:
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * UXMUFoundationMovement
	 */
	
/*--------------------------------------------------------------------------------------------------------------------*/
	/* Helpers */

private:
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AXMUFoundationCharacter> FoundationCharacterOwner;
	
public:
	UFUNCTION(BlueprintPure)
	virtual bool IsCustomMovementMode(EXMUCustomMovementMode InCustomMovementMode) const;

protected:
	virtual bool IsServer() const;
	UFUNCTION(BlueprintCallable, Category = "Foundation Movement")
	virtual float CapR() const;
	UFUNCTION(BlueprintCallable, Category = "Foundation Movement")
	virtual float CapHH() const;

	virtual FVector GetControllerForwardVector() const;
	virtual FVector GetControllerRightVector() const;
	
/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
	
	/* Extension */

protected:
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxAirSpeed;

	
public:
	void SetReplicatedAcceleration(const FVector& InAcceleration);
protected:
	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;

public:
	// Returns the current ground info.  Calling this will update the ground info if it's out of date.
	UFUNCTION(BlueprintCallable, Category = "XyloMovementUtil|CharacterMovement")
	const FXMUCharacterGroundInfo& GetGroundInfo();
protected:
	// Cached ground info for the character.  Do not access this directly! It's only updated when accessed via GetGroundInfo().
	FXMUCharacterGroundInfo CachedGroundInfo;

public:
	virtual bool CheckOverrideJumpInput(float DeltaSeconds);
	
/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
	/* Anim Root Motion Transitions */

public:
	FXMUAnimRootMotion AnimRootMotionTransition;
protected:
	/** Override to run logic after playing a root motion source transition */
	virtual void PostAnimRootMotionTransition(FString TransitionName);
protected:
	bool bHadAnimRootMotion = false;

/*--------------------------------------------------------------------------------------------------------------------*/
	
/*--------------------------------------------------------------------------------------------------------------------*/
	/* Root Motion Source Transitions */

public:
	FXMURootMotionSource RootMotionSourceTransition;
protected:
	/** Override to run logic after playing a root motion source transition */
	virtual void PostRootMotionSourceTransition(FString TransitionName);

/*--------------------------------------------------------------------------------------------------------------------*/
	
/*--------------------------------------------------------------------------------------------------------------------*/
	/* Stamina */
	
public:
	UFUNCTION(BlueprintCallable)
	float GetStamina() const { return Stamina; }
	UFUNCTION(BlueprintCallable)
	float GetMaxStamina() const { return MaxStamina; }
	bool IsStaminaDrained() const { return bStaminaDrained; }
	void SetStamina(float NewStamina);
	void SetMaxStamina(float NewMaxStamina);
	void SetStaminaDrained(bool bNewValue);
	void DebugStamina() const;
protected:
    /*
     * Drain state entry and exit is handled here. Drain state is used to prevent rapid re-entry of sprinting or other
     * such abilities before sufficient stamina has regenerated. However, in the default implementation, 100%
     * stamina must be regenerated. Consider overriding this, check the implementation's comment for more information.
     */
    virtual void OnStaminaChanged(float PrevValue, float NewValue);

    virtual void OnMaxStaminaChanged(float PrevValue, float NewValue) {}
    virtual void OnStaminaDrained() {}
    virtual void OnStaminaDrainRecovered() {}
protected:
	/** THIS SHOULD ONLY BE MODIFIED IN DERIVED CLASSES FROM OnStaminaChanged AND NOWHERE ELSE */
	UPROPERTY()
	float Stamina;
private:
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	bool StaminaRegenRate;
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float DefaultMaxStamina;
	UPROPERTY()
	float MaxStamina;
	UPROPERTY()
	bool bStaminaDrained;
	/** Maximum stamina difference that is allowed between client and server before a correction occurs. */
	UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
	float NetworkStaminaCorrectionThreshold;

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
	/* Charge */
	
public:
	UFUNCTION(BlueprintCallable)
	float GetCharge() const { return Charge; }
	UFUNCTION(BlueprintCallable)
	float GetMaxCharge() const { return MaxCharge; }
	bool IsChargeDrained() const { return bChargeDrained; }
	void SetCharge(float NewCharge);
	void SetMaxCharge(float NewMaxCharge);
	void SetChargeDrained(bool bNewValue);
	void DebugCharge() const;
protected:
	/*
	 * Drain state entry and exit is handled here. Drain state is used to prevent rapid re-entry of sprinting or other
	 * such abilities before sufficient Charge has regenerated. However, in the default implementation, 100%
	 * Charge must be regenerated. Consider overriding this, check the implementation's comment for more information.
	 */
	virtual void OnChargeChanged(float PrevValue, float NewValue);

	virtual void OnMaxChargeChanged(float PrevValue, float NewValue) {}
	virtual void OnChargeDrained() {}
	virtual void OnChargeDrainRecovered() {}
protected:
	/** THIS SHOULD ONLY BE MODIFIED IN DERIVED CLASSES FROM OnChargeChanged AND NOWHERE ELSE */
	UPROPERTY()
	float Charge;
private:
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	float ChargeRegenRate;
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	float DefaultMaxCharge;
	UPROPERTY()
	float MaxCharge;
	UPROPERTY()
	bool bChargeDrained;
	/** Maximum stamina difference that is allowed between client and server before a correction occurs. */
	UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
	float NetworkChargeCorrectionThreshold;

/*--------------------------------------------------------------------------------------------------------------------*/
	
/*--------------------------------------------------------------------------------------------------------------------*/
	/* Networking stuff */

public:
	/** Check for Server-Client disagreement in position or other movement state important enough to trigger a client
	 * correction. */
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
			const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
			UPrimitiveComponent* ClientMovementFoundation, FName ClientFoundationBoneName, uint8 ClientMovementMode) override;
	/** Event notification when client receives correction data from the server, before applying the data. Base
	 * implementation logs relevant data and draws debug info if "p.NetShowCorrections" is not equal to 0.
	 * <p> Call Context: called by ClientAdjustPosition_Implementation */
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
		FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewFoundation, FName NewFoundationBoneName, bool bHasFoundation,
		bool bFoundationRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;
protected:
	/** If bUpdatePosition is true, then replay any unacked moves. Returns whether any moves were actually replayed.
	 * <p> Call Context: called by TickComponent */ 
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	
protected:
	virtual void UpdateFromFoundationCompressedFlags();
public:
	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on
	 * demand and can be overridden to allocate a custom override if desired. Result must be a
	 * FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
private:
	FXMUFoundationMoveResponseDataContainer FoundationMoveResponseDataContainer;
	FXMUFoundationNetworkMoveDataContainer FoundationMoveDataContainer;

/*--------------------------------------------------------------------------------------------------------------------*/
	
};
