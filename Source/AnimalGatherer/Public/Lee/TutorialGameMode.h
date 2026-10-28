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
// 2026.10.08 Lee start（教程拡張：演示動物と技能コンポーネントの参照）
class AAnimalBase;
class USkillSystemComponent;
// 2026.10.08 Lee end

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
	Complete,
	// 2026.10.08 Lee start（教程拡張：技能実習ステップの追加。既存値 0〜4 は不変）
	/** @brief 反転スキルの実習（CurrentActivePlayerId の LB のみ解禁）。 */
	ReverseSkill,
	/** @brief 反転結果の観察（演示動物が返却セルを実際に読み取るのを待つ）。 */
	ObserveReverse,
	// 2026.10.14 Lee start（双方反転還元方式：本ステップは「施放者が反転スキルをもう一度施して
	//  相手の矢印を元の向きへ戻す還元段階」へ意味を変更。列挙値と順序は不変。旧説明はコメント保持）
	/** @brief 還元段階（現在の施放者が反転スキルの再施放で相手の干渉セルを元へ戻す）。 */
	// /** @brief 影響を受けたプレイヤーによる矢印修復（目標セルへの再配置）。 ←元のコードは消さない
	RepairArrow,
	// 2026.10.14 Lee end
	/** @brief 加速実習前の基線観察（閉環ルート上を通常速度で走らせる）。 */
	SpeedBaseline,
	/** @brief 加速スキルの実習（CurrentActivePlayerId の RB のみ解禁）。 */
	SpeedSkill,
	/** @brief 加速効果の観察（共有効果スナップショットの実際の終了を待つ）。 */
	ObserveSpeed,
	/** @brief 加速復帰の観察（通常速度へ戻ったことを SpeedPhaseDelay 秒確認する）。 */
	SpeedRecovered
	// 2026.10.08 Lee end
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

	// 2026.10.09 Lee start（地図区域対調：P1 を (8,4) 側、P2 を (3,1) 側へ変更。
	// 正解方向とゴールセルも対側へ入れ替える。旧既定値は各項の下にコメント保持）
	/** @brief 1P の目標セル（干渉セル）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P1TargetCell = FIntPoint(8, 4);
	// FIntPoint P1TargetCell = FIntPoint(3, 1); ←元のコードは消さない

	/** @brief 2P の目標セル（干渉セル）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P2TargetCell = FIntPoint(3, 1);
	// FIntPoint P2TargetCell = FIntPoint(8, 4); ←元のコードは消さない

	/** @brief 1P が干渉セルに置くべき正解方向（GoalP1 へ向く向き）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETileType P1ExpectedDirection = ETileType::DirUp;
	// ETileType P1ExpectedDirection = ETileType::DirDown; ←元のコードは消さない

	/** @brief 2P が干渉セルに置くべき正解方向（GoalP2 へ向く向き）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ETileType P2ExpectedDirection = ETileType::DirDown;
	// ETileType P2ExpectedDirection = ETileType::DirUp; ←元のコードは消さない

	/** @brief 1P のゴールセル（ScoreGoal ステップのハイライト対象）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P1GoalCell = FIntPoint(8, 5);
	// FIntPoint P1GoalCell = FIntPoint(3, 0); ←元のコードは消さない

	/** @brief 2P のゴールセル（ScoreGoal ステップのハイライト対象）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	FIntPoint P2GoalCell = FIntPoint(3, 0);
	// FIntPoint P2GoalCell = FIntPoint(8, 5); ←元のコードは消さない
	// 2026.10.09 Lee end
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

	// 2026.10.09 Lee start（地図区域対調：出現位置の入替のため生成を上書きする）
	/**
	 * @brief 基底の生成処理で正しいプレイヤー Pawn を選んだうえで、
	 *        既存 InitCursor により出現セルを入れ替える（P1 = (MapWidth-1, MapHeight-1) / P2 = (0,0)）。
	 *        Map と PlayerID の整合は維持し、通常対戦の AMainGameMode 側は変更しない。
	 * @param NewPlayer 生成対象のコントローラー。
	 * @param StartSpot レベル配置の開始地点（教程では使用しない）。
	 * @return 生成した Pawn。
	 */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	// 2026.10.09 Lee end

	// 2026.10.08 Lee start（教程拡張：技能実習のライフサイクルと共有権限の覆写）
	/** @brief 破棄時の後始末。全タイマーを停止し技能通知デリゲートを解除する。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief チュートリアルは技能初期化を支援する（常に true）。 */
	virtual bool SupportsSkillInitialization() const override;

	// 2026.10.14 Lee start（還元段階の権限追加を注記に反映。旧説明はコメント保持）
	/**
	 * @brief 技能使用の許可。ReverseSkill 中は操作者の反転スロットのみ、
	 *        SpeedSkill 中は操作者の加速スロットのみ許可する（成功受領後は即拒否）。
	 *        2026.10.14 からは RepairArrow（還元）中も操作者の反転スロットのみ許可する。
	 * @param PlayerId 使用を試みるプレイヤーID（0 = 1P / 1 = 2P）。
	 * @param SlotIndex スロット番号（0 = 反転 / 1 = 加速）。
	 * @return 許可される場合 true。
	 */
	// 【旧説明（保持）】ReverseSkill / SpeedSkill のみの記載であった。
	virtual bool IsSkillUseAllowed(uint8 PlayerId, int32 SlotIndex) const override;
	// 2026.10.14 Lee end

	/**
	 * @brief 共有効果の文脈。地図バインド済みかつ Complete 前は有効。
	 *        成功受領による段階切替で実行中の加速効果を無効化しない。
	 * @return 効果文脈が有効な場合 true。
	 */
	virtual bool IsSkillEffectContextActive() const override;

	// 2026.10.14 Lee start（双方反転還元方式：本 API は記録動作を停止し互換のため署名のみ維持。
	//  進行は反転スキルの実際成功のみで判定する。旧説明はコメント保持）
	/**
	 * @brief 実際の矢印配置完了の受領（PlayerController::PlaceDirection から呼ばれる）。
	 *        2026.10.14 以降はいずれの段階でも記録せず、即座に返す（旧の修復証拠記録は廃止）。
	 * @param PlayerId 配置したプレイヤーID（0 = 1P / 1 = 2P）。
	 * @param Cell 配置先セル。
	 * @param Direction 配置した方向。
	 */
	// 【旧説明（保持）】修復段階で影響対象プレイヤー・目標セル・正解方向の配置のみ記録する。
	void NotifyArrowPlaced(uint8 PlayerId, FIntPoint Cell, ETileType Direction);
	// 2026.10.14 Lee end

	/** @brief 現在の操作プレイヤーID（技能実習の施放者。0 = 1P / 1 = 2P）。 */
	UFUNCTION(BlueprintPure, Category = "Tutorial|Skill")
	uint8 GetCurrentActivePlayerId() const;

	/**
	 * @brief 基礎ステップの達成状態（技能実習では現在の操作者成否を返す）。
	 * @param PlayerId プレイヤーID（0 / 1）。
	 * @return 達成済みの場合 true。
	 */
	UFUNCTION(BlueprintPure, Category = "Tutorial|State")
	bool IsTutorialPlayerDone(uint8 PlayerId) const;

	/** @brief 現ステップの技能使用が成功受領済みか（HUD の遅延バインド用）。 */
	UFUNCTION(BlueprintPure, Category = "Tutorial|Skill")
	bool HasCurrentSkillSucceeded() const;

	/** @brief 観察待ち（ObserveReverse / ObserveSpeed）か。 */
	UFUNCTION(BlueprintPure, Category = "Tutorial|State")
	bool IsTutorialWaitingObservation() const;

	/** @brief 基線観察待ち（SpeedBaseline）か。 */
	UFUNCTION(BlueprintPure, Category = "Tutorial|State")
	bool IsTutorialWaitingBaseline() const;

	// 2026.10.08 Lee 第三批 start（HUD のボタン強調算出用の公開読み取り）
	/**
	 * @brief 指定プレイヤーの目標セルに置くべき正解方向を取得する。
	 * @param PlayerId プレイヤーID（0 / 1）。
	 * @return 正解方向。ステップ定義が無効な場合は Empty。
	 */
	UFUNCTION(BlueprintPure, Category = "Tutorial|State")
	ETileType GetExpectedDirectionFor(uint8 PlayerId) const;
	// 2026.10.08 Lee 第三批 end
	// 2026.10.08 Lee end

	// 2026.10.21 Lee start（チュートリアル長押しスキップ）
	/**
	 * @brief チュートリアルの長押しスキップ要求（PlayerController から受領する）。
	 *        冪等。受領後は進行・技能・配置の権限を封鎖し、達成ポーリングとフェーズタイマー、
	 *        共有効果・動物・ハイライトを片付けてから、既存 FinishTutorial 経路でレベル遷移する。
	 *        完了演出（OnTutorialComplete）は偽装放送せず、通常完了経路の表示と遅延は変更しない。
	 *        遷移先が未設定の場合は教程を壊さず拒否してログに記録する。
	 * @param PlayerId 要求したプレイヤーID（0 = 1P / 1 = 2P）。
	 */
	void RequestSkipTutorial(uint8 PlayerId);

	/** @brief スキップ要求の受領済みか（権限封鎖と冪等判定に使う）。 */
	bool HasTutorialSkipBeenRequested() const;
	// 2026.10.21 Lee end

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
	// 2026.10.28 Lee start
	// FName TutorialEndLevelName = TEXT("LV_MainGame");
	FName TutorialEndLevelName = TEXT("LV_NewMap");
	// 2026.10.28 Lee end

	// 2026.10.08 Lee start（教程拡張：基線・復帰観察の待ち時間）
	/** @brief SpeedBaseline / SpeedRecovered の観察時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	float SpeedPhaseDelay = 1.0f;
	// 2026.10.08 Lee end

	/** @brief Intro ステップの表示時間（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	float IntroDuration = 3.0f;

	/** @brief Complete 演出表示からレベル遷移までの待ち時間（秒）。
	 *         2026.10.08 Lee：既定 3 → 5 秒へ変更（操作早見表の提示時間。旧値は下にコメント保持）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Tutorial|Flow")
	float CompleteDelay = 5.0f;
	// float CompleteDelay = 3.0f; ←元のコードは消さない

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

	// 2026.10.08 Lee start（教程拡張：技能実習の内部ヘルパー群）
	/** @brief 基礎 3 ステップ（MoveCursor / PlaceArrow / ScoreGoal）の達成ポーリング。 */
	void TickBasicStepCheck();

	/** @brief 技能実習ステップのポーリング（成功受領・観察・修復・基線の各待ち）。 */
	void TickSkillStepCheck();

	/** @brief 技能システムの初期化を試みる（未整備時は呼び出し元の次ポーリングで再試行）。 */
	void TryInitializeTutorialSkills();

	/** @brief 双方の技能使用可否フラグを一括設定する（権限の詳細は IsSkillUseAllowed 側）。 */
	void SetTutorialSkillsEnabled(bool bEnabled);

	/** @brief 技能使用成功の受領（OnSkillUsed）。段階切替は行わず受領フラグのみ立てる。 */
	UFUNCTION()
	void HandleSkillUsed(int32 SlotIndex);

	/** @brief 技能使用失敗の受領（OnSkillUseFailed）。エラーログのみ。 */
	UFUNCTION()
	void HandleSkillUseFailed(int32 SlotIndex, ESkillUseResult Result);

	/** @brief 技能通知デリゲートの購読を解除する。 */
	void UnbindSkillDelegates();

	/** @brief チュートリアル地図の全スポーナーで生成を停止する。 */
	void StopTutorialSpawners();

	/** @brief チュートリアル地図に属する実行中の動物を全て破棄する（演示動物を含む）。 */
	void ClearRuntimeAnimals();

	/**
	 * @brief 既存スポーナーの AnimalClass を使い、指定セル中心に确定方向の演示動物を生成する。
	 *        deferred spawn で地図と格長を確定させてから Finish 後に方向を与える。
	 * @param Cell 出発セル。
	 * @param MoveDirection 出発方向（世界軸の単位ベクトル）。
	 * @return 生成した動物。失敗時は nullptr（エラーログ済み）。
	 */
	AAnimalBase* SpawnTutorialAnimal(FIntPoint Cell, const FVector& MoveDirection);

	/** @brief ReverseSkill ステップの入口処理（受領フラグの初期化と技能の準備）。 */
	void EnterReverseSkill();

	/** @brief ObserveReverse ステップの入口処理（演示動物の生成と返却セルの記録）。 */
	void EnterObserveReverse();

	// 2026.10.14 Lee start（還元入口の説明を更新。旧説明はコメント保持）
	/** @brief RepairArrow（還元）ステップの入口処理（成功門の再武装と技能可否の維持）。 */
	// /** @brief RepairArrow ステップの入口処理（実配置証拠の初期化）。 ←元のコードは消さない
	void EnterRepairArrow();
	// 2026.10.14 Lee end

	/** @brief SpeedBaseline ステップの入口処理（閉環化・基線動物の生成・観察タイマー）。 */
	void EnterSpeedBaseline();

	/** @brief SpeedSkill ステップの入口処理（受領フラグの初期化と技能の準備）。 */
	void EnterSpeedSkill();

	/** @brief SpeedRecovered ステップの入口処理（復帰観察タイマー）。 */
	void EnterSpeedRecovered();

	// 2026.10.08 Lee 第三批修正 start（基線動物の生成成否を推進条件にする）
	/**
	 * @brief 基線動物 2 匹の生成を試みる。双方が成功した場合のみ観察タイマーを設定する。
	 *        失敗時は半端な生成物を片付け、次ポーリングで再試行する（動物なしで進めない）。
	 */
	void TrySpawnBaselineAnimals();

	/** @brief 基線動物の生成が完了済みか（SpeedBaseline の再試行判定用）。 */
	bool bSpeedBaselineAnimalsSpawned = false;
	// 2026.10.08 Lee 第三批修正 end
	// 2026.10.08 Lee end

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

	// 2026.10.08 Lee start（教程拡張：技能実習の内部状態）
	/** @brief 反転スロットの番号（SkillSystemComponent の初期化順序に一致）。 */
	static constexpr int32 SkillSlotIndex_Reverse = 0;

	/** @brief 加速スロットの番号（SkillSystemComponent の初期化順序に一致）。 */
	static constexpr int32 SkillSlotIndex_Speed = 1;

	/** @brief 現ステップの技能施放者（反転の影響対象は 1 - この値）。 */
	uint8 CurrentActivePlayerId = 0;

	// 2026.10.14 Lee start（返工：ReverseRound は 4 回の施放それぞれに 1 対 1 対応する 0..3 の施放番号。
	//  0 = P1 の 1 回目反転（P2 矢印）/ 1 = P2 の 1 回目反転（P1 矢印）/
	//  2 = P1 の還元（P2 矢印への 2 回目反転）/ 3 = P2 の還元（P1 矢印への 2 回目反転）。
	//  旧の「ペア輪回 0/1」説明はコメント保持）
	/** @brief 4 回の反転施放の番号（0..3）。0 = P1 の 1 回目反転（P2 矢印）、
	 *         1 = P2 の 1 回目反転（P1 矢印）、2 = P1 の還元（P2 矢印）、3 = P2 の還元（P1 矢印）。
	 *         CurrentActivePlayerId のみでの輪回判断を避けるための明示状態。 */
	// 【旧説明（保持）】0 = 1P→2P の反転・観察・還元 / 1 = 2P→1P の反転・観察・還元 のペア輪回。
	uint8 ReverseRound = 0;
	// 2026.10.14 Lee end

	/** @brief 現ステップの技能成功受領フラグ（受領時に即ロックし同フレームの再施放を拒む）。 */
	bool bSkillAccepted = false;

	// 2026.10.14 Lee start（還元方式では進行証拠として使用しない旨を注記。旧説明はコメント保持）
	/** @brief 旧の修復配置証拠。2026.10.14 以降は進行に使用しない（NotifyArrowPlaced が記録しないため常時 false。互換で保持）。 */
	// /** @brief 修復段階での実配置証拠（NotifyArrowPlaced による本物の配置のみ true）。 ←元のコードは消さない
	bool bRepairPlacementRecorded = false;
	// 2026.10.14 Lee end

	/** @brief チュートリアル技能の初期化と委譲購読が完了済みか。 */
	bool bTutorialSkillsInitialized = false;

	/** @brief チュートリアル技能の使用可否フラグの現在値（初期化完了前の要求を保持する）。 */
	bool bTutorialSkillsEnabled = false;

	/** @brief 技能初期化失敗の警告済みフラグ（ログの刷り付き防止）。 */
	bool bLoggedSkillInitFailure = false;

	/** @brief 観察用の演示動物（反転観察の主体）。 */
	UPROPERTY()
	TObjectPtr<AAnimalBase> DemoAnimal = nullptr;

	/** @brief この教程で生成した動物の一覧（破棄漏れ防止のため全て登録する）。 */
	UPROPERTY()
	TArray<TObjectPtr<AAnimalBase>> TutorialAnimals;

	/** @brief 反転観察の演示動物の出発セル。 */
	FIntPoint ObserveSpawnCell = FIntPoint(ForceInit);

	/** @brief 反転観察の演示動物の出発方向。 */
	FVector ObserveSpawnDirection = FVector::ZeroVector;

	/** @brief 反転観察で演示動物が実際に読み取るべき返却セル。 */
	FIntPoint ObserveReturnCell = FIntPoint(ForceInit);
	// 2026.10.08 Lee end

	// 2026.10.21 Lee start（チュートリアル長押しスキップ）
	/** @brief 長押しスキップ要求の受領済みフラグ（冪等保証と権限封鎖に使う）。 */
	bool bSkipRequested = false;
	// 2026.10.21 Lee end
};
