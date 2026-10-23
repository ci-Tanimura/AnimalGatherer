// Fill out your copyright notice in the Description page of Project Settings.


#include "Lee/MapManager.h"

// 2026.10.22 Lee start（マップ工作台データ層実装に必要なヘッダー）
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "Engine/World.h"
#include "Engine/Level.h"

// ワークベンチ共通ガード関数の前方宣言（定義は本ファイル末尾。リンケージ一致のため匿名名前空間で宣言）
#if WITH_EDITOR
namespace
{
	bool IsWorkbenchEditableContext(const AMapManager* Map);
	bool IsInSameLevel(const AMapManager* Map, const AActor* Other);
}
#endif
// 2026.10.22 Lee end

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

// 2026.10.06 Lee start（X / Y を独立して範囲外検査し、行跨ぎ誤読を防止）
ETileType AMapManager::GetCellState_Implementation(FIntPoint GridCoords) const
{
	// 【旧実装（保持）】線形インデックスの妥当性のみ確認しており、
	// X だけ範囲外の座標が隣の行の有効セルへ誤対応する問題があった。
	// 例: MapWidth=10 のとき (-1, 5) は 5*10-1 = 49 となり (9, 4) と同一インデックスになる。
	// //
	// // インデックス = Y * MapWidth + X で GridData を線形アクセス
	// const int32 Index = GridCoords.Y * MapWidth + GridCoords.X;
	//
	// if (GridData.IsValidIndex(Index))
	// {
	// 	return GridData[Index].TileType;
	// }

	// X / Y をそれぞれ独立して検査し、範囲外は Goal 誤読などを防ぐため先に Empty を返す
	if (GridCoords.X < 0 || GridCoords.X >= MapWidth ||
		GridCoords.Y < 0 || GridCoords.Y >= MapHeight)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCellState: 座標 (%d, %d) が範囲外です (Map: %d x %d)"),
			GridCoords.X, GridCoords.Y, MapWidth, MapHeight);

		return ETileType::Empty;
	}

	// インデックス = Y * MapWidth + X で GridData を線形アクセス
	const int32 Index = GridCoords.Y * MapWidth + GridCoords.X;
	if (!GridData.IsValidIndex(Index))
	{
		return ETileType::Empty;
	}

	return GridData[Index].TileType;
}
// 2026.10.06 Lee end

// 2026.10.08 Tanimura start----------------------------------------------------
FVector AMapManager::ToWorldCenter_Implementation(FIntPoint GridCoords) const
{
	const FVector MapOrigin = GetActorLocation();

	// タイルは アクター位置 + グリッド座標 * TileSize に配置される
	return FVector(
		MapOrigin.X + GridCoords.X * TileSize,
		MapOrigin.Y + GridCoords.Y * TileSize,
		MapOrigin.Z
	);
}

bool AMapManager::IsValidCoord_Implementation(FIntPoint GridCoords) const
{
	// 0 <= X < MapWidth かつ 0 <= Y < MapHeight なら盤面内
	return GridCoords.X >= 0 && GridCoords.X < MapWidth
		&& GridCoords.Y >= 0 && GridCoords.Y < MapHeight;
}

FIntPoint AMapManager::GetGridSize_Implementation() const
{
	return FIntPoint(MapWidth, MapHeight);
}
// 2026.10.08 Tanimura end-------------------------------------------------------

// 2026.10.01 Lee start
uint8 AMapManager::GetTileOwner(int32 GridX, int32 GridY) const
{
	// 範囲外は「所有者なし(0)」と誤判定させないよう 255 を返す
	if (GridX < 0 || GridX >= MapWidth || GridY < 0 || GridY >= MapHeight)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetTileOwner: 座標 (%d, %d) が範囲外です (Map: %d x %d)"),
			GridX, GridY, MapWidth, MapHeight);
		return 255;
	}

	const int32 Index = GridY * MapWidth + GridX;
	if (!GridData.IsValidIndex(Index))
	{
		return 255;
	}

	return GridData[Index].OwnerPlayerId;
}
// 2026.10.01 Lee end

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

		// 2026.10.23 Lee start
		/** @brief 床の有無が変わる場合だけ基底層を更新し、通常の矢印編集では再生成しない。 */
		if ((OldType == ETileType::Hole) != (NewType == ETileType::Hole))
		{
			RefreshBaseFloorVisuals();
		}
		// 2026.10.23 Lee end

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
	// 2026.10.23 Lee start
	/** @brief エディタ・ランタイム共通の全体更新に床の同期を含める。 */
	RefreshBaseFloorVisuals();
	// 2026.10.23 Lee end
	for (auto& Pair : StateVisuals)
	{
		RefreshStateVisual(Pair.Key);
	}
}

// 2026.10.23 Lee start
/** @brief グリッド順に床を再生成し、空洞では表示・影・衝突の実体を生成しない。 */
void AMapManager::RefreshBaseFloorVisuals()
{
	if (!HISM_BaseFloor || MapWidth <= 0 || MapHeight <= 0 ||
		!FMath::IsFinite(TileSize) || TileSize <= 0.f)
	{
		return;
	}

	HISM_BaseFloor->ClearInstances();
	TArray<FTransform> Transforms;
	Transforms.Reserve(GridData.Num());
	for (int32 Index = 0; Index < GridData.Num(); ++Index)
	{
		if (GridData[Index].TileType != ETileType::Hole)
		{
			Transforms.Emplace(FVector((Index % MapWidth) * TileSize, (Index / MapWidth) * TileSize, 0.f));
		}
	}
	if (!Transforms.IsEmpty())
	{
		HISM_BaseFloor->AddInstances(Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false);
	}
}
// 2026.10.23 Lee end

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

// 2026.10.22 Lee start（旧実装（保持）：エディタ／ランタイム共通で数不一致なら全面クリアしていた）
	// if (GridData.Num() != MapWidth * MapHeight)
	// {
	// 	ResetAndGenerateBlankMap();
	// }
	// else
	// {
	// 	UpdateMapVisuals();
	// }
// 2026.10.22 Lee end

	// 2026.10.22 Lee start（エディタは全面クリアより先にレイアウト保護を行い、ランタイムは旧経路を維持する）
#if WITH_EDITOR
	const UWorld* EditorCheckWorld = GetWorld();
	const bool bEditorContext = EditorCheckWorld
		&& EditorCheckWorld->WorldType == EWorldType::Editor
		&& !EditorCheckWorld->IsPlayInEditor();

	if (bEditorContext)
	{
		// 2026.10.22 Lee start（ロック中レベルではメタデータ書き込み・クリア・視覚再構築も行わない）
		const ULevel* EditorLevel = GetLevel();
		if (!EditorLevel || EditorLevel->bLocked)
		{
			return;
		}
		// 2026.10.22 Lee end

		//----------------------------------------------------------
		// 0. 無効パラメータは旧有効値へ復元してから扱う（除算・負のReserve・上限超過の確保防止）
		//----------------------------------------------------------
		if (MapWidth <= 0 || MapWidth > MaxGridDimension ||
			MapHeight <= 0 || MapHeight > MaxGridDimension ||
			!FMath::IsFinite(TileSize) || TileSize <= 0.f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("OnConstruction: 無効パラメータ (Map=%d x %d, TileSize=%f) を旧有効値へ復元します"),
				MapWidth, MapHeight, TileSize);
			if (MapWidth <= 0 || MapWidth > MaxGridDimension)
			{
				MapWidth = (SavedMapWidth > 0 && SavedMapWidth <= MaxGridDimension) ? SavedMapWidth : 1;
			}
			if (MapHeight <= 0 || MapHeight > MaxGridDimension)
			{
				MapHeight = (SavedMapHeight > 0 && SavedMapHeight <= MaxGridDimension) ? SavedMapHeight : 1;
			}
			if (!FMath::IsFinite(TileSize) || TileSize <= 0.f)
			{
				TileSize = (SavedTileSize > 0.f) ? SavedTileSize : 100.f;
			}
		}

		//----------------------------------------------------------
		// 1. 新規配置（GridData空）：空グリッドを初期化し基線を保存
		//    （新規配置アクターはPostLoadを持たないため、ここで基線を確立する）
		//----------------------------------------------------------
		if (GridData.Num() == 0)
		{
			ResetAndGenerateBlankMap();
		}
		//----------------------------------------------------------
		// 2. 既存GridData数が現W*Hと一致：基線と現属性の関係で処理を分ける
		//    （同面積 5x4→4x5 では数量一致でも幅が変わりXY解釈が変わるため、
		//      基線が異なる場合は保持Resizeを実施し、基線を勝手に受容しない）
		//----------------------------------------------------------
		else if (GridData.Num() == MapWidth * MapHeight)
		{
			// 2026.10.22 Lee start（基線未確立時のみ受容、基線差異時は保持Resizeへ委ねる）
			// 【旧実装（保持）：数量一致のみで無条件に基線を受容し、同面積変更時に再配置を逃していた】
			// SavedMapWidth = MapWidth;
			// SavedMapHeight = MapHeight;
			if (SavedMapWidth <= 0 || SavedMapHeight <= 0)
			{
				SavedMapWidth = MapWidth;
				SavedMapHeight = MapHeight;
			}
			else if (SavedMapWidth != MapWidth || SavedMapHeight != MapHeight)
			{
				ResizeGridPreservingLayout(MapWidth, MapHeight, /*bPreserveOutOfBounds=*/true);
			}
			// 2026.10.22 Lee end
			if (SavedTileSize <= 0.f)
			{
				SavedTileSize = TileSize;
			}
		}
		//----------------------------------------------------------
		// 3. サイズ不一致：旧基線で保持Resize（基線が無い場合は黙ってクリアしない）
		//----------------------------------------------------------
		else
		{
			if (SavedMapWidth > 0 && SavedMapHeight > 0
				&& GridData.Num() == SavedMapWidth * SavedMapHeight)
			{
				ResizeGridPreservingLayout(MapWidth, MapHeight, /*bPreserveOutOfBounds=*/true);
			}
			else
			{
				// 復元不能：勝手な全面クリアは行わず手動修復を促す
				UE_LOG(LogTemp, Error,
					TEXT("OnConstruction: GridData(%d) を現サイズ(%d x %d)へ復元できません（旧基線=%d x %d）。"
						 "レイアウト保護のためクリアは行いません。"),
					GridData.Num(), MapWidth, MapHeight, SavedMapWidth, SavedMapHeight);
			}
		}

		//----------------------------------------------------------
		// 4. 視覚のみ再構築（他アクターへの書き込み・Modify は行わない。
		//    関連Spawner同期は位置/TileSizeの明示的編集時のみ実施する）
		//----------------------------------------------------------
		RebuildFloorAndPositions(/*bSyncSpawnBindings=*/false);
	}
	else
#endif
	{
		// ランタイム（PIE / ゲーム）は従来経路をそのまま使用する
		if (GridData.Num() != MapWidth * MapHeight)
		{
			ResetAndGenerateBlankMap();
		}
		else
		{
			UpdateMapVisuals();
		}
	}
	// 2026.10.22 Lee end
}

void AMapManager::ApplyTileEdit()
{
	if (Edit_X >= 0 && Edit_X < MapWidth && Edit_Y >= 0 && Edit_Y < MapHeight)
	{
		// 2026.10.22 Lee start（未解綁のSpawnタイルは上書き禁止：バインド解消を先に行わせる）
#if WITH_EDITOR
		for (const FSpawnTileBinding& Binding : SpawnTileBindings)
		{
			if (Binding.GridCoords == FIntPoint(Edit_X, Edit_Y) && IsValid(Binding.Spawner))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("ApplyTileEdit: (%d, %d) はバインド済みSpawnタイルのため上書きできません。先にSpawnのバインドを解除してください。"),
					Edit_X, Edit_Y);
				return;
			}
		}
#endif
		// 2026.10.22 Lee end

		Modify();
		int32 Index = Edit_Y * MapWidth + Edit_X;

		if (GridData.IsValidIndex(Index))
		{
			// 2026.09.25 Lee start
			// エディタで配置するタイルはすべて 1P（青）扱いとする
			// 2026.10.23 Lee start
			// GridData[Index].OwnerPlayerId = 0;
			// 2026.09.25 Lee end
			// GridData[Index].TileType = Edit_TileType;
			// UpdateMapVisuals();
			/** @brief 明示編集も共通経路を使い、空洞と床を同期する。 */
			SetTileData(Edit_X, Edit_Y, Edit_TileType, 0);
			// 2026.10.23 Lee end
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
	// 2026.10.22 Lee start（ワークベンチ対応：Super（OnConstruction起動）より先にレイアウトと
	// パラメータを確定させる。無効値は黙って適用せず却下、サイズ変更は旧XYレイアウト保持で再配置、
	// Edit_* の自動Applyは廃止（明示ボタンのみ）。旧実装は下方にコメント保持）
	const FName PropName = PropertyChangedEvent.GetPropertyName();

	// PIE / ゲームワールドではワークベンチ処理を行わない（Game を汚染しない）
	if (IsWorkbenchEditableContext(this))
	{
		if (PropName == GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth) ||
			PropName == GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight))
		{
			const bool bWidthInvalid = (MapWidth <= 0 || MapWidth > MaxGridDimension);
			const bool bHeightInvalid = (MapHeight <= 0 || MapHeight > MaxGridDimension);

			// 無効サイズは却下して旧値へ復元する（黙ってクランプ分配を行わない）
			if (bWidthInvalid || bHeightInvalid)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("PostEditChangeProperty: サイズ (%d x %d) は無効のため却下します (許容: 1 ～ %d)"),
					MapWidth, MapHeight, MaxGridDimension);
				if (bWidthInvalid)	{ MapWidth = (SavedMapWidth > 0) ? SavedMapWidth : 1; }
				if (bHeightInvalid)	{ MapHeight = (SavedMapHeight > 0) ? SavedMapHeight : 1; }
			}
			else
			{
				// 越界は既定で退避（破棄指定はEditorパネルから明示的に行う）。
				// 成功時は GridData が現サイズへ揃うため、Super 経由の OnConstruction は視覚再構築のみとなる。
				ResizeGridPreservingLayout(MapWidth, MapHeight, /*bPreserveOutOfBounds=*/true);
				SavedTileSize = TileSize;
			}
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(AMapManager, TileSize))
		{
			// 非有限・非正のTileSizeは却下して旧値へ復元する（GridDataは清空しない）
			if (!FMath::IsFinite(TileSize) || TileSize <= 0.f)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("PostEditChangeProperty: TileSize (%f) は無効のため却下します"), TileSize);
				if (SavedTileSize > 0.f)
				{
					TileSize = SavedTileSize;
				}
			}
			else
			{
				// TileSize変更はレイアウトを保ったまま床と位置のみ再構築する（旧実装のようなクリアを行わない）。
				// TileSizeは実変更なので関連SpawnerのSpawnLocation同期（Modify伴う）まで行う。
				RebuildFloorAndPositions(/*bSyncSpawnBindings=*/true);
				SavedTileSize = TileSize;
			}
		}
	}

	// Super はレイアウト確定後に呼ぶ（内部のRerunConstructionScripts → OnConstruction で
	// 旧レイアウトが保護処理なしにクリアされるのを防ぐ）
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropName == GET_MEMBER_NAME_CHECKED(AMapManager, Edit_X) ||
		PropName == GET_MEMBER_NAME_CHECKED(AMapManager, Edit_Y) ||
		PropName == GET_MEMBER_NAME_CHECKED(AMapManager, Edit_TileType))
	{
		// 旧実装：Edit_* 変更で自動適用していたが、誤適用防止のため廃止（ApplyTileEdit ボタンは明示実行のまま残す）
		// ApplyTileEdit();
	}
	// 2026.10.22 Lee end

	// 2026.10.22 Lee start（旧実装（保持）：サイズ・TileSize変更で全面クリア、Edit_* で自動Applyしていた）
#if 0
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
#endif
	// 2026.10.22 Lee end
}
#endif

void AMapManager::ResetAndGenerateBlankMap()
{
// 2026.10.22 Lee start（旧実装（保持）：無防護に現在値でReserveしていた）
	// GridData.Empty();
// 2026.10.22 Lee end

	// 2026.10.22 Lee start（明示的クリア動作：不正なパラメータは旧有効値へ復元し、
	// 基線・越界档案・出生バインドも明示的に初期化する）
#if WITH_EDITOR
	if (MapWidth <= 0 || MapWidth > MaxGridDimension ||
		MapHeight <= 0 || MapHeight > MaxGridDimension ||
		!FMath::IsFinite(TileSize) || TileSize <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ResetAndGenerateBlankMap: 無効パラメータ (Map=%d x %d, TileSize=%f) を旧有効値へ復元します"),
			MapWidth, MapHeight, TileSize);
		if (MapWidth <= 0 || MapWidth > MaxGridDimension)
		{
			MapWidth = (SavedMapWidth > 0 && SavedMapWidth <= MaxGridDimension) ? SavedMapWidth : 1;
		}
		if (MapHeight <= 0 || MapHeight > MaxGridDimension)
		{
			MapHeight = (SavedMapHeight > 0 && SavedMapHeight <= MaxGridDimension) ? SavedMapHeight : 1;
		}
		if (!FMath::IsFinite(TileSize) || TileSize <= 0.f)
		{
			TileSize = (SavedTileSize > 0.f) ? SavedTileSize : 100.f;
		}
	}

	SavedMapWidth = MapWidth;
	SavedMapHeight = MapHeight;
	SavedTileSize = TileSize;
	OutOfBoundsTiles.Empty();
	SpawnTileBindings.Empty();
#endif

	GridData.Empty();
	GridData.Reserve(MapWidth * MapHeight);

	// 2026.10.23 Lee start
	/** @brief 床の生成は末尾の UpdateMapVisuals に集約する。 */
	// if (HISM_BaseFloor)
	// {
	// 	HISM_BaseFloor->ClearInstances();
	// }
	// 2026.10.23 Lee end

	FVector StartLocation = GetActorLocation();

	for (int32 Y = 0; Y < MapHeight; Y++)
	{
		for (int32 X = 0; X < MapWidth; X++)
		{
			FMapTileData NewTile;
			NewTile.WorldLocation = StartLocation + FVector(X * TileSize, Y * TileSize, 0.0f);
			NewTile.TileType = ETileType::Empty;
			GridData.Add(NewTile);

			// 2026.10.23 Lee start
			// if (HISM_BaseFloor)
			// {
			// 	FTransform InstanceTransform;
			// 	InstanceTransform.SetLocation(FVector(X * TileSize, Y * TileSize, 0.0f));
			// 	HISM_BaseFloor->AddInstance(InstanceTransform);
			// }
			// 2026.10.23 Lee end
		}
	}

	UpdateMapVisuals();
}

// 2026.10.22 Lee start（マップ工作台データ層：CPP実装。トランザクションは呼び出し側（後続UI）が管理するため、
// ここでは Modify() のみを行い Begin/End Transaction は自分で張らない）
// ※ヘッダー側の宣言が WITH_EDITOR 配置のため、本実装ブロック全体を WITH_EDITOR で囲む
#if WITH_EDITOR

namespace
{
	/**
	 * @brief ワークベンチ編集が許可されたコンテキストかを判定する。
	 *        エディタワールド且つ非PIE且つレベル未ロックである事を確認する。
	 * @param Map 判定対象のマップマネージャー。
	 * @return 編集可能な場合 true。
	 */
	bool IsWorkbenchEditableContext(const AMapManager* Map)
	{
		if (!IsValid(Map))
		{
			return false;
		}

		const UWorld* World = Map->GetWorld();
		if (!World || World->WorldType != EWorldType::Editor || World->IsPlayInEditor())
		{
			return false;
		}

		const ULevel* Level = Map->GetLevel();
		if (!Level || Level->bLocked)
		{
			return false;
		}

		return true;
	}

	/**
	 * @brief 対象アクターがマップと同一レベルに存在するかを判定する。
	 * @param Map 基準となるマップマネージャー。
	 * @param Other 比較対象のアクター。
	 * @return 同一レベルの場合 true。
	 */
	bool IsInSameLevel(const AMapManager* Map, const AActor* Other)
	{
		return IsValid(Other) && Other->GetLevel() == Map->GetLevel();
	}
}

void AMapManager::RebuildFloorAndPositions(bool bSyncSpawnBindings)
{
	// PIE / ゲームワールドでは何もしない（ランタイム経路を污染しない）
	if (!IsWorkbenchEditableContext(this))
	{
		return;
	}

	// 2026.10.22 Lee start（無効TileSize・無効盤面幅では再構築を行わない（除算・負座標の拡散防止））
	if (MapWidth <= 0 || MapHeight <= 0 || !FMath::IsFinite(TileSize) || TileSize <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RebuildFloorAndPositions: 無効パラメータ (Map=%d x %d, TileSize=%f) のため再構築を中止します"),
			MapWidth, MapHeight, TileSize);
		return;
	}
	// 2026.10.22 Lee end

	const FVector MapOrigin = GetActorLocation();

	//----------------------------------------------------------
	// 1. GridData の WorldLocation を現在のアクター位置へ同期
	//----------------------------------------------------------
	for (int32 i = 0; i < GridData.Num(); ++i)
	{
		const int32 X = i % MapWidth;
		const int32 Y = i / MapWidth;
		GridData[i].WorldLocation = MapOrigin + FVector(X * TileSize, Y * TileSize, 0.f);
	}

	//----------------------------------------------------------
	// 2. 床 HISM（ベース層）を現在の TileSize で全量再構築
	//----------------------------------------------------------
	// 2026.10.23 Lee start
	/** @brief 次の全体更新が床も再構築するため、重複する旧生成処理は保持のみとする。 */
	// if (HISM_BaseFloor && GridData.Num() > 0)
	// {
	// 	HISM_BaseFloor->ClearInstances();
	// 	for (int32 i = 0; i < GridData.Num(); ++i)
	// 	{
	// 		const int32 X = i % MapWidth;
	// 		const int32 Y = i / MapWidth;
	// 		FTransform InstanceTransform;
	// 		InstanceTransform.SetLocation(FVector(X * TileSize, Y * TileSize, 0.f));
	// 		HISM_BaseFloor->AddInstance(InstanceTransform);
	// 	}
	// }
	// 2026.10.23 Lee end

	//----------------------------------------------------------
	// 3. 状態レイヤー（矢印・Spawn・Goal等）を全量再構築
	//----------------------------------------------------------
	UpdateMapVisuals();

	//----------------------------------------------------------
	// 4. 関連付け済み Spawner の SpawnLocation のみ同期（明示的編集時のみ）。
	//    視覚再構築（OnConstruction / Undo）では false を指定し、
	//    他アクターへの書き込み・Modify を行わない。
	//    Actor Transform・SpawnRotation・ToolCreatedActors は常に動かさない。
	//----------------------------------------------------------
	if (!bSyncSpawnBindings)
	{
		return;
	}

	for (const FSpawnTileBinding& Binding : SpawnTileBindings)
	{
		AAnimalSpawner* Spawner = Binding.Spawner;
		if (!IsValid(Spawner) || !IsInSameLevel(this, Spawner))
		{
			continue;
		}

		const FVector TileCenter = ToWorldCenter_Implementation(Binding.GridCoords);
		const FVector NewSpawnLocation = TileCenter + FVector(0.f, 0.f, Binding.HeightOffset);

		if (!Spawner->SpawnLocation.Equals(NewSpawnLocation))
		{
			Spawner->Modify();
			Spawner->SpawnLocation = NewSpawnLocation;
		}
	}
}
// 2026.10.22 Lee end（RebuildFloorAndPositions）

// 2026.10.22 Lee start（ResizeGridPreservingLayout：旧XYレイアウト保持リサイズ）
bool AMapManager::ResizeGridPreservingLayout(int32 NewWidth, int32 NewHeight, bool bPreserveOutOfBounds)
{
	// 無効サイズは黙って確保しない（呼び出し元へ明示的に失敗を返す）
	if (NewWidth <= 0 || NewWidth > MaxGridDimension ||
		NewHeight <= 0 || NewHeight > MaxGridDimension)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ResizeGridPreservingLayout: サイズ (%d x %d) は無効のため拒否しました (許容: 1 ～ %d)"),
			NewWidth, NewHeight, MaxGridDimension);
		return false;
	}

	// 2026.10.22 Lee start（無操作判定：引数が現属性・保存基線の双方に一致し、かつ
	// GridData 数も現属性に吻合する場合のみ無変化とする。同面積（5x4→4x5）では数量が
	// 一致しても幅が変わりXY解釈が変わるため、現属性と数量だけの比較では早退させない）
	// 【旧実装（保持）：現属性と数量のみ比較していたため、同面積変更時に誤って早退していた】
	// if (NewWidth == MapWidth && NewHeight == MapHeight
	// 	&& GridData.Num() == MapWidth * MapHeight)
	// {
	// 	return false;
	// }
	const bool bMatchesCurrentSize = (NewWidth == MapWidth && NewHeight == MapHeight);
	const bool bMatchesSavedSize = (NewWidth == SavedMapWidth && NewHeight == SavedMapHeight);
	const bool bDataMatchesCurrentSize = (GridData.Num() == MapWidth * MapHeight);
	if (bMatchesCurrentSize && bMatchesSavedSize && bDataMatchesCurrentSize)
	{
		return false;
	}
	// 2026.10.22 Lee end

	if (!IsWorkbenchEditableContext(this))
	{
		return false;
	}

	// 旧幅は PreEditChange が記録した SavedMapWidth を使用する
	// （0 は履歴なしセンチネル：リサイズの基準が取れないため拒否する）
	if (SavedMapWidth <= 0 || SavedMapHeight <= 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ResizeGridPreservingLayout: 旧サイズ履歴がありません (Saved=%d x %d)。"
				 "先にPostLoad経由で基準を確定してください。"),
			SavedMapWidth, SavedMapHeight);
		return false;
	}

	if (GridData.Num() != SavedMapWidth * SavedMapHeight)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ResizeGridPreservingLayout: GridData(%d) と旧サイズ(%d x %d)が不一致のため拒否しました。"),
			GridData.Num(), SavedMapWidth, SavedMapHeight);
		return false;
	}

	// トランザクション参加（呼び出し側がトランザクション範囲を管理する前提）
	Modify();
	for (const FSpawnTileBinding& Binding : SpawnTileBindings)
	{
		if (IsValid(Binding.Spawner))
		{
			Binding.Spawner->Modify();
		}
	}

	const int32 OldWidth = SavedMapWidth;
	const int32 OldHeight = SavedMapHeight;

	//----------------------------------------------------------
	// 1. 退避档案から Empty 記録を除去し、復元候補を保持する
	//    （実際の復元は新Grid生成後のステップ3で行う）
	//----------------------------------------------------------
	TArray<FOutOfBoundsTileRecord> RemainingRecords;
	for (const FOutOfBoundsTileRecord& Record : OutOfBoundsTiles)
	{
		// Empty記録は復元・退避の両方で無価値ため破棄する
		if (Record.TileData.TileType == ETileType::Empty)
		{
			continue;
		}
		RemainingRecords.Add(Record);
	}

	//----------------------------------------------------------
	// 2. 旧GridData を旧幅基準で走査し、XY を保ったまま新盤面へ写す
	//----------------------------------------------------------
	TArray<FMapTileData> NewGrid;
	NewGrid.Init(FMapTileData(), NewWidth * NewHeight);

	for (int32 OldY = 0; OldY < OldHeight; ++OldY)
	{
		for (int32 OldX = 0; OldX < OldWidth; ++OldX)
		{
			const FMapTileData& Source = GridData[OldY * OldWidth + OldX];

			if (OldX < NewWidth && OldY < NewHeight)
			{
				// XYレイアウトを保ったまま移設（TileType と Owner を保持）
				NewGrid[OldY * NewWidth + OldX] = Source;
			}
			else if (bPreserveOutOfBounds && Source.TileType != ETileType::Empty)
			{
				// 越界した非空タイルのみ档案へ退避する
				FOutOfBoundsTileRecord Record;
				Record.GridCoords = FIntPoint(OldX, OldY);
				Record.TileData = Source;
				RemainingRecords.Add(Record);
			}
		}
	}

	//----------------------------------------------------------
	// 3. 档案の復元（新盤面内かつ空きセルのみ。TileType/Owner を保持）
	//----------------------------------------------------------
	TArray<FOutOfBoundsTileRecord> StillOutOfBounds;
	for (const FOutOfBoundsTileRecord& Record : RemainingRecords)
	{
		const bool bInNewBounds = Record.GridCoords.X >= 0 && Record.GridCoords.X < NewWidth
			&& Record.GridCoords.Y >= 0 && Record.GridCoords.Y < NewHeight;

		const int32 NewIndex = bInNewBounds
			? Record.GridCoords.Y * NewWidth + Record.GridCoords.X
			: INDEX_NONE;

		if (NewIndex != INDEX_NONE && NewGrid[NewIndex].TileType == ETileType::Empty)
		{
			NewGrid[NewIndex] = Record.TileData;
		}
		else
		{
			// 盤面外または空きが無い記録は档案に残す（データは失わせない）
			StillOutOfBounds.Add(Record);
		}
	}

	//----------------------------------------------------------
	// 4. GridData を常に W*H で確保し、Saved* を新状態へ更新
	//----------------------------------------------------------
	GridData = MoveTemp(NewGrid);
	MapWidth = NewWidth;
	MapHeight = NewHeight;
	SavedMapWidth = NewWidth;
	SavedMapHeight = NewHeight;
	OutOfBoundsTiles = MoveTemp(StillOutOfBounds);

	//----------------------------------------------------------
	// 5. bPreserve=false は越界档案を放棄し、越界したSpawn関連付けを解除する
	//----------------------------------------------------------
	if (!bPreserveOutOfBounds)
	{
		OutOfBoundsTiles.Empty();

		for (int32 i = SpawnTileBindings.Num() - 1; i >= 0; --i)
		{
			const FIntPoint Coords = SpawnTileBindings[i].GridCoords;
			const bool bOutOfBounds = Coords.X < 0 || Coords.X >= MapWidth
				|| Coords.Y < 0 || Coords.Y >= MapHeight;
			if (bOutOfBounds)
			{
				SpawnTileBindings.RemoveAt(i);
			}
		}
	}

	// 床・状態レイヤー・関連SpawnerのSpawnLocationを新盤面へ同期（明示的編集操作）。
	// （ToolCreatedActors は移動しない。破棄もEditorパネルに委ねる）
	RebuildFloorAndPositions(/*bSyncSpawnBindings=*/true);

	return true;
}
// 2026.10.22 Lee end（ResizeGridPreservingLayout）

// 2026.10.22 Lee start（出生タイルのバインド／解綁／移動）
bool AMapManager::BindSpawnTile(AAnimalSpawner* Spawner, FIntPoint GridCoords, float HeightOffset)
{
	if (!IsValid(Spawner))
	{
		UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: Spawner が無効です"));
		return false;
	}

	if (!IsWorkbenchEditableContext(this))
	{
		return false;
	}

	// 同一レベルの Spawner のみ受け付ける（他レベル資産の誤参照防止）
	if (!IsInSameLevel(this, Spawner))
	{
		UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: Spawner がマップと別レベルに存在します"));
		return false;
	}

	// 2026.10.22 Lee start（非有限の高さオフセットは不正データとして拒否する）
	if (!FMath::IsFinite(HeightOffset))
	{
		UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: HeightOffset (%f) が非有限のため拒否しました"), HeightOffset);
		return false;
	}
	// 2026.10.22 Lee end

	// 出生タイルは Spawn 型かつ盤面内のみ
	const bool bInBounds = GridCoords.X >= 0 && GridCoords.X < MapWidth
		&& GridCoords.Y >= 0 && GridCoords.Y < MapHeight;
	if (!bInBounds)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: 座標 (%d, %d) が盤面外です"), GridCoords.X, GridCoords.Y);
		return false;
	}

	const int32 Index = GridCoords.Y * MapWidth + GridCoords.X;
	if (!GridData.IsValidIndex(Index) || GridData[Index].TileType != ETileType::Spawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: 座標 (%d, %d) は Spawn タイルではありません"),
			GridCoords.X, GridCoords.Y);
		return false;
	}

	// Spawner・座標の重複バインドは拒否する
	for (const FSpawnTileBinding& Binding : SpawnTileBindings)
	{
		if (Binding.Spawner == Spawner)
		{
			UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: 指定 Spawner は既にバインド済みです"));
			return false;
		}
		if (Binding.GridCoords == GridCoords)
		{
			UE_LOG(LogTemp, Warning, TEXT("BindSpawnTile: 座標 (%d, %d) は既に他の Spawner にバインド済みです"),
				GridCoords.X, GridCoords.Y);
			return false;
		}
	}

	Modify();

	FSpawnTileBinding NewBinding;
	NewBinding.Spawner = Spawner;
	NewBinding.GridCoords = GridCoords;
	NewBinding.HeightOffset = HeightOffset;
	SpawnTileBindings.Add(NewBinding);

	// Actor Transform は動かさず、実際に使用される SpawnLocation のみ同期する
	const FVector NewSpawnLocation = ToWorldCenter_Implementation(GridCoords)
		+ FVector(0.f, 0.f, HeightOffset);
	Spawner->Modify();
	Spawner->SpawnLocation = NewSpawnLocation;

	return true;
}

bool AMapManager::UnbindSpawnTile(AAnimalSpawner* Spawner)
{
	if (!IsValid(Spawner))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnbindSpawnTile: Spawner が無効です"));
		return false;
	}

	if (!IsWorkbenchEditableContext(this))
	{
		return false;
	}

	// 2026.10.22 Lee start（解綁時も Spawner が同一の編集可能レベルに居る事を検証する）
	if (!IsInSameLevel(this, Spawner))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnbindSpawnTile: Spawner がマップと別レベルに存在します"));
		return false;
	}
	// 2026.10.22 Lee end

	for (int32 i = 0; i < SpawnTileBindings.Num(); ++i)
	{
		if (SpawnTileBindings[i].Spawner == Spawner)
		{
			Modify();
			SpawnTileBindings.RemoveAt(i);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UnbindSpawnTile: 指定 Spawner のバインドは存在しません"));
	return false;
}

bool AMapManager::MoveSpawnTile(AAnimalSpawner* Spawner, FIntPoint NewGridCoords)
{
	if (!IsValid(Spawner))
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveSpawnTile: Spawner が無効です"));
		return false;
	}

	if (!IsWorkbenchEditableContext(this))
	{
		return false;
	}

	// 2026.10.22 Lee start（移動・解綁時も Spawner が同一の編集可能レベルに居る事を検証する）
	if (!IsInSameLevel(this, Spawner))
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveSpawnTile: Spawner がマップと別レベルに存在します"));
		return false;
	}
	// 2026.10.22 Lee end

	int32 BindingIndex = INDEX_NONE;
	for (int32 i = 0; i < SpawnTileBindings.Num(); ++i)
	{
		if (SpawnTileBindings[i].Spawner == Spawner)
		{
			BindingIndex = i;
			break;
		}
	}
	if (BindingIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveSpawnTile: 指定 Spawner はバインドされていません"));
		return false;
	}

	const FIntPoint OldGridCoords = SpawnTileBindings[BindingIndex].GridCoords;
	const float HeightOffset = SpawnTileBindings[BindingIndex].HeightOffset;

	// 2026.10.22 Lee start（同一座標への移動は変更なしとして拒否（Modifyも行わない））
	if (NewGridCoords == OldGridCoords)
	{
		return false;
	}
	// 2026.10.22 Lee end

	const bool bInBounds = NewGridCoords.X >= 0 && NewGridCoords.X < MapWidth
		&& NewGridCoords.Y >= 0 && NewGridCoords.Y < MapHeight;
	if (!bInBounds)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveSpawnTile: 座標 (%d, %d) が盤面外です"), NewGridCoords.X, NewGridCoords.Y);
		return false;
	}

	const int32 NewIndex = NewGridCoords.Y * MapWidth + NewGridCoords.X;
	if (!GridData.IsValidIndex(NewIndex))
	{
		return false;
	}

	// 移動先は空きセルのみ。別の Spawn / Goal や既存タイルの上書きは拒否する
	// （Goal 破壊や他 Spawn との競合、プリセット矢印の消失を防ぐ）
	if (GridData[NewIndex].TileType != ETileType::Empty)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MoveSpawnTile: 移動先 (%d, %d) が空きセルではありません（他の Spawn / Goal / 矢印の上書きは拒否）"),
			NewGridCoords.X, NewGridCoords.Y);
		return false;
	}

	Modify();

	// 2026.10.22 Lee start（旧セルは X/Y をそれぞれ範囲検査してから索引する
	// （盤面外の旧バインドが行跨ぎで別の合法セルへ別名するのを防止））
	const bool bOldInBounds = OldGridCoords.X >= 0 && OldGridCoords.X < MapWidth
		&& OldGridCoords.Y >= 0 && OldGridCoords.Y < MapHeight;
	if (bOldInBounds)
	{
		const int32 OldIndex = OldGridCoords.Y * MapWidth + OldGridCoords.X;
		if (GridData.IsValidIndex(OldIndex) && GridData[OldIndex].TileType == ETileType::Spawn)
		{
			GridData[OldIndex].TileType = ETileType::Empty;
			GridData[OldIndex].OwnerPlayerId = 0;
		}
	}
	else
	{
		// 旧出生セルが越界档案に退避されている場合は該当Spawn退避記録を削除する（拡張時の復活防止）
		const int32 Removed = OutOfBoundsTiles.RemoveAll([&](const FOutOfBoundsTileRecord& Record)
		{
			return Record.GridCoords == OldGridCoords
				&& Record.TileData.TileType == ETileType::Spawn;
		});
		if (Removed > 0)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("MoveSpawnTile: 越界档案の旧Spawn (%d, %d) を %d 件削除しました"),
				OldGridCoords.X, OldGridCoords.Y, Removed);
		}
	}
	// 2026.10.22 Lee end

	// 新セルを Spawn へ変更（所有者は必ず 0 に正規化する）
	GridData[NewIndex].TileType = ETileType::Spawn;
	GridData[NewIndex].OwnerPlayerId = 0;

	SpawnTileBindings[BindingIndex].GridCoords = NewGridCoords;

	// 高さオフセットを保持したまま使用中の SpawnLocation のみ同期する
	// （Actor Transform・SpawnRotation は変更しない）
	const FVector NewSpawnLocation = ToWorldCenter_Implementation(NewGridCoords)
		+ FVector(0.f, 0.f, HeightOffset);
	Spawner->Modify();
	Spawner->SpawnLocation = NewSpawnLocation;

	// Spawn レイヤーのみ再構築（Empty は描画レイヤーを持たない）
	RefreshStateVisual(ETileType::Spawn);

	return true;
}
// 2026.10.22 Lee end（出生タイルのバインド／解綁／移動）

// 2026.10.22 Lee start（マップ工作台用エディタライフサイクル）
void AMapManager::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	//----------------------------------------------------------
	// 旧資産（センチネル0）の初回取り込み。
	// ヘッダ既定値の 10 を履歴と誤認しないよう、実際にロードされた
	// レベルの MapWidth / MapHeight / TileSize を基準として受容する。
	// PIEインスタンスへはエディタメタデータを書き込まない。
	// 既存の有効データ（バインド等）の整理・変更は行わない（暗黙の資産変更防止）。
	// HISM 再構築も行わない（コンポーネント登録未完了の可能性）。
	//----------------------------------------------------------
	const UWorld* PostLoadWorld = GetWorld();
	const bool bCanAdoptBaseline = PostLoadWorld
		&& PostLoadWorld->WorldType == EWorldType::Editor
		&& !PostLoadWorld->IsPlayInEditor();

	if (bCanAdoptBaseline)
	{
		if (SavedMapWidth <= 0 || SavedMapHeight <= 0)
		{
			SavedMapWidth = MapWidth;
			SavedMapHeight = MapHeight;
		}
		if (SavedTileSize <= 0.f)
		{
			SavedTileSize = TileSize;
		}
	}
#endif
}

void AMapManager::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

#if WITH_EDITOR
	if (PropertyAboutToChange == nullptr)
	{
		return;
	}

	//----------------------------------------------------------
	// 変更前の旧サイズ・TileSize を常に3項目まとめて履歴退避する
	// （どれが変わっても復元・Resize基線として完全な旧状態を参照できるようにするため。
	//   SavedTileSize は成功した再構築後に新値へ更新される）
	//----------------------------------------------------------
	if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth) ||
		PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight) ||
		PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(AMapManager, TileSize))
	{
		SavedMapWidth = MapWidth;
		SavedMapHeight = MapHeight;
		SavedTileSize = TileSize;

		//----------------------------------------------------------
		// リサイズ・TileSize変更で SpawnLocation の書き換えが発生し得るため、
		// 関連 Spawner を先にトランザクションへ参加させる
		//----------------------------------------------------------
		for (const FSpawnTileBinding& Binding : SpawnTileBindings)
		{
			if (IsValid(Binding.Spawner))
			{
				Binding.Spawner->Modify();
			}
		}
	}
#endif
}

void AMapManager::PostEditUndo()
{
	Super::PostEditUndo();

#if WITH_EDITOR
	// Undo/Redo 後は他アクター（Spawner 等）へ書き戻さず Modify も行わない。
	// トランザクション復元を壊さないよう、自分の表示のみをデータへ再同期する。
	if (!IsWorkbenchEditableContext(this))
	{
		return;
	}

	//----------------------------------------------------------
	// 完全再構築：TileSize取り消しや同面積異サイズのUndoでは実インスタンス数が
	// 一致しても位置がずれるため、インスタンス数比較は行わず毎回全面再構築する
	// （bSyncSpawnBindings=false により他アクターには一切書き込まない）
	//----------------------------------------------------------
	RebuildFloorAndPositions(/*bSyncSpawnBindings=*/false);
#endif
}

void AMapManager::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

#if WITH_EDITOR
	// ドラッグ中は OnConstruction 側の相対座標描画に任せ、完了時のみデータを同期する
	if (!bFinished || !IsWorkbenchEditableContext(this))
	{
		return;
	}

	// 2026.10.22 Lee start（無効パラメータでは同期しない（RebuildFloorAndPositions と同一の防御））
	if (MapWidth <= 0 || MapHeight <= 0 || !FMath::IsFinite(TileSize) || TileSize <= 0.f)
	{
		return;
	}
	// 2026.10.22 Lee end

	//----------------------------------------------------------
	// 座標データのみを同期（床インスタンスは相対座標のため再構築不要）。
	// 装飾アクター（BP_ObjectAnimal 等）・ToolCreatedActors は絶対に動かさない。
	//----------------------------------------------------------
	const FVector MapOrigin = GetActorLocation();
	for (int32 i = 0; i < GridData.Num(); ++i)
	{
		const int32 X = i % MapWidth;
		const int32 Y = i / MapWidth;
		GridData[i].WorldLocation = MapOrigin + FVector(X * TileSize, Y * TileSize, 0.f);
	}

	// 明示的にバインドされた Spawner の SpawnLocation のみ追従させる
	// （Actor Transform は変更しない）
	for (const FSpawnTileBinding& Binding : SpawnTileBindings)
	{
		AAnimalSpawner* Spawner = Binding.Spawner;
		if (!IsValid(Spawner) || !IsInSameLevel(this, Spawner))
		{
			continue;
		}
		const FVector NewSpawnLocation = ToWorldCenter_Implementation(Binding.GridCoords)
			+ FVector(0.f, 0.f, Binding.HeightOffset);
		if (!Spawner->SpawnLocation.Equals(NewSpawnLocation))
		{
			Spawner->Modify();
			Spawner->SpawnLocation = NewSpawnLocation;
		}
	}
#endif
}
// 2026.10.22 Lee end（マップ工作台用エディタライフサイクル）
#endif // WITH_EDITOR
