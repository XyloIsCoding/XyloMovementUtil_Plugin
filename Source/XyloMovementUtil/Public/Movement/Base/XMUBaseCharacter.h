// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "XMUBaseCharacter.generated.h"


/**
 * FXMUReplicatedAcceleration: Compressed representation of acceleration
 */
USTRUCT()
struct FXMUReplicatedAcceleration
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 AccelXYRadians = 0;	// Direction of XY accel component, quantized to represent [0, 2*pi]

	UPROPERTY()
	uint8 AccelXYMagnitude = 0;	//Accel rate of XY component, quantized to represent [0, MaxAcceleration]

	UPROPERTY()
	int8 AccelZ = 0;	// Raw Z accel rate component, quantized to represent [-MaxAcceleration, MaxAcceleration]
};


UCLASS()
class XYLOMOVEMENTUTIL_API AXMUBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AXMUBaseCharacter(const FObjectInitializer& ObjectInitializer);

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	 * AXMUBaseCharacter
	 */

protected:
	UFUNCTION()
	void OnRep_ReplicatedAcceleration();
private:
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FXMUReplicatedAcceleration ReplicatedAcceleration;


};
