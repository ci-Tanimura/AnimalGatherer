// Fill out your copyright notice in the Description page of Project Settings.

#include "Takeuchi/Pawn/AnimalBase.h"

#include "Components/SceneComponent.h"

AAnimalBase::AAnimalBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AAnimalBase::BeginPlay()
{
	Super::BeginPlay();

	SetMoveDirection(InitialMoveDirection);
}

void AAnimalBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateMoveDirectionFromCurrentTile();
	MoveAnimal(DeltaTime);
}

void AAnimalBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AAnimalBase::SetMoveDirection(FVector NewMoveDirection)
{
	CurrentMoveDirection = NewMoveDirection.GetSafeNormal();
}

void AAnimalBase::UpdateMoveDirectionFromCurrentTile()
{
	//足元のマスを読み取り、SetMoveDirectionを呼ぶ
}

void AAnimalBase::MoveAnimal(float DeltaTime)
{
	if (CurrentMoveDirection.IsNearlyZero() || MoveSpeed <= 0.0f)
	{
		return;
	}

	const FVector MoveDelta = CurrentMoveDirection * MoveSpeed * DeltaTime;
	AddActorWorldOffset(MoveDelta, true);
}
