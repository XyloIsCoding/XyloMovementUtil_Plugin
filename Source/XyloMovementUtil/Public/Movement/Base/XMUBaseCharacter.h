// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Movement/Foundation/XMUFoundationCharacter.h"
#include "XMUBaseCharacter.generated.h"

class UXMUBaseMovement;
/**
 * 
 */
UCLASS()
class XYLOMOVEMENTUTIL_API AXMUBaseCharacter : public AXMUFoundationCharacter
{
	GENERATED_BODY()

public:
	AXMUBaseCharacter(const FObjectInitializer& ObjectInitializer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * ACharacter Interface
	 */
	
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * AXMUFoundationCharacter
	 */

public:
	UXMUBaseMovement* GetBaseMovement() const { return BaseMovement; }
private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UXMUBaseMovement> BaseMovement;
};
