// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MainGameMode.h"
#include "Kismet/GameplayStatics.h"
// 2026.07.24 Lee start
#include "Lee/CursorPawn.h"
#include "Lee/MapManager.h"
// 2026.07.24 Lee end
#include "Blueprint/UserWidget.h"
#include "Tanimura/MyGameInstance.h"
// 2026.10.06 Lee start（技能システムと Spawner 一括制御に必要な依存）
#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/Skill/MatchSkillEffectComponent.h"
#include "Lee/Skill/SkillSystemComponent.h"
#include "Lee/Skill/SkillDefinition.h"
#include "Lee/Skill/Skills/SkillDef_ReverseArrows.h"
#include "Lee/Skill/Skills/SkillDef_SpeedUpAnimals.h"
#include "Engine/World.h"
#include "EngineUtils.h"
// 2026.10.06 Lee end

AMainGameMode::AMainGameMode()
{
    P1Score = 0;
    P2Score = 0;
    CachedAnimalSpawner = nullptr;
    TimeRemaining = 0;
    CountdownRemaining = 0;

    // 2026.10.06 Lee start（共有スキル効果を既定サブオブジェクトとして生成）
    MatchSkillEffect = CreateDefaultSubobject<UMatchSkillEffectComponent>(TEXT("MatchSkillEffect"));
    // 2026.10.06 Lee end
}

void AMainGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 2人目のプレイヤーを生成
    APlayerController* P2Controller = UGameplayStatics::CreatePlayer(GetWorld(), 1, true);

    // レベル内のAAnimalSpawnerを探して取得
    CachedAnimalSpawner = Cast<AAnimalSpawner>(UGameplayStatics::GetActorOfClass(GetWorld(), AAnimalSpawner::StaticClass()));

    // 制限時間の初期化
    TimeRemaining = TotalGameTime;

    // 2026.10.06 Lee A範囲手直し start（HUD 生成前にカウントダウン初期値を確定し、初期スナップショットを正しくする）
    CountdownRemaining = FMath::CeilToInt(ReadyDelay);
    // 2026.10.06 Lee A範囲手直し end

    // まずUIに初期時間を通知
    if (OnTimeChanged.IsBound()) {
        OnTimeChanged.Broadcast(TimeRemaining);
    }


    if (HUDWidgetClass) {
        UUserWidget* HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
        }
    }

    // カウントダウン中はプレイヤーの操作を無効化
    SetPlayersInputEnabled(false);

    // カウントダウン初期化（切り上げ整数値化）
    // CountdownRemaining = FMath::CeilToInt(ReadyDelay); ←2026.10.06 Lee A範囲手直し：HUD 生成前へ移動（元のコードは消さない）

    // 初期値をUIへ通知
    if (OnCountdownChanged.IsBound()) {
        OnCountdownChanged.Broadcast(CountdownRemaining);
    }

    // 1秒ごとに AdvanceCountdown を呼び出すタイマーをセット
    GetWorldTimerManager().SetTimer(ReadyTimerHandle, this, &AMainGameMode::AdvanceCountdown, 1.0f, true);

    // 2026.10.06 Lee start（Ready 中に双方の技能準備を開始。タイミング差は短いリトライで吸収する）
    // 2026.10.06 Lee A範囲手直し start（bool 返しメソッドはタイマーへ直接バインド不可のため void ラムダを使用。
    //  BeginPlay で初期化済みの場合はリトライタイマーを起動しない）
    // if (bUseNormalMatchFlow)
    // {
    //     TryInitializeMatchSkills();
    //     GetWorldTimerManager().SetTimer(SkillInitRetryTimerHandle, this, &AMainGameMode::TryInitializeMatchSkills, SkillInitRetryInterval, true);
    // }
    if (bUseNormalMatchFlow && !TryInitializeMatchSkills())
    {
        GetWorldTimerManager().SetTimer(SkillInitRetryTimerHandle,
            FTimerDelegate::CreateWeakLambda(this, [this]()
            {
                TryInitializeMatchSkills();
            }), SkillInitRetryInterval, true);
    }
    // 2026.10.06 Lee A範囲手直し end
    // 2026.10.06 Lee end
}

void AMainGameMode::AdvanceCountdown()
{
    CountdownRemaining--;

    if (OnCountdownChanged.IsBound()) {
        OnCountdownChanged.Broadcast(CountdownRemaining);
    }

    if (CountdownRemaining <= 0) {
        GetWorldTimerManager().ClearTimer(ReadyTimerHandle);
        StartMatch();
    }
}

void AMainGameMode::StartMatch()
{
    // 2026.10.06 Lee start（Ready→Playing は 1 回のみ。タイマー起動前に権威状態を固定する）
    if (!bUseNormalMatchFlow || MatchPhase != EMatchPhase::Ready)
    {
        return;
    }
    MatchPhase = EMatchPhase::Playing;
    if (UWorld* World = GetWorld())
    {
        MatchEndTime = World->GetTimeSeconds() + static_cast<float>(TotalGameTime);
    }
    TimeRemaining = TotalGameTime;

    // 技能初期化のリトライを止める（未準備なら本試合は技能無効のまま開始）。
    GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    if (!bSkillSystemsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::StartMatch: スキルの準備が未完了のため、本試合は技能無効で開始します。"));
    }
    // 2026.10.06 Lee end

    // プレイヤーの操作を許可
    SetPlayersInputEnabled(true);

    // 2026.10.06 Lee start（初期化済みの場合のみ双方のスキル使用を解禁）
    if (bSkillSystemsInitialized)
    {
        for (int32 i = 0; i < 2; ++i)
        {
            if (AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(this, i)))
            {
                if (USkillSystemComponent* SkillSystem = PC->GetSkillSystemComponent())
                {
                    SkillSystem->SetSkillsEnabled(true);
                }
            }
        }
    }
    // 2026.10.06 Lee end

    // スポーナーの開始
    // 2026.10.06 Lee A範囲手直し start（CachedAnimalSpawner も本試合の同地図篩に統一。旧コードは消さない）
    // if (CachedAnimalSpawner) {
    //     CachedAnimalSpawner->StartSpawning();
    // }
    // // 2026.10.06 Lee start（本試合と同地図の他の Spawner も開始）
    // StartMatchingSpawners();
    // // 2026.10.06 Lee end
    StartMatchingSpawners();
    // 2026.10.06 Lee A範囲手直し end

    // 1秒ごとにAdvanceTimerを呼び出すタイマーを設定
    GetWorldTimerManager().SetTimer(GameTimerHandle, this, &AMainGameMode::AdvanceTimer, 1.0f, true);
}

void AMainGameMode::SetPlayersInputEnabled(bool bEnable)
{
    for (int32 i = 0; i < 2; ++i) {
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, i);
        if (PC) {
            PC->SetIgnoreMoveInput(!bEnable);
            PC->SetIgnoreLookInput(!bEnable);
        }
    }
}

void AMainGameMode::AddScore(int32 PlayerID, int32 ScoreToAdd)
{
    // 2026.10.06 Lee start（普通対戦では進行中のみ得点を許可。チュートリアルは従来どおり許可）
    if (bUseNormalMatchFlow && !IsScoringAllowed())
    {
        return;
    }
    // 2026.10.06 Lee end

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
    // 2026.10.06 Lee start（冪等：先に Ended へ固定してから後処理・通知へ進む。重複呼び出しは無副作用）
    if (!bUseNormalMatchFlow)
    {
        return; // チュートリアルは独自の終了フローを使用する
    }
    if (MatchPhase == EMatchPhase::Ended)
    {
        return;
    }
    MatchPhase = EMatchPhase::Ended;

    // Ready / Game / 技能初期化リトライの各タイマーを停止。
    GetWorldTimerManager().ClearTimer(ReadyTimerHandle);
    GetWorldTimerManager().ClearTimer(GameTimerHandle);
    GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    // 2026.10.06 Lee end

    // 動物のスポーンを止める
    // 2026.10.06 Lee A範囲手直し start（CachedAnimalSpawner も本試合の同地図篩に統一。旧コードは消さない）
    // if (CachedAnimalSpawner) {
    //     CachedAnimalSpawner->StopSpawning();
    // }
    // // 2026.10.06 Lee start（本試合と同地図の他の Spawner も停止）
    // StopMatchingSpawners();
    // // 2026.10.06 Lee end
    StopMatchingSpawners();
    // 2026.10.06 Lee A範囲手直し end

    // 2026.10.06 Lee start（双方の連移・入力と技能を解除し、共有効果をクリアしてから通知へ進む）
    for (int32 i = 0; i < 2; ++i)
    {
        if (AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(this, i)))
        {
            PC->ResetTransientInput(true);
            if (USkillSystemComponent* SkillSystem = PC->GetSkillSystemComponent())
            {
                SkillSystem->SetSkillsEnabled(false);
            }
        }
    }
    if (MatchSkillEffect)
    {
        MatchSkillEffect->ClearEffects();
    }

    // 終了を UI へ通知（時間 0）。フェーズは GetMatchPhase() の権威照会で確認できる。
    TimeRemaining = 0;
    if (OnTimeChanged.IsBound()) {
        OnTimeChanged.Broadcast(0);
    }
    if (OnTimeUp.IsBound()) {
        OnTimeUp.Broadcast();
    }
    // 2026.10.06 Lee end

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
    // 2026.10.06 Lee start（終了時刻からの切り上げ残り秒。満了は唯一の EndGame へ集約し OnTimeUp を先行放送しない）
    if (bUseNormalMatchFlow)
    {
        // 2026.10.06 Lee 最終審査小修正 start（0 判定を放送より先に行い、EndGame の Ended ロック後に 0 / OnTimeUp を放送させる。
        //  Ended 再入では放送も終了処理も繰り返さない。旧コードは消さない）
        // TimeRemaining = GetTimeRemaining();
        // if (OnTimeChanged.IsBound()) {
        //     OnTimeChanged.Broadcast(TimeRemaining);
        // }
        // if (TimeRemaining <= 0)
        // {
        //     GetWorldTimerManager().ClearTimer(GameTimerHandle);
        //     EndGame();
        // }
        // return;
        if (MatchPhase == EMatchPhase::Ended)
        {
            return; // Ended 再入：放送も終了処理も繰り返さない
        }
        TimeRemaining = GetTimeRemaining();
        if (TimeRemaining <= 0)
        {
            GetWorldTimerManager().ClearTimer(GameTimerHandle);
            EndGame(); // Ended ロック後に 0 / OnTimeUp を放送する
            return;
        }
        if (OnTimeChanged.IsBound()) {
            OnTimeChanged.Broadcast(TimeRemaining);
        }
        return;
        // 2026.10.06 Lee 最終審査小修正 end
    }
    // 2026.10.06 Lee end

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
// 2026.07.24 Lee start
APawn* AMainGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// プレイヤーIDを先に取得
	int32 PlayerID = 0;
	int32 StartX = 0;
	int32 StartY = 0;

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			PlayerID = LP->GetControllerId();
		}
	}

	// プレイヤーIDに応じて異なる Pawn クラスを選択（BP_CursorPawn_P1 / BP_CursorPawn_P2）
	TSubclassOf<APawn> SelectedPawnClass = (PlayerID == 1) ? CursorPawnClass_P2 : CursorPawnClass_P1;
	if (SelectedPawnClass)
	{
		DefaultPawnClass = SelectedPawnClass;
	}

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

	// P2 は右下隅から開始
	if (PlayerID == 1)
	{
		StartX = MapManager->MapWidth - 1;
		StartY = MapManager->MapHeight - 1;
	}

	CursorPawn->InitCursor(MapManager, PlayerID, StartX, StartY);

	// 2026.10.06 Lee start（Cursor と同じ地図を PC へも注入し、Ready 中の技能準備を再試行）
	if (AAnimalGatherPlayerController* GatherPC = Cast<AAnimalGatherPlayerController>(NewPlayer))
	{
		GatherPC->SetMapManager(MapManager);
	}
	TryInitializeMatchSkills();
	// 2026.10.06 Lee end

	return SpawnedPawn;
}
// 2026.07.24 Lee end

// 2026.10.06 Lee start（試合ライフサイクルと技能準備の実装）

void AMainGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 2026.10.06 Lee A範囲手直し start（先に Ended へ固定してから後片付け。Playing 可用の扱いは残さない）
    MatchPhase = EMatchPhase::Ended;
    // 2026.10.06 Lee A範囲手直し end
    // 全タイマーを停止し、共有効果もクリアする。
    GetWorldTimerManager().ClearTimer(ReadyTimerHandle);
    GetWorldTimerManager().ClearTimer(GameTimerHandle);
    GetWorldTimerManager().ClearTimer(ResultDelayTimerHandle);
    GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    if (MatchSkillEffect)
    {
        MatchSkillEffect->ClearEffects();
    }

    Super::EndPlay(EndPlayReason);
}

bool AMainGameMode::IsNormalMatch() const
{
    return bUseNormalMatchFlow;
}

bool AMainGameMode::IsMatchPlaying() const
{
    UWorld* World = GetWorld();
    if (!bUseNormalMatchFlow || World == nullptr || MatchPhase != EMatchPhase::Playing)
    {
        return false;
    }
    return World->GetTimeSeconds() < MatchEndTime;
}

bool AMainGameMode::IsGameplayInputAllowed() const
{
    // 2026.10.06 Lee A範囲手直し start（非普通対戦は従来どおり入力可。IsMatchPlaying は普通対戦のみ false 扱い）
    // return IsMatchPlaying();
    if (!bUseNormalMatchFlow)
    {
        return true;
    }
    return IsMatchPlaying();
    // 2026.10.06 Lee A範囲手直し end
}

bool AMainGameMode::IsScoringAllowed() const
{
    // 2026.10.06 Lee A範囲手直し start（非普通対戦は採点も従来どおり許可）
    // return IsMatchPlaying();
    if (!bUseNormalMatchFlow)
    {
        return true;
    }
    return IsMatchPlaying();
    // 2026.10.06 Lee A範囲手直し end
}

UMatchSkillEffectComponent* AMainGameMode::GetMatchSkillEffect() const
{
    return MatchSkillEffect;
}

AMapManager* AMainGameMode::GetMatchMap() const
{
    return MatchMap.Get();
}

int32 AMainGameMode::GetP1Score() const
{
    return P1Score;
}

int32 AMainGameMode::GetP2Score() const
{
    return P2Score;
}

int32 AMainGameMode::GetTimeRemaining() const
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return 0;
    }
    if (MatchPhase == EMatchPhase::Ready)
    {
        return TotalGameTime;
    }
    if (MatchPhase != EMatchPhase::Playing)
    {
        return 0;
    }
    return FMath::Max(0, FMath::CeilToInt(MatchEndTime - World->GetTimeSeconds()));
}

int32 AMainGameMode::GetCountdownRemaining() const
{
    return CountdownRemaining;
}

EMatchPhase AMainGameMode::GetMatchPhase() const
{
    return MatchPhase;
}

void AMainGameMode::StartMatchingSpawners()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }
    const AActor* MatchMapActor = MatchMap.Get();
    if (MatchMapActor == nullptr)
    {
        return; // 本試合の地図が未確定のため、追加の Spawner には触れない
    }
    // 2026.10.06 Lee A範囲手直し start（CachedAnimalSpawner も同地図篩に含める。旧コードは消さない）
    // for (TActorIterator<AAnimalSpawner> It(World); It; ++It)
    // {
    //     AAnimalSpawner* Spawner = *It;
    //     if (Spawner && Spawner != CachedAnimalSpawner && Spawner->MapActor.Get() == MatchMapActor)
    //     {
    //         Spawner->StartSpawning();
    //     }
    // }
    for (TActorIterator<AAnimalSpawner> It(World); It; ++It)
    {
        AAnimalSpawner* Spawner = *It;
        if (Spawner && Spawner->MapActor.Get() == MatchMapActor)
        {
            Spawner->StartSpawning();
        }
    }
    // 2026.10.06 Lee A範囲手直し end
}

void AMainGameMode::StopMatchingSpawners()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }
    const AActor* MatchMapActor = MatchMap.Get();
    if (MatchMapActor == nullptr)
    {
        return;
    }
    // 2026.10.06 Lee A範囲手直し start（CachedAnimalSpawner も同地図篩に含める。旧コードは消さない）
    // for (TActorIterator<AAnimalSpawner> It(World); It; ++It)
    // {
    //     AAnimalSpawner* Spawner = *It;
    //     if (Spawner && Spawner != CachedAnimalSpawner && Spawner->MapActor.Get() == MatchMapActor)
    //     {
    //         Spawner->StopSpawning();
    //     }
    // }
    for (TActorIterator<AAnimalSpawner> It(World); It; ++It)
    {
        AAnimalSpawner* Spawner = *It;
        if (Spawner && Spawner->MapActor.Get() == MatchMapActor)
        {
            Spawner->StopSpawning();
        }
    }
    // 2026.10.06 Lee A範囲手直し end
}

bool AMainGameMode::TryInitializeMatchSkills()
{
    // 2026.10.08 Lee start（教程拡張：実初期化本体を protected InitializeSkillSystemsForMode へ抽出。
    //  本入口は「普通対戦フローの Ready 中の再試行」という従来制約を維持する）
    // 【旧実装（保持）】
    // if (!bUseNormalMatchFlow || bSkillSystemsInitialized || MatchPhase != EMatchPhase::Ready)
    // {
    //     return bSkillSystemsInitialized;
    // }
    // UWorld* World = GetWorld();
    // if (World == nullptr)
    // {
    //     return false;
    // }
    //
    // // 実際の LocalPlayer 身分と PC バインド地図を揃えてから初期化する。
    // AAnimalGatherPlayerController* PlayerControllers[2] = { nullptr, nullptr };
    // AMapManager* PlayerMaps[2] = { nullptr, nullptr };
    // for (int32 i = 0; i < 2; ++i)
    // {
    //     AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(World, i));
    //     if (PC == nullptr)
    //     {
    //         return false; // まだ揃っていない：リトライで再試行
    //     }
    //     const uint8 SkillId = PC->GetSkillPlayerId();
    //     // 2026.10.06 Lee A範囲手直し start（255 は未確定扱いで待機リトライ。0/1 以外の実値は警告の上リトライ継続）
    //     // if (SkillId != static_cast<uint8>(i))
    //     // {
    //     //     UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::TryInitializeMatchSkills: Player%d の身分が不正 (id=%u) のため技能を無効化します。"), i, SkillId);
    //     //     GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    //     //     return false; // 身分不備は時間では解決しない
    //     // }
    //     if (SkillId == 255)
    //     {
    //         return false; // 未確定：Ready 中は待機してリトライする
    //     }
    //     if (SkillId != 0 && SkillId != 1)
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::TryInitializeMatchSkills: Player%d の身分が不正 (id=%u) です。"), i, SkillId);
    //         return false;
    //     }
    //     if (SkillId != static_cast<uint8>(i))
    //     {
    //         UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::TryInitializeMatchSkills: Player%d の身分が不一致 (id=%u) のため技能を無効化します。"), i, SkillId);
    //         GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    //         return false; // 有効身分の不一致は時間では解決しない
    //     }
    //     // 2026.10.06 Lee A範囲手直し end
    //     AMapManager* Map = PC->GetMapManager();
    //     if (Map == nullptr)
    //     {
    //         return false; // 地図未注入：リトライで再試行
    //     }
    //     PlayerControllers[i] = PC;
    //     PlayerMaps[i] = Map;
    // }
    //
    // if (PlayerMaps[0] != PlayerMaps[1])
    // {
    //     UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::TryInitializeMatchSkills: 両者の地図が不一致のため、技能は無効のままとします。"));
    //     GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    //     return false;
    // }
    // if (PlayerMaps[0]->GetWorld() != World || MatchSkillEffect == nullptr)
    // {
    //     return false;
    // }
    //
    // 2026.10.08 Lee end

    // 普通対戦フローの Ready 中のみ再試行入口として動作する（従来制約を維持し実初期化へ委譲）
    if (!bUseNormalMatchFlow || bSkillSystemsInitialized || MatchPhase != EMatchPhase::Ready)
    {
        return bSkillSystemsInitialized;
    }
    return InitializeSkillSystemsForMode();
}

// 2026.10.08 Lee start（教程拡張：TryInitializeMatchSkills から抽出した実初期化本体）
bool AMainGameMode::InitializeSkillSystemsForMode()
{
    // 門番はモードの初期化支援と冪等フラグのみ。段階制約は呼び出し側の責務。
    if (!SupportsSkillInitialization() || bSkillSystemsInitialized)
    {
        return bSkillSystemsInitialized;
    }
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return false;
    }

    // 以下は旧 TryInitializeMatchSkills 本体からの准用（検証内容・順序は不変）。
    // 実際の LocalPlayer 身分と PC バインド地図を揃えてから初期化する。
    AAnimalGatherPlayerController* PlayerControllers[2] = { nullptr, nullptr };
    AMapManager* PlayerMaps[2] = { nullptr, nullptr };
    for (int32 i = 0; i < 2; ++i)
    {
        AAnimalGatherPlayerController* PC = Cast<AAnimalGatherPlayerController>(UGameplayStatics::GetPlayerController(World, i));
        if (PC == nullptr)
        {
            return false; // まだ揃っていない：未整備のため再試行可能
        }
        const uint8 SkillId = PC->GetSkillPlayerId();
        // 2026.10.06 Lee A範囲手直し start（255 は未確定扱いで待機リトライ。0/1 以外の実値は警告の上リトライ継続）
        // if (SkillId != static_cast<uint8>(i))
        // {
        //     UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::TryInitializeMatchSkills: Player%d の身分が不正 (id=%u) のため技能を無効化します。"), i, SkillId);
        //     GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
        //     return false; // 身分不備は時間では解決しない
        // }
        if (SkillId == 255)
        {
            return false; // 未確定：呼び出し側の再試行で待機する
        }
        if (SkillId != 0 && SkillId != 1)
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: Player%d の身分が不正 (id=%u) です。"), i, SkillId);
            return false;
        }
        if (SkillId != static_cast<uint8>(i))
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: Player%d の身分が不一致 (id=%u) のため技能を無効化します。"), i, SkillId);
            GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
            return false; // 有効身分の不一致は時間では解決しない
        }
        // 2026.10.06 Lee A範囲手直し end
        AMapManager* Map = PC->GetMapManager();
        if (Map == nullptr)
        {
            return false; // 地図未注入：呼び出し側の再試行で待機する
        }
        PlayerControllers[i] = PC;
        PlayerMaps[i] = Map;
    }

    if (PlayerMaps[0] != PlayerMaps[1])
    {
        UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 両者の地図が不一致のため、技能は無効のままとします。"));
        GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
        return false;
    }
    if (PlayerMaps[0]->GetWorld() != World || MatchSkillEffect == nullptr)
    {
        return false;
    }

    // 2026.10.06 Lee A範囲手直し start（技能定義の検証より前に本試合マップと共有効果を確定する。
    //  技能設定が不適切でも通常対戦・動物生成は妨げない）
    // MatchSkillEffect->InitializeForMatch(PlayerMaps[0]);
    // MatchMap = PlayerMaps[0];
    MatchMap = PlayerMaps[0];
    MatchSkillEffect->InitializeForMatch(PlayerMaps[0]);
    // 2026.10.06 Lee A範囲手直し end

    // 2026.10.06 Lee A範囲手直し start（定義解決：資産は nullptr のときのみ CDO へフォールバック。
    //  設定済みだが不適切な資産はフォールバックせず技能を無効化して警告する）
    USkillDefinition* ResolvedDefinitions[2] = { nullptr, nullptr };
    USkillDefinition* AssetDefinitions[2] = { ReverseArrowsSkillDefinition.Get(), SpeedUpAnimalsSkillDefinition.Get() };
    bool bDefinitionsValid = true;

    if (AssetDefinitions[0] != nullptr)
    {
        // 2026.10.06 Lee HUD範囲手直し start（資産も具体的子クラス型を検証。型不一致はフォールバックせず無効化して警告）
        USkillDef_ReverseArrows* ReverseAsset = Cast<USkillDef_ReverseArrows>(AssetDefinitions[0]);
        if (ReverseAsset == nullptr)
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 反転スロットの定義資産の型が不正のため、技能を無効化します（フォールバックしません）。"));
            bDefinitionsValid = false;
        }
        else if (ReverseAsset->IsConfigurationValid())
        {
            ResolvedDefinitions[0] = ReverseAsset;
        }
        else
        // 2026.10.06 Lee HUD範囲手直し end
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 反転スキルの定義資産が不適切のため、技能を無効化します（フォールバックしません）。"));
            bDefinitionsValid = false;
        }
    }
    else
    {
        // CDO は読み取り専用としてのみ使用する（GetDefault の const を避けるため GetMutableDefault）。
        USkillDef_ReverseArrows* CdoDefinition = GetMutableDefault<USkillDef_ReverseArrows>();
        if (CdoDefinition != nullptr && CdoDefinition->IsA(USkillDef_ReverseArrows::StaticClass()) && CdoDefinition->IsConfigurationValid())
        {
            ResolvedDefinitions[0] = CdoDefinition;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 反転スキルの既定定義が不適切のため、技能を無効化します。"));
            bDefinitionsValid = false;
        }
    }

    if (AssetDefinitions[1] != nullptr)
    {
        // 2026.10.06 Lee HUD範囲手直し start（資産も具体的子クラス型を検証。型不一致はフォールバックせず無効化して警告）
        USkillDef_SpeedUpAnimals* SpeedAsset = Cast<USkillDef_SpeedUpAnimals>(AssetDefinitions[1]);
        if (SpeedAsset == nullptr)
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 加速スロットの定義資産の型が不正のため、技能を無効化します（フォールバックしません）。"));
            bDefinitionsValid = false;
        }
        else if (SpeedAsset->IsConfigurationValid())
        {
            ResolvedDefinitions[1] = SpeedAsset;
        }
        else
        // 2026.10.06 Lee HUD範囲手直し end
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 加速スキルの定義資産が不適切のため、技能を無効化します（フォールバックしません）。"));
            bDefinitionsValid = false;
        }
    }
    else
    {
        // CDO は読み取り専用としてのみ使用する。
        USkillDef_SpeedUpAnimals* CdoDefinition = GetMutableDefault<USkillDef_SpeedUpAnimals>();
        if (CdoDefinition != nullptr && CdoDefinition->IsA(USkillDef_SpeedUpAnimals::StaticClass()) && CdoDefinition->IsConfigurationValid())
        {
            ResolvedDefinitions[1] = CdoDefinition;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 加速スキルの既定定義が不適切のため、技能を無効化します。"));
            bDefinitionsValid = false;
        }
    }

    if (!bDefinitionsValid)
    {
        GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
        return false;
    }
    // 2026.10.06 Lee A範囲手直し end

    // 双方コンポーネントを一度だけ初期化する。
    bool bAllInitialized = true;
    for (int32 i = 0; i < 2; ++i)
    {
        TArray<USkillDefinition*> Definitions;
        Definitions.Add(ResolvedDefinitions[0]);
        Definitions.Add(ResolvedDefinitions[1]);
        USkillSystemComponent* SkillSystem = PlayerControllers[i]->GetSkillSystemComponent();
        if (SkillSystem == nullptr || !SkillSystem->InitializeSkills(this, PlayerMaps[i], static_cast<uint8>(i), Definitions))
        {
            bAllInitialized = false;
        }
    }
    if (!bAllInitialized)
    {
        return false; // 未整備のため呼び出し側の再試行に委ねる
    }

    bSkillSystemsInitialized = true;
    GetWorldTimerManager().ClearTimer(SkillInitRetryTimerHandle);
    UE_LOG(LogTemp, Log, TEXT("AMainGameMode::InitializeSkillSystemsForMode: 両プレイヤーの技能を初期化しました。"));
    return true;
}
// 2026.10.08 Lee end（教程拡張：実初期化本体の抽出）

// 2026.10.08 Lee start（教程拡張用の共有権限インターフェースの既定実装）
bool AMainGameMode::SupportsSkillInitialization() const
{
    // 既定は普通対戦フローのみ技能初期化を支援する
    return bUseNormalMatchFlow;
}

bool AMainGameMode::IsSkillUseAllowed(uint8 PlayerId, int32 SlotIndex) const
{
    // 身分は 0/1、スロットは 0/1（反転/加速）のみ有効。それ以外は即拒否
    if (PlayerId != 0 && PlayerId != 1)
    {
        return false;
    }
    if (SlotIndex != 0 && SlotIndex != 1)
    {
        return false;
    }
    // 既定は普通対戦の権威判定（Playing かつ截止時刻前）
    return IsMatchPlaying();
}

bool AMainGameMode::IsSkillEffectContextActive() const
{
    // 既定は普通対戦の進行中のみ共有効果文脈を有効とする
    return IsMatchPlaying();
}
// 2026.10.08 Lee end（教程拡張用の共有権限インターフェースの既定実装）
// 2026.10.06 Lee end
