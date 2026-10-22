#include "Tanimura/Actor/AnimalBase_New.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/Skill/MatchSkillEffectComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Tanimura/MainGameMode.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AAnimalBase_New::AAnimalBase_New()
{
	PrimaryActorTick.bCanEverTick = true;

	// ルートを作成
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// ゴール演出の既定エフェクトを読み込む
	ConstructorHelpers::FObjectFinder<UNiagaraSystem> GoalNiagaraVFXFinder(
		TEXT("/Game/FreeParticle_SoftTofu/Niagara/NS_Sparkling_Animate_2.NS_Sparkling_Animate_2"));
	if (GoalNiagaraVFXFinder.Succeeded()) {
		GoalNiagaraVFX = GoalNiagaraVFXFinder.Object;
	}
}

// 盤面と初期の向きを受け取る
void AAnimalBase_New::Initialize(const TScriptInterface<IGridInteractInterface>& InGridMap, const EGridDirection InInitialFacing)
{
	GridMap = InGridMap;
	InitialFacing = InInitialFacing;
}

void AAnimalBase_New::BeginPlay()
{
	Super::BeginPlay();

	// 盤面は外部から注入される前提とし、未注入なら移動を行わない
	if (!HasGridMap()) {
		UE_LOG(LogTemp, Warning, TEXT("AnimalBase_New: GridMap is not assigned."));
		return;
	}

	// 居場所は自分の座標から求め、渡された向きを体へ反映する
	CurrentCell = ResolveCurrentCell();
	TargetCell = CurrentCell;
	TurnTo(InitialFacing);
	SetActorLocation(CellToWorld(CurrentCell), false);

	// 出現時点の速度倍率を権威照会しておく
	InitializeMatchSkillEffects();

	// 出現マスの矢印を読んでから前へ進む
	EnterCell();
}

void AAnimalBase_New::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 補間に使えない時間は無視する
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) {
		return;
	}

	// ゴール処理済みなら以後の移動を行わない
	if (bGoalHandled) {
		return;
	}

	// 落下中は移動ではなく落下だけを進める
	if (bFalling) {
		Fall(DeltaSeconds);
		return;
	}

	// 速度倍率の変化をアニメ側へ通知する
	RefreshSpeedMultiplier();

	// 対戦フェーズが移動を許さない場合は停止する
	if (!IsMatchMovementAllowed()) {
		return;
	}

	Advance(DeltaSeconds);
}

// 補間の進捗を進め、マスへ到達したら到達処理へ引き渡す
void AAnimalBase_New::Advance(float DeltaSeconds)
{
	const float SecondsPerCellNow = GetEffectiveSecondsPerCell();
	if (SecondsPerCellNow <= KINDA_SMALL_NUMBER) {
		return;
	}

	MoveAlpha = FMath::Clamp(MoveAlpha + DeltaSeconds / SecondsPerCellNow, 0.0f, 1.0f);

	// 区間を補間して滑らかに移動する
	SetActorLocation(FMath::Lerp(SegmentStart, SegmentEnd, MoveAlpha), false);

	if (MoveAlpha < 1.0f) {
		return;
	}

	// 到達先を現在マスへ確定してから到達処理を行う
	CurrentCell = TargetCell;
	SetActorLocation(SegmentEnd, false);

	// 盤外のマスへ出た場合は移動を続けず落下へ移行する
	if (!IsInside(CurrentCell)) {
		StartFalling();
		return;
	}

	EnterCell();
}

// 到達マスのタイルを読み、ゴール判定と向きの更新を行う
void AAnimalBase_New::EnterCell()
{
	if (!HasGridMap()) {
		return;
	}

	// 到達したマスを読み取り済みとして記録する
	LastReadCell = CurrentCell;
	bHasReadCell = true;

	const ETileType TileType = IGridInteractInterface::Execute_GetCellState(GridMap.GetObject(), CurrentCell);

	// ゴールマスなら計分して移動を終える
	switch (TileType) {
	case ETileType::GoalP1:
		HandleGoalTile(0);
		return;

	case ETileType::GoalP2:
		HandleGoalTile(1);
		return;

	default:
		break;
	}

	// 矢印は向きを変えるだけ。矢印以外はNoneが返り向きは維持される
	const EGridDirection ArrowDirection = ToDirection(TileType);
	if (ArrowDirection != EGridDirection::None) {
		TurnTo(ArrowDirection);
	}

	StepForward();
}

// 向きを変え、体をその方向へ回す
void AAnimalBase_New::TurnTo(EGridDirection NewFacing)
{
	Facing = NewFacing;

	const FIntPoint Delta = ToDelta(Facing);
	if (Delta == FIntPoint::ZeroValue) {
		return;
	}

	// 向きをワールドのYaw角へ変換して体を回す
	const FVector Direction(static_cast<float>(Delta.X), static_cast<float>(Delta.Y), 0.0f);
	SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
}

// 今向いている方向へ1マス進む区間を開始する
void AAnimalBase_New::StepForward()
{
	if (!HasGridMap()) {
		return;
	}

	// 盤外でもそのまま進ませ、到達後に落下させる
	const FIntPoint NextCell = CurrentCell + ToDelta(Facing);

	// 補間区間を確定して移動を開始する
	TargetCell = NextCell;
	SegmentStart = GetActorLocation();
	SegmentEnd = CellToWorld(NextCell);
	MoveAlpha = 0.0f;
}

// 盤外へ出た動物の落下を開始する
void AAnimalBase_New::StartFalling()
{
	bFalling = true;
	FallSpeed = 0.0f;
	FallStartZ = GetActorLocation().Z;
}

// 重力に従って落下し、規定の深さまで落ちたら消える
void AAnimalBase_New::Fall(float DeltaSeconds)
{
	FallSpeed += FallGravity * DeltaSeconds;

	FVector Location = GetActorLocation();
	Location.Z -= FallSpeed * DeltaSeconds;
	SetActorLocation(Location, false);

	// 規定の深さまで落ちた動物は消す
	if (FallStartZ - Location.Z >= FallKillDepth) {
		Destroy();
	}
}

// ゴール到達時の計分と演出を行う
bool AAnimalBase_New::HandleGoalTile(int32 ScoringPlayerId)
{
	if (bGoalHandled || IsActorBeingDestroyed()) {
		return true;
	}

	// 委譲の再入に備えて先にフラグを立てる
	bGoalHandled = true;

	// 試合截止以降は計分も演出も行わない
	if (!IsGoalScoringAllowed()) {
		return true;
	}

	if (AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this))) {
		GameMode->AddScore(ScoringPlayerId, 1);
	}

	if (GoalSound != nullptr) {
		UGameplayStatics::PlaySound2D(this, GoalSound, 1.0f, 1.0f, 0.0f, GoalSoundConcurrency, nullptr, false);
	}

	// 破棄前にエフェクトを再生する
	PlayGoalVFX();

	Destroy();
	return true;
}

// ゴール時のエフェクトを一度だけ再生する
void AAnimalBase_New::PlayGoalVFX()
{
	if (GoalNiagaraVFX == nullptr) {
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr) {
		return;
	}

	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		GoalNiagaraVFX,
		GetActorLocation(),
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,
		true,
		ENCPoolMethod::AutoRelease);

	if (NiagaraComponent == nullptr) {
		return;
	}

	// ループ設定のアセットでも放出を止められるよう下限を保証する
	const float EmissionDuration = FMath::Max(GoalVFXEmissionDuration, 0.01f);

	// 動物本体は破棄されるため、弱参照で放出停止を予約する
	FTimerHandle GoalVFXDeactivateHandle;
	World->GetTimerManager().SetTimer(
		GoalVFXDeactivateHandle,
		[WeakComponent = TWeakObjectPtr<UNiagaraComponent>(NiagaraComponent)]()
		{
			// コンポーネントが先に消えても安全に無視する
			if (UNiagaraComponent* const Component = WeakComponent.Get()) {
				Component->Deactivate();
			}
		},
		EmissionDuration,
		false);
}

// 対戦フェーズが移動を許可するか
bool AAnimalBase_New::IsMatchMovementAllowed() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch()) {
		return true;
	}

	// Ready / Ended では移動しない
	if (!GameMode->IsMatchPlaying()) {
		return false;
	}

	// 移動も試合截止時刻の権威判定に従う
	return GameMode->IsScoringAllowed();
}

// ゴール時の得点が許可されるか
bool AAnimalBase_New::IsGoalScoringAllowed() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch()) {
		return true;
	}

	return GameMode->IsScoringAllowed();
}

// 共有加速を反映した1マスあたりの秒数を返す
float AAnimalBase_New::GetEffectiveSecondsPerCell() const
{
	const float Multiplier = GetSpeedMultiplier();

	// 倍率が不正な場合は素の秒数で動かす
	if (!FMath::IsFinite(Multiplier) || Multiplier <= 0.0f) {
		return SecondsPerCell;
	}

	return SecondsPerCell / Multiplier;
}

// 現在の権威的な速度倍率を返す
float AAnimalBase_New::GetSpeedMultiplier() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr) {
		return 1.0f;
	}

	// 効果文脈が無効なモードでは常に等倍とする
	if (!GameMode->IsSkillEffectContextActive()) {
		return 1.0f;
	}

	const UMatchSkillEffectComponent* MatchSkillEffect = GameMode->GetMatchSkillEffect();
	if (MatchSkillEffect == nullptr) {
		return 1.0f;
	}

	// 放送キャッシュに頼らず、毎回権威側でマップ一致まで判定してもらう
	return MatchSkillEffect->GetSpeedMultiplierForMap(GetMapActor());
}

// 共有加速の権威照会へ冪等に接続する
void AAnimalBase_New::InitializeMatchSkillEffects()
{
	RefreshSpeedMultiplier();
}

// 速度倍率を再照会し、変化時のみ通知する
void AAnimalBase_New::RefreshSpeedMultiplier()
{
	const float NewMultiplier = GetSpeedMultiplier();

	// BeginPlay前の通知はアニメ側の準備前になるため抑止する
	if (!HasActorBegunPlay()) {
		return;
	}

	if (!FMath::IsNearlyEqual(NewMultiplier, NotifiedSpeedMultiplier)) {
		NotifiedSpeedMultiplier = NewMultiplier;
		OnSpeedMultiplierChanged(NewMultiplier);
	}
}

// 直近に読み取ったマスの座標を返す
bool AAnimalBase_New::GetLastReadGridCoords(FIntPoint& OutGridCoords) const
{
	if (!bHasReadCell) {
		OutGridCoords = FIntPoint::NoneValue;
		return false;
	}

	OutGridCoords = LastReadCell;
	return true;
}

// 盤面アクターを返す
AActor* AAnimalBase_New::GetMapActor() const
{
	return Cast<AActor>(GridMap.GetObject());
}

// 自分の座標から最も近いマスを求める
FIntPoint AAnimalBase_New::ResolveCurrentCell() const
{
	if (!HasGridMap()) {
		return FIntPoint::ZeroValue;
	}

	const float TileSize = MeasureTileSize();
	if (TileSize <= KINDA_SMALL_NUMBER) {
		return FIntPoint::ZeroValue;
	}

	// 原点はグリッド座標(0,0)のマス中心
	const FVector Origin = IGridInteractInterface::Execute_ToWorldCenter(GridMap.GetObject(), FIntPoint(0, 0));
	const FVector Location = GetActorLocation();

	return FIntPoint(
		FMath::RoundToInt((Location.X - Origin.X) / TileSize),
		FMath::RoundToInt((Location.Y - Origin.Y) / TileSize));
}

// 隣接マス中心の距離からタイルサイズを測る
float AAnimalBase_New::MeasureTileSize() const
{
	if (!HasGridMap()) {
		return 0.0f;
	}

	const UObject* MapObject = GridMap.GetObject();
	const FVector Origin = IGridInteractInterface::Execute_ToWorldCenter(MapObject, FIntPoint(0, 0));
	const FVector Neighbor = IGridInteractInterface::Execute_ToWorldCenter(MapObject, FIntPoint(1, 0));
	return FVector::Dist2D(Origin, Neighbor);
}

// グリッド座標をマス中心のワールド座標へ変換する（高さは現在値を維持）
FVector AAnimalBase_New::CellToWorld(const FIntPoint& Cell) const
{
	if (!HasGridMap()) {
		return GetActorLocation();
	}

	FVector Center = IGridInteractInterface::Execute_ToWorldCenter(GridMap.GetObject(), Cell);
	Center.Z = GetActorLocation().Z;
	return Center;
}

// 盤面が注入されているか
bool AAnimalBase_New::HasGridMap() const
{
	return GridMap.GetObject() != nullptr;
}

// グリッド座標が盤面内かをマップへ問い合わせる
bool AAnimalBase_New::IsInside(const FIntPoint& Cell) const
{
	return HasGridMap() && IGridInteractInterface::Execute_IsValidCoord(GridMap.GetObject(), Cell);
}

// 盤面の矢印タイルを向きへ対応付ける（矢印以外はNone）
EGridDirection AAnimalBase_New::ToDirection(ETileType TileType)
{
	switch (TileType) {
	case ETileType::DirUp:
		return EGridDirection::Up;

	case ETileType::DirDown:
		return EGridDirection::Down;

	case ETileType::DirLeft:
		return EGridDirection::Left;

	case ETileType::DirRight:
		return EGridDirection::Right;

	default:
		return EGridDirection::None;
	}
}

// 向きをグリッドの移動量へ対応付ける（左右は既存マップの描画規約に合わせる）
FIntPoint AAnimalBase_New::ToDelta(EGridDirection Direction)
{
	switch (Direction) {
	case EGridDirection::Up:
		return FIntPoint(0, 1);

	case EGridDirection::Down:
		return FIntPoint(0, -1);

	case EGridDirection::Left:
		return FIntPoint(1, 0);

	case EGridDirection::Right:
		return FIntPoint(-1, 0);

	default:
		return FIntPoint::ZeroValue;
	}
}
