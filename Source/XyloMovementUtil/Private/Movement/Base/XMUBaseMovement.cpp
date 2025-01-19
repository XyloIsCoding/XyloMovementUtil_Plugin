// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Base/XMUBaseMovement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

// Helper Macros
#if 0
float MacroDuration = 2.f;
#define SLOG(x) GEngine->AddOnScreenDebugMessage(-1, MacroDuration ? MacroDuration : -1.f, FColor::Yellow, x);
#define POINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, !MacroDuration, MacroDuration);
#define LINE(x1, x2, c) DrawDebugLine(GetWorld(), x1, x2, c, !MacroDuration, MacroDuration);
#define ARROW(x1, x2, c) DrawDebugDirectionalArrow(GetWorld(), x1, x2, 20.f, c, !MacroDuration, MacroDuration);
#define BIGARROW(x1, x2, c) DrawDebugDirectionalArrow(GetWorld(), x1, x2, 60.f, c, !MacroDuration, MacroDuration, 0, 4.f);
#define SPHERE(x, r, c) DrawDebugSphere(GetWorld(), x, r, 16, c, !MacroDuration, MacroDuration);
#define PLAYERCAPSULE(x, c) DrawDebugCapsule(GetWorld(), x, CapHH(), CapR(), FQuat::Identity, c, !MacroDuration, MacroDuration);
#define CAPSULE(x, r, hh, c) DrawDebugCapsule(GetWorld(), x, hh, r, FQuat::Identity, c, !MacroDuration, MacroDuration);
#else
#define SLOG(x) 
#define POINT(x, c) 
#define LINE(x1, x2, c) 
#define ARROW(x1, x2, c) 
#define BIGARROW(x1, x2, c) 
#define SPHERE(x, r, c) 
#define PLAYERCAPSULE(x, c) 
#define CAPSULE(x, hh, r, c) 
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Move Response Data
 */

void FXMUBaseMoveResponseDataContainer::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	const UXMUBaseMovement* MoveComp = Cast<UXMUBaseMovement>(&CharacterMovement);

	Stamina = MoveComp->GetStamina();
	bStaminaDrained = MoveComp->IsStaminaDrained();
	Charge = MoveComp->GetCharge();
	bChargeDrained = MoveComp->IsChargeDrained();
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
		Ar << Charge;
		Ar << bChargeDrained;
	}

	return !Ar.IsError();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Network Move Data
 */

void FXMUBaseNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	const FXMUSavedMove_Character_Base BaseClientMove = static_cast<const FXMUSavedMove_Character_Base&>(ClientMove);
	
	BaseCompressedMoveFlags = BaseClientMove.GetBaseCompressedFlags();
	Stamina = BaseClientMove.Stamina;
	Charge = BaseClientMove.Charge;
}

bool FXMUBaseNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);
    
	SerializeOptionalValue<uint8>(Ar.IsSaving(), Ar, CompressedMoveFlags, 0);
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Charge, 0.f);
	return !Ar.IsError();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Saved Move
 */

void FXMUSavedMove_Character_Base::Clear()
{
	Super::Clear();
	
	Stamina = 0.f;
	bStaminaDrained = false;
	Charge = 0.f;
	bChargeDrained = false;
}

void FXMUSavedMove_Character_Base::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UXMUBaseMovement* MoveComp = C ? Cast<UXMUBaseMovement>(C->GetCharacterMovement()) : nullptr)
	{
		Stamina = MoveComp->GetStamina();
		bStaminaDrained = MoveComp->IsStaminaDrained();
		Charge = MoveComp->GetCharge();
		bChargeDrained = MoveComp->IsChargeDrained();
	}
}

bool FXMUSavedMove_Character_Base::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	const TSharedPtr<FXMUSavedMove_Character_Base>& SavedMove = StaticCastSharedPtr<FXMUSavedMove_Character_Base>(NewMove);

	if (bStaminaDrained != SavedMove->bStaminaDrained)
	{
		return false;
	}

	if (bChargeDrained != SavedMove->bChargeDrained)
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
		MoveComp->SetCharge(SavedOldMove->Charge);
		MoveComp->SetChargeDrained(SavedOldMove->bChargeDrained);
	}
}

void FXMUSavedMove_Character_Base::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
}

void FXMUSavedMove_Character_Base::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);
}

uint8 FXMUSavedMove_Character_Base::GetBaseCompressedFlags() const
{
	uint8 Result = 0;

	return Result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * UXMUBaseMovement
 */

namespace XMUBaseCharacter
{
	static float GroundTraceDistance = 100000.0f;
	FAutoConsoleVariableRef CVar_GroundTraceDistance(TEXT("LyraCharacter.GroundTraceDistance"), GroundTraceDistance, TEXT("Distance to trace down when generating ground information."), ECVF_Cheat);
}


UXMUBaseMovement::UXMUBaseMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(BaseMoveResponseDataContainer);
	SetNetworkMoveDataContainer(BaseMoveDataContainer);

	NetworkStaminaCorrectionThreshold = 2.f;
	NetworkChargeCorrectionThreshold = 2.f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * UCharacterMovementComponent Interface
 */

void UXMUBaseMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	
	/*----------------------------------------------------------------------------------------------------------------*/
	/* Post Root Motion Source Transition */
	
	if (RootMotionSourceTransition.bFinishedLastFrame)
	{
		SLOG(FString::Printf(TEXT("Root Motion Transition Finished %s"), *RootMotionSourceTransition.Name))
		UE_LOG(LogTemp, Warning, TEXT("Root Motion Transition Finished %s"), *RootMotionSourceTransition.Name)

		const FString RMSTransitionName = RootMotionSourceTransition.Name;
		RootMotionSourceTransition.Reset();
		PostRootMotionSourceTransition(RMSTransitionName);
	}
	
	/*----------------------------------------------------------------------------------------------------------------*/
}

void UXMUBaseMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	/*----------------------------------------------------------------------------------------------------------------*/
	/* Track Root Motion Source End */
	
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (GetRootMotionSourceByID(RootMotionSourceTransition.ID) && GetRootMotionSourceByID(RootMotionSourceTransition.ID)->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
		{
			RemoveRootMotionSourceByID(RootMotionSourceTransition.ID);
			RootMotionSourceTransition.bFinishedLastFrame = true;
		}
	}
	
	/*----------------------------------------------------------------------------------------------------------------*/
}

void UXMUBaseMovement::SimulateMovement(float DeltaTime)
{
	/*----------------------------------------------------------------------------------------------------------------*/
	/* Save acceleration that we might have set through replication */
	const FVector OriginalAcceleration = Acceleration;
	/*----------------------------------------------------------------------------------------------------------------*/

	Super::SimulateMovement(DeltaTime);


	
	/*----------------------------------------------------------------------------------------------------------------*/
	/* Restore replicated acceleration if needed */
	if (bHasReplicatedAcceleration)
	{
		Acceleration = OriginalAcceleration;
	}
	/*----------------------------------------------------------------------------------------------------------------*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * UXMUBaseMovement
 */

/*--------------------------------------------------------------------------------------------------------------------*/
/* Helpers */

bool UXMUBaseMovement::IsCustomMovementMode(EXMUCustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

bool UXMUBaseMovement::IsServer() const
{
	return CharacterOwner->HasAuthority();
}

float UXMUBaseMovement::CapR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UXMUBaseMovement::CapHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

FVector UXMUBaseMovement::GetControllerForwardVector() const
{
	return FRotator(0.f,CharacterOwner->GetControlRotation().Yaw,0.f).Vector();
}

FVector UXMUBaseMovement::GetControllerRightVector() const
{
	return FVector::CrossProduct(FVector::DownVector, GetControllerForwardVector());
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
	
/* Extension */

void UXMUBaseMovement::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

const FXMUCharacterGroundInfo& UXMUBaseMovement::GetGroundInfo()
{
	if (!CharacterOwner || (GFrameCounter == CachedGroundInfo.LastUpdateFrame))
	{
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking)
	{
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else
	{
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = (UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn);
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, (TraceStart.Z - XMUBaseCharacter::GroundTraceDistance - CapsuleHalfHeight));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LyraCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = XMUBaseCharacter::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			CachedGroundInfo.GroundDistance = FMath::Max((HitResult.Distance - CapsuleHalfHeight), 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;

	return CachedGroundInfo;
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Root Motion Source Transitions */

void UXMUBaseMovement::PostRootMotionSourceTransition(FString TransitionName)
{
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Stamina */

void UXMUBaseMovement::SetStamina(float NewStamina)
{
	const float PrevStamina = Stamina;
	Stamina = FMath::Clamp(NewStamina, 0.f, MaxStamina);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevStamina, Stamina))
		{
			OnStaminaChanged(PrevStamina, Stamina);
		}
	}
}

void UXMUBaseMovement::SetMaxStamina(float NewMaxStamina)
{
	const float PrevMaxStamina = MaxStamina;
	MaxStamina = FMath::Max(0.f, NewMaxStamina);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevMaxStamina, MaxStamina))
		{
			OnMaxStaminaChanged(PrevMaxStamina, MaxStamina);
		}
	}
}

void UXMUBaseMovement::SetStaminaDrained(bool bNewValue)
{
	const bool bWasStaminaDrained = bStaminaDrained;
	bStaminaDrained = bNewValue;
	if (CharacterOwner != nullptr)
	{
		if (bWasStaminaDrained != bStaminaDrained)
		{
			if (bStaminaDrained)
			{
				OnStaminaDrained();
			}
			else
			{
				OnStaminaDrainRecovered();
			}
		}
	}
}

void UXMUBaseMovement::DebugStamina() const
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		if (CharacterOwner->HasAuthority())
		{
			GEngine->AddOnScreenDebugMessage(38265, 1.f, FColor::Orange, FString::Printf(TEXT("[Authority] Stamina %f    Drained %d"), GetStamina(), IsStaminaDrained()));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(38266, 1.f, FColor::Orange, FString::Printf(TEXT("[Autonomous] Stamina %f    Drained %d"), GetStamina(), IsStaminaDrained()));
		}
	}
#endif
}

void UXMUBaseMovement::OnStaminaChanged(float PrevValue, float NewValue)
{
	if (FMath::IsNearlyZero(Stamina))
	{
		Stamina = 0.f;
		if (!bStaminaDrained)
		{
			SetStaminaDrained(true);
		}
	}
	// This will need to change if not using MaxStamina for recovery, here is an example (commented out) that uses
	// 10% instead; to use this, comment out the existing else if statement, and change the 0.1f to the percentage
	// you want to use (0.1f is 10%)
	//
	// else if (bStaminaDrained && Stamina >= MaxStamina * 0.1f)
	// {
	// 	SetStaminaDrained(false);
	// }
	else if (FMath::IsNearlyEqual(Stamina, MaxStamina))
	{
		Stamina = MaxStamina;
		if (bStaminaDrained)
		{
			SetStaminaDrained(false);
		}
	}
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Charge */

void UXMUBaseMovement::SetCharge(float NewCharge)
{
	const float PrevCharge = Charge;
	Charge = FMath::Clamp(NewCharge, 0.f, MaxCharge);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevCharge, Charge))
		{
			OnChargeChanged(PrevCharge, Charge);
		}
	}
}

void UXMUBaseMovement::SetMaxCharge(float NewMaxCharge)
{
	const float PrevMaxCharge = MaxCharge;
	MaxCharge = FMath::Max(0.f, NewMaxCharge);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevMaxCharge, MaxCharge))
		{
			OnMaxChargeChanged(PrevMaxCharge, MaxCharge);
		}
	}
}

void UXMUBaseMovement::SetChargeDrained(bool bNewValue)
{
	const bool bWasChargeDrained = bChargeDrained;
	bChargeDrained = bNewValue;
	if (CharacterOwner != nullptr)
	{
		if (bWasChargeDrained != bChargeDrained)
		{
			if (bChargeDrained)
			{
				OnChargeDrained();
			}
			else
			{
				OnChargeDrainRecovered();
			}
		}
	}
}

void UXMUBaseMovement::DebugCharge() const
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		if (CharacterOwner->HasAuthority())
		{
			GEngine->AddOnScreenDebugMessage(38265, 1.f, FColor::Orange, FString::Printf(TEXT("[Authority] Charge %f    Drained %d"), GetCharge(), IsChargeDrained()));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(38266, 1.f, FColor::Orange, FString::Printf(TEXT("[Autonomous] Charge %f    Drained %d"), GetCharge(), IsChargeDrained()));
		}
	}
#endif
}

void UXMUBaseMovement::OnChargeChanged(float PrevValue, float NewValue)
{
	if (FMath::IsNearlyZero(Charge))
	{
		Charge = 0.f;
		if (!bChargeDrained)
		{
			SetChargeDrained(true);
		}
	}
	// This will need to change if not using MaxCharge for recovery, here is an example (commented out) that uses
	// 10% instead; to use this, comment out the existing else if statement, and change the 0.1f to the percentage
	// you want to use (0.1f is 10%)
	//
	// else if (bChargeDrained && Charge >= MaxCharge * 0.1f)
	// {
	// 	SetChargeDrained(false);
	// }
	else if (FMath::IsNearlyEqual(Charge, MaxCharge))
	{
		Charge = MaxCharge;
		if (bChargeDrained)
		{
			SetChargeDrained(false);
		}
	}
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Networking stuff */

void UXMUBaseMovement::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
	bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	// ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	const FXMUBaseMoveResponseDataContainer& BaseMoveResponse = static_cast<const FXMUBaseMoveResponseDataContainer&>(GetMoveResponseDataContainer());
	
	SetStamina(BaseMoveResponse.Stamina);
	SetStaminaDrained(BaseMoveResponse.bStaminaDrained);
	SetCharge(BaseMoveResponse.Charge);
	SetChargeDrained(BaseMoveResponse.bChargeDrained);

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
	bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UXMUBaseMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName, uint8 ClientMovementMode)
{
    if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation,
    	ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
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
	if (!FMath::IsNearlyEqual(CurrentMoveData->Charge, Charge, NetworkChargeCorrectionThreshold))
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

void UXMUBaseMovement::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	UpdateFromBaseCompressedFlags();
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UXMUBaseMovement::UpdateFromBaseCompressedFlags()
{
	const FXMUBaseNetworkMoveData* CurrentMoveData = static_cast<const FXMUBaseNetworkMoveData*>(GetCurrentNetworkMoveData());
	uint8 Flags = CurrentMoveData->BaseCompressedMoveFlags;
	//...
}

/*--------------------------------------------------------------------------------------------------------------------*/
