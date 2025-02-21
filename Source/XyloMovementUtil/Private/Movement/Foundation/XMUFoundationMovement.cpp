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


DEFINE_LOG_CATEGORY_STATIC(LogCharacterMovement, Log, All);

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

	CoyoteTimeDuration = MoveComp->GetCoyoteTimeDuration();
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
		
		Ar << CoyoteTimeDuration;
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
	
	Stamina = FoundationClientMove.SavedStamina;
	Charge = FoundationClientMove.SavedCharge;

	CoyoteTimeDuration = FoundationClientMove.SavedCoyoteTimeDuration;
}

bool FXMUFoundationNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);
    
	SerializeOptionalValue<uint8>(Ar.IsSaving(), Ar, FoundationCompressedMoveFlags, 0);
	
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Charge, 0.f);

	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, CoyoteTimeDuration, 0.f);
	
	return !Ar.IsError();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Saved Move
 */

void FXMUSavedMove_Character_Foundation::Clear()
{
	Super::Clear();
	
	StartStamina = 0.f;
	SavedStamina = 0.f;
	bStaminaDrained = false;
	StartCharge = 0.f;
	SavedCharge = 0.f;
	bChargeDrained = false;

	StartCoyoteTimeDuration = 0.f;
	SavedCoyoteTimeDuration = 0.f;

	CrouchProgress = 0.f;
	bCrouchTransitioning = false;

	AnimRootMotionTransitionName = "";
	bAnimRootMotionTransitionFinishedLastFrame = false;
	RootMotionSourceTransitionName = "";
	bRootMotionSourceTransitionFinishedLastFrame = false;
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

	// TODO: find out if this is even needed
	if ((StartCoyoteTimeDuration == 0.f) != (NewFoundationMove->StartCoyoteTimeDuration == 0.f))
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

	if (bCrouchTransitioning != NewFoundationMove->bCrouchTransitioning)
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
		MoveComp->SetStamina(OldFoundationMove->StartStamina);
		MoveComp->SetStaminaDrained(OldFoundationMove->bStaminaDrained);
		MoveComp->SetCharge(OldFoundationMove->StartCharge);
		MoveComp->SetChargeDrained(OldFoundationMove->bChargeDrained);

		MoveComp->SetCoyoteTimeDuration(OldFoundationMove->StartCoyoteTimeDuration);

		MoveComp->SetCrouchProgress(OldFoundationMove->CrouchProgress);
	}
}

void FXMUSavedMove_Character_Foundation::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	AXMUFoundationCharacter* FoundationCharacter = Cast<AXMUFoundationCharacter>(C);
	UXMUFoundationMovement* FoundationMovement = Cast<UXMUFoundationMovement>(C->GetCharacterMovement());

	bCrouchTransitioning = FoundationMovement->IsCrouchTransitioning();
	
	AnimRootMotionTransitionName = FoundationMovement->AnimRootMotionTransition.Name;
	bAnimRootMotionTransitionFinishedLastFrame = FoundationMovement->AnimRootMotionTransition.bFinishedLastFrame;
	RootMotionSourceTransitionName = FoundationMovement->RootMotionSourceTransition.Name;
	bRootMotionSourceTransitionFinishedLastFrame = FoundationMovement->RootMotionSourceTransition.bFinishedLastFrame;
}

void FXMUSavedMove_Character_Foundation::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UXMUFoundationMovement* MoveComp = C ? Cast<UXMUFoundationMovement>(C->GetCharacterMovement()) : nullptr)
	{
		StartStamina = MoveComp->GetStamina();
		bStaminaDrained = MoveComp->IsStaminaDrained();
		StartCharge = MoveComp->GetCharge();
		bChargeDrained = MoveComp->IsChargeDrained();

		StartCoyoteTimeDuration = MoveComp->GetCoyoteTimeDuration();

		CrouchProgress = MoveComp->GetCrouchProgress();
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

	FoundationMovement->SetCrouchProgress(CrouchProgress);
	FoundationMovement->SetCrouchTransitioning(bCrouchTransitioning);
}

void FXMUSavedMove_Character_Foundation::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	FSavedMove_Character::PostUpdate(C, PostUpdateMode);

	if (const UXMUFoundationMovement* MoveComp = C ? Cast<UXMUFoundationMovement>(C->GetCharacterMovement()) : nullptr)
	{
		SavedStamina = MoveComp->GetStamina();
		SavedCharge = MoveComp->GetCharge();

		SavedCoyoteTimeDuration = MoveComp->GetCoyoteTimeDuration();

		if (PostUpdateMode == PostUpdate_Record)
		{
			if (bStaminaDrained != MoveComp->IsStaminaDrained())
			{
				bForceNoCombine = true;
			}

			if (bChargeDrained != MoveComp->IsChargeDrained())
			{
				bForceNoCombine = true;
			}

			if (bCrouchTransitioning != MoveComp->IsCrouchTransitioning())
			{
				bForceNoCombine = true;
			}
		}
	}
}

uint8 FXMUSavedMove_Character_Foundation::GetFoundationCompressedFlags() const
{
	uint8 Result = 0;
	
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

	GravityScale = 1.5f;
	JumpZVelocity = 500.f;
	
	MaxWalkSpeed = 400.f;
	BrakingFrictionFactor = 1.f;
	
	SetCrouchedHalfHeight(70.f);
	NavAgentProps.bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
	MaxWalkSpeedCrouched = 200.f;
	
	
	/* Custom Stuff */
	
	NetworkStaminaCorrectionThreshold = 2.f;
	NetworkChargeCorrectionThreshold = 2.f;
	SetMaxStamina(100.f);
	SetMaxCharge(100.f);

	NetworkCoyoteTimeDurationCorrectionThreshold = 0.1f;
	SetMaxCoyoteTimeDuration(0.4f);
	SetCoyoteTimeFullDurationVelocity(1200.f);

	WalkingCrouchTransitionTime = 0.2f;
	FallingCrouchTransitionTime = 0.2f;
	
	MaxAirSpeed = 200.f;
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
	UpdateStaminaBeforeMovement(DeltaSeconds);
	UpdateChargeBeforeMovement(DeltaSeconds);
	UpdateCoyoteTimeBeforeMovement(DeltaSeconds);
	UpdateCrouchBeforeMovement(DeltaSeconds);
	
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
	UpdateCrouchAfterMovement(DeltaSeconds);

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
		// if it has AnimRootMotionTransition montage and we have a RootMotionMontageInstance, check if they are different
		const bool bHasAnimRootMotionMontage = AnimRootMotionTransition.Montage.Get();
		FAnimMontageInstance* RootMotionMontageInstance = GetCharacterOwner()->GetRootMotionAnimMontageInstance();
		const bool bMontageChanged = (bHasAnimRootMotionMontage && RootMotionMontageInstance) ? AnimRootMotionTransition.Montage.Get() != RootMotionMontageInstance->Montage : false;
		// if we had AnimRootMotion, but we don't anymore, or montage changed then set that we finished transition
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
	return IsJumpAllowed() &&
		   (!bWantsToCrouch || FoundationCharacterOwner->bCanJumpWhileCrouching) && // XMU Change: added check for bCanJumpWhileCrouching
		   (IsMovingOnGround() || IsFalling()); // Falling included for double-jump and non-zero jump hold time, but validated by character.
}

bool UXMUFoundationMovement::DoJump(bool bReplayingMoves)
{
	return Super::DoJump(bReplayingMoves);
}

void UXMUFoundationMovement::Crouch(bool bClientSimulation)
{
	if (!bClientSimulation && !CanCrouchInCurrentState())
	{
		return;
	}
	
	EXMUCapsuleScalingMode ScalingMode = bCrouchMaintainsBaseLocation ? EXMUCapsuleScalingMode::CSM_Bottom : EXMUCapsuleScalingMode::CSM_Center;
	FXMUResizeCapsuleHHResult Result;
	ResizeCapsuleHH(GetCrouchedHalfHeight(), ScalingMode, bClientSimulation, Result);
	
	if (!bClientSimulation && Result.Success)
	{
		CharacterOwner->bIsCrouched = true;
	}
	
	if (Result.Success || bClientSimulation)
	{
		CharacterOwner->OnStartCrouch( Result.HalfHeightAdjust, Result.ScaledHalfHeightAdjust);
	}
}

void UXMUFoundationMovement::UnCrouch(bool bClientSimulation)
{
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	EXMUCapsuleScalingMode ScalingMode = bCrouchMaintainsBaseLocation ? EXMUCapsuleScalingMode::CSM_Bottom : EXMUCapsuleScalingMode::CSM_Center;
	FXMUResizeCapsuleHHResult Result;
	ResizeCapsuleHH(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), ScalingMode, bClientSimulation, Result);
	
	if (!bClientSimulation && Result.Success)
	{
		CharacterOwner->bIsCrouched = false;
	}
	
	if (Result.Success || bClientSimulation)
	{
		CharacterOwner->OnEndCrouch( Result.HalfHeightAdjust, Result.ScaledHalfHeightAdjust);
	}
}

void UXMUFoundationMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// copied from Super (except marked spots)
	// changed to add Titanfall-like air strafing
	
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();
	
	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else 
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);
	
	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);
	
		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}


	/** XMU Change */

	/*
	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}
	*/

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		// Find veer
		const FVector AccelDir = Acceleration.GetSafeNormal2D();  //AcellDir = WishDir
		const float CurrentSpeed = Velocity.X * AccelDir.X + Velocity.Y * AccelDir.Y; // DotProduct(Velocity, AccelDir) in 2D
		// Get add speed with air speed cap
		const float RealMaxSpeed = (IsFalling() ? MaxAirSpeed : MaxSpeed);
		// Apply acceleration
		const float AddSpeed = FMath::Clamp(RealMaxSpeed - CurrentSpeed, 0.0, MaxAccel * DeltaTime);
		Velocity += AddSpeed * AccelDir;
	}

	/** ~XMU Change */

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}

void UXMUFoundationMovement::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	UpdateFromFoundationCompressedFlags();
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UXMUFoundationMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	
	if (MovementMode == MOVE_Falling && PreviousMovementMode != MOVE_Falling)
	{
		SetCoyoteTimeDuration(MaxCoyoteTimeDuration * Velocity.Size2D() / GetCoyoteTimeFullDurationVelocity());
	}
}

void UXMUFoundationMovement::UpdateStaminaBeforeMovement(float DeltaSeconds)
{
	SetStamina(GetStamina() + StaminaRegenRate * DeltaSeconds);
}

void UXMUFoundationMovement::UpdateChargeBeforeMovement(float DeltaSeconds)
{
	SetCharge(GetCharge() + ChargeRegenRate * DeltaSeconds);
}

void UXMUFoundationMovement::UpdateCoyoteTimeBeforeMovement(float DeltaSeconds)
{
	SetCoyoteTimeDuration(GetCoyoteTimeDuration() - DeltaSeconds);
}

void UXMUFoundationMovement::UpdateCrouchBeforeMovement(float DeltaSeconds)
{
	// tick crouch progress (we keep track of the time for sim proxies too, for animation purposes)
	SetCrouchProgress(GetCrouchProgress() + DeltaSeconds);
	
	// Proxies get replicated crouch state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Check for a change in crouch state. Players toggle crouch by changing bWantsToCrouch.
		const bool bIsCrouching = IsCrouching();
		if (bIsCrouching && (!bWantsToCrouch || !CanCrouchInCurrentState()))
		{
			BeginUnCrouch(false);
		}
		else if (!bIsCrouching && bWantsToCrouch && CanCrouchInCurrentState())
		{
			BeginCrouch(false);
		}

		// check if it needs to finish the crouch transition
		if (IsCrouchTransitioning() && GetCrouchProgress() == GetCrouchTransitionTime())
		{
			if (IsCrouching())
			{
				FinishCrouch(false);
			}
			else
			{
				FinishUnCrouch(false);
			}
		}
	}
}

void UXMUFoundationMovement::UpdateCrouchAfterMovement(float DeltaSeconds)
{
	// Proxies get replicated crouch state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Uncrouch if no longer allowed to be crouched
		if (IsCrouching() && !CanCrouchInCurrentState())
		{
			BeginUnCrouch(false);
		}
	}
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

float UXMUFoundationMovement::GetScaledCapsuleRadius() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UXMUFoundationMovement::GetScaledCapsuleHalfHeight() const
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

void UXMUFoundationMovement::ResizeCapsuleHH(float NewCapsuleHalfHeight, EXMUCapsuleScalingMode ScalingMode, bool bClientSimulation, FXMUResizeCapsuleHHResult& Result)
{
	if (!HasValidData())
	{
		return;
	}
	
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	
	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == NewCapsuleHalfHeight)
	{
		if (!bClientSimulation)
		{
			Result.Success = true;
		}
		Result.HalfHeightAdjust = 0.f;
		Result.ScaledHalfHeightAdjust = 0.f;
		return;
	}
	
	if (bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// restore collision size before changing size
		CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bShrinkProxyCapsule = true;
	}
	
	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	// Height is not allowed to be smaller than radius.
	const float ClampedNewHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, NewCapsuleHalfHeight);
	float HalfHeightAdjust = (ClampedNewHalfHeight - OldUnscaledHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	
	
	// Resizing
	if (HalfHeightAdjust > 0.f)
	{
		IncreaseCapsuleHH(ClampedNewHalfHeight, ScaledHalfHeightAdjust, ScalingMode, bClientSimulation, Result);
	}
	else
	{
		DecreaseCapsuleHH(ClampedNewHalfHeight, ScaledHalfHeightAdjust, ScalingMode, bClientSimulation, Result);
	}
	
	
	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	Result.HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedNewHalfHeight);
	Result.ScaledHalfHeightAdjust = Result.HalfHeightAdjust * ComponentScale;
	
	AdjustProxyCapsuleSize();

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset += FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

void UXMUFoundationMovement::IncreaseCapsuleHH(float ClampedNewHalfHeight, float ScaledHalfHeightAdjust, EXMUCapsuleScalingMode ScalingMode, bool bClientSimulation, FXMUResizeCapsuleHHResult& Result)
{
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	
	if( !bClientSimulation )
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		const float CurrentHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		
		// Try to stay in place and see if the larger capsule fits. We use a slightly taller capsule to avoid penetration.
		const UWorld* MyWorld = GetWorld();
		const float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f;
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);

		// Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation - ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		bool bEncroached = true;

		if (ScalingMode == EXMUCapsuleScalingMode::CSM_Center)
		{
			// Expand in place
			bEncroached = MyWorld->OverlapBlockingTestByChannel(PawnLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
		
			if (bEncroached)
			{
				// Try adjusting capsule position to see if we can avoid encroachment.
				if (ScaledHalfHeightAdjust > 0.f)
				{
					// Shrink to a short capsule, sweep down to base to find where that would hit something, and then try to stand up from there.
					float PawnRadius, PawnHalfHeight;
					CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
					const float ShrinkHalfHeight = PawnHalfHeight - PawnRadius;
					const float TraceDist = PawnHalfHeight - ShrinkHalfHeight;
					const FVector Down = FVector(0.f, 0.f, -TraceDist);

					FHitResult Hit(1.f);
					const FCollisionShape ShortCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, ShrinkHalfHeight);
					const bool bBlockingHit = MyWorld->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + Down, FQuat::Identity, CollisionChannel, ShortCapsuleShape, CapsuleParams);
					if (Hit.bStartPenetrating)
					{
						bEncroached = true;
					}
					else
					{
						// Compute where the base of the sweep ended up, and see if we can stand there
						const float DistanceToBase = (Hit.Time * TraceDist) + ShortCapsuleShape.Capsule.HalfHeight;
						const FVector NewLoc = FVector(PawnLocation.X, PawnLocation.Y, PawnLocation.Z - DistanceToBase + StandingCapsuleShape.Capsule.HalfHeight + SweepInflation + MIN_FLOOR_DIST / 2.f);
						bEncroached = MyWorld->OverlapBlockingTestByChannel(NewLoc, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
						if (!bEncroached)
						{
							// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
							UpdatedComponent->MoveComponent(NewLoc - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						}
					}
				}
			}
		}
		else if (ScalingMode == EXMUCapsuleScalingMode::CSM_Bottom)
		{
			// Expand while keeping base location the same.
			FVector StandingLocation = PawnLocation + FVector(0.f, 0.f, StandingCapsuleShape.GetCapsuleHalfHeight() - CurrentHalfHeight);
			bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				if (IsMovingOnGround())
				{
					// Something might be just barely overhead, try moving down closer to the floor to avoid it.
					const float MinFloorDist = UE_KINDA_SMALL_NUMBER * 10.f;
					if (CurrentFloor.bBlockingHit && CurrentFloor.FloorDist > MinFloorDist)
					{
						StandingLocation.Z -= CurrentFloor.FloorDist - MinFloorDist;
						bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
					}
				}				
			}

			if (!bEncroached)
			{
				// Commit the change in location.
				UpdatedComponent->MoveComponent(StandingLocation - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
				bForceNextFloorCheck = true;
			}
		}
		else if (ScalingMode == EXMUCapsuleScalingMode::CSM_Top)
		{
			// TODO: add
		}

		// If still encroached then abort.
		if (bEncroached)
		{
			return;
		}

		Result.Success = true;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, ClampedNewHalfHeight, true);
}

void UXMUFoundationMovement::DecreaseCapsuleHH(float ClampedNewHalfHeight, float ScaledHalfHeightAdjust, EXMUCapsuleScalingMode ScalingMode, bool bClientSimulation, FXMUResizeCapsuleHHResult& Result)
{
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, ClampedNewHalfHeight);
	
	if( !bClientSimulation )
	{
		if (ScalingMode == EXMUCapsuleScalingMode::CSM_Bottom)
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(FVector(0.f, 0.f, ScaledHalfHeightAdjust), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}
		else if (ScalingMode == EXMUCapsuleScalingMode::CSM_Top)
		{
			// TODO: add
		}

		Result.Success = true;
	}

	bForceNextFloorCheck = true;
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
/* Two Steps Crouch */

void UXMUFoundationMovement::SetWalkingCrouchTransitionTime(float NewCrouchTransitionTime)
{
	WalkingCrouchTransitionTime = FMath::Min(0.f, NewCrouchTransitionTime);
}

void UXMUFoundationMovement::SetFallingCrouchTransitionTime(float NewCrouchTransitionTime)
{
	FallingCrouchTransitionTime = FMath::Min(0.f, NewCrouchTransitionTime);
}

float UXMUFoundationMovement::GetCrouchTransitionTime() const
{
	if (IsFalling())
	{
		return FallingCrouchTransitionTime;
	}
	return WalkingCrouchTransitionTime;
}

void UXMUFoundationMovement::SetCrouchProgress(float NewCrouchProgress)
{
	CrouchProgress = FMath::Clamp(NewCrouchProgress, 0.f, GetCrouchTransitionTime());

	// set crouch transitioning to false for sim proxies (auth proxy and authority do
	// it in FinishCrouch / FinishUnCrouch functions)
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (CrouchProgress == GetCrouchTransitionTime())
		{
			SetCrouchTransitioning(false);
		}
	}
}

float UXMUFoundationMovement::GetCrouchProgress() const
{
	if (IsCrouchTransitioning())
	{
		return CrouchProgress;
	}
	return GetCrouchTransitionTime();
}

void UXMUFoundationMovement::SetCrouchTransitioning(bool NewValue)
{
	bCrouchTransitioning = NewValue;
}

bool UXMUFoundationMovement::IsEnteringCrouch() const
{
	return IsCrouchTransitioning() && IsCrouching();
}

bool UXMUFoundationMovement::IsLeavingCrouch() const
{
	return IsCrouchTransitioning() && !IsCrouching();
}

void UXMUFoundationMovement::BeginCrouch(bool bClientSimulation)
{
	if (!bClientSimulation && !CanCrouchInCurrentState())
	{
		return;
	}

	SetCrouchProgress(GetCrouchTransitionTime() - GetCrouchProgress());
	SetCrouchTransitioning(true);
	
	if (!bClientSimulation)
	{
		CharacterOwner->bIsCrouched = true;
		if (GetCrouchTransitionTime() == 0.f)
		{
			FinishCrouch(bClientSimulation);
		}
	}
	else
	{
		FinishCrouch(bClientSimulation);	
	}
}

void UXMUFoundationMovement::BeginUnCrouch(bool bClientSimulation)
{
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	EXMUCapsuleScalingMode ScalingMode = bCrouchMaintainsBaseLocation ? EXMUCapsuleScalingMode::CSM_Bottom : EXMUCapsuleScalingMode::CSM_Center;
	FXMUResizeCapsuleHHResult Result;
	ResizeCapsuleHH(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), ScalingMode, bClientSimulation, Result);
	if (!bClientSimulation && !Result.Success) return;

	SetCrouchProgress(GetCrouchTransitionTime() - GetCrouchProgress());
	SetCrouchTransitioning(true);
		
	if (!bClientSimulation)
	{
		CharacterOwner->bIsCrouched = false;
		if (GetCrouchTransitionTime() == 0.f)
		{
			FinishUnCrouch(bClientSimulation);
		}
	}
	else
	{
		FinishUnCrouch(bClientSimulation);
	}

	CharacterOwner->OnEndCrouch( Result.HalfHeightAdjust, Result.ScaledHalfHeightAdjust);
}

void UXMUFoundationMovement::FinishCrouch(bool bClientSimulation)
{
	if (!bClientSimulation)
	{
		SetCrouchTransitioning(false);
	}
	
	EXMUCapsuleScalingMode ScalingMode = bCrouchMaintainsBaseLocation ? EXMUCapsuleScalingMode::CSM_Bottom : EXMUCapsuleScalingMode::CSM_Center;
	FXMUResizeCapsuleHHResult Result;
	ResizeCapsuleHH(GetCrouchedHalfHeight(), ScalingMode, bClientSimulation, Result);

	// no result checks needed because we are shrinking the capsule
	CharacterOwner->OnStartCrouch( Result.HalfHeightAdjust, Result.ScaledHalfHeightAdjust);
}

void UXMUFoundationMovement::FinishUnCrouch(bool bClientSimulation)
{
	if (!bClientSimulation)
	{
		SetCrouchTransitioning(false);
	}
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Coyote Time */

void UXMUFoundationMovement::SetCoyoteTimeDuration(float NewCoyoteTimeDuration)
{
	const float PrevCoyoteTimeDuration = CoyoteTimeDuration;
	CoyoteTimeDuration = FMath::Clamp(NewCoyoteTimeDuration, 0.f, MaxCoyoteTimeDuration);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevCoyoteTimeDuration, CoyoteTimeDuration))
		{
			OnCoyoteTimeDurationChanged(PrevCoyoteTimeDuration, CoyoteTimeDuration);
		}
	}
}

void UXMUFoundationMovement::SetMaxCoyoteTimeDuration(float NewMaxCoyoteTimeDuration)
{
	const float PrevMaxCoyoteTimeDuration = MaxCoyoteTimeDuration;
	MaxCoyoteTimeDuration = FMath::Max(0.f, NewMaxCoyoteTimeDuration);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevMaxCoyoteTimeDuration, MaxCoyoteTimeDuration))
		{
			OnMaxCoyoteTimeDurationChanged(PrevMaxCoyoteTimeDuration, MaxCoyoteTimeDuration);
		}
	}
}

void UXMUFoundationMovement::SetCoyoteTimeFullDurationVelocity(float NewCoyoteTimeVelocityScale)
{
	CoyoteTimeFullDurationVelocity = FMath::Max(0.f, NewCoyoteTimeVelocityScale);
}

void UXMUFoundationMovement::DebugCoyoteTimeDuration() const
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		if (CharacterOwner->HasAuthority())
		{
			GEngine->AddOnScreenDebugMessage(38265, 1.f, FColor::Orange, FString::Printf(TEXT("[Authority] CoyoteTimeDuration %f    Drained %d"), GetCoyoteTimeDuration(), IsCoyoteTimeDurationDrained()));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(38266, 1.f, FColor::Orange, FString::Printf(TEXT("[Autonomous] CoyoteTimeDuration %f    Drained %d"), GetCoyoteTimeDuration(), IsCoyoteTimeDurationDrained()));
		}
	}
#endif
}

void UXMUFoundationMovement::OnCoyoteTimeDurationChanged(float PrevValue, float NewValue)
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
	
	const FXMUFoundationNetworkMoveData* CurrentMoveData = static_cast<const FXMUFoundationNetworkMoveData*>(GetCurrentNetworkMoveData());

	/*
	GEngine->AddOnScreenDebugMessage(38262, 1.f, FColor::Orange, FString::Printf(TEXT("Delta Stamina From Regen Alone %f    "), DeltaTime*StaminaRegenRate));
	GEngine->AddOnScreenDebugMessage(38263, 1.f, FColor::Orange, FString::Printf(TEXT("Error (Autonomous - Authority) %f    "), CurrentMoveData->Stamina - Stamina));
	GEngine->AddOnScreenDebugMessage(38264, 1.f, FColor::Orange, FString::Printf(TEXT("[Authority] Stamina %f    "), Stamina));
	GEngine->AddOnScreenDebugMessage(38265, 1.f, FColor::Orange, FString::Printf(TEXT("[Autonomous] Stamina %f    "), CurrentMoveData->Stamina));

	if (FMath::Abs(CurrentMoveData->Stamina - Stamina) > NetworkStaminaCorrectionThreshold)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stamina error %f"), CurrentMoveData->Stamina - Stamina)
	}
	*/
	
	// This will trigger a client correction if the Stamina value in the Client differs NetworkStaminaCorrectionThreshold (2.f default) units from the one in the server
	// Desyncs can happen if we set the Stamina directly in Gameplay code (ie: GAS)
	if (!FMath::IsNearlyEqual(CurrentMoveData->Stamina, Stamina, NetworkStaminaCorrectionThreshold))
	{
		return true;
	}
	// This will trigger a client correction if the Charge value in the Client differs NetworkChargeCorrectionThreshold (2.f default) units from the one in the server
	// Desyncs can happen if we set the Charge directly in Gameplay code (ie: GAS)
	if (!FMath::IsNearlyEqual(CurrentMoveData->Charge, Charge, NetworkChargeCorrectionThreshold))
	{
		return true;
	}

	// This will trigger a client correction if the CoyoteTimeDuration value in the Client differs NetworkCoyoteTimeDurationCorrectionThreshold (2.f default) units from the one in the server
	if (!FMath::IsNearlyEqual(CurrentMoveData->CoyoteTimeDuration, CoyoteTimeDuration, NetworkCoyoteTimeDurationCorrectionThreshold))
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

	SetCoyoteTimeDuration(FoundationMoveResponse.CoyoteTimeDuration);

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewFoundation, NewFoundationBoneName,
	bHasFoundation, bFoundationRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UXMUFoundationMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealARMTFinishedLastFrame = AnimRootMotionTransition.bFinishedLastFrame;
	const bool bRealRMSTFinishedLastFrame = RootMotionSourceTransition.bFinishedLastFrame;
	
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();

	AnimRootMotionTransition.bFinishedLastFrame = bRealARMTFinishedLastFrame;
	RootMotionSourceTransition.bFinishedLastFrame = bRealRMSTFinishedLastFrame;

	return bResult;
}

void UXMUFoundationMovement::UpdateFromFoundationCompressedFlags()
{
	const FXMUFoundationNetworkMoveData* CurrentMoveData = static_cast<const FXMUFoundationNetworkMoveData*>(GetCurrentNetworkMoveData());
	uint8 Flags = CurrentMoveData->FoundationCompressedMoveFlags;
	//...
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
