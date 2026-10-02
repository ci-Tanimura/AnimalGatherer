// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameTypes.h"
#include "Tanimura/MainGameMode.h"
#include "Lee/TutorialHighlightActor.h"
#include "TutorialGameMode.generated.h"

class UUserWidget;
class ACursorPawn;
class AMapManager;

/**
 * @brief チュートリアルの進行ステップ。
 *        Intro → MoveCursor → PlaceArrow → ScoreGoal → Complete の順で進む。
 */
UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	/** @brief 導入説明（ゲーム目標の提示）。IntroDuration 経過で自動遷移。 */
	Intro,
	/** @brief カーソル移動の練習（目標セルへ移動する）。 */
	MoveCursor,
	/** @brief 矢印設置の練習（目標セルへ正しい向きの矢印を置く）。 */
	PlaceArrow,
	/** @brief 動物をゴールへ誘導して得点する。 */
	ScoreGoal,
	/** @brief チュートリアル完了。CompleteDelay 後にレベル遷移。 */
	Complete
};

// ステップが変わったことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTutorialStepChanged, ETutorialStep, NewStep);
// プレイヤーのステップ達成状態が変わったことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTutorialPlayerDone, uint8, PlayerID, bool, bDone);
// チュートリアル完了を通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTutorialComplete);

/**
 * @brief MoveCursor / PlaceArrow / ScoreGoal の 3 ステップ共通の目標定義。
 *        1 要素で P1 / P2 両方の目標セルと正解方向を記述する。
 */
USTRUCT(BlueprintType)
struct FTutorialStepDef
{
	GENERATED_BODY()

	/** @brief 1P の目標セル（干渉セル）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P1TargetCell = FIntPoint(3, 1);

	/** @brief 2P の目標セル（干渉セル）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P2TargetCell = FIntPoint(8, 4);

	/** @brief 1P が干渉セルに置くべき正解方向（GoalP1 へ向く向き）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETileType P1ExpectedDirection = ETileType::DirDown;

	/** @brief 2P が干渉セルに置くべき正解方向（GoalP2 へ向く向き）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETileType P2ExpectedDirection = ETileType::DirUp;

	/** @brief 1P のゴールセル（ScoreGoal ステップのハイライト対象）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P1GoalCell = FIntPoint(3, 0);

	/** @brief 2P のゴールセル（ScoreGoal ステップのハイライト対象）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P2GoalCell = FIntPoint(8, 5);
};

/**
 * @brief チュートリアルで事前に敷き詰めるタイル 1 マス分の定義。
 *        BeginPlay で AMapManager::SetTileData により適用される。
 */
USTRUCT(BlueprintType)
struct FTutorialTileDef
{
	GENERATED_BODY()

	/** @brief セルのグリッドX座標。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 CellX = 0;

	/** @brief セルのグリッドY座標。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	int32 CellY = 0;

	/** @brief 設置するタイル種類。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETileType TileType = ETileType::Empty;

	/** @brief 所有プレイヤーID（0 = 1P 青 / 1 = 2P 赤、方向タイルのみ有効）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	uint8 OwnerPlayerId = 0;
};

/**
 * @brief 2P 同図の基本操作チュートリアル用 GameMode。
 *
 *        AMainGameMode を継承してカーソル生成・スコア判定の仕組みを流用しつつ、
 *        ポーリング型のステップ進行で「カーソル移動 → 矢印設置 → ゴール得点」を教える。
 *        全検出は 0.2 秒間隔のポーリングで行い、デリゲートは Widget 通知専用。
 */
UCLASS()
class ANIMALGATHERER_API ATutorialGameMode : public AMainGameMode
{
	GENERATED_BODY()

public:
	ATutorialGameMode();

	virtual void BeginPlay() override;

	// 2026.10.02 Lee start
	/** @brief 毎フレーム：カーソルがハイライトセル上にいる間そのハイライトを非表示にする。 */
	virtual void Tick(float DeltaTime) override;
	// 2026.10.02 Lee end

	/** @brief 現在のチュートリアルステップを取得する。 */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	ETutorialStep GetCurrentStep() const;

	// 2026.10.02 Lee start
	/**
	 * @brief チュートリアル中の矢印配置可否を判定する（PlayerController から呼ばれる）。
	 *        自プレイヤーの干渉セル以外には配置できない（環状レーンの保護）。
	 * @param PlayerID 判定するプレイヤーID（0 = 1P / 1 = 2P）。
	 * @param Cell 配置しようとしているセル座標。
	 * @return 配置可能な場合 true。Intro / Complete 中は常に false。
	 */
	bool IsPlacementAllowed(uint8 PlayerID, FIntPoint Cell) const;
	// 2026.10.02 Lee end

	// ステップ変化通知（Widget が購読する）
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FOnTutorialStepChanged OnTutorialStepChanged;

	// プレイヤー達成状態の変化通知（Widget が購読する）
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FOnTutorialPlayerDone OnTutorialPlayerDone;

	// チュートリアル完了通知（Widget が購読する）
	UPROPERTY(BlueprintAssignable, Category = "Tutorial|Events")
	FOnTutorialComplete OnTutorialComplete;

protected:
	/** @brief チュートリアル用 HUD のクラス（WBP_Tutorial）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|UI")
	TSubclassOf<UUserWidget> TutorialWidgetClass;

	/** @brief ハイライト用マテリアル（Vector パラメータ HighlightColor 必須）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|UI")
	TObjectPtr<UMaterialInterface> HighlightMaterial = nullptr;

	/** @brief ハイライトアクターのクラス（既定 = C++ の ATutorialHighlightActor）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|UI")
	TSubclassOf<ATutorialHighlightActor> HighlightActorClass;

	/** @brief チュートリアル完了後の遷移先レベル名。None ならその場に留まる。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	FName TutorialEndLevelName = TEXT("LV_MainGame");

	/** @brief Intro ステップの表示時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	float IntroDuration = 3.0f;

	/** @brief Complete 演出表示からレベル遷移までの待ち時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	float CompleteDelay = 3.0f;

	/** @brief ステップ達成判定のポーリング間隔（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	float PollInterval = 0.2f;

	/** @brief ステップ目標定義（1 要素で P1 / P2 両方を記述）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Steps")
	TArray<FTutorialStepDef> Steps;

	/** @brief 事前に敷き詰めるタイル定義（環状レーンのレイアウト）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Map")
	TArray<FTutorialTileDef> PreplacedTiles;

private:
	// ポーリング判定の本体（MoveCursor / PlaceArrow / ScoreGoal のみ処理）
	void TickTutorialCheck();

	// 指定プレイヤーが現在のステップを達成したか
	bool IsPlayerStepDone(int32 PlayerID);

	// 両プレイヤー達成時に次のステップへ進む
	void AdvanceStep();

	// ステップ開始処理（デリゲート通知 + 入口副作用 + ハイライト更新）
	void EnterStep(ETutorialStep NewStep);

	// ScoreGoal ステップの入口処理（全スポーナーで動物供給を開始）
	void EnterScoreGoal();

	// Complete ステップの入口処理（完了演出 + 遷移タイマー）
	void EnterComplete();

	// チュートリアル終了時のレベル遷移
	void FinishTutorial();

	// PreplacedTiles を AMapManager::SetTileData で適用する
	void ApplyPreplacedTiles();

	// 現ステップのハイライトを再生成（ScoreGoal はゴールセル、他は干渉セル）
	void RefreshHighlights();

	// ハイライトを全削除
	void ClearHighlights();

	// ハイライトを 1 枚スポーンして P1 / P2 の参照へ登録する
	// 2026.10.02 Lee start
	// （カーソル重なり時の個別非表示のため、プレイヤーごとの参照を保持する）
	ATutorialHighlightActor* SpawnHighlightFor(int32 PlayerID, FIntPoint Cell, const FLinearColor& Color);
	// 2026.10.02 Lee end

	// 2026.10.02 Lee start
	// カーソルがハイライトセル上にいる間、そのハイライトを非表示にする（毎フレーム）
	void UpdateHighlightVisibility();
	// 2026.10.02 Lee end

	// 指定プレイヤー（0 / 1）のカーソル Pawn を取得する（null 許容）
	ACursorPawn* GetCursorPawn(int32 PlayerID) const;

	// 現ステップに対応する目標定義を取得する（未定義なら nullptr）
	const FTutorialStepDef* GetCurrentStepDef() const;

	// レベル上のマップマネージャ（BeginPlay で取得）
	UPROPERTY()
	TObjectPtr<AMapManager> CachedMapManager = nullptr;

	// 生成したチュートリアル HUD
	UPROPERTY()
	TObjectPtr<UUserWidget> TutorialWidget = nullptr;

	// 現在のステップ
	ETutorialStep CurrentStep = ETutorialStep::Intro;

	// 各プレイヤーの現ステップ達成フラグ
	bool bP1Done = false;
	bool bP2Done = false;

	// ScoreGoal 判定用のステップ開始時スコア基準値
	int32 P1ScoreAtStepStart = 0;
	int32 P2ScoreAtStepStart = 0;

	// 達成判定ポーリング用タイマー
	FTimerHandle PollTimerHandle;

	// Intro / Complete のフェーズ進行用タイマー
	FTimerHandle PhaseTimerHandle;

	// 2026.10.02 Lee start
	// 表示中のハイライト（P1 = 青 / P2 = 赤）とその対象セル
	UPROPERTY()
	TObjectPtr<ATutorialHighlightActor> P1Highlight = nullptr;
	UPROPERTY()
	TObjectPtr<ATutorialHighlightActor> P2Highlight = nullptr;
	FIntPoint P1HighlightCell = FIntPoint(ForceInit);
	FIntPoint P2HighlightCell = FIntPoint(ForceInit);
	// 2026.10.02 Lee end
};
