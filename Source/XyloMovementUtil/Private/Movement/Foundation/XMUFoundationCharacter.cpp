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
	}
	Super::CheckJumpInput(DeltaTime);
}

bool AXMUFoundationCharacter::CanJumpInternal_Implementation() const
{
	return JumpIsAllowedInternal(); // removed !bIsCrouched
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


