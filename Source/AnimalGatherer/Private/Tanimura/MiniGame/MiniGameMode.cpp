// Fill out your copyright notice in the Description page of Project Settings.

#include "Tanimura/MiniGame/MiniGameMode.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Tanimura/MiniGame/Actor/PlayerCharacter.h"
#include "Tanimura/MiniGame/PenguinController.h"

AMiniGameMode::AMiniGameMode()
{
    // ペンギン操作用コントローラーを既定で使用する
    PlayerControllerClass = APenguinController::StaticClass();

    // プレイヤー番号順の既定カラー（P1〜P4）
    PlayerColors = {
        FLinearColor::Red,
        FLinearColor::Blue,
        FLinearColor::Green,
        FLinearColor::Yellow
    };
}

void AMiniGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 生成するプレイヤー数を2〜4に制限する
    NumberOfPlayers = FMath::Clamp(NumberOfPlayers, 2, 4);

    // プレイヤーが操作するPawnクラスを設定する
    DefaultPawnClass = PlayerPawnClass;

    if (!PlayerPawnClass) {
        UE_LOG(LogTemp, Error, TEXT("AMiniGameMode: PlayerPawnClass が設定されていません。BPで BP_PlayerCharacter を指定してください。"));
    }

    // 2人目以降のプレイヤーを生成する
    SpawnExtraPlayers();
}

void AMiniGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // ログインしたプレイヤーを生存リストへ登録する
    if (NewPlayer && !AlivePlayers.Contains(NewPlayer)) {
        AlivePlayers.Add(NewPlayer);
    }
}

void AMiniGameMode::SpawnExtraPlayers()
{
    // 1人目は既定で生成済みのため、2人目以降を生成する
    for (int32 Index = 1; Index < NumberOfPlayers; ++Index) {
        UGameplayStatics::CreatePlayer(GetWorld(), Index, true);
    }
}

APawn* AMiniGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
    APawn* SpawnedPawn = Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);

    // 生成されたプレイヤーへ死亡通知の購読と足元カラーを設定する
    if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(SpawnedPawn)) {
        PlayerCharacter->OnPlayerDied.AddUniqueDynamic(this, &AMiniGameMode::HandlePlayerDied);

        const int32 PlayerIndex = GetPlayerIndex(NewPlayer);
        if (PlayerColors.IsValidIndex(PlayerIndex)) {
            PlayerCharacter->SetPlayerColor(PlayerColors[PlayerIndex]);
        }
    }

    return SpawnedPawn;
}

void AMiniGameMode::HandlePlayerDied(AController* DeadController)
{
    if (!DeadController) {
        return;
    }

    // 死亡したプレイヤーを生存リストから除外する
    AlivePlayers.Remove(DeadController);

    // 残り人数に応じて勝敗を判定する
    CheckMatchEnd();
}

void AMiniGameMode::CheckMatchEnd()
{
    // すでに試合が終了している場合は処理しない
    if (bMatchEnded) {
        return;
    }

    // 2人以上生存している場合はまだ決着しない
    if (AlivePlayers.Num() >= 2) {
        return;
    }

    bMatchEnded = true;

    // 勝者を決定する（1人ならそのプレイヤー、0人なら引き分け）
    int32 WinnerIndex = -1;
    if (AlivePlayers.Num() == 1) {
        WinnerIndex = GetPlayerIndex(AlivePlayers[0]);
    }

    // 全プレイヤーの操作を無効化する
    SetPlayersInputEnabled(false);

    // 勝敗を通知する
    OnMatchEnd.Broadcast(WinnerIndex);

    UE_LOG(LogTemp, Warning, TEXT("MATCH END: Winner = %d"), WinnerIndex);
}

void AMiniGameMode::SetPlayersInputEnabled(bool bEnable)
{
    for (int32 Index = 0; Index < NumberOfPlayers; ++Index) {
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, Index);
        if (PC) {
            PC->SetIgnoreMoveInput(!bEnable);
            PC->SetIgnoreLookInput(!bEnable);
        }
    }
}

int32 AMiniGameMode::GetPlayerIndex(AController* Controller) const
{
    if (const APlayerController* PC = Cast<APlayerController>(Controller)) {
        if (const ULocalPlayer* LP = PC->GetLocalPlayer()) {
            return LP->GetControllerId();
        }
    }
    return 0;
}
