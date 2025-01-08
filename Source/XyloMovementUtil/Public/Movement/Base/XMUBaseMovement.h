// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "XMUBaseMovement.generated.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Move Data: What we send to server
 */

struct EXOMOVEMENTS_API FXMUBaseNetworkMoveData : public FCharacterNetworkMoveData
{
public:
	typedef FCharacterNetworkMoveData Super;
 
	FXMUBaseNetworkMoveData()
		: Stamina(0)
	{

	}
 
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	/*
	 * Custom Data
	 */

	//uint8 BaseCompressedFlags;
	float Stamina;
	//float Charge;
	
};

struct EXOMOVEMENTS_API FXMUBaseNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
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
 * Move Response Data: What we receive from server
 */

struct EXOMOVEMENTS_API FXMUBaseMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;
	
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;

	float Stamina;
	bool bStaminaDrained;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Saved Move: passed to FCharacterNetworkMoveData to be sent to server when necessary
 */

class EXOMOVEMENTS_API FXMUSavedMove_Character_Base : public FSavedMove_Character
{
	using Super = FSavedMove_Character;
	
public:
	FXMUSavedMove_Character_Base()
		: bStaminaDrained(0)
		, Stamina(0)
	{
	}

	virtual ~FXMUSavedMove_Character_Base() override
	{}

	uint32 bStaminaDrained : 1;
	float Stamina;

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void Clear() override;
	virtual void SetInitialPosition(ACharacter* C) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Prediction Data
 */

class EXOMOVEMENTS_API FXMUNetworkPredictionData_Client_Character_Base : public FNetworkPredictionData_Client_Character
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
