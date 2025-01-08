// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "XMUBaseMovement.generated.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Move Response Data: What we receive from server
 *	call UCharacterMovementComponent::SetMoveResponseDataContainer in cmc constructor to use this class
 */

struct XYLOMOVEMENTUTIL_API FXMUBaseMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;
	
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

struct XYLOMOVEMENTUTIL_API FXMUBaseNetworkMoveData : public FCharacterNetworkMoveData
{
public:
	typedef FCharacterNetworkMoveData Super;
 
	FXMUBaseNetworkMoveData()
		: BaseCompressedFlags(0)
		, Stamina(0)
		, Charge(0)
	{
	}
 
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	/*
	 * Custom Data
	 */

	uint8 BaseCompressedFlags; // generated using FXMUSavedMove_Character_Base::GetBaseCompressedFlags
	float Stamina;
	float Charge;
	
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Move Data Container: Wraps Network Move Data to send to server
 *	call UCharacterMovementComponent::SetNetworkMoveDataContainer in cmc constructor to use this class
 */

struct XYLOMOVEMENTUTIL_API FXMUBaseNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
public:
	typedef FCharacterNetworkMoveDataContainer Super;
 
	FXMUBaseNetworkMoveDataContainer()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}
 
private:
	FXMUBaseNetworkMoveData MoveData[3];
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Saved Move: passed to FCharacterNetworkMoveData to be sent to server when necessary
 */

class XYLOMOVEMENTUTIL_API FXMUSavedMove_Character_Base : public FSavedMove_Character
{
	using Super = FSavedMove_Character;
	
public:
	FXMUSavedMove_Character_Base()
		: Stamina(0)
		, bStaminaDrained(0)
		, Charge(0)
		, bChargeDrained(0)
	{
	}

	virtual ~FXMUSavedMove_Character_Base() override
	{}
	
	float Stamina;
	uint32 bStaminaDrained : 1;
	float Charge;
	uint32 bChargeDrained : 1;

	virtual void Clear() override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void SetInitialPosition(ACharacter* C) override;
	/** Generate a compressed flags based on saved moved variables */
	virtual uint8 GetBaseCompressedFlags() const;

	// Bit masks used by GetCompressedFlags() to encode movement information.
	enum BaseCompressedFlags
	{
		FLAG_Reserved_1		= 0x01,
		FLAG_Reserved_2		= 0x02,
		FLAG_Reserved_3		= 0x04,
		FLAG_Reserved_4		= 0x08,
		FLAG_Custom_0		= 0x10,
		FLAG_Custom_1		= 0x20,
		FLAG_Custom_2		= 0x40,
		FLAG_Custom_3		= 0x80,
	};
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Prediction Data: manages saved moves
 *	it is returned by UXMUBaseMovement::GetPredictionData_Client, this is the way we set the SavedMove class
 *	for this component
 */

class XYLOMOVEMENTUTIL_API FXMUNetworkPredictionData_Client_Character_Base : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FXMUNetworkPredictionData_Client_Character_Base(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return MakeShared<FXMUSavedMove_Character_Base>();
	}
};







////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * UXMUBaseMovement
 */


/**
 * 
 */
UCLASS()
class XYLOMOVEMENTUTIL_API UXMUBaseMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UXMUBaseMovement(const FObjectInitializer& ObjectInitializer);

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
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
		FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
		bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;
	
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on
	 * demand and can be overridden to allocate a custom override if desired. Result must be a
	 * FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	
private:
	FXMUBaseMoveResponseDataContainer BaseMoveResponseDataContainer;
	FXMUBaseNetworkMoveDataContainer BaseMoveDataContainer;

/*--------------------------------------------------------------------------------------------------------------------*/
	
};
