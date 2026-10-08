// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/TutorialGameMode.h"
#include "Lee/TutorialHighlightActor.h"
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

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
bool ATutorialGameMode::IsPlacementAllowed(uint8 PlayerID, FIntPoint Cell) const
{
	// Intro / Complete 中は一切の配置を禁止する
	if (CurrentStep != ETutorialStep::MoveCursor
		&& CurrentStep != ETutorialStep::PlaceArrow
		&& CurrentStep != ETutorialStep::ScoreGoal)
	{
		return false;
	}

	// 自プレイヤーの干渉セル以外には配置できない（環状レーンの保護）
	const FTutorialStepDef* Def = GetCurrentStepDef();
	if (!Def)
	{
		return false;
	}
	const FIntPoint AllowedCell = (PlayerID == 0) ? Def->P1TargetCell : Def->P2TargetCell;
	return Cell == AllowedCell;
}
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

void ATutorialGameMode::TickTutorialCheck()
{
	// Intro / Complete はタイマー主導のためポーリング対象外
	if (CurrentStep != ETutorialStep::MoveCursor
		&& CurrentStep != ETutorialStep::PlaceArrow
		&& CurrentStep != ETutorialStep::ScoreGoal)
	{
		return;
	}

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
	case ETutorialStep::ScoreGoal:   NextStep = ETutorialStep::Complete;   break;
	case ETutorialStep::Complete:    return;
	default: return;
	}

	EnterStep(NextStep);
}

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

void ATutorialGameMode::RefreshHighlights()
{
	ClearHighlights();

	if (!CachedMapManager || !HighlightActorClass)
	{
		return;
	}

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
