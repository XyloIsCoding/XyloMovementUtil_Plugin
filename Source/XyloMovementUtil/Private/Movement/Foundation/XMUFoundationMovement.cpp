// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Foundation/XMUFoundationMovement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Movement/Foundation/XMUFoundationCharacter.h"

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

void FXMUFoundationMoveResponseDataContainer::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	const UXMUFoundationMovement* MoveComp = Cast<UXMUFoundationMovement>(&CharacterMovement);

	Stamina = MoveComp->GetStamina();
	bStaminaDrained = MoveComp->IsStaminaDrained();
	Charge = MoveComp->GetCharge();
	bChargeDrained = MoveComp->IsChargeDrained();
}

bool FXMUFoundationMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
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

void FXMUFoundationNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	const FXMUSavedMove_Character_Foundation FoundationClientMove = static_cast<const FXMUSavedMove_Character_Foundation&>(ClientMove);
	
	FoundationCompressedMoveFlags = FoundationClientMove.GetFoundationCompressedFlags();
	Stamina = FoundationClientMove.Stamina;
	Charge = FoundationClientMove.Charge;
}

bool FXMUFoundationNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
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

void FXMUSavedMove_Character_Foundation::Clear()
{
	Super::Clear();
	
	Stamina = 0.f;
	bStaminaDrained = false;
	Charge = 0.f;
	bChargeDrained = false;

	AnimRootMotionTransitionName = "";
	bAnimRootMotionTransitionFinishedLastFrame = false;
	RootMotionSourceTransitionName = "";
	bRootMotionSourceTransitionFinishedLastFrame = false;

	bPressedJumpOverride = false;
}

bool FXMUSavedMove_Character_Foundation::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	const TSharedPtr<FXMUSavedMove_Character_Foundation>& LastFoundationAckedMove = StaticCastSharedPtr<FXMUSavedMove_Character_Foundation>(LastAckedMove);
	
	// Check if any important movement flags have changed status.
	if (GetFoundationCompressedFlags() != LastFoundationAckedMove->GetFoundationCompressedFlags())
	{
		return true;
	}
	
	return FSavedMove_Character::IsImportantMove(LastAckedMove);
}

bool FXMUSavedMove_Character_Foundation::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const TSharedPtr<FXMUSavedMove_Character_Foundation>& NewFoundationMove = StaticCastSharedPtr<FXMUSavedMove_Character_Foundation>(NewMove);

	if (bStaminaDrained != NewFoundationMove->bStaminaDrained)
	{
		return false;
	}

	if (bChargeDrained != NewFoundationMove->bChargeDrained)
	{
		return false;
	}

	if (bAnimRootMotionTransitionFinishedLastFrame != NewFoundationMove->bAnimRootMotionTransitionFinishedLastFrame)
	{
		return false;
	}

	if (bRootMotionSourceTransitionFinishedLastFrame != NewFoundationMove->bRootMotionSourceTransitionFinishedLastFrame)
	{
		return false;
	}

	// Compressed flags not equal, can't combine. This covers jump and crouch as well as any custom movement flags from overrides.
	if (GetFoundationCompressedFlags() != NewFoundationMove->GetFoundationCompressedFlags())
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FXMUSavedMove_Character_Foundation::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FXMUSavedMove_Character_Foundation* OldFoundationMove = static_cast<const FXMUSavedMove_Character_Foundation*>(OldMove);

	if (UXMUFoundationMovement* MoveComp = C ? Cast<UXMUFoundationMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStamina(OldFoundationMove->Stamina);
		MoveComp->SetStaminaDrained(OldFoundationMove->bStaminaDrained);
		MoveComp->SetCharge(OldFoundationMove->Charge);
		MoveComp->SetChargeDrained(OldFoundationMove->bChargeDrained);
	}
}

void FXMUSavedMove_Character_Foundation::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	AXMUFoundationCharacter* FoundationCharacter = Cast<AXMUFoundationCharacter>(C);
	UXMUFoundationMovement* FoundationMovement = Cast<UXMUFoundationMovement>(C->GetCharacterMovement());
	
	AnimRootMotionTransitionName = FoundationMovement->AnimRootMotionTransition.Name;
	bAnimRootMotionTransitionFinishedLastFrame = FoundationMovement->AnimRootMotionTransition.bFinishedLastFrame;
	RootMotionSourceTransitionName = FoundationMovement->RootMotionSourceTransition.Name;
	bRootMotionSourceTransitionFinishedLastFrame = FoundationMovement->RootMotionSourceTransition.bFinishedLastFrame;

	bPressedJumpOverride = FoundationCharacter->bPressedJumpOverride;
}

void FXMUSavedMove_Character_Foundation::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UXMUFoundationMovement* MoveComp = C ? Cast<UXMUFoundationMovement>(C->GetCharacterMovement()) : nullptr)
	{
		Stamina = MoveComp->GetStamina();
		bStaminaDrained = MoveComp->IsStaminaDrained();
		Charge = MoveComp->GetCharge();
		bChargeDrained = MoveComp->IsChargeDrained();
	}
}

void FXMUSavedMove_Character_Foundation::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);

	AXMUFoundationCharacter* FoundationCharacter = Cast<AXMUFoundationCharacter>(C);
	UXMUFoundationMovement* FoundationMovement = Cast<UXMUFoundationMovement>(C->GetCharacterMovement());
	
	FoundationMovement->AnimRootMotionTransition.Name = AnimRootMotionTransitionName;
	FoundationMovement->AnimRootMotionTransition.bFinishedLastFrame = bAnimRootMotionTransitionFinishedLastFrame;
	FoundationMovement->RootMotionSourceTransition.Name = RootMotionSourceTransitionName;
	FoundationMovement->RootMotionSourceTransition.bFinishedLastFrame = bRootMotionSourceTransitionFinishedLastFrame;

	FoundationCharacter->bPressedJumpOverride = bPressedJumpOverride;
}

void FXMUSavedMove_Character_Foundation::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	FSavedMove_Character::PostUpdate(C, PostUpdateMode);
}

uint8 FXMUSavedMove_Character_Foundation::GetFoundationCompressedFlags() const
{
	uint8 Result = 0;

	if (bPressedJumpOverride)
	{
		Result |= FLAG_Foundation_JumpOverride;
	}
	
	return Result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * UXMUFoundationMovement
 */

namespace XMUFoundationCharacter
{
	static float GroundTraceDistance = 100000.0f;
	FAutoConsoleVariableRef CVar_GroundTraceDistance(TEXT("LyraCharacter.GroundTraceDistance"), GroundTraceDistance, TEXT("Distance to trace down when generating ground information."), ECVF_Cheat);
}


UXMUFoundationMovement::UXMUFoundationMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(FoundationMoveResponseDataContainer);
	SetNetworkMoveDataContainer(FoundationMoveDataContainer);

	NetworkStaminaCorrectionThreshold = 2.f;
	NetworkChargeCorrectionThreshold = 2.f;

	SetStamina(MaxStamina);
	SetCharge(MaxCharge);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * UCharacterMovementComponent Interface
 */

bool UXMUFoundationMovement::HasValidData() const
{
	return Super::HasValidData() && FoundationCharacterOwner;
}

void UXMUFoundationMovement::PostLoad()
{
	Super::PostLoad();

	FoundationCharacterOwner = Cast<AXMUFoundationCharacter>(CharacterOwner);
}

void UXMUFoundationMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	FoundationCharacterOwner = Cast<AXMUFoundationCharacter>(CharacterOwner);
}

void UXMUFoundationMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	SetStamina(GetStamina() + StaminaRegenRate * DeltaSeconds);
	SetCharge(GetCharge() + ChargeRegenRate * DeltaSeconds);
	
	// Proxies get replicated jump / ledge state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (FoundationCharacterOwner->bPressedJumpOverride)
		{
			if (CheckOverrideJumpInput(DeltaSeconds))
			{
				FoundationCharacterOwner->StopJumping();
			}
			else
			{
				FoundationCharacterOwner->bPressedJumpOverride = false;
				CharacterOwner->bPressedJump = true;
				CharacterOwner->CheckJumpInput(DeltaSeconds);
			}
		}
	}

	/*----------------------------------------------------------------------------------------------------------------*/
	/* Post Anim Root Motion Transition */
	
	if (AnimRootMotionTransition.bFinishedLastFrame)
	{
		SLOG(FString::Printf(TEXT("Anim Root Motion Transition Finished %s"), *RootMotionSourceTransition.Name))
		UE_LOG(LogTemp, Warning, TEXT("Anim Root Motion Transition Finished %s"), *AnimRootMotionTransition.Name)

		const FString ARMTransitionName = AnimRootMotionTransition.Name;
		AnimRootMotionTransition.Reset();
		PostAnimRootMotionTransition(ARMTransitionName);
	}
	
	/*----------------------------------------------------------------------------------------------------------------*/
	
	/*----------------------------------------------------------------------------------------------------------------*/
	/* Post Root Motion Source Transition */
	
	if (RootMotionSourceTransition.bFinishedLastFrame)
	{
		SLOG(FString::Printf(TEXT("Root Motion Source Transition Finished %s"), *RootMotionSourceTransition.Name))
		UE_LOG(LogTemp, Warning, TEXT("Root Motion Source Transition Finished %s"), *RootMotionSourceTransition.Name)

		const FString RMSTransitionName = RootMotionSourceTransition.Name;
		RootMotionSourceTransition.Reset();
		PostRootMotionSourceTransition(RMSTransitionName);
	}
	
	/*----------------------------------------------------------------------------------------------------------------*/
}

void UXMUFoundationMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
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
	
	/*----------------------------------------------------------------------------------------------------------------*/
	/* Track Anim Root Motion End */

	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		FAnimMontageInstance* RootMotionMontageInstance = GetCharacterOwner()->GetRootMotionAnimMontageInstance();
		const bool bMontageChanged = !RootMotionMontageInstance || AnimRootMotionTransition.Montage != RootMotionMontageInstance->Montage;
		if ((bHadAnimRootMotion && !HasAnimRootMotion()) || bMontageChanged)
		{
			AnimRootMotionTransition.bFinishedLastFrame = true;
		}
		bHadAnimRootMotion = HasAnimRootMotion();
	}
	
	/*----------------------------------------------------------------------------------------------------------------*/
}

void UXMUFoundationMovement::SimulateMovement(float DeltaTime)
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

bool UXMUFoundationMovement::CanAttemptJump() const
{
	return Super::CanAttemptJump();
}

bool UXMUFoundationMovement::DoJump(bool bReplayingMoves)
{
	return Super::DoJump(bReplayingMoves);
}

void UXMUFoundationMovement::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	UpdateFromFoundationCompressedFlags();
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * UXMUFoundationMovement
 */

/*--------------------------------------------------------------------------------------------------------------------*/
/* Helpers */

bool UXMUFoundationMovement::IsCustomMovementMode(EXMUCustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

bool UXMUFoundationMovement::IsServer() const
{
	return CharacterOwner->HasAuthority();
}

float UXMUFoundationMovement::CapR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UXMUFoundationMovement::CapHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

FVector UXMUFoundationMovement::GetControllerForwardVector() const
{
	return FRotator(0.f,CharacterOwner->GetControlRotation().Yaw,0.f).Vector();
}

FVector UXMUFoundationMovement::GetControllerRightVector() const
{
	return FVector::CrossProduct(FVector::DownVector, GetControllerForwardVector());
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
	
/* Extension */

void UXMUFoundationMovement::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

const FXMUCharacterGroundInfo& UXMUFoundationMovement::GetGroundInfo()
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
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, (TraceStart.Z - XMUFoundationCharacter::GroundTraceDistance - CapsuleHalfHeight));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LyraCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = XMUFoundationCharacter::GroundTraceDistance;

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

bool UXMUFoundationMovement::CheckOverrideJumpInput(float DeltaSeconds)
{
	return false;
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Anim Root Motion Transitions */

void UXMUFoundationMovement::PostAnimRootMotionTransition(FString TransitionName)
{
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Root Motion Source Transitions */

void UXMUFoundationMovement::PostRootMotionSourceTransition(FString TransitionName)
{
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Stamina */

void UXMUFoundationMovement::SetStamina(float NewStamina)
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

void UXMUFoundationMovement::SetMaxStamina(float NewMaxStamina)
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

void UXMUFoundationMovement::SetStaminaDrained(bool bNewValue)
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

void UXMUFoundationMovement::DebugStamina() const
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

void UXMUFoundationMovement::OnStaminaChanged(float PrevValue, float NewValue)
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

void UXMUFoundationMovement::SetCharge(float NewCharge)
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

void UXMUFoundationMovement::SetMaxCharge(float NewMaxCharge)
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

void UXMUFoundationMovement::SetChargeDrained(bool bNewValue)
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

void UXMUFoundationMovement::DebugCharge() const
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

void UXMUFoundationMovement::OnChargeChanged(float PrevValue, float NewValue)
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

bool UXMUFoundationMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
													const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementFoundation,
													FName ClientFoundationBoneName, uint8 ClientMovementMode)
{
	if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation,
		ClientMovementFoundation, ClientFoundationBoneName, ClientMovementMode))
	{
		return true;
	}
    
	// This will trigger a client correction if the Stamina value in the Client differs NetworkStaminaCorrectionThreshold (2.f default) units from the one in the server
	// Desyncs can happen if we set the Stamina directly in Gameplay code (ie: GAS)
	const FXMUFoundationNetworkMoveData* CurrentMoveData = static_cast<const FXMUFoundationNetworkMoveData*>(GetCurrentNetworkMoveData());

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

void UXMUFoundationMovement::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewFoundation, FName NewFoundationBoneName, bool bHasFoundation,
	bool bFoundationRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	// ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	const FXMUFoundationMoveResponseDataContainer& FoundationMoveResponse = static_cast<const FXMUFoundationMoveResponseDataContainer&>(GetMoveResponseDataContainer());
	
	SetStamina(FoundationMoveResponse.Stamina);
	SetStaminaDrained(FoundationMoveResponse.bStaminaDrained);
	SetCharge(FoundationMoveResponse.Charge);
	SetChargeDrained(FoundationMoveResponse.bChargeDrained);

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewFoundation, NewFoundationBoneName,
	bHasFoundation, bFoundationRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UXMUFoundationMovement::ClientUpdatePositionAfterServerUpdate()
{
	return Super::ClientUpdatePositionAfterServerUpdate();
}

void UXMUFoundationMovement::UpdateFromFoundationCompressedFlags()
{
	const FXMUFoundationNetworkMoveData* CurrentMoveData = static_cast<const FXMUFoundationNetworkMoveData*>(GetCurrentNetworkMoveData());
	uint8 Flags = CurrentMoveData->FoundationCompressedMoveFlags;

	FoundationCharacterOwner->bPressedJumpOverride = ((Flags & FXMUSavedMove_Character_Foundation::FLAG_Foundation_JumpOverride) != 0);
}

FNetworkPredictionData_Client* UXMUFoundationMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UXMUFoundationMovement* MutableThis = const_cast<UXMUFoundationMovement*>(this);
		MutableThis->ClientPredictionData = new FXMUNetworkPredictionData_Client_Character_Foundation(*this);
	}

	return ClientPredictionData;
}

/*--------------------------------------------------------------------------------------------------------------------*/
