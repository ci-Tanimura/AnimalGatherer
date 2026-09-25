// Fill out your copyright notice in the Description page of Project Settings.

#include "Tanimura/MiniGame/MiniGameMode.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Tanimura/MiniGame/Actor/PlayerCharacter.h"
#include "Tanimura/MiniGame/PenguinController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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

    // プレイヤーが操作するPawnを既定で設定する（BPのクラスデフォルトで上書き可能）
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBP(TEXT("/Game/MiniGame/Blueprints/BP_PlayerCharacter.BP_PlayerCharacter_C"));
    if (PlayerPawnBP.Succeeded()) {
        DefaultPawnClass = PlayerPawnBP.Class;
    }
}

void AMiniGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 生成するプレイヤー数を2〜4に制限する
    NumberOfPlayers = FMath::Clamp(NumberOfPlayers, 2, 4);

    // 初期プレイヤーのログイン完了後に追加プレイヤーを生成する（初期化中の競合を回避）
    GetWorldTimerManager().SetTimer(ExtraPlayerSpawnTimer, this, &AMiniGameMode::SpawnExtraPlayers, 0.3f, false);

    // 初期スポーンが飛ばされたプレイヤーのPawnを確実に再生成する
    GetWorldTimerManager().SetTimer(EnsurePawnTimer, this, &AMiniGameMode::EnsureAllPlayersSpawned, 0.6f, false);
}

void AMiniGameMode::EnsureAllPlayersSpawned()
{
    // 生存プレイヤーのうちBP_PlayerCharacterを所持していないものを確実に生成する
    for (AController* Player : AlivePlayers)
    {
        if (!IsValid(Player)) {
            continue;
        }

        // すでに正しいPawnを所持していれば何もしない
        if (Cast<APlayerCharacter>(Player->GetPawn())) {
            continue;
        }

        SpawnPlayerCharacterManually(Player);
    }

    // 全プレイヤーの視点を共有カメラへ統一する
    SyncPlayersToSharedCamera();
}

void AMiniGameMode::SyncPlayersToSharedCamera()
{
    // 基準の共有カメラを最初のプレイヤーの視点から取得する
    APlayerController* FirstPC = GetWorld()->GetFirstPlayerController();
    AActor* SharedCamera = FirstPC ? FirstPC->GetViewTarget() : nullptr;
    if (!SharedCamera) {
        return;
    }

    // 全プレイヤーの視点を共有カメラへ合わせる（俯瞰移動の方向基準を一致させる）
    for (AController* Player : AlivePlayers)
    {
        if (APlayerController* PC = Cast<APlayerController>(Player)) {
            if (PC->GetViewTarget() != SharedCamera) {
                PC->SetViewTarget(SharedCamera);
            }
        }
    }
}

void AMiniGameMode::SpawnPlayerCharacterManually(AController* Player)
{
    if (!Player) {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Player);

    // 所持によって視点がPawnへ切り替わるため、所持前の視点（共有カメラ）を退避する
    AActor* PrevViewTarget = PC ? PC->GetViewTarget() : nullptr;

    // 古い不正なPawnを破棄する（共有カメラが所持Pawnの場合は破棄せず残す）
    APawn* OldPawn = Player->GetPawn();
    if (OldPawn && OldPawn != PrevViewTarget) {
        Player->UnPossess();
        OldPawn->Destroy();
    }

    // プレイヤー番号に対応するスタート地点を決定する
    const int32 PlayerIndex = GetPlayerIndex(Player);
    TArray<AActor*> PlayerStarts;
    UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

    FTransform SpawnTransform = FTransform::Identity;
    if (PlayerStarts.IsValidIndex(PlayerIndex)) {
        SpawnTransform = PlayerStarts[PlayerIndex]->GetActorTransform();
    }

    // 既定のPawnクラスで生成する
    UClass* PawnClass = GetDefaultPawnClassForController(Player);
    if (!PawnClass) {
        UE_LOG(LogTemp, Error, TEXT("MiniGameMode: DefaultPawnClass is null for idx=%d"), PlayerIndex);
        return;
    }

    APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform);
    if (!NewPawn) {
        UE_LOG(LogTemp, Error, TEXT("MiniGameMode: Failed to spawn pawn for idx=%d"), PlayerIndex);
        return;
    }

    // コントローラーに所持させる（入力も有効になる）
    Player->Possess(NewPawn);

    // 所持で視点がPawnへ切り替わるため、共有カメラの視点へ戻す
    if (PC && PrevViewTarget && PrevViewTarget != NewPawn) {
        PC->SetViewTarget(PrevViewTarget);
    }

    // 足元カラーと死亡通知の購読を設定する
    if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(NewPawn)) {
        PlayerCharacter->OnPlayerDied.AddUniqueDynamic(this, &AMiniGameMode::HandlePlayerDied);
        if (PlayerColors.IsValidIndex(PlayerIndex)) {
            PlayerCharacter->SetPlayerColor(PlayerColors[PlayerIndex]);
        }
    }

}

void AMiniGameMode::PostLogin(APlayerController* NewPlayer)
{
    // 生存リストへ登録してから既定処理を実行する（Pawn生成時にログイン順の番号を確定するため）
    if (NewPlayer && !AlivePlayers.Contains(NewPlayer)) {
        AlivePlayers.Add(NewPlayer);
    }

    Super::PostLogin(NewPlayer);
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

AActor* AMiniGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    // ログイン済みならログイン順、Login中の未登録なら次に来る番号で割り当てる
    int32 PlayerIndex = AlivePlayers.Find(Player);
    if (PlayerIndex == INDEX_NONE) {
        PlayerIndex = AlivePlayers.Num();
    }

    TArray<AActor*> PlayerStarts;
    UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

    // 配置されたPlayerStartが足りていれば決定的に割り当てる
    if (PlayerStarts.IsValidIndex(PlayerIndex)) {
        return PlayerStarts[PlayerIndex];
    }

    // 不足時は既定の選択ロジックへ委ねる
    return Super::ChoosePlayerStart_Implementation(Player);
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
    // ログイン順の番号を優先する（ControllerIdに依存せず確実に振り分ける）
    const int32 LoginOrderIndex = AlivePlayers.Find(Controller);
    if (LoginOrderIndex != INDEX_NONE) {
        return LoginOrderIndex;
    }

    // 未登録時はControllerIdでフォールバックする
    if (const APlayerController* PC = Cast<APlayerController>(Controller)) {
        if (const ULocalPlayer* LP = PC->GetLocalPlayer()) {
            return LP->GetControllerId();
        }
    }
    return 0;
}
