// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
#include "Components/StaticMeshComponent.h"

/**
 * @brief デフォルトコンストラクタ。
 *        CursorMesh を RootComponent として生成し、コリジョンを無効化。
 *        Tick は不要のため無効。
 */
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

/**
 * @brief カーソル初期化。マップ参照を保持し、指定位置へスナップする。
 */
void ACursorPawn::InitCursor(AMapManager* InMapManager, int32 InPlayerID, int32 StartGridX, int32 StartGridY)
{
	MapManagerRef = InMapManager;
	PlayerID = InPlayerID;
	GridX = StartGridX;
	GridY = StartGridY;

	SnapToGrid(GridX, GridY);
}

/**
 * @brief カーソル移動。境界チェック → 座標更新 → スナップ。
 */
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

/**
 * @brief グリッド座標からワールド座標を計算し、カーソル位置を格子中心に合わせる。
 *        Z方向にオフセットを加えてタイル上に浮かせる。
 */
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

/**
 * @brief 指定座標がマップ範囲内 (0 <= X < MapWidth, 0 <= Y < MapHeight) かを返す。
 */
bool ACursorPawn::IsWithinBounds(int32 X, int32 Y) const
{
	if (!MapManagerRef)
	{
		return false;
	}

	return X >= 0 && X < MapManagerRef->MapWidth && Y >= 0 && Y < MapManagerRef->MapHeight;
}
