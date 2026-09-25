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

	// 2026.09.25 Lee start
	// 2P（赤）用の方向矢印レイヤー（DirUp～DirRight の4種のみ）
	for (uint8 i = 1; i <= 4; ++i)
	{
		ETileType Type = static_cast<ETileType>(i);
		FString CompName = FString::Printf(TEXT("HISM_DirP2_%d"), i);

		UHierarchicalInstancedStaticMeshComponent* NewHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*CompName);
		NewHISM->SetupAttachment(RootComponent);

		P2DirectionVisuals.Add(Type, NewHISM);
	}
	// 2026.09.25 Lee end
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

// 2026.09.25 Lee start
// void AMapManager::SetTileData(int32 GridX, int32 GridY, ETileType NewType)
void AMapManager::SetTileData(int32 GridX, int32 GridY, ETileType NewType, uint8 InOwnerPlayerId)
// 2026.09.25 Lee end
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
		// 2026.09.25 Lee start
		// 同じプレイヤーが同じ種類を置き直す場合のみ変更なしと判定する。
		// （相手が同方向で上書きした場合は所有権が変わるため処理を続行する）
		// if (OldType == NewType)
		// {
		// 	return;
		// }
		const uint8 NewOwner = IsDirectionTile(NewType) ? InOwnerPlayerId : 0;
		if (OldType == NewType && GridData[Index].OwnerPlayerId == NewOwner)
		{
			return;
		}

		// GridData[Index].TileType = NewType;
		GridData[Index].TileType = NewType;
		GridData[Index].OwnerPlayerId = NewOwner;
		// 2026.09.25 Lee end

		// 変更のあったレイヤーのみ再構築（全レイヤー再構築によるちらつき防止）
		// 2026.09.25 Lee start
		// 所有権のみ変化した場合（OldType == NewType）は同一レイヤーのため1回だけ再構築する
		// if (OldType != ETileType::Empty)
		if (OldType != ETileType::Empty && OldType != NewType)
		// 2026.09.25 Lee end
		{
			RefreshStateVisual(OldType);
		}
		if (NewType != ETileType::Empty)
		{
			RefreshStateVisual(NewType);
		}
	}
}

// 2026.09.25 Lee start
bool AMapManager::ClearArrowIfOwned(int32 GridX, int32 GridY, uint8 OwnerPlayerId)
{
	// 範囲外は何もしない（SetTileData と同じガード）
	if (GridX < 0 || GridX >= MapWidth || GridY < 0 || GridY >= MapHeight)
	{
		return false;
	}

	const int32 Index = GridY * MapWidth + GridX;
	if (!GridData.IsValidIndex(Index))
	{
		return false;
	}

	// 方向タイルかつ所有者が一致する場合のみ消去する
	if (IsDirectionTile(GridData[Index].TileType) &&
		GridData[Index].OwnerPlayerId == OwnerPlayerId)
	{
		SetTileData(GridX, GridY, ETileType::Empty, 0);
		return true;
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("ClearArrowIfOwned: (%d, %d) は所有者不一致のため消去スキップ"), GridX, GridY);
	return false;
}
// 2026.09.25 Lee end

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

	// 2026.09.25 Lee start
	// 方向タイルは 2P 用レイヤーも同時に再構築する
	const bool bIsDirection = IsDirectionTile(StateType);
	UHierarchicalInstancedStaticMeshComponent* P2HISM = nullptr;
	if (bIsDirection)
	{
		if (UHierarchicalInstancedStaticMeshComponent** FoundP2 = P2DirectionVisuals.Find(StateType))
		{
			P2HISM = *FoundP2;
			P2HISM->ClearInstances();
			P2HISM->SetRelativeLocation(FVector::ZeroVector);
		}
	}
	// 2026.09.25 Lee end

	// GridData を走査して該当 StateType のタイルのみ収集
	TArray<FTransform> Transforms;
	// 2026.09.25 Lee start
	TArray<FTransform> TransformsP2;	// 2P（赤）用インスタンス
	// 2026.09.25 Lee end
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
		// 2026.09.25 Lee start
		// 方向タイルは所有者で出力先レイヤーを振り分ける
		TArray<FTransform>& TargetTransforms =
			(bIsDirection && Tile.OwnerPlayerId != 0) ? TransformsP2 : Transforms;
		TargetTransforms.Add(InstanceTransform);
		// Transforms.Add(InstanceTransform);
		// 2026.09.25 Lee end
	}

	if (Transforms.Num() > 0)
	{
		(*TargetHISM)->AddInstances(Transforms, false, false);
	}
	// 2026.09.25 Lee start
	if (P2HISM && TransformsP2.Num() > 0)
	{
		P2HISM->AddInstances(TransformsP2, false, false);
	}
	// 2026.09.25 Lee end
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
			// 2026.09.25 Lee start
			// エディタで配置するタイルはすべて 1P（青）扱いとする
			GridData[Index].OwnerPlayerId = 0;
			// 2026.09.25 Lee end
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
