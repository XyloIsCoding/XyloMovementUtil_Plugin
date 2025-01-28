// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Foundation/XMUFoundationCharacter.h"

#include "InputActionValue.h"
#include "Movement/Foundation/XMUFoundationMovement.h"
#include "Net/UnrealNetwork.h"

AXMUFoundationCharacter::AXMUFoundationCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UXMUFoundationMovement>(ACharacter::CharacterMovementComponentName))
{
	FoundationMovement = Cast<UXMUFoundationMovement>(GetCharacterMovement());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * ACharacter Interface
 */

void AXMUFoundationCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, ReplicatedAcceleration, COND_SimulatedOnly);
}

void AXMUFoundationCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		// Compress Acceleration: XY components as direction + magnitude, Z component as direct value
		const double MaxAccel = MovementComponent->MaxAcceleration;
		const FVector CurrentAccel = MovementComponent->GetCurrentAcceleration();
		double AccelXYRadians, AccelXYMagnitude;
		FMath::CartesianToPolar(CurrentAccel.X, CurrentAccel.Y, AccelXYMagnitude, AccelXYRadians);

		ReplicatedAcceleration.AccelXYRadians   = FMath::FloorToInt((AccelXYRadians / TWO_PI) * 255.0);     // [0, 2PI] -> [0, 255]
		ReplicatedAcceleration.AccelXYMagnitude = FMath::FloorToInt((AccelXYMagnitude / MaxAccel) * 255.0);	// [0, MaxAccel] -> [0, 255]
		ReplicatedAcceleration.AccelZ           = FMath::FloorToInt((CurrentAccel.Z / MaxAccel) * 127.0);   // [-MaxAccel, MaxAccel] -> [-127, 127]
	}
}

/*--------------------------------------------------------------------------------------------------------------------*/
/* Jump */

void AXMUFoundationCharacter::CheckJumpInput(float DeltaTime)
{
	if (FoundationMovement && bPressedJump)
	{
		if (FoundationMovement->CheckOverrideJumpInput(DeltaTime))
		{
			bPressedJump = false;
		}
		else
		{
			UnCrouch(false);
			if (bIsCrouched) FoundationMovement->BeginUnCrouch(false);
		}
	}


	// copied from Super except marked spots

	JumpCurrentCountPreJump = JumpCurrentCount;

	if (GetFoundationMovement())
	{
		if (bPressedJump)
		{
			// If this is the first jump and we're already falling,
			// then increment the JumpCount to compensate.
			const bool bFirstJump = JumpCurrentCount == 0;
			if (bFirstJump && GetCharacterMovement()->IsFalling() && GetFoundationMovement()->IsCoyoteTimeDurationDrained()) // XMU Change: added coyote time duration check
			{
				JumpCurrentCount++;
			}

			const bool bDidJump = CanJump() && GetCharacterMovement()->DoJump(bClientUpdating);
			if (bDidJump)
			{
				// Transition from not (actively) jumping to jumping.
				if (!bWasJumping)
				{
					JumpCurrentCount++;
					JumpForceTimeRemaining = GetJumpMaxHoldTime();
					OnJumped();
				}
			}

			bWasJumping = bDidJump;
		}
	}
}

bool AXMUFoundationCharacter::CanJumpInternal_Implementation() const
{
	return (!bIsCrouched || GetFoundationMovement()->IsLeavingCrouch()) && JumpIsAllowed(); // XMU Change: changed JumpIsAllowedInternal in JumpIsAllowed
}

bool AXMUFoundationCharacter::JumpIsAllowed() const
{
	// copied from JumpIsAllowedInternal except marked spots
	
	// Ensure that the CharacterMovement state is valid
	bool bJumpIsAllowed = GetCharacterMovement()->CanAttemptJump();

	if (bJumpIsAllowed)
	{
		// Ensure JumpHoldTime and JumpCount are valid.
		if (!bWasJumping || GetJumpMaxHoldTime() <= 0.0f)
		{
			// If our first jump is in air we check if the incremented JumpCurrentCount is less then the max value
			// cause it counts as double jump
			// since we added coyote time, we also check that we are outside the coyote time duration
			if (JumpCurrentCount == 0 && GetCharacterMovement()->IsFalling() && GetFoundationMovement()->IsCoyoteTimeDurationDrained()) // XMU Change: added coyote time duration check
			{
				bJumpIsAllowed = JumpCurrentCount + 1 < JumpMaxCount;
			}
			else
			{
				bJumpIsAllowed = JumpCurrentCount < JumpMaxCount;
			}
		}
		else
		{
			// Only consider JumpKeyHoldTime as long as:
			// A) The jump limit hasn't been met OR
			// B) The jump limit has been met AND we were already jumping
			const bool bJumpKeyHeld = (bPressedJump && JumpKeyHoldTime < GetJumpMaxHoldTime());
			bJumpIsAllowed = bJumpKeyHeld &&
				((JumpCurrentCount < JumpMaxCount) || (bWasJumping && JumpCurrentCount == JumpMaxCount));
		}
	}

	return bJumpIsAllowed;
}

/*--------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------*/
/* Crouch */

void AXMUFoundationCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AXMUFoundationCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AXMUFoundationCharacter::OnRep_IsCrouched()
{
	// copied from super but using deferred time crouch functions
	
	if (GetFoundationMovement())
	{
		if (bIsCrouched)
		{
			GetFoundationMovement()->bWantsToCrouch = true;
			GetFoundationMovement()->BeginCrouch(true);
		}
		else
		{
			GetFoundationMovement()->bWantsToCrouch = false;
			GetFoundationMovement()->BeginUnCrouch(true);
		}
		GetFoundationMovement()->bNetworkUpdateReceived = true;
	}
}

/*--------------------------------------------------------------------------------------------------------------------*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * AXMUFoundationCharacter
 */

FCollisionQueryParams AXMUFoundationCharacter::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams Params;

	TArray<AActor*> CharacterChildren;
	GetAllChildActors(CharacterChildren);
	Params.AddIgnoredActors(CharacterChildren);
	Params.AddIgnoredActor(this);

	return Params;
}

void AXMUFoundationCharacter::OnRep_ReplicatedAcceleration()
{
	if (UXMUFoundationMovement* XMUMovementComponent = Cast<UXMUFoundationMovement>(GetCharacterMovement()))
	{
		// Decompress Acceleration
		const double MaxAccel         = XMUMovementComponent->MaxAcceleration;
		const double AccelXYMagnitude = double(ReplicatedAcceleration.AccelXYMagnitude) * MaxAccel / 255.0; // [0, 255] -> [0, MaxAccel]
		const double AccelXYRadians   = double(ReplicatedAcceleration.AccelXYRadians) * TWO_PI / 255.0;     // [0, 255] -> [0, 2PI]

		FVector UnpackedAcceleration(FVector::ZeroVector);
		FMath::PolarToCartesian(AccelXYMagnitude, AccelXYRadians, UnpackedAcceleration.X, UnpackedAcceleration.Y);
		UnpackedAcceleration.Z = double(ReplicatedAcceleration.AccelZ) * MaxAccel / 127.0; // [-127, 127] -> [-MaxAccel, MaxAccel]

		XMUMovementComponent->SetReplicatedAcceleration(UnpackedAcceleration);
	}
}

void AXMUFoundationCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AXMUFoundationCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}


