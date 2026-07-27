// Fill out your copyright notice in the Description page of Project Settings.


#include "Lee/MapManager.h"

AMapManager::AMapManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// ベースフロア（常時表示される床メッシュ）
	HISM_BaseFloor = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_BaseFloor"));
	RootComponent = HISM_BaseFloor;

	// Empty(0) を除く7状態分の HISM レイヤーを作成
	for (uint8 i = 1; i <= 7; ++i)
	{
		ETileType Type = static_cast<ETileType>(i);
		FString CompName = FString::Printf(TEXT("HISM_State_%d"), i);

		UHierarchicalInstancedStaticMeshComponent* NewHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*CompName);
		NewHISM->SetupAttachment(RootComponent);

		StateVisuals.Add(Type, NewHISM);
	}
}

void AMapManager::BeginPlay()
{
	Super::BeginPlay();
}

ETileType AMapManager::GetCellState_Implementation(FIntPoint GridCoords) const
{
	// インデックス = Y * MapWidth + X で GridData を線形アクセス
	const int32 Index = GridCoords.Y * MapWidth + GridCoords.X;

	if (GridData.IsValidIndex(Index))
	{
		return GridData[Index].TileType;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetCellState: 座標 (%d, %d) が範囲外です (Map: %d x %d)"),
		GridCoords.X, GridCoords.Y, MapWidth, MapHeight);

	return ETileType::Empty;
}

void AMapManager::SetTileData(int32 GridX, int32 GridY, ETileType NewType)
{
	if (GridX < 0 || GridX >= MapWidth || GridY < 0 || GridY >= MapHeight)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetTileData: 座標 (%d, %d) が範囲外です (Map: %d x %d)"),
			GridX, GridY, MapWidth, MapHeight);
		return;
	}

	const int32 Index = GridY * MapWidth + GridX;
	if (GridData.IsValidIndex(Index))
	{
		const ETileType OldType = GridData[Index].TileType;
		if (OldType == NewType)
		{
			return;
		}

		GridData[Index].TileType = NewType;

		// 変更のあったレイヤーのみ再構築（全レイヤー再構築によるちらつき防止）
		if (OldType != ETileType::Empty)
		{
			RefreshStateVisual(OldType);
		}
		if (NewType != ETileType::Empty)
		{
			RefreshStateVisual(NewType);
		}
	}
}

void AMapManager::UpdateMapVisuals()
{
	for (auto& Pair : StateVisuals)
	{
		RefreshStateVisual(Pair.Key);
	}
}

void AMapManager::RefreshStateVisual(ETileType StateType)
{
	UHierarchicalInstancedStaticMeshComponent** TargetHISM = StateVisuals.Find(StateType);
	if (!TargetHISM)
	{
		return;
	}

	(*TargetHISM)->ClearInstances();
	(*TargetHISM)->SetRelativeLocation(FVector::ZeroVector);

	// GridData を走査して該当 StateType のタイルのみ収集
	TArray<FTransform> Transforms;
	for (int32 i = 0; i < GridData.Num(); ++i)
	{
		const FMapTileData& Tile = GridData[i];

		if (Tile.TileType != StateType)
		{
			continue;
		}

		const int32 X = i % MapWidth;
		const int32 Y = i / MapWidth;

		// Z オフセットは StateType の数値に比例（z-fighting 防止）
		const float ZOffset = static_cast<uint8>(StateType) * 0.1f;

		FTransform InstanceTransform;
		InstanceTransform.SetLocation(FVector(X * TileSize, Y * TileSize, ZOffset));

		// 矢印方向に応じた回転を設定（初期メッシュは上向き想定）
		FRotator TileRot = FRotator::ZeroRotator;

		if (StateType == ETileType::DirUp)
		{
			TileRot.Yaw = 180.0f;
		}
		else if (StateType == ETileType::DirDown)
		{
			TileRot.Yaw = 0.0f;
		}
		else if (StateType == ETileType::DirLeft)
		{
			TileRot.Yaw = 90.0f;
		}
		else if (StateType == ETileType::DirRight)
		{
			TileRot.Yaw = -90.0f;
		}

		InstanceTransform.SetRotation(TileRot.Quaternion());
		Transforms.Add(InstanceTransform);
	}

	if (Transforms.Num() > 0)
	{
		(*TargetHISM)->AddInstances(Transforms, false, false);
	}
}

void AMapManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (GridData.Num() != MapWidth * MapHeight)
	{
		ResetAndGenerateBlankMap();
	}
	else
	{
		UpdateMapVisuals();
	}
}

void AMapManager::ApplyTileEdit()
{
	if (Edit_X >= 0 && Edit_X < MapWidth && Edit_Y >= 0 && Edit_Y < MapHeight)
	{
		Modify();
		int32 Index = Edit_Y * MapWidth + Edit_X;

		if (GridData.IsValidIndex(Index))
		{
			GridData[Index].TileType = Edit_TileType;
			UpdateMapVisuals();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyTileEdit: 座標 (%d, %d) が範囲外です (Map: %d x %d)"),
			Edit_X, Edit_Y, MapWidth, MapHeight);
	}
}

#if WITH_EDITOR
void AMapManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth) ||
		PropName == GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight) ||
		PropName == GET_MEMBER_NAME_CHECKED(AMapManager, TileSize))
	{
		ResetAndGenerateBlankMap();
		return;
	}

	if (PropName == GET_MEMBER_NAME_CHECKED(AMapManager, Edit_X) ||
		PropName == GET_MEMBER_NAME_CHECKED(AMapManager, Edit_Y) ||
		PropName == GET_MEMBER_NAME_CHECKED(AMapManager, Edit_TileType))
	{
		ApplyTileEdit();
	}
}
#endif

void AMapManager::ResetAndGenerateBlankMap()
{
	GridData.Empty();
	GridData.Reserve(MapWidth * MapHeight);

	if (HISM_BaseFloor)
	{
		HISM_BaseFloor->ClearInstances();
	}

	FVector StartLocation = GetActorLocation();

	for (int32 Y = 0; Y < MapHeight; Y++)
	{
		for (int32 X = 0; X < MapWidth; X++)
		{
			FMapTileData NewTile;
			NewTile.WorldLocation = StartLocation + FVector(X * TileSize, Y * TileSize, 0.0f);
			NewTile.TileType = ETileType::Empty;
			GridData.Add(NewTile);

			if (HISM_BaseFloor)
			{
				FTransform InstanceTransform;
				InstanceTransform.SetLocation(FVector(X * TileSize, Y * TileSize, 0.0f));
				HISM_BaseFloor->AddInstance(InstanceTransform);
			}
		}
	}

	UpdateMapVisuals();
}
