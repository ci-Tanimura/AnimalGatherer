// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/TutorialGameMode.h"
#include "Lee/TutorialHighlightActor.h"
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
// 2026.10.08 Lee start（教程拡張：演示動物と技能コンポーネントの操作）
#include "Takeuchi/Pawn/AnimalBase.h"
#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/Skill/SkillSystemComponent.h"
#include "Lee/Skill/MatchSkillEffectComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
// 2026.10.08 Lee end
// 2026.10.09 Lee start（地図区域対調：出現入替の ControllerId 参照に必要）
#include "Engine/LocalPlayer.h"
// 2026.10.09 Lee end

ATutorialGameMode::ATutorialGameMode()
{
	// 2026.10.02 Lee start
	// ハイライトのカーソル重なり判定を毎フレーム行うため Tick を有効化
	PrimaryActorTick.bCanEverTick = true;
	// 2026.10.02 Lee end

	// 2026.10.06 Lee start（チュートリアルは普通対戦フローを使わない）
	bUseNormalMatchFlow = false;
	// 2026.10.06 Lee end

	// ハイライトは既定で C++ 実装を使用（BP での差し替えも可）
	HighlightActorClass = ATutorialHighlightActor::StaticClass();

	// ステップ目標定義（1 要素で P1 / P2 両方を記述。メンバー既定値がレイアウトと一致している）
	Steps.AddDefaulted();

	// ── 事前に敷き詰めるタイル定義 ──
	// P1 側（Owner=0 青）：反時計回りの環状レーン。干渉セル (3,1) は空け、
	// 唯一の下向き出口が GoalP1 (3,0) につながるレイアウト。
	auto AddTile = [this](int32 X, int32 Y, ETileType Type, uint8 InOwnerId)
	{
		FTutorialTileDef Def;
		Def.CellX = X;
		Def.CellY = Y;
		Def.TileType = Type;
		Def.OwnerPlayerId = InOwnerId;
		PreplacedTiles.Add(Def);
	};

	// P1 側：環本体
	AddTile(2, 2, ETileType::Spawn, 0);       // 動物の出生セル（環の内側）
	AddTile(3, 2, ETileType::DirRight, 0);    // 環の内側セル
	AddTile(1, 1, ETileType::DirLeft, 0);     // 環の下辺（+X 流れ）
	AddTile(2, 1, ETileType::DirLeft, 0);     // 環の下辺（+X 流れ、干渉セルの手前）
	AddTile(4, 1, ETileType::DirUp, 0);       // 環の右辺（+Y 流れ）
	AddTile(4, 2, ETileType::DirUp, 0);       // 環の右辺（+Y 流れ）
	AddTile(4, 3, ETileType::DirRight, 0);    // 環の上辺（-X 流れ）
	AddTile(3, 3, ETileType::DirRight, 0);    // 環の上辺（-X 流れ）
	AddTile(2, 3, ETileType::DirRight, 0);    // 環の上辺（-X 流れ）
	AddTile(1, 3, ETileType::DirDown, 0);     // 環の左辺（-Y 流れ）
	AddTile(1, 2, ETileType::DirDown, 0);     // 環の左辺（-Y 流れ）
	AddTile(3, 0, ETileType::GoalP1, 0);      // 1P ゴール（Y=0 のボーダーで配置保護）

	// P1 側：ガード（脱出した動物を環へ弾き戻す）
	AddTile(0, 2, ETileType::DirLeft, 0);     // 西端ガード
	AddTile(4, 0, ETileType::DirUp, 0);       // 北東ガード
	AddTile(1, 4, ETileType::DirDown, 0);     // 北側ガード
	AddTile(2, 4, ETileType::DirDown, 0);     // 北側ガード
	AddTile(3, 4, ETileType::DirDown, 0);     // 北側ガード
	AddTile(5, 2, ETileType::DirRight, 0);    // 中央の仕切り（P2 側への逸走防止）

	// P2 側（Owner=1 赤）：P1 の点対称ミラー (x,y)→(11-x,5-y)。時計回りの環状レーン。
	// 干渉セル (8,4) は空け、唯一の上向き出口が GoalP2 (8,5) につながる。
	AddTile(9, 3, ETileType::Spawn, 1);       // 動物の出生セル（環の内側）
	AddTile(8, 3, ETileType::DirLeft, 1);     // 環の内側セル
	AddTile(10, 4, ETileType::DirRight, 1);   // 環の下辺（-X 流れ）
	AddTile(9, 4, ETileType::DirRight, 1);    // 環の下辺（-X 流れ、干渉セルの手前）
	AddTile(7, 4, ETileType::DirDown, 1);     // 環の左辺（-Y 流れ）
	AddTile(7, 3, ETileType::DirDown, 1);     // 環の左辺（-Y 流れ）
	AddTile(7, 2, ETileType::DirLeft, 1);     // 環の上辺（+X 流れ）
	AddTile(8, 2, ETileType::DirLeft, 1);     // 環の上辺（+X 流れ）
	AddTile(9, 2, ETileType::DirLeft, 1);     // 環の上辺（+X 流れ）
	AddTile(10, 2, ETileType::DirUp, 1);      // 環の右辺（+Y 流れ）
	AddTile(10, 3, ETileType::DirUp, 1);      // 環の右辺（+Y 流れ）
	AddTile(8, 5, ETileType::GoalP2, 1);      // 2P ゴール（Y=5 のボーダーで配置保護）

	// P2 側：ガード
	AddTile(11, 3, ETileType::DirRight, 1);   // 東端ガード
	AddTile(7, 5, ETileType::DirDown, 1);     // 南西ガード
	AddTile(10, 1, ETileType::DirUp, 1);      // 南側ガード
	AddTile(9, 1, ETileType::DirUp, 1);       // 南側ガード
	AddTile(8, 1, ETileType::DirUp, 1);       // 南側ガード
	AddTile(6, 3, ETileType::DirLeft, 1);     // 中央の仕切り（P1 側への逸走防止）

	// 2026.10.09 Lee start（地図区域対調：既設の敷設リスト（幾何・矢印方向は不変）のあとで、
	// 全タイルの有効プレイヤー所属を 0/1 入替し、GoalP1/GoalP2 の型も交換する（色替えのみは不可）。
	// 36 枚の敷設清单自体は書き換えず、ここで一括処理する）
	for (FTutorialTileDef& Def : PreplacedTiles)
	{
		Def.OwnerPlayerId = (Def.OwnerPlayerId == 0) ? 1u : 0u;
		if (Def.TileType == ETileType::GoalP1)
		{
			Def.TileType = ETileType::GoalP2;
		}
		else if (Def.TileType == ETileType::GoalP2)
		{
			Def.TileType = ETileType::GoalP1;
		}
	}
	// 2026.10.09 Lee end
}

void ATutorialGameMode::BeginPlay()
{
	// 注意：AMainGameMode::BeginPlay は意図的にスキップする（カウントダウン・制限時間・通常 HUD が不要なため）。
	// 解像度修正 (1920x1080) は基底クラスの BeginPlay でのみ行う。
	// 将来 AMainGameMode::BeginPlay に必須処理が追加された場合はここを見直すこと。
	AAnimalGathererGameModeBase::BeginPlay();

	// 2人目のプレイヤーを生成（P1 はエンジンが自動生成済み）
	UGameplayStatics::CreatePlayer(GetWorld(), 1, true);

	// レベル上のマップマネージャを取得
	CachedMapManager = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass()));
	if (!CachedMapManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TutorialGameMode] AMapManager が見つかりません。チュートリアルが正常に動作しません"));
	}

	// 環状レーンのレイアウトを適用
	ApplyPreplacedTiles();

	// チュートリアル用 HUD を生成（AddToViewport 時に NativeConstruct でデリゲート購読が完了する）
	if (TutorialWidgetClass)
	{
		TutorialWidget = CreateWidget<UUserWidget>(GetWorld(), TutorialWidgetClass);
		if (TutorialWidget)
		{
			TutorialWidget->AddToViewport();
		}
	}

	// Widget の購読完了後に最初のステップを通知
	EnterStep(ETutorialStep::Intro);

	// 達成判定ポーリング開始（初回のみ遅延させて Pawn 生成を待つ）
	GetWorldTimerManager().SetTimer(PollTimerHandle, this, &ATutorialGameMode::TickTutorialCheck, PollInterval, true, 0.5f);
}

ETutorialStep ATutorialGameMode::GetCurrentStep() const
{
	return CurrentStep;
}

// 2026.10.02 Lee start
// 2026.10.08 Lee start（教程拡張：技能実習ステップの配置可否を追加。旧実装はコメントとして保持）
// bool ATutorialGameMode::IsPlacementAllowed(uint8 PlayerID, FIntPoint Cell) const
// {
// 	// Intro / Complete 中は一切の配置を禁止する
// 	if (CurrentStep != ETutorialStep::MoveCursor
// 		&& CurrentStep != ETutorialStep::PlaceArrow
// 		&& CurrentStep != ETutorialStep::ScoreGoal)
// 	{
// 		return false;
// 	}
//
// 	// 自プレイヤーの干渉セル以外には配置できない（環状レーンの保護）
// 	const FTutorialStepDef* Def = GetCurrentStepDef();
// 	if (!Def)
// 	{
// 		return false;
// 	}
// 	const FIntPoint AllowedCell = (PlayerID == 0) ? Def->P1TargetCell : Def->P2TargetCell;
// 	return Cell == AllowedCell;
// }
bool ATutorialGameMode::IsPlacementAllowed(uint8 PlayerID, FIntPoint Cell) const
{
	// 2026.10.08 Lee 返工 start（ScoreGoal 中は観察のみとし配置を禁止する。
	//  得点後に誤った向きへ置き直すと反転演示の返却経路が保証できなくなるため）
	// 基礎ステップは従来どおり自プレイヤーの干渉セルのみ許可する（ScoreGoal を除く）
	// if (CurrentStep == ETutorialStep::MoveCursor
	// 	|| CurrentStep == ETutorialStep::PlaceArrow
	// 	|| CurrentStep == ETutorialStep::ScoreGoal)
	if (CurrentStep == ETutorialStep::MoveCursor
		|| CurrentStep == ETutorialStep::PlaceArrow)
	{
		const FTutorialStepDef* Def = GetCurrentStepDef();
		if (!Def)
		{
			return false;
		}
		const FIntPoint AllowedCell = (PlayerID == 0) ? Def->P1TargetCell : Def->P2TargetCell;
		return Cell == AllowedCell;
	}
	// 2026.10.08 Lee 返工 end

	// 修復段階は影響を受けたプレイヤーが自分の干渉セルへ配置する場合のみ許可する
	if (CurrentStep == ETutorialStep::RepairArrow)
	{
		const FTutorialStepDef* Def = GetCurrentStepDef();
		if (!Def)
		{
			return false;
		}
		const uint8 AffectedPlayerId = 1u - CurrentActivePlayerId;
		if (PlayerID != AffectedPlayerId)
		{
			return false;
		}
		const FIntPoint AllowedCell = (AffectedPlayerId == 0) ? Def->P1TargetCell : Def->P2TargetCell;
		return Cell == AllowedCell;
	}

	// Intro / Complete / 技能実習ステップ（観察・基線を含む）は一切の配置を禁止する
	return false;
}
// 2026.10.08 Lee end
// 2026.10.02 Lee end

// 2026.10.02 Lee start
void ATutorialGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ポーリング（0.2 秒）より高頻度で反映させるため毎フレーム処理する
	UpdateHighlightVisibility();
}

void ATutorialGameMode::UpdateHighlightVisibility()
{
	for (int32 i = 0; i < 2; ++i)
	{
		ATutorialHighlightActor* Highlight = (i == 0) ? P1Highlight.Get() : P2Highlight.Get();
		if (!Highlight)
		{
			continue;
		}
		const FIntPoint Cell = (i == 0) ? P1HighlightCell : P2HighlightCell;

		// カーソルが自分のハイライトセル上にいる間はハイライトを隠す（セルを見えるようにする）
		const ACursorPawn* Cursor = GetCursorPawn(i);
		const bool bCursorOnCell = Cursor != nullptr
			&& Cursor->GridX == Cell.X
			&& Cursor->GridY == Cell.Y;
		Highlight->SetActorHiddenInGame(bCursorOnCell);
	}
}
// 2026.10.02 Lee end

// 2026.10.08 Lee start（教程拡張：技能実習ステップの追加に伴いポーリング本体を分離。
//  旧実装（基礎 3 ステップのみ）はコメントとして保持）
// void ATutorialGameMode::TickTutorialCheck()
// {
// 	// Intro / Complete はタイマー主導のためポーリング対象外
// 	if (CurrentStep != ETutorialStep::MoveCursor
// 		&& CurrentStep != ETutorialStep::PlaceArrow
// 		&& CurrentStep != ETutorialStep::ScoreGoal)
// 	{
// 		return;
// 	}
//
// 	const bool bWasP1Done = bP1Done;
// 	const bool bWasP2Done = bP2Done;
//
// 	bP1Done = IsPlayerStepDone(0);
// 	bP2Done = IsPlayerStepDone(1);
//
// 	// 2026.10.02 Lee start
// 	// 立上り・立下りの両エッジで現在の達成状態を通知する
// 	// （「今まさに条件を満たしている間だけ OK 表示する」仕様。
// 	//   例：MoveCursor 中はカーソルが目標セルにいる間のみ P1 OK! を表示する）
// 	if (bWasP1Done != bP1Done)
// 	{
// 		OnTutorialPlayerDone.Broadcast(0, bP1Done);
// 	}
// 	if (bWasP2Done != bP2Done)
// 	{
// 		OnTutorialPlayerDone.Broadcast(1, bP2Done);
// 	}
// 	// 2026.10.02 Lee end
//
// 	// 両プレイヤーが達成したら次のステップへ
// 	if (bP1Done && bP2Done)
// 	{
// 		AdvanceStep();
// 	}
// }
void ATutorialGameMode::TickTutorialCheck()
{
	// 基礎 3 ステップと技能実習ステップで判定本体を分ける
	switch (CurrentStep)
	{
	case ETutorialStep::MoveCursor:
	case ETutorialStep::PlaceArrow:
	case ETutorialStep::ScoreGoal:
		TickBasicStepCheck();
		break;
	case ETutorialStep::ReverseSkill:
	case ETutorialStep::ObserveReverse:
	case ETutorialStep::RepairArrow:
	case ETutorialStep::SpeedBaseline:
	case ETutorialStep::SpeedSkill:
	case ETutorialStep::ObserveSpeed:
		TickSkillStepCheck();
		break;
	default:
		// Intro / Complete / SpeedRecovered はタイマー主導のためポーリング対象外
		break;
	}
}

void ATutorialGameMode::TickBasicStepCheck()
{
	const bool bWasP1Done = bP1Done;
	const bool bWasP2Done = bP2Done;

	bP1Done = IsPlayerStepDone(0);
	bP2Done = IsPlayerStepDone(1);

	// 2026.10.02 Lee start
	// 立上り・立下りの両エッジで現在の達成状態を通知する
	// （「今まさに条件を満たしている間だけ OK 表示する」仕様。
	//   例：MoveCursor 中はカーソルが目標セルにいる間のみ P1 OK! を表示する）
	if (bWasP1Done != bP1Done)
	{
		OnTutorialPlayerDone.Broadcast(0, bP1Done);
	}
	if (bWasP2Done != bP2Done)
	{
		OnTutorialPlayerDone.Broadcast(1, bP2Done);
	}
	// 2026.10.02 Lee end

	// 両プレイヤーが達成したら次のステップへ
	if (bP1Done && bP2Done)
	{
		AdvanceStep();
	}
}

void ATutorialGameMode::TickSkillStepCheck()
{
	switch (CurrentStep)
	{
	case ETutorialStep::ReverseSkill:
		// 技能未初期化ならここで再試行する（Pawn・地図の整備待ち。通常試合は開始しない）
		TryInitializeTutorialSkills();
		if (bSkillAccepted)
		{
			AdvanceStep();
		}
		break;

	case ETutorialStep::ObserveReverse:
	{
		// 演示動物が実際に返却セルを読み取るまで待つ（定時跳過はしない）
		if (!IsValid(DemoAnimal))
		{
			// 生成失敗・予期しない破棄：偽の成功をせず再生成して待ち続ける
			UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] 演示動物が無効なため再生成します（返却セル待ち継続）"));
			DemoAnimal = SpawnTutorialAnimal(ObserveSpawnCell, ObserveSpawnDirection);
			break;
		}
		FIntPoint LastRead = FIntPoint::NoneValue;
		if (DemoAnimal->GetLastReadGridCoords(LastRead) && LastRead == ObserveReturnCell)
		{
			AdvanceStep();
		}
		break;
	}

	case ETutorialStep::RepairArrow:
	{
		// 実配置証拠（NotifyArrowPlaced）と地図状態の双方で修復を確認する
		const FTutorialStepDef* Def = GetCurrentStepDef();
		if (!Def || !CachedMapManager || !bRepairPlacementRecorded)
		{
			break;
		}
		const uint8 AffectedPlayerId = 1u - CurrentActivePlayerId;
		const FIntPoint TargetCell = (AffectedPlayerId == 0) ? Def->P1TargetCell : Def->P2TargetCell;
		const ETileType ExpectedDir = (AffectedPlayerId == 0) ? Def->P1ExpectedDirection : Def->P2ExpectedDirection;
		if (CachedMapManager->GetCellState_Implementation(TargetCell) == ExpectedDir
			&& CachedMapManager->GetTileOwner(TargetCell.X, TargetCell.Y) == AffectedPlayerId)
		{
			AdvanceStep();
		}
		break;
	}

	// 2026.10.08 Lee 第三批修正 start（基線動物の生成失敗時の再試行）
	case ETutorialStep::SpeedBaseline:
		// 生成が未完了なら再試行する（成功時に観察タイマーが設定される）
		if (!bSpeedBaselineAnimalsSpawned)
		{
			TrySpawnBaselineAnimals();
		}
		break;
	// 2026.10.08 Lee 第三批修正 end

	case ETutorialStep::SpeedSkill:
		if (bSkillAccepted)
		{
			AdvanceStep();
		}
		break;

	case ETutorialStep::ObserveSpeed:
	{
		// 共有効果スナップショットが実際に終了するのを待つ（表示上の秒数で代用しない）
		UMatchSkillEffectComponent* Effect = GetMatchSkillEffect();
		if (Effect == nullptr)
		{
			break;
		}
		const FMatchSpeedSnapshot Snapshot = Effect->GetSnapshot();
		if (Snapshot.SpeedMultiplier <= 1.0f && Snapshot.RemainingDuration <= 0.0f)
		{
			AdvanceStep();
		}
		break;
	}

	default:
		break;
	}
}
// 2026.10.08 Lee end

bool ATutorialGameMode::IsPlayerStepDone(int32 PlayerID)
{
	const FTutorialStepDef* Def = GetCurrentStepDef();
	if (!Def)
	{
		return false;
	}

	const FIntPoint TargetCell = (PlayerID == 0) ? Def->P1TargetCell : Def->P2TargetCell;

	switch (CurrentStep)
	{
	case ETutorialStep::MoveCursor:
	{
		// カーソルが目標セルに到達したら達成
		const ACursorPawn* Cursor = GetCursorPawn(PlayerID);
		return Cursor != nullptr
			&& Cursor->GridX == TargetCell.X
			&& Cursor->GridY == TargetCell.Y;
	}
	case ETutorialStep::PlaceArrow:
	{
		// 目標セルに「正しい向きかつ自分所有」の矢印が置かれたら達成
		if (!CachedMapManager)
		{
			return false;
		}
		const ETileType ExpectedDir = (PlayerID == 0) ? Def->P1ExpectedDirection : Def->P2ExpectedDirection;
		const ETileType ActualType = CachedMapManager->GetCellState_Implementation(TargetCell);
		const uint8 TileOwner = CachedMapManager->GetTileOwner(TargetCell.X, TargetCell.Y);
		return ActualType == ExpectedDir && TileOwner == static_cast<uint8>(PlayerID);
	}
	case ETutorialStep::ScoreGoal:
		// ステップ開始後のスコア加算で達成
		return (PlayerID == 0) ? (P1Score > P1ScoreAtStepStart) : (P2Score > P2ScoreAtStepStart);
	default:
		return false;
	}
}

// 2026.10.08 Lee start（教程拡張：技能実習ステップの遷移を追加。旧実装はコメントとして保持）
// void ATutorialGameMode::AdvanceStep()
// {
// 	// 達成フラグをリセットしてから次のステップへ
// 	bP1Done = false;
// 	bP2Done = false;
//
// 	ETutorialStep NextStep = CurrentStep;
// 	switch (CurrentStep)
// 	{
// 	case ETutorialStep::Intro:       NextStep = ETutorialStep::MoveCursor; break;
// 	case ETutorialStep::MoveCursor:  NextStep = ETutorialStep::PlaceArrow; break;
// 	case ETutorialStep::PlaceArrow:  NextStep = ETutorialStep::ScoreGoal;  break;
// 	case ETutorialStep::ScoreGoal:   NextStep = ETutorialStep::Complete;   break;
// 	case ETutorialStep::Complete:    return;
// 	default: return;
// 	}
//
// 	EnterStep(NextStep);
// }
void ATutorialGameMode::AdvanceStep()
{
	// 達成フラグをリセットしてから次のステップへ
	bP1Done = false;
	bP2Done = false;

	ETutorialStep NextStep = CurrentStep;
	switch (CurrentStep)
	{
	case ETutorialStep::Intro:       NextStep = ETutorialStep::MoveCursor; break;
	case ETutorialStep::MoveCursor:  NextStep = ETutorialStep::PlaceArrow; break;
	case ETutorialStep::PlaceArrow:  NextStep = ETutorialStep::ScoreGoal;  break;
	// 2026.10.08 Lee start（教程拡張：ScoreGoal の次は技能実習へ）
	case ETutorialStep::ScoreGoal:   NextStep = ETutorialStep::ReverseSkill; break;
	// 2026.10.08 Lee end
	case ETutorialStep::Complete:    return;
	// 2026.10.08 Lee start（教程拡張：技能実習の遷移）
	case ETutorialStep::ReverseSkill:  NextStep = ETutorialStep::ObserveReverse; break;
	case ETutorialStep::ObserveReverse: NextStep = ETutorialStep::RepairArrow; break;
	case ETutorialStep::RepairArrow:
		// 観察動物を片付けてから役割を交換する（1 回目は反転の 2 回目へ、2 回目は加速へ）
		ClearRuntimeAnimals();
		if (CurrentActivePlayerId == 0)
		{
			CurrentActivePlayerId = 1;
			NextStep = ETutorialStep::ReverseSkill;
		}
		else
		{
			CurrentActivePlayerId = 0;
			NextStep = ETutorialStep::SpeedBaseline;
		}
		break;
	case ETutorialStep::SpeedBaseline:  NextStep = ETutorialStep::SpeedSkill; break;
	case ETutorialStep::SpeedSkill:     NextStep = ETutorialStep::ObserveSpeed; break;
	case ETutorialStep::ObserveSpeed:   NextStep = ETutorialStep::SpeedRecovered; break;
	case ETutorialStep::SpeedRecovered:
		// 1 人目は基線観察済みのため 2 人目の加速へ直行し、2 人目の終了で完結する
		if (CurrentActivePlayerId == 0)
		{
			CurrentActivePlayerId = 1;
			NextStep = ETutorialStep::SpeedSkill;
		}
		else
		{
			NextStep = ETutorialStep::Complete;
		}
		break;
	// 2026.10.08 Lee end
	default: return;
	}

	EnterStep(NextStep);
}
// 2026.10.08 Lee end

void ATutorialGameMode::EnterStep(ETutorialStep NewStep)
{
	CurrentStep = NewStep;

	switch (NewStep)
	{
	case ETutorialStep::Intro:
		// IntroDuration 経過で MoveCursor へ自動遷移
		GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATutorialGameMode::AdvanceStep, IntroDuration, false);
		RefreshHighlights();
		break;

	case ETutorialStep::PlaceArrow:
		// ハイライトは干渉セルのまま維持
		RefreshHighlights();
		break;

	case ETutorialStep::ScoreGoal:
		// スコア基準値を記録してから動物の供給を開始
		P1ScoreAtStepStart = P1Score;
		P2ScoreAtStepStart = P2Score;
		EnterScoreGoal();
		RefreshHighlights();
		break;

	case ETutorialStep::Complete:
		// 完了したらポーリングを停止し、演出 → レベル遷移へ
		GetWorldTimerManager().ClearTimer(PollTimerHandle);
		EnterComplete();
		break;

	// 2026.10.08 Lee start（教程拡張：技能実習ステップの入口処理）
	case ETutorialStep::ReverseSkill:
		EnterReverseSkill();
		RefreshHighlights();
		break;

	case ETutorialStep::ObserveReverse:
		EnterObserveReverse();
		RefreshHighlights();
		break;

	case ETutorialStep::RepairArrow:
		EnterRepairArrow();
		RefreshHighlights();
		break;

	case ETutorialStep::SpeedBaseline:
		EnterSpeedBaseline();
		ClearHighlights();
		break;

	case ETutorialStep::SpeedSkill:
		EnterSpeedSkill();
		ClearHighlights();
		break;

	case ETutorialStep::ObserveSpeed:
		ClearHighlights();
		break;

	case ETutorialStep::SpeedRecovered:
		EnterSpeedRecovered();
		ClearHighlights();
		break;
	// 2026.10.08 Lee end

	default:
		RefreshHighlights();
		break;
	}

	OnTutorialStepChanged.Broadcast(NewStep);
	UE_LOG(LogTemp, Display, TEXT("[TutorialGameMode] Step: %s"), *UEnum::GetValueAsString(NewStep));
}

void ATutorialGameMode::EnterScoreGoal()
{
	// レベル内の全スポーナーで動物の供給を開始する
	TArray<AActor*> Spawners;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAnimalSpawner::StaticClass(), Spawners);
	for (AActor* Actor : Spawners)
	{
		if (AAnimalSpawner* Spawner = Cast<AAnimalSpawner>(Actor))
		{
			Spawner->StartSpawning();
		}
	}
}

void ATutorialGameMode::EnterComplete()
{
	// 2026.10.08 Lee start（教程拡張：完了時は技能・配置・共有効果・動物を先に停止してから UI へ通知する）
	// 技能の使用可否を閉じる（権威判定 IsSkillUseAllowed も Complete では拒否される）
	SetTutorialSkillsEnabled(false);
	// 共有加速効果を即時解除する（残留タイマーも停止される）
	if (UMatchSkillEffectComponent* Effect = GetMatchSkillEffect())
	{
		Effect->ClearEffects();
	}
	// 生成停止と実行中動物の破棄（演示動物を含む）
	ClearRuntimeAnimals();
	// 2026.10.08 Lee end

	ClearHighlights();
	OnTutorialComplete.Broadcast();

	// 遷移先が指定されている場合のみ演出後にレベル遷移する
	if (!TutorialEndLevelName.IsNone())
	{
		GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATutorialGameMode::FinishTutorial, CompleteDelay, false);
	}
}

void ATutorialGameMode::FinishTutorial()
{
	if (!TutorialEndLevelName.IsNone())
	{
		UGameplayStatics::OpenLevel(this, TutorialEndLevelName);
	}
}

void ATutorialGameMode::ApplyPreplacedTiles()
{
	if (!CachedMapManager)
	{
		return;
	}

	for (const FTutorialTileDef& Def : PreplacedTiles)
	{
		CachedMapManager->SetTileData(Def.CellX, Def.CellY, Def.TileType, Def.OwnerPlayerId);
	}

	UE_LOG(LogTemp, Display, TEXT("[TutorialGameMode] PreplacedTiles を %d 枚適用しました"), PreplacedTiles.Num());
}

// 2026.10.08 Lee start（教程拡張：技能実習ステップのハイライト。旧実装はコメントとして保持）
// void ATutorialGameMode::RefreshHighlights()
// {
// 	ClearHighlights();
//
// 	if (!CachedMapManager || !HighlightActorClass)
// 	{
// 		return;
// 	}
//
// 	const FTutorialStepDef* Def = GetCurrentStepDef();
// 	if (!Def)
// 	{
// 		return;
// 	}
//
// 	// ScoreGoal ではゴールセル、それ以外では干渉セルを強調する
// 	FIntPoint P1Cell;
// 	FIntPoint P2Cell;
// 	if (CurrentStep == ETutorialStep::ScoreGoal)
// 	{
// 		P1Cell = Def->P1GoalCell;
// 		P2Cell = Def->P2GoalCell;
// 	}
// 	else
// 	{
// 		P1Cell = Def->P1TargetCell;
// 		P2Cell = Def->P2TargetCell;
// 	}
//
// 	// 2026.10.02 Lee start
// 	SpawnHighlightFor(0, P1Cell, FLinearColor(0.1f, 0.3f, 1.0f)); // 1P = 青
// 	SpawnHighlightFor(1, P2Cell, FLinearColor(1.0f, 0.15f, 0.15f)); // 2P = 赤
// 	// 2026.10.02 Lee end
// }
/**
 * @brief 技能実習ステップのハイライトを選択する。
 *        ObserveReverse / RepairArrow では影響を受けたプレイヤーの干渉セルのみ、
 *        加速系ステップではハイライトなしとする。
 */
void ATutorialGameMode::RefreshHighlights()
{
	ClearHighlights();

	if (!CachedMapManager || !HighlightActorClass)
	{
		return;
	}

	// 2026.10.08 Lee start（技能実習ステップの分岐を追加。基礎ステップは従来経路を維持）
	if (CurrentStep == ETutorialStep::ObserveReverse || CurrentStep == ETutorialStep::RepairArrow)
	{
		// 影響を受けたプレイヤーの干渉セル（反転された矢印・修復対象）のみ強調する
		const FTutorialStepDef* Def = GetCurrentStepDef();
		if (!Def)
		{
			return;
		}
		const uint8 AffectedPlayerId = 1u - CurrentActivePlayerId;
		if (AffectedPlayerId == 0)
		{
			SpawnHighlightFor(0, Def->P1TargetCell, FLinearColor(0.1f, 0.3f, 1.0f)); // 1P = 青
		}
		else
		{
			SpawnHighlightFor(1, Def->P2TargetCell, FLinearColor(1.0f, 0.15f, 0.15f)); // 2P = 赤
		}
		return;
	}

	if (CurrentStep == ETutorialStep::ReverseSkill
		|| CurrentStep == ETutorialStep::SpeedBaseline
		|| CurrentStep == ETutorialStep::SpeedSkill
		|| CurrentStep == ETutorialStep::ObserveSpeed
		|| CurrentStep == ETutorialStep::SpeedRecovered)
	{
		// 技能実習中は地図上のハイライトを行わない（ボタン強調は HUD 側の担当）
		return;
	}
	// 2026.10.08 Lee end

	const FTutorialStepDef* Def = GetCurrentStepDef();
	if (!Def)
	{
		return;
	}

	// ScoreGoal ではゴールセル、それ以外では干渉セルを強調する
	FIntPoint P1Cell;
	FIntPoint P2Cell;
	if (CurrentStep == ETutorialStep::ScoreGoal)
	{
		P1Cell = Def->P1GoalCell;
		P2Cell = Def->P2GoalCell;
	}
	else
	{
		P1Cell = Def->P1TargetCell;
		P2Cell = Def->P2TargetCell;
	}

	// 2026.10.02 Lee start
	SpawnHighlightFor(0, P1Cell, FLinearColor(0.1f, 0.3f, 1.0f)); // 1P = 青
	SpawnHighlightFor(1, P2Cell, FLinearColor(1.0f, 0.15f, 0.15f)); // 2P = 赤
	// 2026.10.02 Lee end
}
// 2026.10.08 Lee end

void ATutorialGameMode::ClearHighlights()
{
	// 2026.10.02 Lee start
	if (IsValid(P1Highlight))
	{
		P1Highlight->Destroy();
	}
	if (IsValid(P2Highlight))
	{
		P2Highlight->Destroy();
	}
	P1Highlight = nullptr;
	P2Highlight = nullptr;
	// 2026.10.02 Lee end
}

// 2026.10.02 Lee start
ATutorialHighlightActor* ATutorialGameMode::SpawnHighlightFor(int32 PlayerID, FIntPoint Cell, const FLinearColor& Color)
{
	FActorSpawnParameters Params;
	Params.Owner = this;

	ATutorialHighlightActor* Highlight = GetWorld()->SpawnActor<ATutorialHighlightActor>(
		HighlightActorClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!Highlight)
	{
		return nullptr;
	}

	Highlight->Initialize(HighlightMaterial, Color, Cell, CachedMapManager);
	if (PlayerID == 0)
	{
		P1Highlight = Highlight;
		P1HighlightCell = Cell;
	}
	else
	{
		P2Highlight = Highlight;
		P2HighlightCell = Cell;
	}
	return Highlight;
}
// 2026.10.02 Lee end

ACursorPawn* ATutorialGameMode::GetCursorPawn(int32 PlayerID) const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), PlayerID);
	return PC ? Cast<ACursorPawn>(PC->GetPawn()) : nullptr;
}

const FTutorialStepDef* ATutorialGameMode::GetCurrentStepDef() const
{
	// 3 ステップ（MoveCursor / PlaceArrow / ScoreGoal）は 1 要素の定義を共有する
	if (Steps.Num() > 0)
	{
		return &Steps[0];
	}
	return nullptr;
}

//==============================================================================
// 2026.10.08 Lee start（教程拡張：技能実習のライフサイクル・共有権限・演示動物）
//==============================================================================

void ATutorialGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 教程独自のタイマーと技能通知デリゲートを先に停止する（通常試合側の後始末は Super に委ねる）
	GetWorldTimerManager().ClearTimer(PollTimerHandle);
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	UnbindSkillDelegates();

	Super::EndPlay(EndPlayReason);
}

bool ATutorialGameMode::SupportsSkillInitialization() const
{
	// チュートリアルは技能実習のため InitializeSkillSystemsForMode の使用を許可する
	return true;
}

bool ATutorialGameMode::IsSkillUseAllowed(uint8 PlayerId, int32 SlotIndex) const
{
	// 成功受領済みなら即拒否（同フレームの重複施放・長押しの追加消費を防ぐ）
	if (bSkillAccepted)
	{
		return false;
	}
	// ReverseSkill 中は操作者の反転スロットのみ、SpeedSkill 中は操作者の加速スロットのみ許可する
	if (CurrentStep == ETutorialStep::ReverseSkill
		&& SlotIndex == SkillSlotIndex_Reverse
		&& PlayerId == CurrentActivePlayerId)
	{
		return true;
	}
	if (CurrentStep == ETutorialStep::SpeedSkill
		&& SlotIndex == SkillSlotIndex_Speed
		&& PlayerId == CurrentActivePlayerId)
	{
		return true;
	}
	return false;
}

bool ATutorialGameMode::IsSkillEffectContextActive() const
{
	// 地図バインド済みで完了前は有効。成功受領による段階切替では効果を無効化しない
	// （加速の終了は ObserveSpeed が実際のスナップショット終了を待って判定する）
	return GetMatchMap() != nullptr && CurrentStep != ETutorialStep::Complete;
}

void ATutorialGameMode::NotifyArrowPlaced(uint8 PlayerId, FIntPoint Cell, ETileType Direction)
{
	// 修復段階以外は記録しない
	if (CurrentStep != ETutorialStep::RepairArrow)
	{
		return;
	}
	// 影響を受けたプレイヤー（反転の対象 = 1 - 施放者）の配置のみが修復操作
	const uint8 AffectedPlayerId = 1u - CurrentActivePlayerId;
	if (PlayerId != AffectedPlayerId)
	{
		return;
	}
	const FTutorialStepDef* Def = GetCurrentStepDef();
	if (!Def)
	{
		return;
	}
	// 目標セル・正解方向の配置のみ実修復証拠として記録する
	const FIntPoint TargetCell = (AffectedPlayerId == 0) ? Def->P1TargetCell : Def->P2TargetCell;
	if (Cell != TargetCell)
	{
		return;
	}
	const ETileType ExpectedDir = (AffectedPlayerId == 0) ? Def->P1ExpectedDirection : Def->P2ExpectedDirection;
	if (Direction != ExpectedDir)
	{
		return;
	}
	bRepairPlacementRecorded = true;
	UE_LOG(LogTemp, Display, TEXT("[TutorialGameMode] 修復配置を記録: Player%u が (%d, %d) へ配置"), PlayerId, Cell.X, Cell.Y);
}

uint8 ATutorialGameMode::GetCurrentActivePlayerId() const
{
	return CurrentActivePlayerId;
}

bool ATutorialGameMode::IsTutorialPlayerDone(uint8 PlayerId) const
{
	switch (CurrentStep)
	{
	case ETutorialStep::MoveCursor:
	case ETutorialStep::PlaceArrow:
	case ETutorialStep::ScoreGoal:
		return (PlayerId == 0) ? bP1Done : bP2Done;
	case ETutorialStep::ReverseSkill:
	case ETutorialStep::SpeedSkill:
		// 実習ステップは操作者の施放成功が達成
		return bSkillAccepted && PlayerId == CurrentActivePlayerId;
	case ETutorialStep::ObserveReverse:
	case ETutorialStep::ObserveSpeed:
	case ETutorialStep::SpeedRecovered:
		// 観察中は施放者側が完了扱い（効果の帰結を待つのは全体）
		return PlayerId == CurrentActivePlayerId;
	case ETutorialStep::RepairArrow:
		// 修復担当は影響を受けたプレイヤー
		return PlayerId == (1u - CurrentActivePlayerId) && bRepairPlacementRecorded;
	default:
		return false;
	}
}

bool ATutorialGameMode::HasCurrentSkillSucceeded() const
{
	return bSkillAccepted;
}

bool ATutorialGameMode::IsTutorialWaitingObservation() const
{
	return CurrentStep == ETutorialStep::ObserveReverse || CurrentStep == ETutorialStep::ObserveSpeed;
}

bool ATutorialGameMode::IsTutorialWaitingBaseline() const
{
	return CurrentStep == ETutorialStep::SpeedBaseline;
}

// 2026.10.08 Lee 第三批 start（HUD のボタン強調算出用の公開読み取り）
ETileType ATutorialGameMode::GetExpectedDirectionFor(uint8 PlayerId) const
{
	const FTutorialStepDef* Def = GetCurrentStepDef();
	if (Def == nullptr)
	{
		return ETileType::Empty;
	}
	return (PlayerId == 0) ? Def->P1ExpectedDirection : Def->P2ExpectedDirection;
}
// 2026.10.08 Lee 第三批 end

void ATutorialGameMode::TryInitializeTutorialSkills()
{
	if (bTutorialSkillsInitialized)
	{
		return;
	}
	// 第一批の共有初期化本体を流用（プレイヤー/地図/World 検証 → バインド → 冪等初期化）。
	// 通常試合（StartMatch・スポーナー解禁）は起動しない
	if (!InitializeSkillSystemsForMode())
	{
		if (!bLoggedSkillInitFailure)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TutorialGameMode] 技能初期化が未整備です。次のポーリングで再試行します"));
			bLoggedSkillInitFailure = true;
		}
		return;
	}

	// 成功したら双方の成功/失敗通知を購読する
	for (int32 i = 0; i < 2; ++i)
	{
		AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), i));
		if (PC == nullptr)
		{
			continue;
		}
		if (USkillSystemComponent* SkillSystem = PC->GetSkillSystemComponent())
		{
			SkillSystem->OnSkillUsed.AddDynamic(this, &ATutorialGameMode::HandleSkillUsed);
			SkillSystem->OnSkillUseFailed.AddDynamic(this, &ATutorialGameMode::HandleSkillUseFailed);
		}
	}
	bTutorialSkillsInitialized = true;
	// 初期化時に bEnabled が false へ戻るため、保留中の可否を反映し直す
	SetTutorialSkillsEnabled(bTutorialSkillsEnabled);
	UE_LOG(LogTemp, Log, TEXT("[TutorialGameMode] 技能システムの初期化と購読が完了しました"));
}

void ATutorialGameMode::SetTutorialSkillsEnabled(bool bEnabled)
{
	bTutorialSkillsEnabled = bEnabled;
	for (int32 i = 0; i < 2; ++i)
	{
		AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), i));
		if (PC == nullptr)
		{
			continue;
		}
		if (USkillSystemComponent* SkillSystem = PC->GetSkillSystemComponent())
		{
			// 権限の詳細（段階・操作者・スロット）は IsSkillUseAllowed 側で判定する
			SkillSystem->SetSkillsEnabled(bEnabled);
		}
	}
}

void ATutorialGameMode::HandleSkillUsed(int32 SlotIndex)
{
	// 受領のみ行い段階切替は次ポーリングに委ねる（デリゲート放送中の状態進行・重入を避ける）
	if (CurrentStep == ETutorialStep::ReverseSkill && SlotIndex == SkillSlotIndex_Reverse)
	{
		bSkillAccepted = true;
	}
	else if (CurrentStep == ETutorialStep::SpeedSkill && SlotIndex == SkillSlotIndex_Speed)
	{
		bSkillAccepted = true;
	}
}

void ATutorialGameMode::HandleSkillUseFailed(int32 SlotIndex, ESkillUseResult Result)
{
	// 失敗は回数・CD に影響しない。教程の権限拒否も想定内のため Warning のみ
	UE_LOG(LogTemp, Warning, TEXT("[TutorialGameMode] 技能使用失敗: Slot=%d Result=%d"), SlotIndex, static_cast<int32>(Result));
}

void ATutorialGameMode::UnbindSkillDelegates()
{
	for (int32 i = 0; i < 2; ++i)
	{
		AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), i));
		if (PC == nullptr)
		{
			continue;
		}
		if (USkillSystemComponent* SkillSystem = PC->GetSkillSystemComponent())
		{
			SkillSystem->OnSkillUsed.RemoveAll(this);
			SkillSystem->OnSkillUseFailed.RemoveAll(this);
		}
	}
}

void ATutorialGameMode::StopTutorialSpawners()
{
	for (TActorIterator<AAnimalSpawner> It(GetWorld()); It; ++It)
	{
		AAnimalSpawner* Spawner = *It;
		if (Spawner && Spawner->MapActor == CachedMapManager)
		{
			Spawner->StopSpawning();
		}
	}
}

void ATutorialGameMode::ClearRuntimeAnimals()
{
	// 先に生成を止めてから、教程地図に属する実行中の動物（スポーナー産・演示の両方）を破棄する
	StopTutorialSpawners();
	for (TActorIterator<AAnimalBase> It(GetWorld()); It; ++It)
	{
		AAnimalBase* Animal = *It;
		if (Animal && IsValid(Animal) && Animal->MapActor == CachedMapManager && !Animal->IsActorBeingDestroyed())
		{
			Animal->Destroy();
		}
	}
	DemoAnimal = nullptr;
	TutorialAnimals.Reset();
}

AAnimalBase* ATutorialGameMode::SpawnTutorialAnimal(FIntPoint Cell, const FVector& MoveDirection)
{
	if (!CachedMapManager || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] 演示動物の生成に失敗: 地図または World が無効"));
		return nullptr;
	}

	// 既存スポーナーの AnimalClass を読み、教程独自の動物資産は作らない
	AAnimalSpawner* SourceSpawner = nullptr;
	for (TActorIterator<AAnimalSpawner> It(GetWorld()); It; ++It)
	{
		AAnimalSpawner* Spawner = *It;
		if (Spawner && Spawner->MapActor == CachedMapManager && Spawner->AnimalClass != nullptr)
		{
			SourceSpawner = Spawner;
			break;
		}
	}
	UClass* AnimalUClass = SourceSpawner ? SourceSpawner->AnimalClass.Get() : nullptr;
	if (AnimalUClass == nullptr || !AnimalUClass->IsChildOf<AAnimalBase>())
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] 演示動物の生成に失敗: 有効な AnimalClass を持つスポーナーが見つかりません"));
		return nullptr;
	}

	// 2026.10.08 Lee 第三批修正 start（出生 Z は絶対世界高度のため地図原点に足さない。XY のみ原点 + 格長で算出）
	// const FVector CellCenter = CachedMapManager->GetActorLocation()
	// 	+ FVector(Cell.X * TileSize, Cell.Y * TileSize, SpawnZ);
	// セル中心（XY = 地図原点 + 格長、Z = 既存スポーナーの出生高度そのまま）
	const float TileSize = CachedMapManager->TileSize;
	const float SpawnZ = SourceSpawner ? SourceSpawner->SpawnLocation.Z : 0.0f;
	const FVector MapOrigin = CachedMapManager->GetActorLocation();
	const FVector CellCenter = FVector(
		MapOrigin.X + Cell.X * TileSize,
		MapOrigin.Y + Cell.Y * TileSize,
		SpawnZ);
	// 2026.10.08 Lee 第三批修正 end

	// deferred spawn で Finish 前に地図と格長を確定させ、Finish 後に确定方向を与える
	AAnimalBase* Animal = GetWorld()->SpawnActorDeferred<AAnimalBase>(
		AnimalUClass,
		FTransform(FRotator::ZeroRotator, CellCenter),
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Animal == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] 演示動物の生成に失敗: SpawnActorDeferred が null を返しました (cell=%d,%d)"), Cell.X, Cell.Y);
		return nullptr;
	}
	Animal->SetMapActor(CachedMapManager);
	Animal->FinishSpawning(FTransform(FRotator::ZeroRotator, CellCenter));
	if (!IsValid(Animal))
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] 演示動物の生成に失敗: FinishSpawning 後に無効です (cell=%d,%d)"), Cell.X, Cell.Y);
		return nullptr;
	}
	// BeginPlay 内の随机初方向を确定方向で上書きする
	Animal->SetMoveDirection(MoveDirection);
	TutorialAnimals.Add(Animal);
	return Animal;
}

void ATutorialGameMode::EnterReverseSkill()
{
	// 基礎ステップ（または修復 1 回目）の動物を片付ける: 生成停止 + 実行中動物の破棄。
	// 地図・カーソル・スコア・配置履歴はそのまま残す
	ClearRuntimeAnimals();

	// 施放受領フラグを初期化し、技能の準備（未初期化なら次ポーリングで再試行）を行う
	bSkillAccepted = false;
	TryInitializeTutorialSkills();
	SetTutorialSkillsEnabled(true);
}

void ATutorialGameMode::EnterObserveReverse()
{
	// 2026.10.09 Lee start（地図区域対調：影響対象の環も入替。旧分岐はコメントとして保持）
	// 【旧実装（保持）】
	// if (CurrentActivePlayerId == 0)
	// {
	//     // P1 施放 → P2 の (8,4) が反転: (9,4) から -X で侵入し (8,3) を読んで環へ戻る
	//     ObserveSpawnCell = FIntPoint(9, 4);
	//     ObserveSpawnDirection = FVector(-1.0f, 0.0f, 0.0f);
	//     ObserveReturnCell = FIntPoint(8, 3);
	// }
	// else
	// {
	//     // P2 施放 → P1 の (3,1) が反転: (2,1) から +X で侵入し (3,2) を読んで環へ戻る
	//     ObserveSpawnCell = FIntPoint(2, 1);
	//     ObserveSpawnDirection = FVector(1.0f, 0.0f, 0.0f);
	//     ObserveReturnCell = FIntPoint(3, 2);
	// }
	// 影響対象の環に沿って演示動物を流す（施放者側のルート設定は固定）
	if (CurrentActivePlayerId == 0)
	{
		// P1 施放 → P2 の (3,1) が反転: (2,1) から +X で侵入し (3,2) を読んで環へ戻る
		ObserveSpawnCell = FIntPoint(2, 1);
		ObserveSpawnDirection = FVector(1.0f, 0.0f, 0.0f);
		ObserveReturnCell = FIntPoint(3, 2);
	}
	else
	{
		// P2 施放 → P1 の (8,4) が反転: (9,4) から -X で侵入し (8,3) を読んで環へ戻る
		ObserveSpawnCell = FIntPoint(9, 4);
		ObserveSpawnDirection = FVector(-1.0f, 0.0f, 0.0f);
		ObserveReturnCell = FIntPoint(8, 3);
	}
	// 2026.10.09 Lee end
	DemoAnimal = SpawnTutorialAnimal(ObserveSpawnCell, ObserveSpawnDirection);
}

void ATutorialGameMode::EnterRepairArrow()
{
	// 実配置証拠を初期化する（予め正しいセルでは代用しない）
	bRepairPlacementRecorded = false;
}

void ATutorialGameMode::EnterSpeedBaseline()
{
	// 生成停止と実行中動物の破棄（反転観察の残留を確実に掃く）
	ClearRuntimeAnimals();
	const FTutorialStepDef* Def = GetCurrentStepDef();
	if (!Def || !CachedMapManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] SpeedBaseline の準備に失敗: 地図またはステップ定義が無効"));
		return;
	}

	// 2026.10.09 Lee start（地図区域対調：閉環方向を P1 = DirRight / P2 = DirLeft へ入替。
	// 旧実装はコメントとして保持）
	// 出口セルを閉環方向へ置き換える（シナリオ準備のための設定。プレイヤー配置履歴は変更しない）
	// CachedMapManager->SetTileData(Def->P1TargetCell.X, Def->P1TargetCell.Y, ETileType::DirLeft, 0);
	// CachedMapManager->SetTileData(Def->P2TargetCell.X, Def->P2TargetCell.Y, ETileType::DirRight, 1);
	CachedMapManager->SetTileData(Def->P1TargetCell.X, Def->P1TargetCell.Y, ETileType::DirRight, 0);
	CachedMapManager->SetTileData(Def->P2TargetCell.X, Def->P2TargetCell.Y, ETileType::DirLeft, 1);
	// 2026.10.09 Lee end

	// 2026.10.08 Lee 第三批修正 start（基線動物 2 匹の生成成否を推進条件にする）。
	// 失敗時は次ポーリングで再試行する（動物なしで観察を完了させない）
	bSpeedBaselineAnimalsSpawned = false;
	TrySpawnBaselineAnimals();
	// 2026.10.08 Lee 第三批修正 end
}

// 2026.10.08 Lee 第三批修正 start（基線動物の生成と観察タイマーの設定）
void ATutorialGameMode::TrySpawnBaselineAnimals()
{
	if (bSpeedBaselineAnimalsSpawned)
	{
		return;
	}
	// 2026.10.09 Lee start（地図区域対調：基線動物の出発も入替。旧実装はコメントとして保持）
	// 【旧実装（保持）】
	// AAnimalBase* P1SideAnimal = SpawnTutorialAnimal(FIntPoint(2, 1), FVector(1.0f, 0.0f, 0.0f));
	// AAnimalBase* P2SideAnimal = SpawnTutorialAnimal(FIntPoint(9, 4), FVector(-1.0f, 0.0f, 0.0f));
	// 基線動物: P1 側は (9,4) から -X、P2 側は (2,1) から +X（外围ルートとゴールはそのまま）
	AAnimalBase* P1SideAnimal = SpawnTutorialAnimal(FIntPoint(9, 4), FVector(-1.0f, 0.0f, 0.0f));
	AAnimalBase* P2SideAnimal = SpawnTutorialAnimal(FIntPoint(2, 1), FVector(1.0f, 0.0f, 0.0f));
	// 2026.10.09 Lee end
	if (P1SideAnimal == nullptr || P2SideAnimal == nullptr)
	{
		// 半端な生成物を片付けて次ポーリングの再試行に備える（偽の成功をしない）
		UE_LOG(LogTemp, Error, TEXT("[TutorialGameMode] 基線動物の生成に失敗。再試行するまで観察を開始しません"));
		ClearRuntimeAnimals();
		return;
	}
	bSpeedBaselineAnimalsSpawned = true;

	// 通常速度の基線を SpeedPhaseDelay 秒だけ観察してから加速実習へ進む
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATutorialGameMode::AdvanceStep, SpeedPhaseDelay, false);
}
// 2026.10.08 Lee 第三批修正 end

void ATutorialGameMode::EnterSpeedSkill()
{
	bSkillAccepted = false;
	SetTutorialSkillsEnabled(true);
}

void ATutorialGameMode::EnterSpeedRecovered()
{
	// 通常速度への復帰を SpeedPhaseDelay 秒だけ観察する（その後の遷移は AdvanceStep が決める）
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &ATutorialGameMode::AdvanceStep, SpeedPhaseDelay, false);
}

// 2026.10.09 Lee start（地図区域対調：出現セルの入替）
APawn* ATutorialGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// 基底処理で PlayerID に応じた正しい Pawn 選択と InitCursor（旧規定位置）を済ませる。
	// AMainGameMode 側の通常対戦ロジックは変更しない
	APawn* SpawnedPawn = Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);

	// LocalPlayer の ControllerId から PlayerID を取得（0 = 1P / 1 = 2P。基底と同じ規約）
	int32 PlayerID = 0;
	if (const APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			PlayerID = LocalPlayer->GetControllerId();
		}
	}

	// 地図はキャッシュを優先しつつ、P2 生成が BeginPlay の地図取得より先に走る場合に備えて検索する
	AMapManager* MapManager = CachedMapManager;
	if (MapManager == nullptr)
	{
		MapManager = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass()));
	}

	ACursorPawn* CursorPawn = Cast<ACursorPawn>(SpawnedPawn);
	if (CursorPawn == nullptr || MapManager == nullptr)
	{
		return SpawnedPawn;
	}

	// 既存 InitCursor で出現セルだけ入れ替える: P1 = (MapWidth-1, MapHeight-1) / P2 = (0,0)。
	// PlayerID は基底が InitCursor へ渡した値（CursorPawn->PlayerID）をそのまま維持する
	if (PlayerID == 1)
	{
		CursorPawn->InitCursor(MapManager, CursorPawn->PlayerID, 0, 0);
	}
	else
	{
		CursorPawn->InitCursor(MapManager, CursorPawn->PlayerID, MapManager->MapWidth - 1, MapManager->MapHeight - 1);
	}

	return SpawnedPawn;
}
// 2026.10.09 Lee end（地図区域対調：出現セルの入替）
//==============================================================================
// 2026.10.08 Lee end（教程拡張：技能実習のライフサイクル・共有権限・演示動物）
//==============================================================================
