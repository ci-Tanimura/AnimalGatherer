// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gu/AnimalGathererGameModeBase.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
// 2026.10.06 Lee start（対戦フェーズ共有型の利用）
#include "GameTypes.h"
// 2026.10.06 Lee end
#include "MainGameMode.generated.h"

// 2026.07.24 Lee start
class ACursorPawn;
// 2026.07.24 Lee end

// 2026.10.06 Lee start（技能システム用の前方宣言）
class AMapManager;
class UMatchSkillEffectComponent;
class USkillDefinition;
// 2026.10.06 Lee end

// スコアが変わったことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChangedSignature, int32, NewP1Score, int32, NewP2Score);
// 残り時間が更新されたことを通知するデリゲート（引数：残り秒数）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeChangedSignature, int32, RemainingTime);
// ゲーム開始カウントダウン進捗を通知するデリゲート（引数：残りカウントダウン秒数）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCountdownChangedSignature, int32, RemainingCountdown);
// タイムアップを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeUpSignature);

/**
 *
 */
UCLASS()
class ANIMALGATHERER_API AMainGameMode : public AAnimalGathererGameModeBase
{
	GENERATED_BODY()

public:
	AMainGameMode();

	virtual void BeginPlay() override;

	// 2026.10.06 Lee start（破棄時のタイマー停止と共有効果解除のため EndPlay をオーバーライド）
	/** @brief 破棄時の後始末。全タイマーを停止し共有スキル効果を解除してから基底クラスへ委譲する。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 2026.10.06 Lee end

	// スコア加算処理
	UFUNCTION(BlueprintCallable, Category = "GameMode|Score")
	void AddScore(int32 PlayerID, int32 ScoreToAdd = 1);

	// タイムアップ時にゲームを終わらせる
	UFUNCTION(BlueprintCallable, Category = "GameMode|Flow")
	void EndGame();

	// 2025.09.07 Lee start
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	// 2025.09.07 Lee end

	// 得点表示更新用イベント
	UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
	FOnScoreChangedSignature OnScoreChanged;

	// 残り時間更新用イベント
	UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
	FOnTimeChangedSignature OnTimeChanged;

	// タイムアップ演出用イベント
	UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
	FOnTimeUpSignature OnTimeUp;

	// カウントダウン通知用イベント
	UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
	FOnCountdownChangedSignature OnCountdownChanged;

	// 2026.10.06 Lee start（試合ライフサイクルと技能の公開インターフェース）
	/** @brief 普通対戦フローか（チュートリアルは false）。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Match")
	bool IsNormalMatch() const;

	/** @brief 普通対戦が進行中か（Playing かつ現在時刻 < MatchEndTime の権威判定）。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Match")
	bool IsMatchPlaying() const;

	/** @brief 通常の移動・配置入力が許可されているか。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Match")
	bool IsGameplayInputAllowed() const;

	/** @brief 得点加算が許可されているか。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Match")
	bool IsScoringAllowed() const;

	/** @brief 本試合の共有スキル効果コンポーネントを取得する。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Skill")
	UMatchSkillEffectComponent* GetMatchSkillEffect() const;

	/** @brief 本試合に確定したマップを取得する（未確定は nullptr）。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Skill")
	AMapManager* GetMatchMap() const;

	/** @brief 1P の現在スコア。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Score")
	int32 GetP1Score() const;

	/** @brief 2P の現在スコア。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Score")
	int32 GetP2Score() const;

	/** @brief 残り時間（秒・切り上げ）。Ready 中は設定値、終了後は 0。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Timer")
	int32 GetTimeRemaining() const;

	/** @brief 開始前カウントダウンの残り秒。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Timer")
	int32 GetCountdownRemaining() const;

	/** @brief 現在の対戦フェーズ。 */
	UFUNCTION(BlueprintPure, Category = "GameMode|Match")
	EMatchPhase GetMatchPhase() const;

	/**
	 * @brief 双方の SkillSystemComponent と共有効果の本試合初期化を試みる（Ready 中に準備）。
	 *        実際の LocalPlayer 身分と PC のバインド地図を検証し、定義資産を優先する。
	 *        資産が未設定の場合のみ C++ 既定オブジェクトへフォールバックし、
	 *        設定済みだが不備の資産はフォールバックせず当該技能を無効化する。
	 *        未準備は false を返し、リトライタイマーで再試行する。
	 * @return 双方の初期化が完了した場合 true。
	 */
	bool TryInitializeMatchSkills();
	// 2026.10.06 Lee end

	// 2026.10.08 Lee start（教程拡張用の共有権限インターフェース）
	/**
	 * @brief このモードが技能システムの初期化を支援するか。
	 *        既定は普通対戦フロー（bUseNormalMatchFlow）のみ true。教程側は覆写して許可する。
	 * @return 初期化を支援する場合 true。
	 */
	virtual bool SupportsSkillInitialization() const;

	/**
	 * @brief 指定プレイヤー・スロットの技能使用が現時点で許可されるか。
	 *        既定は身分 0/1・スロット 0/1 のみ有効とし、普通対戦の IsMatchPlaying 権威判定に従う。
	 * @param PlayerId 使用を試みるプレイヤーID（0 = 1P / 1 = 2P）。
	 * @param SlotIndex スロット番号（0 = 反転 / 1 = 加速）。
	 * @return 許可される場合 true。不正な身分・スロットは false。
	 */
	virtual bool IsSkillUseAllowed(uint8 PlayerId, int32 SlotIndex) const;

	/**
	 * @brief 共有効果（加速）の効果文脈が現時点で有効か。
	 *        既定は普通対戦の IsMatchPlaying に従う（Ready / Ended は無効）。
	 * @return 効果文脈が有効な場合 true。
	 */
	virtual bool IsSkillEffectContextActive() const;
	// 2026.10.08 Lee end

protected:
	UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
	int32 P1Score;

	UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
	int32 P2Score;

	// 制限時間（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Timer")
	int32 TotalGameTime = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;


	// 遷移先のリザルトレベル名
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Level")
	FName ResultLevelName = TEXT("LV_Result");

	// ゲーム終了時に再生する効果音
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Audio")
	USoundBase* TimeUpSound;

	// タイムアップSEが鳴ってからレベル遷移するまでの待ち時間（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Flow")
	float TimeUpDelay = 1.0f;

	// ゲーム開始前のカウントダウン時間（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Flow")
	float ReadyDelay = 3.0f;

	// 2026.07.24 Lee start
	/** @brief 1P用カーソル Pawn のブループリントクラス。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Cursor")
	TSubclassOf<ACursorPawn> CursorPawnClass_P1;

	/** @brief 2P用カーソル Pawn のブループリントクラス。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Cursor")
	TSubclassOf<ACursorPawn> CursorPawnClass_P2;
	// 2026.07.24 Lee end

	// 2026.10.06 Lee start（試合フェーズの権威状態）
	/** @brief 普通対戦フローを使うか。チュートリアルはコンストラクタで false にする。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameMode|Match")
	bool bUseNormalMatchFlow = true;

	/** @brief 現在の対戦フェーズ（権威値）。 */
	UPROPERTY(BlueprintReadOnly, Category = "GameMode|Match")
	EMatchPhase MatchPhase = EMatchPhase::Ready;

	/** @brief 対戦終了時刻（同 World の GetTimeSeconds 基準）。権威判定に使用。 */
	float MatchEndTime = 0.0f;
	// 2026.10.06 Lee end

private:
	// レベル上のスポーナーへの参照
	UPROPERTY()
	AAnimalSpawner* CachedAnimalSpawner;

	// 現在の残り時間
	int32 TimeRemaining;

	// 開始カウントダウン用のタイマーハンドル
	FTimerHandle ReadyTimerHandle;

	// タイマーを管理するためのハンドル
	FTimerHandle GameTimerHandle;

	// 演出用タイマーのハンドル
	FTimerHandle ResultDelayTimerHandle;

	// カウントダウン用タイマーで毎秒呼ぶ処理
	int32 CountdownRemaining;
	void AdvanceCountdown();

	// カウントダウン終了後にゲーム本編を開始
	void StartMatch();

	// プレイヤーの入力許可/不許可を切り替え
	void SetPlayersInputEnabled(bool bEnable);

	// 残り時間を減らす
	void AdvanceTimer();

	// レベル遷移を行う処理
	void TransitionToResultLevel();

	// 2026.10.06 Lee start（技能システムの内部状態と Spawner 一括制御）
	/** @brief 本試合の共有スキル効果（デフォルトサブオブジェクト）。 */
	UPROPERTY()
	TObjectPtr<UMatchSkillEffectComponent> MatchSkillEffect = nullptr;

	/** @brief 本試合に確定したマップ（TryInitializeMatchSkills で設定）。 */
	UPROPERTY()
	TObjectPtr<AMapManager> MatchMap = nullptr;

	/** @brief 反転スキルの定義資産（任意。空・不備なら C++ 既定オブジェクトへフォールバック）。 */
	UPROPERTY(EditAnywhere, Category = "GameMode|Skill")
	TObjectPtr<USkillDefinition> ReverseArrowsSkillDefinition = nullptr;

	/** @brief 加速スキルの定義資産（任意。空・不備なら C++ 既定オブジェクトへフォールバック）。 */
	UPROPERTY(EditAnywhere, Category = "GameMode|Skill")
	TObjectPtr<USkillDefinition> SpeedUpAnimalsSkillDefinition = nullptr;

	/** @brief 技能初期化リトライの間隔（秒）。 */
	UPROPERTY(EditAnywhere, Category = "GameMode|Skill")
	float SkillInitRetryInterval = 0.25f;

	/** @brief 双方の技能初期化済みか（一度きり。HUD 再構築・断線で補填しない）。 */
	bool bSkillSystemsInitialized = false;

	/** @brief 技能初期化のリトライタイマー。 */
	FTimerHandle SkillInitRetryTimerHandle;

	/** @brief 本試合と同地図の CachedAnimalSpawner 以外の Spawner を一括開始する。 */
	void StartMatchingSpawners();

	/** @brief 本試合と同地図の CachedAnimalSpawner 以外の Spawner を一括停止する。 */
	void StopMatchingSpawners();
	// 2026.10.06 Lee end

	// 2026.10.08 Lee start（教程拡張：実初期化部の抽出）
	// 2026.10.08 Lee 第二批修正 start（チュートリアル GameMode から呼ぶため宣言を private から protected へ移動。
	//  旧 private 位置はコメントとして保持。以降に他の private メンバーは無い）
	// private:
	/**
	 * @brief 技能システムの実初期化（プレイヤー/地図/World/定義検証 → MatchMap・共有効果バインド → 冪等初期化）。
	 *        試合の開始（StartMatch・スポーナー起動・使用解禁）は行わない。
	 *        門番は SupportsSkillInitialization と冪等フラグのみで、Ready 等の段階制約は
	 *        呼び出し側（TryInitializeMatchSkills または教程側）の責務とする。
	 * @return 双方の初期化が完了した場合 true。未整備時は false で再試行可能。
	 */
protected:
	bool InitializeSkillSystemsForMode();
	// 2026.10.08 Lee 第二批修正 end
	// 2026.10.08 Lee end
};