// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MainGameMode.h"
#include "Kismet/GameplayStatics.h"
// 2025.09.07 Lee start
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
// 2025.09.07 Lee end
#include "Blueprint/UserWidget.h"
#include "Tanimura/MyGameInstance.h"

AMainGameMode::AMainGameMode()
{
    P1Score = 0;
    P2Score = 0;
    CachedAnimalSpawner = nullptr;
    TimeRemaining = 0;
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

    // 制限時間の初期化
    TimeRemaining = TotalGameTime;

    // まずUIに初期時間を通知
    if (OnTimeChanged.IsBound()) {
        OnTimeChanged.Broadcast(TimeRemaining);
    }

    // 1秒ごとにAdvanceTimerを呼び出すタイマーを設定
    GetWorldTimerManager().SetTimer( GameTimerHandle, this, &AMainGameMode::AdvanceTimer, 1.0f, true );

    if (HUDWidgetClass)
    {
        UUserWidget* HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
        }
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

    // デバッグ表示
    UE_LOG(LogTemp, Warning, TEXT("GAME OVER! P1: %d vs P2: %d"), P1Score, P2Score);

    // 2. タイムアップ効果音の再生
    if (TimeUpSound) {
        UGameplayStatics::PlaySound2D(this, TimeUpSound);
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::EndGame: TimeUpSound が設定されていません。"));
    }

    // GameInstanceを取得してスコアと勝敗をセット
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance())) {
        GI->SetFinalResult(P1Score, P2Score);
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::EndGame: UMyGameInstance の取得に失敗しました。"));
    }

    // 3. 指定した秒数（1秒）待ってから TransitionToResultLevel を呼び出すタイマーをセット
    GetWorldTimerManager().SetTimer(ResultDelayTimerHandle, this, &AMainGameMode::TransitionToResultLevel, TimeUpDelay, false);
}

void AMainGameMode::TransitionToResultLevel()
{
    // リザルトレベルへ移動
    if (!ResultLevelName.IsNone()) {
        UGameplayStatics::OpenLevel(this, ResultLevelName);
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("AMainGameMode::TransitionToResultLevel: ResultLevelName が設定されていません。"));
    }
}

void AMainGameMode::AdvanceTimer()
{
    // 残り時間を1秒減らす
    TimeRemaining--;

    // 残り時間の変更を通知
    if (OnTimeChanged.IsBound()) {
        OnTimeChanged.Broadcast(TimeRemaining);
    }

    // タイムアップ判定
    if (TimeRemaining <= 0) {
        // タイマーを停止
        GetWorldTimerManager().ClearTimer(GameTimerHandle);

        // タイムアップを通知
        if (OnTimeUp.IsBound()) {
            OnTimeUp.Broadcast();
        }

        // ゲーム終了処理（動物のスポーン停止など）を実行
        EndGame();
    }
}
// 2025.09.07 Lee start
APawn* AMainGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	APawn* SpawnedPawn = Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);

	ACursorPawn* CursorPawn = Cast<ACursorPawn>(SpawnedPawn);
	if (!CursorPawn)
	{
		return SpawnedPawn;
	}

	AMapManager* MapManager = Cast<AMapManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMapManager::StaticClass()));
	if (!MapManager)
	{
		return SpawnedPawn;
	}

	int32 PlayerID = 0;
	int32 StartX = 0;
	int32 StartY = 0;

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			PlayerID = LP->GetControllerId();
			if (PlayerID == 1)
			{
				StartX = MapManager->MapWidth - 1;
				StartY = MapManager->MapHeight - 1;
			}
		}
	}

	CursorPawn->InitCursor(MapManager, PlayerID, StartX, StartY);

	return SpawnedPawn;
}
// 2025.09.07 Lee end
