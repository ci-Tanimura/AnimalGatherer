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
		if (!Test.TestEqual(What + TEXT(" / 床インスタンス数"), Map->HISM_BaseFloor->GetInstanceCount(), Map->MapWidth * Map->MapHeight))
		{
			return;
		}
		for (int32 i = 0; i < Map->HISM_BaseFloor->GetInstanceCount(); ++i)
		{
			FTransform InstanceTransform;
			Map->HISM_BaseFloor->GetInstanceTransform(i, InstanceTransform, /*bWorldSpace=*/false);
			const int32 X = i % Map->MapWidth;
			const int32 Y = i / Map->MapWidth;
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

#endif // WITH_DEV_AUTOMATION_TESTS
// 2026.10.22 Lee end（マップ工作台データ層のエディタ自動テスト）
