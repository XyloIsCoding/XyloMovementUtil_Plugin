// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Advanced/XMUAdvancedCharacter.h"

#include "Movement/Advanced/XMUAdvancedMovement.h"

AXMUAdvancedCharacter::AXMUAdvancedCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UXMUAdvancedMovement>(ACharacter::CharacterMovementComponentName))
{
	AdvancedMovement = Cast<UXMUAdvancedMovement>(GetCharacterMovement());
}
