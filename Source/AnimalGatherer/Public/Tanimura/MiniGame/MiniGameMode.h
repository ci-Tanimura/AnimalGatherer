// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MiniGameMode.generated.h"

class AController;
class APlayerController;
class APlayerCharacter;

// 試合終了を通知するデリゲート（引数：勝利プレイヤー番号、-1は引き分け）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchEndSignature, int32, WinnerPlayerIndex);

/**
 * ミニゲーム用ゲームモード
 * プレイヤー生成、足元カラーの割り当て、生存者管理と勝利判定を担う
 */
UCLASS()
class ANIMALGATHERER_API AMiniGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMiniGameMode();

    // 追加プレイヤーのログイン時に生存リストへ登録する
    virtual void PostLogin(APlayerController* NewPlayer) override;

    // 試合終了通知イベント
    UPROPERTY(BlueprintAssignable, Category = "MiniGame|Events")
    FOnMatchEndSignature OnMatchEnd;

protected:
    virtual void BeginPlay() override;

    // プレイヤー生成時に足元円へカラーを割り当てる
    virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

    // 対戦人数（2〜4人）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MiniGame|Settings", meta = (ClampMin = "2", ClampMax = "4"))
    int32 NumberOfPlayers = 2;

    // プレイヤーが操作するPawnクラス（BP_PlayerCharacter を指定）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MiniGame|Settings")
    TSubclassOf<APlayerCharacter> PlayerPawnClass;

    // プレイヤー番号に対応する足元円のカラー
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniGame|Settings")
    TArray<FLinearColor> PlayerColors;

private:
    // プレイヤー死亡デリゲートから呼ばれ、生存リストから除外して勝敗を判定する
    UFUNCTION()
    void HandlePlayerDied(AController* DeadController);

    // 2人目以降のローカルプレイヤーを生成する
    void SpawnExtraPlayers();

    // 生存プレイヤーが1人以下になったら勝敗を確定する
    void CheckMatchEnd();

    // 全プレイヤーの入力を許可/禁止する
    void SetPlayersInputEnabled(bool bEnable);

    // コントローラーからプレイヤー番号を取得する
    int32 GetPlayerIndex(AController* Controller) const;

    // 生存しているプレイヤーコントローラーのリスト
    UPROPERTY()
    TArray<AController*> AlivePlayers;

    // 勝敗判定が完了したかどうか
    bool bMatchEnded = false;
};
