// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gu/AnimalGathererGameModeBase.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "MainGameMode.generated.h"

// 2026.07.24 Lee start
class ACursorPawn;
// 2026.07.24 Lee end

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

protected:
    UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
    int32 P1Score;

    UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
    int32 P2Score;

    // 制限時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Timer")
    int32 TotalGameTime = 3;

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
};