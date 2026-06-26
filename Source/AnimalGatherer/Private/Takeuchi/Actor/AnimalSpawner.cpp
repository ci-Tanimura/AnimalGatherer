// Fill out your copyright notice in the Description page of Project Settings.


#include "Takeuchi/Actor/AnimalSpawner.h"

// Sets default values
AAnimalSpawner::AAnimalSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AAnimalSpawner::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AAnimalSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

