// Copyright Epic Games, Inc. All Rights Reserved.

// 2026.10.22 Lee start（マップ工作台データ層のエディタ自動テスト）
// 検証対象 : AMapManager のレイアウト保持リサイズ / Details パラメータ編集フロー /
//            トランザクション Undo・Redo / 出生タイル バインド操作 / メモリシリアライズ復元
// 前提     : テスト用の一時エディタ世界は FAutomationEditorCommonUtils のスコープ付き世界で生成し、
//            終了時に自動破棄する。Content 資産は一切作成・削除しない。
#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include <limits>
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/World.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "UObject/UnrealType.h"
#include "Lee/MapManager.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "GameTypes.h"
#include "Lee/MapWorkbench/SMapWorkbench.h"
#include "Framework/Application/SlateApplication.h"

// 2026.10.23 Lee start
#include "Engine/StaticMesh.h"
#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/CursorPawn.h"
// 2026.10.23 Lee start
#include "Takeuchi/Pawn/AnimalBase.h"
#include "Tanimura/MainGameMode.h"
#include "Components/BoxComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"
// 2026.10.23 Lee end
// 2026.10.23 Lee end

namespace
{
	/**
	 * @brief テスト用の一時エディタ世界を生成する。
	 *        RAII スコープで終了時に自動破棄され、エディタの現在マップを汚さない。
	 * @return 生成されたスコープ付き世界。
	 */
	TUniquePtr<FScopedEditorWorld> MakeTestWorld()
	{
		const UWorld::InitializationValues InitValues;
		return FAutomationEditorCommonUtils::CreateScopedEditorWorld(TEXT("Basic"), InitValues);
	}

	/**
	 * @brief RF_Transactional 付きでアクターをスポーンする。
	 * @param World スポーン先のエディタ世界。
	 * @return スポーンされたアクター。
	 */
	template <typename T>
	T* SpawnTransactionalActor(UWorld* World, const FTransform& AtTransform = FTransform::Identity)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transactional;
		return World->SpawnActor<T>(T::StaticClass(), AtTransform, SpawnParams);
	}

	/**
	 * @brief 指定セルへタイル種別と所有者IDを直接設定する。
	 * @return 設定に成功した場合 true。
	 */
	bool SetTile(AMapManager* Map, int32 X, int32 Y, ETileType Type, uint8 OwnerPlayerId = 0)
	{
		if (X < 0 || X >= Map->MapWidth || Y < 0 || Y >= Map->MapHeight)
		{
			return false;
		}
		const int32 Index = Y * Map->MapWidth + X;
		if (!Map->GridData.IsValidIndex(Index))
		{
			return false;
		}
		Map->GridData[Index].TileType = Type;
		Map->GridData[Index].OwnerPlayerId = OwnerPlayerId;
		return true;
	}

	/**
	 * @brief 指定セルのタイルデータを取得する（範囲外は nullptr）。
	 */
	const FMapTileData* GetTile(const AMapManager* Map, int32 X, int32 Y)
	{
		if (X < 0 || X >= Map->MapWidth || Y < 0 || Y >= Map->MapHeight)
		{
			return nullptr;
		}
		const int32 Index = Y * Map->MapWidth + X;
		return Map->GridData.IsValidIndex(Index) ? &Map->GridData[Index] : nullptr;
	}

	/**
	 * @brief 指定セルのタイル型を取得する。範囲外・欠落は 255（無効値）を返す。
	 *        実装不具合時にテストがクラッシュせず明確に失敗するための保護。
	 */
	uint8 TypeAt(const AMapManager* Map, int32 X, int32 Y)
	{
		const FMapTileData* Tile = GetTile(Map, X, Y);
		return Tile ? static_cast<uint8>(Tile->TileType) : 255;
	}

	/**
	 * @brief 指定セルの所有者IDを取得する。範囲外・欠落は 255（無効値）を返す。
	 */
	uint8 OwnerAt(const AMapManager* Map, int32 X, int32 Y)
	{
		const FMapTileData* Tile = GetTile(Map, X, Y);
		return Tile ? Tile->OwnerPlayerId : 255;
	}

	/**
	 * @brief Details パネルと同一のプロパティ変更フローを実行する。
	 *        PreEditChange → 値設定 → PostEditChangeProperty の順で実データ経路を通す。
	 * @param Map 対象マップ。
	 * @param PropertyName 変更するプロパティ名。
	 * @param NewValue 設定する値。
	 */
	template <typename TValue>
	void SetPropertyViaDetails(AMapManager* Map, const FName PropertyName, const TValue& NewValue)
	{
		FProperty* Property = FindFProperty<FProperty>(AMapManager::StaticClass(), PropertyName);
		if (!Property)
		{
			return;
		}
		FPropertyChangedEvent ChangeEvent(Property);
		Map->PreEditChange(Property);
		TValue* ValuePtr = Property->ContainerPtrToValuePtr<TValue>(Map);
		*ValuePtr = NewValue;
		Map->PostEditChangeProperty(ChangeEvent);
	}

	/**
	 * @brief HISM ベース床の全インスタンス位置が「現在の幅・TileSize」の格子に一致するか検査する。
	 * @param Test 自動テスト本体。
	 * @param Map 対象マップ。
	 * @param What 検査名（アサーション表示用）。
	 */
	void VerifyBaseFloorInstanceGrid(FAutomationTestBase& Test, const AMapManager* Map, const FString& What)
	{
		if (!Map->HISM_BaseFloor)
		{
			Test.AddError(FString::Printf(TEXT("[%s] HISM_BaseFloor が null です"), *What));
			return;
		}
		// 2026.10.23 Lee start
		// if (!Test.TestEqual(What + TEXT(" / 床インスタンス数"), Map->HISM_BaseFloor->GetInstanceCount(), Map->MapWidth * Map->MapHeight))
		/** @brief 空洞を除外したグリッド順を期待値とし、インスタンス番号とセル番号を分離する。 */
		TArray<int32> FloorCells;
		for (int32 Cell = 0; Cell < Map->GridData.Num(); ++Cell)
		{
			if (Map->GridData[Cell].TileType != ETileType::Hole)
			{
				FloorCells.Add(Cell);
			}
		}
		if (!Test.TestEqual(What + TEXT(" / 床インスタンス数"), Map->HISM_BaseFloor->GetInstanceCount(), FloorCells.Num()))
		// 2026.10.23 Lee end
		{
			return;
		}
		for (int32 i = 0; i < Map->HISM_BaseFloor->GetInstanceCount(); ++i)
		{
			FTransform InstanceTransform;
			Map->HISM_BaseFloor->GetInstanceTransform(i, InstanceTransform, /*bWorldSpace=*/false);
			// 2026.10.23 Lee start
			// const int32 X = i % Map->MapWidth;
			// const int32 Y = i / Map->MapWidth;
			const int32 X = FloorCells[i] % Map->MapWidth;
			const int32 Y = FloorCells[i] / Map->MapWidth;
			// 2026.10.23 Lee end
			const FVector Expected(X * Map->TileSize, Y * Map->TileSize, 0.f);
			if (!InstanceTransform.GetLocation().Equals(Expected, 0.01f))
			{
				Test.AddError(FString::Printf(TEXT("[%s] インスタンス %d の位置が不正です (実績=%s, 期待=%s)"),
					*What, i, *InstanceTransform.GetLocation().ToCompactString(), *Expected.ToCompactString()));
				return;
			}
		}
	}
}

/**
 * 非正方形グリッドの XY レイアウト保持リサイズを検証する。
 * ・拡大時の既存セル（型・所有者）の XY 保持
 * ・縮小時の越界セルの退避記録化
 * ・再拡大時の退避記録からの復元
 * ・GridData 数が常に W*H に一致する事
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchResizePreservesLayout,
	"AnimalGatherer.MapWorkbench.ResizeGridPreservesLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchResizePreservesLayout::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("検証：非正方形グリッドのリサイズで XY レイアウト・所有者を保持し、越界退避→復元が成立する"));

	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	if (!TestNotNull(TEXT("一時エディタ世界"), World))
	{
		return false;
	}

	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}

	// 既定 10x10 から非正方形 5x3 へ縮小（テスト基盤を整える）
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 5);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 3);
	TestEqual(TEXT("基盤 5x3 の GridData 数"), Map->GridData.Num(), 15);

	// 初期レイアウト：(4,1)=2Pの矢印 / (0,0)=1Pゴール / (2,1)=1Pの矢印
	TestTrue(TEXT("(4,1) 設定"), SetTile(Map, 4, 1, ETileType::DirRight, 1));
	TestTrue(TEXT("(0,0) 設定"), SetTile(Map, 0, 0, ETileType::GoalP1, 0));
	TestTrue(TEXT("(2,1) 設定"), SetTile(Map, 2, 1, ETileType::DirUp, 0));

	//----------------------------------------------------------
	// 1. 拡大 5x3 → 8x3：XY・型・所有者を保持する
	//----------------------------------------------------------
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 8);
	TestEqual(TEXT("拡大後 8x3 の GridData 数"), Map->GridData.Num(), 24);

	if (const FMapTileData* Tile41 = GetTile(Map, 4, 1))
	{
		TestEqual(TEXT("拡大後 (4,1) 型"), static_cast<uint8>(Tile41->TileType), static_cast<uint8>(ETileType::DirRight));
		TestEqual(TEXT("拡大後 (4,1) 所有者"), Tile41->OwnerPlayerId, static_cast<uint8>(1));
	}
	else
	{
		AddError(TEXT("拡大後 (4,1) が範囲外扱いされた"));
	}
	if (const FMapTileData* Tile00 = GetTile(Map, 0, 0))
	{
		TestEqual(TEXT("拡大後 (0,0) 型"), static_cast<uint8>(Tile00->TileType), static_cast<uint8>(ETileType::GoalP1));
	}
	else
	{
		AddError(TEXT("拡大後 (0,0) が範囲外扱いされた"));
	}

	//----------------------------------------------------------
	// 2. 縮小 8x3 → 3x2：(4,1) は越界して退避記録化、GridData は 3*2
	//----------------------------------------------------------
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 3);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 2);
	TestEqual(TEXT("縮小後 3x2 の GridData 数"), Map->GridData.Num(), 6);
	TestEqual(TEXT("退避記録は 1 件"), Map->OutOfBoundsTiles.Num(), 1);
	if (Map->OutOfBoundsTiles.Num() == 1)
	{
		const FOutOfBoundsTileRecord& Record = Map->OutOfBoundsTiles[0];
		TestTrue(TEXT("退避記録の座標は (4,1)"), Record.GridCoords == FIntPoint(4, 1));
		TestEqual(TEXT("退避記録の型"), static_cast<uint8>(Record.TileData.TileType), static_cast<uint8>(ETileType::DirRight));
		TestEqual(TEXT("退避記録の所有者"), Record.TileData.OwnerPlayerId, static_cast<uint8>(1));
	}
	TestEqual(TEXT("縮小後 (2,1) 型は盤面内で保持"), TypeAt(Map, 2, 1), static_cast<uint8>(ETileType::DirUp));

	//----------------------------------------------------------
	// 3. 再拡大 3x2 → 5x3：退避記録から (4,1) を復元し、記録は空になる
	//----------------------------------------------------------
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 5);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 3);
	TestEqual(TEXT("再拡大後 5x3 の GridData 数"), Map->GridData.Num(), 15);
	TestEqual(TEXT("再拡大後の退避記録は空"), Map->OutOfBoundsTiles.Num(), 0);

	if (const FMapTileData* Tile41 = GetTile(Map, 4, 1))
	{
		TestEqual(TEXT("復元後 (4,1) 型"), static_cast<uint8>(Tile41->TileType), static_cast<uint8>(ETileType::DirRight));
		TestEqual(TEXT("復元後 (4,1) 所有者"), Tile41->OwnerPlayerId, static_cast<uint8>(1));
	}
	else
	{
		AddError(TEXT("復元後 (4,1) が存在しない（退避記録から復元されていない）"));
	}

	//----------------------------------------------------------
	// 4. Details を経由しない外部代入 + 手動 OnConstruction：同面積 5x4 → 4x5
	//    （数量一致でも幅が変わるため保持Resizeが走り、XYの串刺しや無操作早退が無い事。
	//      旧実装は数量一致のみで早退し、再配置と越界退避が行われなかった）
	//----------------------------------------------------------
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 5);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);
	TestTrue(TEXT("(1,2) を GoalP1 へ"), SetTile(Map, 1, 2, ETileType::GoalP1, 0));
	TestTrue(TEXT("(4,3) を 2Pの矢印へ"), SetTile(Map, 4, 3, ETileType::DirRight, 1));

	// PreEditChange / PostEditChangeProperty を経由しない外部代入を再現する
	Map->MapWidth = 4;
	Map->MapHeight = 5;
	Map->OnConstruction(Map->GetActorTransform());

	TestEqual(TEXT("同面積 4x5 の GridData 数"), Map->GridData.Num(), 20);
	// XY が串刺しになっていない事（旧5幅の線形Indexのまま読むと別セルに化ける）
	TestEqual(TEXT("(1,2) は GoalP1 のまま（串刺しなし）"), TypeAt(Map, 1, 2), static_cast<uint8>(ETileType::GoalP1));
	TestTrue(TEXT("(4,3) は越界退避されている（型と所有者を保持）"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(4, 3)
			&& Record.TileData.TileType == ETileType::DirRight
			&& Record.TileData.OwnerPlayerId == 1;
	}));

	// 手動 OnConstruction で拡大 4x5 → 5x4：退避記録から (4,3) を復元する
	Map->MapWidth = 5;
	Map->MapHeight = 4;
	Map->OnConstruction(Map->GetActorTransform());

	TestEqual(TEXT("再拡大 5x4 の GridData 数"), Map->GridData.Num(), 20);
	TestEqual(TEXT("(4,3) は退避記録から復元"), TypeAt(Map, 4, 3), static_cast<uint8>(ETileType::DirRight));
	TestEqual(TEXT("(4,3) の所有者は復元"), OwnerAt(Map, 4, 3), static_cast<uint8>(1));
	TestEqual(TEXT("(1,2) は拡大後も維持"), TypeAt(Map, 1, 2), static_cast<uint8>(ETileType::GoalP1));
	TestEqual(TEXT("再拡大後の退避記録は空"), Map->OutOfBoundsTiles.Num(), 0);

	return true;
}

/**
 * Details パネルと同一の PreEditChange → 値設定 → PostEditChangeProperty フローを検証する。
 * ・新規配置アクターは PostLoad 無しで基線が確立され Resize 可能
 * ・サイズ変更でレイアウトがクリアされない
 * ・Edit_X / Edit_Y / Edit_TileType の変更は自動適用されない
 * ・TileSize 変更は位置のみ変え、型を保持する
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchDetailsParameterEditFlow,
	"AnimalGatherer.MapWorkbench.DetailsParameterEditFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchDetailsParameterEditFlow::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("検証：Details パラメータ編集の実フローでレイアウト保護・非自動適用・TileSize 位置反映を確認"));

	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	if (!TestNotNull(TEXT("一時エディタ世界"), World))
	{
		return false;
	}

	//----------------------------------------------------------
	// (a) 新規配置アクターは PostLoad 無しで基線が確立され、直接 Resize 可能
	//----------------------------------------------------------
	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}
	TestTrue(TEXT("新規配置で基線（SavedMapWidth）が確立済み"), Map->SavedMapWidth > 0);
	TestTrue(TEXT("新規配置で基線（SavedMapHeight）が確立済み"), Map->SavedMapHeight > 0);
	TestEqual(TEXT("新規配置直後の GridData 数"), Map->GridData.Num(), Map->MapWidth * Map->MapHeight);

	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 6);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);
	TestEqual(TEXT("新規配置アクターの直接 Resize 6x4"), Map->GridData.Num(), 24);

	//----------------------------------------------------------
	// (b) サイズ変更で既存レイアウトがクリアされない
	//----------------------------------------------------------
	TestTrue(TEXT("(1,1) 設定"), SetTile(Map, 1, 1, ETileType::DirUp, 0));
	TestTrue(TEXT("(5,3) 設定"), SetTile(Map, 5, 3, ETileType::GoalP2, 0));

	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 7);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 5);
	TestEqual(TEXT("拡大後 7x5 の GridData 数"), Map->GridData.Num(), 35);
	TestEqual(TEXT("(1,1) 型は保持"), TypeAt(Map, 1, 1), static_cast<uint8>(ETileType::DirUp));
	TestEqual(TEXT("(5,3) 型は保持"), TypeAt(Map, 5, 3), static_cast<uint8>(ETileType::GoalP2));

	//----------------------------------------------------------
	// (c) Edit_X / Edit_Y / Edit_TileType の変更は自動適用されない
	//----------------------------------------------------------
	TestTrue(TEXT("(2,2) 設定"), SetTile(Map, 2, 2, ETileType::DirUp, 0));

	Map->Edit_X = 2;
	Map->Edit_Y = 2;
	Map->Edit_TileType = ETileType::Empty;
	if (FProperty* EditXProperty = FindFProperty<FProperty>(AMapManager::StaticClass(), GET_MEMBER_NAME_CHECKED(AMapManager, Edit_X)))
	{
		FPropertyChangedEvent EditEvent(EditXProperty);
		Map->PostEditChangeProperty(EditEvent);
	}
	else
	{
		AddError(TEXT("Edit_X プロパティが見つからない"));
	}
	TestEqual(TEXT("Edit_* 変更は自動適用されない"), TypeAt(Map, 2, 2), static_cast<uint8>(ETileType::DirUp));

	//----------------------------------------------------------
	// (d) TileSize 変更は位置のみ変え、型・セル数を保持する
	//----------------------------------------------------------
	const FMapTileData* TileBefore = GetTile(Map, 1, 1);
	if (!TileBefore)
	{
		AddError(TEXT("(1,1) が存在しないため TileSize 検証を継続できない"));
		return false;
	}
	const FVector LocationBefore = TileBefore->WorldLocation;
	const float TileSizeBefore = Map->TileSize;

	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, TileSize), 250.f);

	// 期待位置 = 変更前位置 + (新TileSize - 旧TileSize) * セル座標（アクター位置は不変のため）
	const FVector ExpectedLocation = LocationBefore
		+ FVector(1 * (250.f - TileSizeBefore), 1 * (250.f - TileSizeBefore), 0.f);

	const FMapTileData* TileAfter = GetTile(Map, 1, 1);
	TestTrue(TEXT("(1,1) WorldLocation は TileSize に追従"),
		TileAfter && TileAfter->WorldLocation.Equals(ExpectedLocation, 0.01f));
	TestFalse(TEXT("TileSize 変更前後で位置は変化する"),
		TileAfter && TileAfter->WorldLocation.Equals(LocationBefore, 0.01f));
	TestEqual(TEXT("(1,1) 型は保持"), TypeAt(Map, 1, 1), static_cast<uint8>(ETileType::DirUp));
	TestEqual(TEXT("TileSize 変更後もセル数は不変"), Map->GridData.Num(), 35);
	TestTrue(TEXT("成功した再構築後に SavedTileSize が更新される"), FMath::IsNearlyEqual(Map->SavedTileSize, 250.f));

	return true;
}

/**
 * FScopedTransaction + GEditor の Undo / Redo を検証する。
 * ・同面積 5x4 → 4x5 の変更で XY レイアウトを保持する
 * ・Undo 後に型・WorldLocation・床 HISM インスタンス位置が元へ復元される
 * ・Redo 後に再び 4x5 へ遷移する
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchTransactionUndoRedo,
	"AnimalGatherer.MapWorkbench.TransactionUndoRedo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchTransactionUndoRedo::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("検証：同面積 5x4→4x5 変更のトランザクション Undo/Redo で型・座標・床インスタンス位置が正しく復元される"));

	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	if (!TestNotNull(TEXT("一時エディタ世界"), World))
	{
		return false;
	}

	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}

	// 基盤を 5x4（同面積テストのため）へ整形
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 5);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);

	TestTrue(TEXT("(4,3) 設定"), SetTile(Map, 4, 3, ETileType::Spawn, 0));
	TestTrue(TEXT("(0,0) 設定"), SetTile(Map, 0, 0, ETileType::GoalP1, 0));

	//----------------------------------------------------------
	// 1. トランザクション内で 5x4 → 4x5（同面積 20 セル）
	//----------------------------------------------------------
	{
		FScopedTransaction Transaction(NSLOCTEXT("MapWorkbenchTests", "Resize54to45", "MapWorkbench Resize 5x4 to 4x5"));
		Map->Modify();
		SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 4);
		SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 5);
	}

	// 4x5 の検証：GridData 数は同面積でも 20、(4,3) は X 越界で退避、(0,0) は保持
	TestEqual(TEXT("4x5 の GridData 数"), Map->GridData.Num(), 20);
	TestEqual(TEXT("MapWidth は 4"), Map->MapWidth, 4);
	TestTrue(TEXT("(4,3) は越界退避されている"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(4, 3)
			&& Record.TileData.TileType == ETileType::Spawn;
	}));
	TestEqual(TEXT("(0,0) 型は保持"), TypeAt(Map, 0, 0), static_cast<uint8>(ETileType::GoalP1));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("4x5 再構築後"));

	//----------------------------------------------------------
	// 2. Undo：5x4 へ復元（同面積ゆえインスタンス数は同じでも位置が変わる）
	//----------------------------------------------------------
	TestTrue(TEXT("UndoTransaction の実行"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo 後 MapWidth は 5"), Map->MapWidth, 5);
	TestEqual(TEXT("Undo 後 MapHeight は 4"), Map->MapHeight, 4);
	TestEqual(TEXT("Undo 後 GridData 数"), Map->GridData.Num(), 20);
	TestEqual(TEXT("Undo 後 (4,3) は Spawn に復元"), TypeAt(Map, 4, 3), static_cast<uint8>(ETileType::Spawn));
	TestEqual(TEXT("Undo 後 (0,0) は GoalP1 のまま"), TypeAt(Map, 0, 0), static_cast<uint8>(ETileType::GoalP1));
	TestEqual(TEXT("Undo 後の越界退避は空"), Map->OutOfBoundsTiles.Num(), 0);

	// Undo 後の WorldLocation は 5 幅の格子へ復元されている事
	const FVector ExpectedWorldLocation43 = Map->GetActorLocation() + FVector(4 * Map->TileSize, 3 * Map->TileSize, 0.f);
	const FMapTileData* Tile43AfterUndo = GetTile(Map, 4, 3);
	TestTrue(TEXT("Undo 後 (4,3) WorldLocation 復元"),
		Tile43AfterUndo && Tile43AfterUndo->WorldLocation.Equals(ExpectedWorldLocation43, 0.01f));

	// Undo 後の床 HISM は 5 幅基準の位置へ完全再構築されている事（同面積でも実位置が変わる）
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Undo 後"));

	//----------------------------------------------------------
	// 3. Redo：再び 4x5 へ遷移する
	//----------------------------------------------------------
	TestTrue(TEXT("RedoTransaction の実行"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo 後 MapWidth は 4"), Map->MapWidth, 4);
	TestEqual(TEXT("Redo 後 GridData 数"), Map->GridData.Num(), 20);
	TestTrue(TEXT("Redo 後 (4,3) は再び越界退避"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(4, 3);
	}));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Redo 後"));

	return true;
}

/**
 * 出生タイルのバインド操作を検証する。
 * ・バインド時の SpawnLocation 同期と高さオフセット・SpawnRotation 保持
 * ・重複バインド / 同一セル重複 / NaN HeightOffset の拒否
 * ・移動時の旧セル解消・他 Spawner 非干渉・同座標移動の拒否
 * ・縮小→越界退避→界内移動で退避記録が消費され、拡大で旧出生セルが復活しない
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchSpawnTileBindings,
	"AnimalGatherer.MapWorkbench.SpawnTileBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchSpawnTileBindings::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("検証：出生タイルのバインド／移動の制約と、越界退避した出生セルの復活防止"));

	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	if (!TestNotNull(TEXT("一時エディタ世界"), World))
	{
		return false;
	}

	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 4);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);

	TestTrue(TEXT("(2,2) を Spawn へ"), SetTile(Map, 2, 2, ETileType::Spawn, 0));
	TestTrue(TEXT("(0,0) を GoalP1 へ"), SetTile(Map, 0, 0, ETileType::GoalP1, 0));

	AAnimalSpawner* SpawnerA = SpawnTransactionalActor<AAnimalSpawner>(World);
	AAnimalSpawner* SpawnerB = SpawnTransactionalActor<AAnimalSpawner>(World);
	if (!TestNotNull(TEXT("SpawnerA"), SpawnerA) || !TestNotNull(TEXT("SpawnerB"), SpawnerB))
	{
		return false;
	}
	SpawnerA->SpawnRotation = FRotator(0.f, 90.f, 0.f);
	SpawnerB->SpawnLocation = FVector(99999.f, 88888.f, 777.f);

	const FVector ExpectedSpawnA22 = Map->GetActorLocation() + FVector(200.f, 200.f, 50.f);

	//----------------------------------------------------------
	// 1. 拒否系：NaN 高さオフセット・重複バインド・同一セル重複
	//----------------------------------------------------------
	TestFalse(TEXT("NaN HeightOffset は拒否"), Map->BindSpawnTile(SpawnerB, FIntPoint(2, 2), std::numeric_limits<float>::quiet_NaN()));
	TestEqual(TEXT("NaN 拒否後もバインド無し"), Map->SpawnTileBindings.Num(), 0);

	TestTrue(TEXT("SpawnerA を (2,2) へバインド"), Map->BindSpawnTile(SpawnerA, FIntPoint(2, 2), 50.f));
	TestEqual(TEXT("バインドは 1 件"), Map->SpawnTileBindings.Num(), 1);
	TestTrue(TEXT("バインド後 SpawnLocation はセル中心+高さ"), SpawnerA->SpawnLocation.Equals(ExpectedSpawnA22, 0.01f));
	TestTrue(TEXT("SpawnRotation は保持"), SpawnerA->SpawnRotation.Equals(FRotator(0.f, 90.f, 0.f)));

	TestFalse(TEXT("同一 Spawner の再バインドは拒否"), Map->BindSpawnTile(SpawnerA, FIntPoint(2, 2), 50.f));
	TestFalse(TEXT("同一セルへの他 Spawner バインドは拒否"), Map->BindSpawnTile(SpawnerB, FIntPoint(2, 2), 10.f));
	TestEqual(TEXT("拒否後もバインドは 1 件"), Map->SpawnTileBindings.Num(), 1);

	//----------------------------------------------------------
	// 2. 移動：旧セル解消・高さ保持・他 Spawner 非干渉・同座標拒否
	//----------------------------------------------------------
	TestTrue(TEXT("(3,2) へ移動"), Map->MoveSpawnTile(SpawnerA, FIntPoint(3, 2)));
	TestEqual(TEXT("旧セル (2,2) は Empty"), TypeAt(Map, 2, 2), static_cast<uint8>(ETileType::Empty));
	TestEqual(TEXT("新セル (3,2) は Spawn"), TypeAt(Map, 3, 2), static_cast<uint8>(ETileType::Spawn));
	TestEqual(TEXT("新セル (3,2) の所有者は 0"), OwnerAt(Map, 3, 2), static_cast<uint8>(0));
	TestTrue(TEXT("移動後 SpawnLocation は (3,2) 中心+高さ 50"),
		SpawnerA->SpawnLocation.Equals(Map->GetActorLocation() + FVector(300.f, 200.f, 50.f), 0.01f));
	TestTrue(TEXT("移動後も SpawnRotation は保持"), SpawnerA->SpawnRotation.Equals(FRotator(0.f, 90.f, 0.f)));
	TestTrue(TEXT("他 Spawner の SpawnLocation は不変"), SpawnerB->SpawnLocation.Equals(FVector(99999.f, 88888.f, 777.f), 0.01f));
	TestFalse(TEXT("同一座標への再移動は拒否"), Map->MoveSpawnTile(SpawnerA, FIntPoint(3, 2)));

	//----------------------------------------------------------
	// 3. 縮小 4x4 → 2x2：(3,2) の Spawn は越界退避、バインドは存続
	//----------------------------------------------------------
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 2);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 2);
	TestEqual(TEXT("縮小後 GridData 数"), Map->GridData.Num(), 4);
	TestTrue(TEXT("(3,2) が越界退避されている"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(3, 2)
			&& Record.TileData.TileType == ETileType::Spawn;
	}));
	TestEqual(TEXT("縮小後もバインドは存続"), Map->SpawnTileBindings.Num(), 1);

	// 界内移動で他セルを誤って消さない事を確認するための検証セル
	TestTrue(TEXT("(1,0) を矢印へ"), SetTile(Map, 1, 0, ETileType::DirUp, 0));

	//----------------------------------------------------------
	// 4. 界内へ移動：越界退避記録の旧 Spawn は消費される
	//----------------------------------------------------------
	TestTrue(TEXT("(1,1) へ移動"), Map->MoveSpawnTile(SpawnerA, FIntPoint(1, 1)));
	TestEqual(TEXT("移動先 (1,1) は Spawn"), TypeAt(Map, 1, 1), static_cast<uint8>(ETileType::Spawn));
	TestFalse(TEXT("越界退避から (3,2) の Spawn 記録は削除済み"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(3, 2);
	}));
	TestEqual(TEXT("移動しても (1,0) の矢印は無傷"), TypeAt(Map, 1, 0), static_cast<uint8>(ETileType::DirUp));
	TestTrue(TEXT("移動後も SpawnRotation は保持（2 回目）"), SpawnerA->SpawnRotation.Equals(FRotator(0.f, 90.f, 0.f)));

	//----------------------------------------------------------
	// 5. 再拡大 2x2 → 4x4：移動済みの旧出生セル (3,2) は復活しない
	//----------------------------------------------------------
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 4);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);
	TestEqual(TEXT("再拡大後 GridData 数"), Map->GridData.Num(), 16);
	TestEqual(TEXT("(3,2) は復活しない"), TypeAt(Map, 3, 2), static_cast<uint8>(ETileType::Empty));
	TestEqual(TEXT("(1,1) の Spawn は維持"), TypeAt(Map, 1, 1), static_cast<uint8>(ETileType::Spawn));
	TestEqual(TEXT("(0,0) の GoalP1 は維持"), TypeAt(Map, 0, 0), static_cast<uint8>(ETileType::GoalP1));

	return true;
}

/**
 * FObjectWriter / FObjectReader によるメモリシリアライズ復元を検証する。
 * ・越界退避記録・ツール生成アクター参照・出生バインドが保存→改変→復元で再現される
 * ・GridData・盤面サイズも復元される
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchMemorySerializationRoundtrip,
	"AnimalGatherer.MapWorkbench.MemorySerializationRoundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchMemorySerializationRoundtrip::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("検証：FObjectWriter/FObjectReader のメモリ往復でワークベンチ関連データが復元される"));

	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	if (!TestNotNull(TEXT("一時エディタ世界"), World))
	{
		return false;
	}

	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	AAnimalSpawner* Spawner = SpawnTransactionalActor<AAnimalSpawner>(World);
	if (!TestNotNull(TEXT("MapManager"), Map) || !TestNotNull(TEXT("AnimalSpawner"), Spawner))
	{
		return false;
	}

	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 4);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);
	TestTrue(TEXT("(2,2) を Spawn へ"), SetTile(Map, 2, 2, ETileType::Spawn, 0));
	TestTrue(TEXT("バインド作成"), Map->BindSpawnTile(Spawner, FIntPoint(2, 2), 25.f));

	// 縮小で越界退避記録を作成：(2,2) の Spawn と (3,3) の矢印が退避される
	TestTrue(TEXT("(3,3) を矢印へ"), SetTile(Map, 3, 3, ETileType::DirLeft, 1));
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 2);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 2);
	TestEqual(TEXT("縮小で越界退避は 2 件"), Map->OutOfBoundsTiles.Num(), 2);

	// ツール生成アクター参照を設定（実体はテスト内でスポーンした Spawner を使用）
	Map->ToolCreatedActors.Add(Spawner);

	//----------------------------------------------------------
	// スナップショット取得 → 改変 → 復元
	//----------------------------------------------------------
	TArray<uint8> MemoryBytes;
	{
		FObjectWriter Writer(Map, MemoryBytes);
		TestTrue(TEXT("シリアライズでバイト列が得られた"), MemoryBytes.Num() > 0);
	}

	// 改変：全ワークベンチデータを破壊する
	const int32 RecordedByteCount = MemoryBytes.Num();
	Map->OutOfBoundsTiles.Empty();
	Map->SpawnTileBindings.Empty();
	Map->ToolCreatedActors.Empty();
	Map->GridData.Empty();
	Map->MapWidth = 9;
	Map->MapHeight = 9;
	Spawner->SpawnLocation = FVector::ZeroVector;
	TestEqual(TEXT("改変後は退避記録が空"), Map->OutOfBoundsTiles.Num(), 0);

	{
		FObjectReader Reader(Map, MemoryBytes);
	}

	//----------------------------------------------------------
	// 復元検証
	//----------------------------------------------------------
	TestEqual(TEXT("バイト列は不変"), MemoryBytes.Num(), RecordedByteCount);
	TestEqual(TEXT("MapWidth 復元"), Map->MapWidth, 2);
	TestEqual(TEXT("MapHeight 復元"), Map->MapHeight, 2);
	TestEqual(TEXT("GridData 数復元"), Map->GridData.Num(), 4);

	TestEqual(TEXT("越界退避記録の件数復元"), Map->OutOfBoundsTiles.Num(), 2);
	TestTrue(TEXT("退避記録 (2,2) Spawn 復元"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(2, 2)
			&& Record.TileData.TileType == ETileType::Spawn;
	}));
	TestTrue(TEXT("退避記録 (3,3) 矢印+所有者復元"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(3, 3)
			&& Record.TileData.TileType == ETileType::DirLeft
			&& Record.TileData.OwnerPlayerId == 1;
	}));

	TestEqual(TEXT("出生バインドの件数復元"), Map->SpawnTileBindings.Num(), 1);
	if (Map->SpawnTileBindings.Num() == 1)
	{
		const FSpawnTileBinding& Binding = Map->SpawnTileBindings[0];
		TestTrue(TEXT("バインドの Spawner 参照復元"), Binding.Spawner == Spawner);
		TestTrue(TEXT("バインドの座標復元 (2,2)"), Binding.GridCoords == FIntPoint(2, 2));
		TestTrue(TEXT("バインドの高さオフセット復元"), FMath::IsNearlyEqual(Binding.HeightOffset, 25.f));
	}

	TestEqual(TEXT("ツール生成アクター参照の件数復元"), Map->ToolCreatedActors.Num(), 1);
	if (Map->ToolCreatedActors.Num() == 1)
	{
		TestTrue(TEXT("ツール生成アクターの参照先復元"), Map->ToolCreatedActors[0].Get() == Spawner);
	}

	return true;
}

/**
 * パネルWidget（SMapWorkbench）の構築・破棄がシーンデータを書き換えない事を検証する。
 * ・SNew(SMapWorkbench) による実Widget構築のみを行い、Window表示・タブ登録・入力送出は行わない
 * ・GridData・盤面パラメータ・越界退避・出生バインド・保存基線・パッケージDirty標識の不変を確認する
 * ・マウス操作やディスク保存は検証対象外（構築/破棄の副作用のみを検証する）
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchPanelConstructionReadOnly,
	"AnimalGatherer.MapWorkbench.PanelConstructionReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchPanelConstructionReadOnly::RunTest(const FString& Parameters)
{
	AddInfo(TEXT("検証：パネルWidgetの構築と解放だけでシーンデータとDirty標識が変わらない事を確認（マウス操作・保存は対象外）"));

	// SlateApplication 未初期化ではWidget構築を検証できないため、静黙スキップせず失敗扱いにする
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("FSlateApplication が未初期化のためパネル構築検証を実行できません"));
		return false;
	}

	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	if (!TestNotNull(TEXT("一時エディタ世界"), World))
	{
		return false;
	}

	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 4);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 4);
	TestTrue(TEXT("(1,1) を Spawn へ"), SetTile(Map, 1, 1, ETileType::Spawn, 0));

	AAnimalSpawner* Spawner = SpawnTransactionalActor<AAnimalSpawner>(World);
	if (!TestNotNull(TEXT("AnimalSpawner"), Spawner))
	{
		return false;
	}
	TestTrue(TEXT("バインド作成"), Map->BindSpawnTile(Spawner, FIntPoint(1, 1), 10.f));

	//----------------------------------------------------------
	// スナップショット（GridData・パラメータ・越界退避・バインド・基線・Dirty標識）
	//----------------------------------------------------------
	const TArray<FMapTileData> GridDataBefore = Map->GridData;
	const int32 MapWidthBefore = Map->MapWidth;
	const int32 MapHeightBefore = Map->MapHeight;
	const float TileSizeBefore = Map->TileSize;
	const int32 SavedWidthBefore = Map->SavedMapWidth;
	const int32 SavedHeightBefore = Map->SavedMapHeight;
	const float SavedTileSizeBefore = Map->SavedTileSize;
	const TArray<FOutOfBoundsTileRecord> OutOfBoundsBefore = Map->OutOfBoundsTiles;
	const TArray<FSpawnTileBinding> BindingsBefore = Map->SpawnTileBindings;
	const bool bPackageDirtyBefore = Map->GetPackage()->IsDirty();

	//----------------------------------------------------------
	// 実Widgetを構築して直ちに解放する（Window表示・タブ登録・入力送出は行わない）
	//----------------------------------------------------------
	{
		TSharedPtr<SMapWorkbench> Panel = SNew(SMapWorkbench);
		if (!Panel.IsValid())
		{
			AddError(TEXT("パネルWidgetの構築に失敗しました"));
			return false;
		}
	}

	//----------------------------------------------------------
	// 構築・破棄の前後でシーンデータとDirty標識が不変である事（USTRUCTにoperator==が無いため要素比較）
	//----------------------------------------------------------
	TestEqual(TEXT("GridData 件数不変"), Map->GridData.Num(), GridDataBefore.Num());
	bool bGridDataIdentical = (Map->GridData.Num() == GridDataBefore.Num());
	for (int32 i = 0; bGridDataIdentical && i < GridDataBefore.Num(); ++i)
	{
		bGridDataIdentical = Map->GridData[i].TileType == GridDataBefore[i].TileType
			&& Map->GridData[i].OwnerPlayerId == GridDataBefore[i].OwnerPlayerId
			&& Map->GridData[i].WorldLocation.Equals(GridDataBefore[i].WorldLocation, 0.01f);
	}
	TestTrue(TEXT("GridData 内容不変"), bGridDataIdentical);

	TestEqual(TEXT("MapWidth 不変"), Map->MapWidth, MapWidthBefore);
	TestEqual(TEXT("MapHeight 不変"), Map->MapHeight, MapHeightBefore);
	TestTrue(TEXT("TileSize 不変"), FMath::IsNearlyEqual(Map->TileSize, TileSizeBefore));
	TestEqual(TEXT("保存基線（幅）不変"), Map->SavedMapWidth, SavedWidthBefore);
	TestEqual(TEXT("保存基線（高さ）不変"), Map->SavedMapHeight, SavedHeightBefore);
	TestTrue(TEXT("保存基線（TileSize）不変"), FMath::IsNearlyEqual(Map->SavedTileSize, SavedTileSizeBefore));

	TestEqual(TEXT("越界退避の件数不変"), Map->OutOfBoundsTiles.Num(), OutOfBoundsBefore.Num());
	bool bOutOfBoundsIdentical = (Map->OutOfBoundsTiles.Num() == OutOfBoundsBefore.Num());
	for (int32 i = 0; bOutOfBoundsIdentical && i < OutOfBoundsBefore.Num(); ++i)
	{
		bOutOfBoundsIdentical = Map->OutOfBoundsTiles[i].GridCoords == OutOfBoundsBefore[i].GridCoords
			&& Map->OutOfBoundsTiles[i].TileData.TileType == OutOfBoundsBefore[i].TileData.TileType;
	}
	TestTrue(TEXT("越界退避の内容不変"), bOutOfBoundsIdentical);

	TestEqual(TEXT("出生バインドの件数不変"), Map->SpawnTileBindings.Num(), BindingsBefore.Num());
	bool bBindingsIdentical = (Map->SpawnTileBindings.Num() == BindingsBefore.Num());
	for (int32 i = 0; bBindingsIdentical && i < BindingsBefore.Num(); ++i)
	{
		bBindingsIdentical = Map->SpawnTileBindings[i].Spawner == BindingsBefore[i].Spawner
			&& Map->SpawnTileBindings[i].GridCoords == BindingsBefore[i].GridCoords
			&& FMath::IsNearlyEqual(Map->SpawnTileBindings[i].HeightOffset, BindingsBefore[i].HeightOffset);
	}
	TestTrue(TEXT("出生バインドの内容不変"), bBindingsIdentical);

	TestEqual(TEXT("パッケージDirty標識不変"), static_cast<uint8>(Map->GetPackage()->IsDirty()), static_cast<uint8>(bPackageDirtyBefore));

	return true;
}

// 2026.10.23 Lee start
/** @brief 空洞への切替・復元・全空洞・構築経路を実際の床インスタンスで検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchHoleFloor,
	"AnimalGatherer.MapWorkbench.HoleFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchHoleFloor::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	AMapManager* Map = SpawnTransactionalActor<AMapManager>(ScopedWorld->GetWorld());
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 3);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 2);
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Initial floor"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Engine test mesh"), Cube))
	{
		return false;
	}
	Map->HISM_BaseFloor->SetStaticMesh(Cube);
	Map->P2DirectionVisuals[ETileType::DirUp]->SetStaticMesh(Cube);
	const FVector BeforeLocation = Map->GridData[1].WorldLocation;
	Map->SetTileData(1, 0, ETileType::DirUp, 1);
	TestEqual(TEXT("P2 arrow instance exists before conversion"), Map->P2DirectionVisuals[ETileType::DirUp]->GetInstanceCount(), 1);
	Map->SetTileData(1, 0, ETileType::Hole, 1);
	TestEqual(TEXT("Hole enum appended"), static_cast<uint8>(ETileType::Hole), uint8(8));
	TestTrue(TEXT("Hole remains valid coordinate"), Map->IsValidCoord_Implementation(FIntPoint(1, 0)));
	TestEqual(TEXT("Hole is readable"), static_cast<uint8>(Map->GetCellState_Implementation(FIntPoint(1, 0))), uint8(8));
	TestTrue(TEXT("World location retained"), Map->GridData[1].WorldLocation.Equals(BeforeLocation));
	TestEqual(TEXT("Non-direction owner normalized"), Map->GridData[1].OwnerPlayerId, uint8(0));
	TestEqual(TEXT("Grid data retained"), Map->GridData.Num(), 6);
	TestFalse(TEXT("Hole has no state component"), Map->StateVisuals.Contains(ETileType::Hole));
	TestEqual(TEXT("Old P2 arrow removed"), Map->P2DirectionVisuals[ETileType::DirUp]->GetInstanceCount(), 0);
	TestFalse(TEXT("FIFO clearing cannot fill hole"), Map->ClearArrowIfOwned(1, 0, 0));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("One hole"));
	Map->SetTileData(1, 0, ETileType::Empty);
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Erased hole"));

	/** @brief Details の明示編集も床を同期する。 */
	Map->Edit_X = 1;
	Map->Edit_Y = 0;
	Map->Edit_TileType = ETileType::Hole;
	Map->ApplyTileEdit();
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Details hole"));
	Map->Edit_TileType = ETileType::Empty;
	Map->ApplyTileEdit();
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Details restored floor"));

	/** @brief 構築経路は直接変更されたデータからも床を再構築する。 */
	Map->GridData[1].TileType = ETileType::Hole;
	Map->OnConstruction(Map->GetActorTransform());
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Editor construction"));
	UWorld* World = ScopedWorld->GetWorld();
	const EWorldType::Type PreviousWorldType = World->WorldType;
	World->WorldType = EWorldType::Game;
	Map->GridData[2].TileType = ETileType::Hole;
	Map->OnConstruction(Map->GetActorTransform());
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Runtime construction"));
	World->WorldType = PreviousWorldType;

	for (int32 Y = 0; Y < Map->MapHeight; ++Y)
	{
		for (int32 X = 0; X < Map->MapWidth; ++X)
		{
			Map->SetTileData(X, Y, ETileType::Hole);
		}
	}
	Map->UpdateMapVisuals();
	TestEqual(TEXT("All holes have zero floor instances"), Map->HISM_BaseFloor->GetInstanceCount(), 0);
	Map->SetTileData(2, 1, ETileType::DirRight);
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Arrow restores one floor"));
	Map->GridData.Empty();
	Map->UpdateMapVisuals();
	TestEqual(TEXT("Empty data clears stale floor"), Map->HISM_BaseFloor->GetInstanceCount(), 0);
	return true;
}

/** @brief 一筆相当の取引・縮図退避復元・保存往復で空洞のデータと床が一致することを検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchHolePersistence,
	"AnimalGatherer.MapWorkbench.HolePersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchHolePersistence::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	AMapManager* Map = SpawnTransactionalActor<AMapManager>(ScopedWorld->GetWorld());
	if (!TestNotNull(TEXT("MapManager"), Map))
	{
		return false;
	}
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 4);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 3);
	{
		FScopedTransaction Transaction(NSLOCTEXT("MapWorkbenchTests", "HoleStroke", "Hole stroke"));
		Map->Modify();
		Map->SetTileData(0, 0, ETileType::Hole);
		Map->SetTileData(1, 0, ETileType::Hole);
		Map->SetTileData(3, 2, ETileType::Hole);
	}
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Stroke"));
	TestTrue(TEXT("Undo stroke"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores all three cells"), Map->HISM_BaseFloor->GetInstanceCount(), 12);
	TestTrue(TEXT("Redo stroke"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo removes all three floors"), Map->HISM_BaseFloor->GetInstanceCount(), 9);
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Redo"));
	TestTrue(TEXT("Shrink map"), Map->ResizeGridPreservingLayout(2, 2, true));
	TestTrue(TEXT("Hole is archived"), Map->OutOfBoundsTiles.ContainsByPredicate([](const FOutOfBoundsTileRecord& Record)
	{
		return Record.GridCoords == FIntPoint(3, 2) && Record.TileData.TileType == ETileType::Hole;
	}));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Shrunk"));
	TArray<uint8> Bytes;
	{
		FObjectWriter Writer(Map, Bytes);
	}
	Map->SetTileData(0, 0, ETileType::Empty);
	Map->OutOfBoundsTiles.Empty();
	{
		FObjectReader Reader(Map, Bytes);
	}
	Map->RebuildFloorAndPositions(false);
	TestEqual(TEXT("Serialized hole restored"), TypeAt(Map, 0, 0), uint8(ETileType::Hole));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Serialization"));
	TestTrue(TEXT("Expand map"), Map->ResizeGridPreservingLayout(4, 3, true));
	TestEqual(TEXT("Archived hole restored"), TypeAt(Map, 3, 2), uint8(ETileType::Hole));
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, TileSize), 175.f);
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Resized tiles"));

	/** @brief 関連付き出生マスの保護を共通書き込みへの委譲後も保持する。 */
	AAnimalSpawner* Spawner = SpawnTransactionalActor<AAnimalSpawner>(ScopedWorld->GetWorld());
	if (!TestNotNull(TEXT("Spawner"), Spawner))
	{
		return false;
	}
	Map->SetTileData(2, 1, ETileType::Spawn);
	TestTrue(TEXT("Bind spawn"), Map->BindSpawnTile(Spawner, FIntPoint(2, 1), 0.f));
	Map->Edit_X = 2;
	Map->Edit_Y = 1;
	Map->Edit_TileType = ETileType::Hole;
	Map->ApplyTileEdit();
	TestEqual(TEXT("Bound spawn cannot become a hole"), TypeAt(Map, 2, 1), uint8(ETileType::Spawn));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Protected spawn"));
	return true;
}

/** @brief 実際の配置APIを通し、穴への拒否が既存矢印とFIFO順序を変えないことを検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchHolePlacement,
	"AnimalGatherer.MapWorkbench.HolePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchHolePlacement::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	UWorld* World = ScopedWorld->GetWorld();
	AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
	AAnimalGatherPlayerController* Controller = SpawnTransactionalActor<AAnimalGatherPlayerController>(World);
	ACursorPawn* Cursor = SpawnTransactionalActor<ACursorPawn>(World);
	if (!TestNotNull(TEXT("Map"), Map) || !TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Cursor"), Cursor))
	{
		return false;
	}
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 5);
	SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 2);
	Controller->SetMapManager(Map);
	Controller->Possess(Cursor);
	if (!TestTrue(TEXT("Controller possesses cursor"), Controller->GetPawn() == Cursor))
	{
		return false;
	}
	Cursor->PlayerID = 0;
	Cursor->GridY = 0;
	for (int32 X = 0; X < 3; ++X)
	{
		Cursor->GridX = X;
		Controller->PlaceDirection(ETileType::DirUp);
		TestEqual(TEXT("Initial arrow placed"), TypeAt(Map, X, 0), uint8(ETileType::DirUp));
	}
	Map->SetTileData(3, 0, ETileType::Hole);
	Cursor->GridX = 3;
	Controller->PlaceDirection(ETileType::DirLeft);
	TestEqual(TEXT("Hole cannot be filled by player"), TypeAt(Map, 3, 0), uint8(ETileType::Hole));
	for (int32 X = 0; X < 3; ++X)
	{
		TestEqual(TEXT("Rejection retains previous arrows"), TypeAt(Map, X, 0), uint8(ETileType::DirUp));
	}
	Cursor->GridX = 4;
	Controller->PlaceDirection(ETileType::DirDown);
	TestEqual(TEXT("Next placement evicts original oldest arrow"), TypeAt(Map, 0, 0), uint8(ETileType::Empty));
	TestEqual(TEXT("Second arrow retained"), TypeAt(Map, 1, 0), uint8(ETileType::DirUp));
	TestEqual(TEXT("Third arrow retained"), TypeAt(Map, 2, 0), uint8(ETileType::DirUp));
	TestEqual(TEXT("Fourth valid arrow placed"), TypeAt(Map, 4, 0), uint8(ETileType::DirDown));
	VerifyBaseFloorInstanceGrid(*this, Map, TEXT("Player cannot restore hole floor"));
	return true;
}
// 2026.10.23 Lee end

// 2026.10.23 Lee start
namespace
{
	/** @brief 平行移動済みの一時マップを生成し、座標原点への依存も検証する。 */
	AMapManager* MakeAnimalHoleMap(UWorld* World)
	{
		AMapManager* Map = SpawnTransactionalActor<AMapManager>(World);
		if (Map)
		{
			SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapWidth), 6);
			SetPropertyViaDetails(Map, GET_MEMBER_NAME_CHECKED(AMapManager, MapHeight), 5);
			Map->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
			Map->RebuildFloorAndPositions(false);
			Map->SetTileData(2, 2, ETileType::Hole);
		}
		return Map;
	}

	/** @brief 実際のTickから検証する動物。音・描画の結果は無描画テストの対象外とする。 */
	AAnimalBase* MakeHoleAnimal(AMapManager* Map, const FVector& RelativePosition, const FVector& Direction)
	{
		AAnimalBase* Animal = SpawnTransactionalActor<AAnimalBase>(Map->GetWorld());
		if (Animal)
		{
			Animal->SetMapActor(Map);
			Animal->SetActorLocation(Map->GetActorLocation() + RelativePosition);
			Animal->SetMoveDirection(Direction);
			Animal->MoveSpeed = 1000.0f;
			Animal->FallGravity = 1000.0f;
			Animal->bFallOffBoard = false;
			Animal->bSplashOnWater = false;
			Animal->GoalNiagaraVFX = nullptr;
			Animal->GoalSound = nullptr;
		}
		return Animal;
	}
}

/** @brief 四方向の入口で停止し、次Tickで慣性付き落下へ入ることを検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchAnimalHoleEntry,
	"AnimalGatherer.MapWorkbench.AnimalHoleEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchAnimalHoleEntry::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	/** @brief エディタ世界でも実際のBlueprintNativeEventインターフェースを実行し、終了時に許可状態を復元する。 */
	FEditorScriptExecutionGuard ScriptGuard;
	AMapManager* Map = MakeAnimalHoleMap(ScopedWorld->GetWorld());
	if (!TestNotNull(TEXT("Map"), Map)) { return false; }
	struct FEntryCase { FVector Start; FVector Direction; FVector Entry; };
	const FEntryCase Cases[] =
	{
		{ FVector(100, 200, 100), FVector(1, 0, 0), FVector(150, 200, 100) },
		{ FVector(300, 200, 100), FVector(-1, 0, 0), FVector(250, 200, 100) },
		{ FVector(200, 100, 100), FVector(0, 1, 0), FVector(200, 150, 100) },
		{ FVector(200, 300, 100), FVector(0, -1, 0), FVector(200, 250, 100) }
	};
	for (const FEntryCase& Case : Cases)
	{
		AAnimalBase* Animal = MakeHoleAnimal(Map, Case.Start, Case.Direction);
		if (!TestNotNull(TEXT("Animal"), Animal)) { return false; }
		Animal->Tick(1.0f);
		const FVector Entry = Map->GetActorLocation() + Case.Entry;
		TestTrue(FString::Printf(TEXT("Large frame entry: actual=%s expected=%s speed=%f map=%s"),
			*Animal->GetActorLocation().ToString(), *Entry.ToString(), Animal->GetEffectiveMoveSpeed(), *GetNameSafe(ScopedWorld->GetWorld()->GetAuthGameMode())),
			Animal->GetActorLocation().Equals(Entry, 0.001));
		FIntPoint LastRead;
		TestTrue(TEXT("Previous center was read"), Animal->GetLastReadGridCoords(LastRead));
		TestTrue(TEXT("Hole center was not processed"), LastRead != FIntPoint(2, 2));
		Animal->Tick(0.1f);
		const FVector Fallen = Animal->GetActorLocation();
		TestTrue(TEXT("Gravity progresses after entry"), Fallen.Z < Entry.Z);
		TestTrue(TEXT("Existing horizontal inertia retained"),
			FVector::Dist2D(Fallen, Entry + Case.Direction * 100.0) < 0.001);
	}
	/** @brief 同じ距離を小さなフレームに分けても同じ入口で止まる。 */
	AAnimalBase* SmallFrames = MakeHoleAnimal(Map, FVector(100, 200, 100), FVector(1, 0, 0));
	if (!TestNotNull(TEXT("Small frame animal"), SmallFrames)) { return false; }
	SmallFrames->MoveSpeed = 2000.0f;
	SmallFrames->Tick(0.02f);
	SmallFrames->Tick(0.02f);
	TestTrue(TEXT("Higher speed and split frames use same boundary"),
		SmallFrames->GetActorLocation().Equals(Map->GetActorLocation() + FVector(150, 200, 100), 0.001));
	return true;
}

/** @brief 初期位置・整列・非軸方向・接線と角の接触を検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchAnimalHolePaths,
	"AnimalGatherer.MapWorkbench.AnimalHolePaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchAnimalHolePaths::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	FEditorScriptExecutionGuard ScriptGuard;
	AMapManager* Map = MakeAnimalHoleMap(ScopedWorld->GetWorld());
	if (!TestNotNull(TEXT("Map"), Map)) { return false; }
	struct FPathCase { FVector Start; FVector Direction; FVector Entry; };
	const FPathCase Paths[] =
	{
		{ FVector(0, 225, 100), FVector(1, 0, 0), FVector(150, 225, 100) },
		{ FVector(0, 0, 100), FVector(1, 1, 0), FVector(150, 150, 100) },
		{ FVector(400, 400, 100), FVector(-1, -1, 0), FVector(250, 250, 100) },
		{ FVector(150, 200, 100), FVector(1, 0, 0), FVector(150, 200, 100) },
		{ FVector(150, 151, 100), FVector(-1, 0, 0), FVector(150, 151, 100) }
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Paths); ++Index)
	{
		const FPathCase& Path = Paths[Index];
		AAnimalBase* Animal = MakeHoleAnimal(Map, Path.Start, Path.Direction);
		if (!TestNotNull(TEXT("Path animal"), Animal)) { return false; }
		if (Index == UE_ARRAY_COUNT(Paths) - 1)
		{
			/** @brief 大きい初期整列許容でも、整列線分上の穴を飛び越さない。 */
			Animal->DirectionReadTolerance = 100.0f;
		}
		Animal->Tick(1.0f);
		TestTrue(TEXT("Path stops at hole entry"), Animal->GetActorLocation().Equals(Map->GetActorLocation() + Path.Entry, 0.001));
		const double StartZ = Animal->GetActorLocation().Z;
		Animal->Tick(0.1f);
		TestTrue(TEXT("Path starts falling"), Animal->GetActorLocation().Z < StartZ);
	}
	Map->SetTileData(1, 2, ETileType::Empty);
	Map->SetTileData(2, 2, ETileType::Empty);
	Map->SetTileData(1, 0, ETileType::Hole);
	AAnimalBase* Corner = MakeHoleAnimal(Map, FVector(0, 0, 100), FVector(1, 1, 0));
	if (!TestNotNull(TEXT("Corner animal"), Corner)) { return false; }
	Corner->Tick(0.4f);
	TestTrue(TEXT("Touching a hole corner does not truncate movement"),
		Corner->GetActorLocation().Equals(Map->GetActorLocation() + FVector(400.0 / FMath::Sqrt(2.0), 400.0 / FMath::Sqrt(2.0), 100), 0.001));
	Corner->Tick(0.01f);
	TestEqual(TEXT("Corner touch does not start falling"), Corner->GetActorLocation().Z, Map->GetActorLocation().Z + 100.0);
	Map->SetTileData(1, 0, ETileType::Empty);
	Map->SetTileData(2, 2, ETileType::Hole);
	AAnimalBase* Tangent = MakeHoleAnimal(Map, FVector(0, 150, 100), FVector(1, 0, 0));
	AAnimalBase* Away = MakeHoleAnimal(Map, FVector(150, 200, 100), FVector(-1, 0, 0));
	if (!TestNotNull(TEXT("Tangent animal"), Tangent) || !TestNotNull(TEXT("Moving away animal"), Away)) { return false; }
	Tangent->Tick(0.4f);
	Tangent->Tick(0.01f);
	TestEqual(TEXT("Movement along boundary is not inside hole"), Tangent->GetActorLocation().Z, Map->GetActorLocation().Z + 100.0);
	Away->Tick(0.1f);
	Away->Tick(0.01f);
	TestEqual(TEXT("Leaving boundary toward solid cell is safe"), Away->GetActorLocation().Z, Map->GetActorLocation().Z + 100.0);
	return true;
}

/** @brief 実衝突が穴入口より手前の場合、落下も透過移動も起こらないことを検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchAnimalHoleBlocked,
	"AnimalGatherer.MapWorkbench.AnimalHoleBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchAnimalHoleBlocked::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = ScopedWorld->GetWorld();
	AMapManager* Map = MakeAnimalHoleMap(World);
	if (!TestNotNull(TEXT("Map"), Map)) { return false; }
	AAnimalBase* Animal = MakeHoleAnimal(Map, FVector(100, 200, 100), FVector(1, 0, 0));
	AActor* Wall = SpawnTransactionalActor<AActor>(World);
	if (!TestNotNull(TEXT("Animal"), Animal) || !TestNotNull(TEXT("Wall"), Wall)) { return false; }
	UBoxComponent* AnimalBox = NewObject<UBoxComponent>(Animal);
	AnimalBox->SetBoxExtent(FVector(5.0));
	AnimalBox->SetCollisionProfileName(TEXT("BlockAll"));
	Animal->SetRootComponent(AnimalBox);
	AnimalBox->RegisterComponent();
	Animal->SetActorLocation(Map->GetActorLocation() + FVector(100, 200, 100));
	UBoxComponent* WallBox = NewObject<UBoxComponent>(Wall);
	WallBox->SetBoxExtent(FVector(5.0, 20.0, 20.0));
	WallBox->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->SetRootComponent(WallBox);
	WallBox->RegisterComponent();
	Wall->SetActorLocation(Map->GetActorLocation() + FVector(135, 200, 100));
	Animal->Tick(0.5f);
	const FVector Blocked = Animal->GetActorLocation();
	TestTrue(TEXT("Actual sweep blocked before entry"), Blocked.X > Map->GetActorLocation().X + 100.0 && Blocked.X < Map->GetActorLocation().X + 150.0);
	Animal->Tick(0.1f);
	TestEqual(TEXT("Blocked animal does not fall"), Animal->GetActorLocation().Z, Blocked.Z);
	TestTrue(TEXT("Blocked animal is not teleported into hole"), Animal->GetActorLocation().X < Map->GetActorLocation().X + 150.0);
	return true;
}

/** @brief 穴の後ろのGoalを読まず、既存の破棄とスポナー枠回収へ接続することを検証する。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchAnimalHoleLifecycle,
	"AnimalGatherer.MapWorkbench.AnimalHoleLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchAnimalHoleLifecycle::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = ScopedWorld->GetWorld();
	AMapManager* Map = MakeAnimalHoleMap(World);
	if (!TestNotNull(TEXT("Map"), Map)) { return false; }
	AMainGameMode* Mode = SpawnTransactionalActor<AMainGameMode>(World);
	FObjectPropertyBase* Authority = FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), TEXT("AuthorityGameMode"));
	FBoolProperty* NormalFlow = FindFProperty<FBoolProperty>(AMainGameMode::StaticClass(), TEXT("bUseNormalMatchFlow"));
	if (!TestNotNull(TEXT("Game mode"), Mode) || !TestNotNull(TEXT("Authority property"), Authority) || !TestNotNull(TEXT("Flow property"), NormalFlow)) { return false; }
	/** @brief 独立テストの非通常試合に接続し、実際の得点値も確認する。 */
	NormalFlow->SetPropertyValue_InContainer(Mode, false);
	UObject* PreviousAuthority = Authority->GetObjectPropertyValue_InContainer(World);
	ON_SCOPE_EXIT { Authority->SetObjectPropertyValue_InContainer(World, PreviousAuthority); };
	Authority->SetObjectPropertyValue_InContainer(World, Mode);
	Map->SetTileData(3, 2, ETileType::GoalP1);
	AAnimalBase* Animal = MakeHoleAnimal(Map, FVector(100, 200, 100), FVector(1, 0, 0));
	if (!TestNotNull(TEXT("Animal"), Animal)) { return false; }
	Animal->Tick(1.0f);
	TestFalse(TEXT("Goal beyond hole does not destroy animal as scored"), Animal->IsActorBeingDestroyed());
	FIntPoint LastRead;
	Animal->GetLastReadGridCoords(LastRead);
	TestTrue(TEXT("Goal center was not processed"), LastRead != FIntPoint(3, 2));
	Animal->Tick(0.1f);
	Animal->GetLastReadGridCoords(LastRead);
	TestTrue(TEXT("Falling never reads the next center"), LastRead != FIntPoint(3, 2));
	TestEqual(TEXT("No P1 score through hole"), Mode->GetP1Score(), 0);
	TestEqual(TEXT("No P2 score through hole"), Mode->GetP2Score(), 0);

	AAnimalSpawner* Spawner = SpawnTransactionalActor<AAnimalSpawner>(World);
	if (!TestNotNull(TEXT("Spawner"), Spawner)) { return false; }
	Spawner->AnimalClass = AAnimalBase::StaticClass();
	Spawner->MapActor = Map;
	Spawner->SpawnLocation = Map->GetActorLocation() + FVector(200, 200, 100);
	Spawner->MaxAnimalCount = 1;
	AAnimalBase* Spawned = Cast<AAnimalBase>(Spawner->SpawnAnimal());
	if (!TestNotNull(TEXT("Spawned animal"), Spawned)) { return false; }
	Spawned->bSplashOnWater = false;
	Spawned->FallKillDepth = 10.0f;
	Spawned->Tick(0.1f);
	TestEqual(TEXT("Falling animal still occupies a slot"), Spawner->GetSpawnedAnimalCount(), 1);
	TestNull(TEXT("Cannot spawn while slot occupied"), Spawner->SpawnAnimal());
	Spawned->Tick(1.0f);
	TestTrue(TEXT("Existing kill depth destroys animal"), Spawned->IsActorBeingDestroyed() || !IsValid(Spawned));
	TestEqual(TEXT("Destroyed animal releases slot"), Spawner->GetSpawnedAnimalCount(), 0);
	TestNotNull(TEXT("Spawner can create replacement"), Spawner->SpawnAnimal());

	/** @brief 穴でない床と従来の外周設定を回帰検証する。 */
	AAnimalBase* Solid = MakeHoleAnimal(Map, FVector(0, 0, 100), FVector(1, 0, 0));
	AAnimalBase* Edge = MakeHoleAnimal(Map, FVector(-100, 100, 100), FVector(-1, 0, 0));
	AAnimalBase* EdgeDisabled = MakeHoleAnimal(Map, FVector(-100, 100, 100), FVector(-1, 0, 0));
	if (!TestNotNull(TEXT("Solid"), Solid) || !TestNotNull(TEXT("Edge"), Edge) || !TestNotNull(TEXT("EdgeDisabled"), EdgeDisabled)) { return false; }
	Edge->bFallOffBoard = true;
	Solid->Tick(0.1f);
	Solid->Tick(0.1f);
	Edge->Tick(0.1f);
	Edge->Tick(0.1f);
	EdgeDisabled->Tick(0.1f);
	EdgeDisabled->Tick(0.1f);
	TestEqual(TEXT("Solid floor remains level"), Solid->GetActorLocation().Z, Map->GetActorLocation().Z + 100.0);
	TestTrue(TEXT("Original outer fall remains enabled"), Edge->GetActorLocation().Z < Map->GetActorLocation().Z + 100.0);
	TestEqual(TEXT("Outer fall toggle retains original meaning"), EdgeDisabled->GetActorLocation().Z, Map->GetActorLocation().Z + 100.0);
	return true;
}

/** @brief 通常試合のReadyとEndedでは穴上にいても新しい落下を開始しない。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapWorkbenchAnimalHoleMatchPhase,
	"AnimalGatherer.MapWorkbench.AnimalHoleMatchPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapWorkbenchAnimalHoleMatchPhase::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> ScopedWorld = MakeTestWorld();
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = ScopedWorld->GetWorld();
	AMapManager* Map = MakeAnimalHoleMap(World);
	AMainGameMode* Mode = SpawnTransactionalActor<AMainGameMode>(World);
	FObjectPropertyBase* Authority = FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), TEXT("AuthorityGameMode"));
	FProperty* PhaseProperty = FindFProperty<FProperty>(AMainGameMode::StaticClass(), TEXT("MatchPhase"));
	if (!TestNotNull(TEXT("Map"), Map) || !TestNotNull(TEXT("Mode"), Mode) ||
		!TestNotNull(TEXT("Authority property"), Authority) || !TestNotNull(TEXT("Phase property"), PhaseProperty)) { return false; }
	UObject* PreviousAuthority = Authority->GetObjectPropertyValue_InContainer(World);
	ON_SCOPE_EXIT { Authority->SetObjectPropertyValue_InContainer(World, PreviousAuthority); };
	Authority->SetObjectPropertyValue_InContainer(World, Mode);
	for (const EMatchPhase Phase : { EMatchPhase::Ready, EMatchPhase::Ended })
	{
		*PhaseProperty->ContainerPtrToValuePtr<EMatchPhase>(Mode) = Phase;
		AAnimalBase* Animal = MakeHoleAnimal(Map, FVector(200, 200, 100), FVector(1, 0, 0));
		if (!TestNotNull(TEXT("Phase animal"), Animal)) { return false; }
		const FVector Before = Animal->GetActorLocation();
		Animal->Tick(0.5f);
		Animal->Tick(0.5f);
		TestTrue(TEXT("Non-playing phase does not start falling"), Animal->GetActorLocation().Equals(Before));
		TestEqual(TEXT("P1 score unchanged"), Mode->GetP1Score(), 0);
		TestEqual(TEXT("P2 score unchanged"), Mode->GetP2Score(), 0);
	}
	return true;
}
// 2026.10.23 Lee end

#endif // WITH_DEV_AUTOMATION_TESTS
// 2026.10.22 Lee end（マップ工作台データ層のエディタ自動テスト）
