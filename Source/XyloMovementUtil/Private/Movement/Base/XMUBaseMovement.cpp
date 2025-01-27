// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Base/XMUBaseMovement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*--------------------------------------------------------------------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * UXMUBaseMovement
 */

UXMUBaseMovement::UXMUBaseMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//
}

void UXMUBaseMovement::Crouch(bool bClientSimulation)
{

	float NewHalfHeight;
	float OutHalfHeightAdjust;
	float OutScaledHalfHeightAdjust;
	bool OutResult;

	
	if (!HasValidData())
	{
		return;
	}
	
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	
	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == NewHalfHeight)
	{
		OutHalfHeightAdjust = 0.f;
		OutScaledHalfHeightAdjust = 0.f;
		OutResult = true;
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
	const float ClampedNewHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, NewHalfHeight);
	float HalfHeightAdjust = (ClampedNewHalfHeight - OldUnscaledHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;


	
	

	// TODO: decrease size







	
	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	OutHalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedNewHalfHeight);
	OutScaledHalfHeightAdjust = OutHalfHeightAdjust * ComponentScale;
	
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

void UXMUBaseMovement::UnCrouch(bool bClientSimulation)
{
	float NewHalfHeight;
	
	float OutHalfHeightAdjust;
	float OutScaledHalfHeightAdjust;
	bool OutResult;

	
	if (!HasValidData())
	{
		return;
	}
	
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	
	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == NewHalfHeight)
	{
		OutHalfHeightAdjust = 0.f;
		OutScaledHalfHeightAdjust = 0.f;
		OutResult = true;
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
	const float ClampedNewHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, NewHalfHeight);
	float HalfHeightAdjust = (ClampedNewHalfHeight - OldUnscaledHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;


	
	

	// TODO increase size



	
	
	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	OutHalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedNewHalfHeight);
	OutScaledHalfHeightAdjust = OutHalfHeightAdjust * ComponentScale;
	
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

