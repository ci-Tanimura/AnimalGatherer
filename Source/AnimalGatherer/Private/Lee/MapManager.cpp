// Fill out your copyright notice in the Description page of Project Settings.


#include "Lee/MapManager.h"

/**
 * @brief デフォルトコンストラクタ。
 *        Tick を無効化し、ベースフロア用 HISM と7状態分の HISM レイヤーを生成する。
 *        各レイヤーは RootComponent にアタッチされ、StateVisuals に格納される。
 */
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

/**
 * @brief ゲーム開始時またはスポーン時に呼ばれる。
 *        現状は基底クラスへの委譲のみ。必要に応じて拡張可能。
 */
void AMapManager::BeginPlay()
{
	Super::BeginPlay();
}

/**
 * @brief IGridInteractInterface の実装。指定グリッド座標のタイル状態を返す。
 * インデックス = Y * MapWidth + X で GridData を線形アクセスする。
 * BlueprintNativeEvent のため、関数名の後ろに _Implementationが必要。
 * @param GridCoords グリッド座標（X: 列, Y: 行）。
 * @return 該当タイルの ETileType。範囲外の場合は Empty を返す。
 */
ETileType AMapManager::GetCellState_Implementation(FIntPoint GridCoords) const
{
	const int32 Index = GridCoords.Y * MapWidth + GridCoords.X;

	if (GridData.IsValidIndex(Index))
	{
		return GridData[Index].TileType;
	}

	UE_LOG(LogTemp, Warning, TEXT("GetCellState: 座標 (%d, %d) が範囲外です (Map: %d x %d)"),
		GridCoords.X, GridCoords.Y, MapWidth, MapHeight);

	return ETileType::Empty;
}

/**
 * @brief 指定グリッド座標のタイルデータを実行時に変更する。
 *        範囲チェック後、GridData を更新しビジュアルも即時反映する。
 * @param GridX グリッドX座標（列）。
 * @param GridY グリッドY座標（行）。
 * @param NewType 設定する新しいタイル種類。
 */
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
		GridData[Index].TileType = NewType;
		UpdateMapVisuals();
	}
}

/**
 * @brief 全タイルのビジュアルを最新状態に更新する。
 *        全 StateVisuals をクリア後、GridData を走査し、Empty 以外のタイルに対応する
 *        HISM レイヤーへインスタンスを追加する。
 *        矢印タイルは方向に応じて Yaw 回転を適用する。
 *        Zオフセットはタイル種別ごとに微小差をつけ、z-fighting を防止する。
 */
void AMapManager::UpdateMapVisuals()
{
	// 全 StateVisuals レイヤーをクリア
	for (auto& Pair : StateVisuals)
	{
		Pair.Value->ClearInstances();
		Pair.Value->SetRelativeLocation(FVector::ZeroVector);
	}

	// GridData を走査してメッシュを配置
	for (int32 i = 0; i < GridData.Num(); ++i)
	{
		const FMapTileData& Tile = GridData[i];

		if (Tile.TileType != ETileType::Empty)
		{
			if (UHierarchicalInstancedStaticMeshComponent** TargetHISM = StateVisuals.Find(Tile.TileType))
			{
				FTransform InstanceTransform;

				// グリッド座標からローカル位置を計算
				// WorldLocation は使わず、自身の座標からの相対位置で設定する
				const int32 X = i % MapWidth;
				const int32 Y = i / MapWidth;
				const float ZOffset = static_cast<uint8>(Tile.TileType) * 0.1f;
				InstanceTransform.SetLocation(FVector(X * TileSize, Y * TileSize, ZOffset));

				// 矢印方向に応じた回転を設定（初期メッシュは上向きを想定）
				FRotator TileRot = FRotator::ZeroRotator;

				if (Tile.TileType == ETileType::DirDown)
				{
					TileRot.Yaw = 180.0f;
				}
				else if (Tile.TileType == ETileType::DirRight)
				{
					TileRot.Yaw = 90.0f;
				}
				else if (Tile.TileType == ETileType::DirLeft)
				{
					TileRot.Yaw = -90.0f;
				}

				InstanceTransform.SetRotation(TileRot.Quaternion());
				(*TargetHISM)->AddInstance(InstanceTransform);
			}
		}
	}
}

/**
 * @brief エディタ上での配置・プロパティ変更時に自動実行される構築処理。
 *        GridData のサイズが MapWidth×MapHeight と不一致なら全再生成、
 *        一致すればビジュアルのみを更新する。
 */
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

/**
 * @brief エディタの Level Design Tools で指定されたタイルの種類を変更する。
 *        座標が範囲内なら GridData を更新し即座にビジュアルへ反映する。
 *        範囲外の場合は警告ログを出力する。
 *        Modify() を呼び、エディタの Undo/Redo に対応する。
 */
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
/**
 * @brief エディタのプロパティ変更を検知し、適切な後処理を実行する。
 *        MapWidth/MapHeight/TileSize 変更時は全マップを再生成する。
 *        Edit_X/Edit_Y/Edit_TileType 変更時は ApplyTileEdit() を自動呼び出しする。
 * @param PropertyChangedEvent 変更されたプロパティ情報。
 */
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

/**
 * @brief 全マップをクリアし、現在の MapWidth×MapHeight で空グリッドを再生成する。
 *        GridData を初期化し、ベースフロアの HISM インスタンスも再配置する。
 */
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
