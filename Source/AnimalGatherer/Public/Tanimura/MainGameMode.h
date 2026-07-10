// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gu/AnimalGathererGameModeBase.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "MainGameMode.generated.h"

// スコアが変わったことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChangedSignature, int32, NewP1Score, int32, NewP2Score);
// 残り時間が更新されたことを通知するデリゲート（引数：残り秒数）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeChangedSignature, int32, RemainingTime);
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

    // 得点表示更新用イベント
    UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
    FOnScoreChangedSignature OnScoreChanged;

    // 残り時間更新用イベント
    UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
    FOnTimeChangedSignature OnTimeChanged;

    // タイムアップ演出用イベント
    UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
    FOnTimeUpSignature OnTimeUp;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
    int32 P1Score;

    UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
    int32 P2Score;

    // 制限時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameMode|Timer")
    int32 TotalGameTime = 120;

private:
    // レベル上のスポーナーへの参照
    UPROPERTY()
    AAnimalSpawner* CachedAnimalSpawner;

    // 現在の残り時間
    int32 TimeRemaining;

    // タイマーを管理するためのハンドル
    FTimerHandle GameTimerHandle;

    // 残り時間を減らす
    void AdvanceTimer();
};