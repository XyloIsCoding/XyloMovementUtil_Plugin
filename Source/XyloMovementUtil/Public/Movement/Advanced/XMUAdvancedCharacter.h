// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Movement/Base/XMUBaseCharacter.h"
#include "XMUAdvancedCharacter.generated.h"

class UXMUAdvancedMovement;
/**
 * 
 */
UCLASS()
class XYLOMOVEMENTUTIL_API AXMUAdvancedCharacter : public AXMUBaseCharacter
{
	GENERATED_BODY()

public:
	AXMUAdvancedCharacter(const FObjectInitializer& ObjectInitializer);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * ACharacter Interface
	 */
	
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * AXMUAdvancedCharacter
	 */

public:
	UXMUAdvancedMovement* GetAdvancedMovement() const { return AdvancedMovement; }
private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UXMUAdvancedMovement> AdvancedMovement;
};
