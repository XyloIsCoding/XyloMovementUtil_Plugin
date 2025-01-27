// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Movement/Foundation/XMUFoundationMovement.h"
#include "XMUBaseMovement.generated.h"


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
class XYLOMOVEMENTUTIL_API UXMUBaseMovement : public UXMUFoundationMovement
{
	GENERATED_BODY()
	
public:
	UXMUBaseMovement(const FObjectInitializer& ObjectInitializer);
	
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * UCharacterMovementComponent Interface
	 */

public:
	virtual void Crouch(bool bClientSimulation) override;
	virtual void UnCrouch(bool bClientSimulation) override;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * UXMUFoundationMovement
	 */
	
};
