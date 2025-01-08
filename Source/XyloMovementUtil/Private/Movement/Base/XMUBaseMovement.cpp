// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Base/XMUBaseMovement.h"

#include "GameFramework/Character.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Move Data
 */

void FXMUBaseNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);
 
	Stamina = static_cast<const FXMUSavedMove_Character_Base&>(ClientMove).Stamina;
}

bool FXMUBaseNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);
    
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);
	return !Ar.IsError();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Move Response Data
 */

void FXMUBaseMoveResponseDataContainer::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	const UXMUBaseMovement* MoveComp = Cast<UXMUBaseMovement>(&CharacterMovement);

	bStaminaDrained = MoveComp->IsStaminaDrained();
	Stamina = MoveComp->GetStamina();
}

bool FXMUBaseMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	if (IsCorrection())
	{
		Ar << Stamina;
		Ar << bStaminaDrained;
	}

	return !Ar.IsError();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Saved Move
 */

bool FXMUSavedMove_Character_Base::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	const TSharedPtr<FXMUSavedMove_Character_Base>& SavedMove = StaticCastSharedPtr<FXMUSavedMove_Character_Base>(NewMove);

	if (bStaminaDrained != SavedMove->bStaminaDrained)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FXMUSavedMove_Character_Base::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FXMUSavedMove_Character_Base* SavedOldMove = static_cast<const FXMUSavedMove_Character_Base*>(OldMove);

	if (UXMUBaseMovement* MoveComp = C ? Cast<UXMUBaseMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStamina(SavedOldMove->Stamina);
		MoveComp->SetStaminaDrained(SavedOldMove->bStaminaDrained);
	}
}

void FXMUSavedMove_Character_Base::Clear()
{
	Super::Clear();

	bStaminaDrained = false;
	Stamina = 0.f;
}

void FXMUSavedMove_Character_Base::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UXMUBaseMovement* MoveComp = C ? Cast<UXMUBaseMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bStaminaDrained = MoveComp->IsStaminaDrained();
		Stamina = MoveComp->GetStamina();
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * UXMUBaseMovement
 */


UXMUBaseMovement::UXMUBaseMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(BaseMoveResponseDataContainer);
	SetNetworkMoveDataContainer(BaseMoveDataContainer);
}


/*--------------------------------------------------------------------------------------------------------------------*/
/* Networking stuff */

void UXMUBaseMovement::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
                                                  FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
                                                  bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	// ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	const FXMUBaseMoveResponseDataContainer& StaminaMoveResponse = static_cast<const FXMUBaseMoveResponseDataContainer&>(GetMoveResponseDataContainer());
	
	SetStamina(StaminaMoveResponse.Stamina);
	SetStaminaDrained(StaminaMoveResponse.bStaminaDrained);

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
	bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UXMUBaseMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
    if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
    {
        return true;
    }
    
	// This will trigger a client correction if the Stamina value in the Client differs NetworkStaminaCorrectionThreshold (2.f default) units from the one in the server
	// Desyncs can happen if we set the Stamina directly in Gameplay code (ie: GAS)
    const FXMUBaseNetworkMoveData* CurrentMoveData = static_cast<const FXMUBaseNetworkMoveData*>(GetCurrentNetworkMoveData());
    if (!FMath::IsNearlyEqual(CurrentMoveData->Stamina, Stamina, NetworkStaminaCorrectionThreshold))
    {
        return true;
    }
    
    return false;
}

FNetworkPredictionData_Client* UXMUBaseMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UXMUBaseMovement* MutableThis = const_cast<UXMUBaseMovement*>(this);
		MutableThis->ClientPredictionData = new FXMUNetworkPredictionData_Client_Character_Base(*this);
	}

	return ClientPredictionData;
}

/*--------------------------------------------------------------------------------------------------------------------*/
