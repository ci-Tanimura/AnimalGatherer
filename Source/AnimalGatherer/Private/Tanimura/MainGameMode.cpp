// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MainGameMode.h"
#include "Kismet/GameplayStatics.h"

AMainGameMode::AMainGameMode()
{
    P1Score = 0;
    P2Score = 0;
    CachedAnimalSpawner = nullptr;
}

void AMainGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 2人目のプレイヤーを生成
    APlayerController* P2Controller = UGameplayStatics::CreatePlayer(GetWorld(), 1, true);

    // レベル内のAAnimalSpawnerを探して取得
    CachedAnimalSpawner = Cast<AAnimalSpawner>(UGameplayStatics::GetActorOfClass(GetWorld(), AAnimalSpawner::StaticClass()));

    // 問題なければスポーナーの開始関数を呼び出す
    if (P2Controller && CachedAnimalSpawner) {
        CachedAnimalSpawner->StartSpawning();
    }
}

void AMainGameMode::AddScore(int32 PlayerID, int32 ScoreToAdd)
{
    if (PlayerID == 0) {
        P1Score += ScoreToAdd;
    }
    else if (PlayerID == 1) {
        P2Score += ScoreToAdd;
    }

    if (OnScoreChanged.IsBound()) {
        OnScoreChanged.Broadcast(P1Score, P2Score);
    }
}

void AMainGameMode::EndGame()
{
    // 動物のスポーンを止める
    if (CachedAnimalSpawner) {
        CachedAnimalSpawner->StopSpawning();
    }

    // ここに「勝敗画面のUIを表示する」などの処理を今後追加していく
}