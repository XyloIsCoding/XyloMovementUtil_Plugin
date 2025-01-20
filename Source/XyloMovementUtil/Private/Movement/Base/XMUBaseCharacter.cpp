// Fill out your copyright notice in the Description page of Project Settings.


#include "Movement/Base/XMUBaseCharacter.h"

#include "Movement/Base/XMUBaseMovement.h"

AXMUBaseCharacter::AXMUBaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UXMUBaseMovement>(ACharacter::CharacterMovementComponentName))
{
	BaseMovement = Cast<UXMUBaseMovement>(GetCharacterMovement());
}
