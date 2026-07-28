// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
#include "Components/StaticMeshComponent.h"

ACursorPawn::ACursorPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	CursorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CursorMesh"));
	RootComponent = CursorMesh;

	// 純粋なビジュアルカーソル：物理不要
	CursorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACursorPawn::BeginPlay()
{
	Super::BeginPlay();
}

void ACursorPawn::InitCursor(AMapManager* InMapManager, int32 InPlayerID, int32 StartGridX, int32 StartGridY)
{
	MapManagerRef = InMapManager;
	PlayerID = InPlayerID;
	GridX = StartGridX;
	GridY = StartGridY;

	SnapToGrid(GridX, GridY);
}

void ACursorPawn::MoveCursor(int32 DeltaX, int32 DeltaY)
{
	if (!MapManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveCursor: MapManagerRef が未設定です"));
		return;
	}

	const int32 NewX = GridX + DeltaX;
	const int32 NewY = GridY + DeltaY;

	if (!IsWithinBounds(NewX, NewY))
	{
		return;
	}

	GridX = NewX;
	GridY = NewY;

	SnapToGrid(GridX, GridY);
}

void ACursorPawn::SnapToGrid(int32 X, int32 Y)
{
	if (!MapManagerRef)
	{
		return;
	}

	const FVector MapOrigin = MapManagerRef->GetActorLocation();
	const float TileSz = MapManagerRef->TileSize;

	// タイル中心 + Zオフセット（タイルより上に表示）
	// HISM インスタンスの pivot はメッシュ中心なので、X*TileSz がそのままタイル中心
	const FVector NewLocation = MapOrigin + FVector(
		X * TileSz,
		Y * TileSz,
		50.0f
	);

	SetActorLocation(NewLocation);
}

bool ACursorPawn::IsWithinBounds(int32 X, int32 Y) const
{
	if (!MapManagerRef)
	{
		return false;
	}

	return X >= 0 && X < MapManagerRef->MapWidth && Y >= 0 && Y < MapManagerRef->MapHeight;
}
