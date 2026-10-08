#include "Takeuchi/Pawn/AnimalBase.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/MapManager.h"
#include "Tanimura/GridInteractInterface.h"
#include "Tanimura/MainGameMode.h"
// 2026.10.06 Lee start（共有加速の権威照会と sweep 結果検証に必要な依存を追加）
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Lee/Skill/MatchSkillEffectComponent.h"
// 2026.10.06 Lee end

// 2026.10.06 Lee start（セル中心の到達判定に使う極小の世界許容（cm）。
// sweep 後の浮動小数の累積誤差は吸収しつつ、実質的な横ずれは許さない大きさ）
namespace
{
	/** @brief セル中心へ「実際に到達した」とみなす各軸方向の許容誤差（cm）。 */
	constexpr float CenterArrivalTolerance = 0.1f;
}
// 2026.10.06 Lee end

AAnimalBase::AAnimalBase()
{
	PrimaryActorTick.bCanEverTick = true;

	//動物の位置や回転の基準になるルートコンポーネントを作成する
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AAnimalBase::BeginPlay()
{
	Super::BeginPlay();

	//マップ参照が未設定の場合は、レベル上のMapManagerを自動取得する
	if (!MapActor)
	{
		for (TActorIterator<AMapManager> It(GetWorld()); It; ++It)
		{
			SetMapActor(*It);
			break;
		}
	}

	//動物を生成するたびに、上下左右の4方向から初期移動方向をランダムに選ぶ
	static const FVector InitialMoveDirections[] =
	{
		FVector(1.0f, 0.0f, 0.0f),
		FVector(-1.0f, 0.0f, 0.0f),
		FVector(0.0f, 1.0f, 0.0f),
		FVector(0.0f, -1.0f, 0.0f)
	};

	const int32 RandomDirectionIndex =
		FMath::RandRange(0, UE_ARRAY_COUNT(InitialMoveDirections) - 1);
	SetMoveDirection(InitialMoveDirections[RandomDirectionIndex]);

	if (!MapActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimalBase: MapActor is not set."));
	}

	// 2026.10.06 Lee start（Spawner 経由以外の生成経路向けに、BeginPlay でも冪等に共有加速へ接続する）
	InitializeMatchSkillEffects();
	// 2026.10.06 Lee end
}

void AAnimalBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 2026.10.06 Lee start（Goal 処理済み・破壊開始済みの再 Tick を遮断する。
	// 計分委譲による再入や破壊済みアクターの二重処理を防ぐ）
	if (bGoalHandled || IsActorBeingDestroyed())
	{
		return;
	}

	// 非有限・負の DeltaTime は移動量を壊すため何もしない（NaN ループ防止）
	if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
	{
		return;
	}
	// 2026.10.06 Lee end（Goal 処理済み・破壊開始済みの再 Tick を遮断する）

	// 2026.10.06 Lee start（マス中心ごとの分割移動へ置き換え。旧実装はコメントとして保持）
	// //現在いるタイルの情報を読み取り、必要なら移動方向を更新する
	// UpdateMoveDirectionFromCurrentTile(DeltaTime);
	//
	// MoveAnimal(DeltaTime);
	// 2026.10.06 Lee end

	// 2026.10.06 Lee start（権威倍率照会 → 試合段階判定 → 分割移動の新経路）
	// 権威倍率を毎フレーム照会し、変化があった場合のみアニメ入口へ通知する
	RefreshSpeedMultiplier();

	// 普通対戦の Ready / Ended（および試合截止以降）では移動もゴール処理も行わない
	// （教程および AMainGameMode を使わない独立テストは従来どおり動く）
	if (!IsMatchMovementAllowed())
	{
		return;
	}

	// 有効速度（共有加速を含む）で今フレーム分を移動する。
	// 進行上のセル中心ごとに区間を分割し、中心到達時に方向 / Goal を読む
	MoveAlongGrid(DeltaTime);
	// 2026.10.06 Lee end
}

//渡された方向ベクトルを正規化し、現在の移動方向として保存する
void AAnimalBase::SetMoveDirection(FVector NewMoveDirection)
{
	CurrentMoveDirection = NewMoveDirection.GetSafeNormal();

	if (bRotateToMoveDirection && !CurrentMoveDirection.IsNearlyZero())
	{
		const FRotator DirectionRotation = CurrentMoveDirection.Rotation();
		SetActorRotation(FRotator(0.0f, DirectionRotation.Yaw + FacingYawOffset, 0.0f));
	}
}

void AAnimalBase::SetMapActor(AActor* NewMapActor)
{
	// 2026.10.06 Lee start（同一マップの再設定では LastGridCoords を壊さない。
	// マップ変更時のみ状態を初期化し、TileSize は MapManager を権威として採用する。
	// 旧実装はコメントとして保持）
	// MapActor = NewMapActor;
	// bHasLastGridCoords = false;
	if (MapActor == NewMapActor)
	{
		// 2026.10.06 Lee start（同一マップの再設定では TileSize の同期のみ行い、
		// 格子履歴（LastGridCoords / processed 記録）は壊さない。
		// 旧実装「同一マップの再設定は無視する」はコメントとして保持）
		// // 同一マップの再設定は無視する（加速復帰時の同セル再処理・再計分を防ぐ）
		// return;
		if (const AMapManager* MapManager = Cast<AMapManager>(NewMapActor))
		{
			// TileSize は引き続き MapManager を権威として同期する（履歴は保持）
			TileSize = MapManager->TileSize;
		}
		return;
		// 2026.10.06 Lee end（同一マップの再設定では TileSize の同期のみ行う）
	}

	MapActor = NewMapActor;
	bHasLastGridCoords = false;
	bProcessedLastGridCoords = false;

	if (const AMapManager* MapManager = Cast<AMapManager>(NewMapActor))
	{
		// グリッド計算に使う TileSize はバインド先 MapManager の値を権威とする
		TileSize = MapManager->TileSize;
	}

	// 最終マップ確定後に共有加速の権威照会へ冪等に接続する（不一致マップなら 1.0 のまま）
	RefreshSpeedMultiplier();
	// 2026.10.06 Lee end
}

// 2026.10.06 Lee start
// 【旧実装（未使用・保持）】
// Tick は現在、セル中心ごとに区間を分割して方向 / Goal を読む新経路
// （MoveAlongGrid / ProcessGridCenter）を使用するため、本関数は呼び出されない。
// 既存の継承やデバッグ呼び出しを壊さないよう、本体ごと保持している。
// 2026.10.06 Lee end
//足元のマスを読み取り、SetMoveDirectionを呼ぶ
void AAnimalBase::UpdateMoveDirectionFromCurrentTile(float DeltaTime)
{
	//マップ参照が設定されていない場合は、タイル情報を取得できないため処理しない
	if (!MapActor)return;

	//参照先が GridInteractInterface を実装していない場合は、タイル情報を取得できないため処理しない
	if (!MapActor->GetClass()->ImplementsInterface(UGridInteractInterface::StaticClass()))return;

	//動物の座標を取得
	const FVector AnimalLocation = GetActorLocation();

	//マップアクターの位置を、グリッド計算の原点として扱う
	const FVector MapOrigin = MapActor->GetActorLocation();

	//各タイルは MapOrigin + GridCoords * TileSize の位置を中心として配置されるため、
	//最も近いタイル中心のグリッド座標へ変換する
	const FIntPoint CurrentGridCoords(
		FMath::RoundToInt((AnimalLocation.X - MapOrigin.X) / TileSize),
		FMath::RoundToInt((AnimalLocation.Y - MapOrigin.Y) / TileSize)
	);

	//前回と同じマスにいる場合は、同じタイル処理を繰り返さない
	if (bHasLastGridCoords && CurrentGridCoords == LastGridCoords)return;


	//現在マスの中心座標
	const FVector CurrentTileCenter(
		MapOrigin.X + CurrentGridCoords.X * TileSize,
		MapOrigin.Y + CurrentGridCoords.Y * TileSize,
		AnimalLocation.Z
	);

	const float FrameMoveDistance = MoveSpeed * DeltaTime;
	const float EffectiveTolerance =
		FMath::Max(DirectionReadTolerance, FrameMoveDistance * 2.0f);

	const float DistanceToTileCenter =
		FVector::Dist2D(AnimalLocation, CurrentTileCenter);

	//中央付近に来るまではタイルを読まない
	if (DistanceToTileCenter > EffectiveTolerance)return;

	//方向転換を繰り返しても中心線から少しずつずれないよう、判定時にマス中央へ揃える
	SetActorLocation(CurrentTileCenter, false);

	//中央に到達してから、このマスを処理済みとして記録する
	LastGridCoords = CurrentGridCoords;
	bHasLastGridCoords = true;

	const ETileType TileType =
		IGridInteractInterface::Execute_GetCellState(MapActor, CurrentGridCoords);

	//タイル種類に応じて、動物の移動方向を変更する
	switch (TileType)
	{
	case ETileType::DirUp:
		SetMoveDirection(FVector(0.0f, 1.0f, 0.0f));
		break;

	case ETileType::DirDown:
		SetMoveDirection(FVector(0.0f, -1.0f, 0.0f));
		break;

	case ETileType::DirLeft:
		SetMoveDirection(FVector(1.0f, 0.0f, 0.0f));
		break;

	case ETileType::DirRight:
		SetMoveDirection(FVector(-1.0f, 0.0f, 0.0f));
		break;

	case ETileType::GoalP1:
	{
		if (AMainGameMode* GameMode =
			Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GameMode->AddScore(0, 1);
		}
		// 2026.07.31 Gu start
		// ゴールの効果音再生
		if (GoalSound)
		{
			UGameplayStatics::PlaySound2D(this, GoalSound);
		}
		// 2026.07.31 Gu end
		Destroy();
		return;
	}

	case ETileType::GoalP2:
	{
		if (AMainGameMode* GameMode =
			Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GameMode->AddScore(1, 1);
		}
		// 2026.07.31 Gu start
		// ゴールの効果音再生
		if (GoalSound)
		{
			UGameplayStatics::PlaySound2D(this, GoalSound);
		}
		// 2026.07.31 Gu end
		Destroy();
		return;
	}

	default:
		break;
	}
}

// 2026.10.06 Lee start
// 【旧実装（未使用・保持）】
// 移動は MoveAlongGrid / MoveStraight（sweep 結果と実移動距離を検証する新実装）に置き換わった。
// 呼び出しは無いが、旧挙動の参照実装として本体ごと保持している。
// 2026.10.06 Lee end
void AAnimalBase::MoveAnimal(float DeltaTime)
{
	//移動方向がない、または移動速度が0以下の場合は移動しない
	if (CurrentMoveDirection.IsNearlyZero() || MoveSpeed <= 0.0f)return;

	//現在の移動方向、移動速度、前フレームからの経過時間を使って移動量を計算する
	const FVector MoveDelta = CurrentMoveDirection * MoveSpeed * DeltaTime;

	//衝突を無視して、計算した移動量だけワールド座標で移動する
	AddActorWorldOffset(MoveDelta, true);
}

// 2026.10.06 Lee start（試合段階判定・権威倍率・マス中心分割移動の新実装）

/** @brief 普通対戦では Playing かつ試合截止前のみ移動を許可する（教程・独立テストは常に許可） */
bool AAnimalBase::IsMatchMovementAllowed() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch())
	{
		return true;
	}

	// Ready / Ended では移動しない
	if (!GameMode->IsMatchPlaying())
	{
		return false;
	}

	// 移動も試合截止時刻の権威判定に従う（截止ちょうどは拒否）
	return GameMode->IsScoringAllowed();
}

/** @brief ゴール到達時の計分許可。普通対戦は GameMode の権威判定、それ以外は従来どおり許可 */
bool AAnimalBase::IsGoalScoringAllowed() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch())
	{
		return true;
	}

	// 試合截止点以降の新規計分を拒否する（移動権限と同一の権威条件）
	return GameMode->IsScoringAllowed();
}

/** @brief 現在の権威倍率を照会する（本局・バインド済みマップ以外は常に 1.0） */
float AAnimalBase::GetSpeedMultiplier() const
{
	const AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode == nullptr || !GameMode->IsNormalMatch())
	{
		return 1.0f;
	}

	const UMatchSkillEffectComponent* MatchSkillEffect = GameMode->GetMatchSkillEffect();
	if (MatchSkillEffect == nullptr)
	{
		return 1.0f;
	}

	// 放送キャッシュに頼らず、毎回現在時刻・試合状態・マップ一致を権威側で判定してもらう
	return MatchSkillEffect->GetSpeedMultiplierForMap(MapActor);
}

/** @brief 共有加速を含む実効速度。MoveSpeed の基底値は変更しない */
float AAnimalBase::GetEffectiveMoveSpeed() const
{
	return MoveSpeed * GetSpeedMultiplier();
}

/** @brief 新生動物を含む冪等な初期照会。変化があった場合のみ通知が走る */
void AAnimalBase::InitializeMatchSkillEffects()
{
	RefreshSpeedMultiplier();
}

/** @brief 権威倍率を再照会し、変化があった場合のみ OnSpeedMultiplierChanged を通知する */
void AAnimalBase::RefreshSpeedMultiplier()
{
	const float NewMultiplier = GetSpeedMultiplier();

	// 2026.10.06 Lee start（BeginPlay 前の通知抑止。SpawnActorDeferred の FinishSpawning 前に
	// Spawner から呼ばれる SetMapActor が Construction Script・コンポーネント準備前に
	// 初回通知とキャッシュ更新を消費し、その後の初期化が同値扱いで再通知できなくなる問題の修正。
	// BeginPlay 前は通知せず NotifiedSpeedMultiplier も更新しない。これにより
	// BeginPlay からの InitializeMatchSkillEffects() が実際の初回変化として通知できる）
	// if (!FMath::IsNearlyEqual(NewMultiplier, NotifiedSpeedMultiplier))
	// {
	// 	NotifiedSpeedMultiplier = NewMultiplier;
	// 	OnSpeedMultiplierChanged(NewMultiplier);
	// }
	if (!HasActorBegunPlay())
	{
		// 移動側は毎フレーム GetSpeedMultiplier() を権威照会するため、
		// この早期リターンで実移動・Goal 処理への影響は発生しない
		return;
	}

	if (!FMath::IsNearlyEqual(NewMultiplier, NotifiedSpeedMultiplier))
	{
		NotifiedSpeedMultiplier = NewMultiplier;
		OnSpeedMultiplierChanged(NewMultiplier);
	}
	// 2026.10.06 Lee end（BeginPlay 前の通知抑止）
}

/** @brief マップ原点は MapActor のワールド位置（MapManager 権威） */
FVector AAnimalBase::GetMapOrigin() const
{
	return MapActor ? MapActor->GetActorLocation() : FVector::ZeroVector;
}

/** @brief グリッド座標 → セル中心ワールド座標（Z は動物の現在値を維持） */
FVector AAnimalBase::GridCenterToWorld(const FIntPoint& GridCoords) const
{
	const FVector Origin = GetMapOrigin();
	return FVector(
		Origin.X + GridCoords.X * TileSize,
		Origin.Y + GridCoords.Y * TileSize,
		GetActorLocation().Z);
}

/** @brief ワールド座標 → 最寄りセルのグリッド座標 */
FIntPoint AAnimalBase::WorldToNearestGrid(const FVector& WorldLocation) const
{
	const FVector Origin = GetMapOrigin();
	return FIntPoint(
		FMath::RoundToInt((WorldLocation.X - Origin.X) / TileSize),
		FMath::RoundToInt((WorldLocation.Y - Origin.Y) / TileSize));
}

// 2026.10.06 Lee start（前方の最寄り中心を厳密に選ぶ方式へ修正。旧 Doxygen と旧本体はコメントとして保持）
/** @brief 進行方向側で「最も近い前方のセル中心」を幾何的に求める。
 *         中心ちょうどの位置ではその中心自身（距離 0）を返して未読なら必ず読ませ、
 *         実際に処理済みの中心だけを到達許容で一度だけスキップする */
// /** @brief 進行方向側で次に通過するセル中心を幾何的に求める。
//  *         中心ちょうどの位置では厳密に前方の 1 セル先を返すため、
//  *         処理済み中心を二度読むことなく、大 DeltaTime での跨ぎも取りこぼさない */
bool AAnimalBase::ComputeNextCenterAhead(FIntPoint& OutGridCoords, float& OutDistance) const
{
	// 【旧実装（保持）】CenterEpsilon=0.01 は TileSize=100・位置 99.5 のとき
	// 0.995 + 0.01 → Floor が 1 となり距離 0.5 の未読中心を読み飛ばして 200 を返していた。
	// //
	// const FVector Origin = GetMapOrigin();
	// const float RelX = GetActorLocation().X - Origin.X;
	// const float RelY = GetActorLocation().Y - Origin.Y;
	//
	// // 中心の丸め誤差で手前の中心を拾わないための微小オフセット
	// constexpr float CenterEpsilon = 0.01f;
	//
	// if (FMath::Abs(CurrentMoveDirection.X) > 0.5f)
	// {
	// 	const bool bPositive = CurrentMoveDirection.X > 0.0f;
	// 	const int32 NextX = bPositive
	// 		? FMath::FloorToInt(RelX / TileSize + CenterEpsilon) + 1
	// 		: FMath::CeilToInt(RelX / TileSize - CenterEpsilon) - 1;
	// 	const int32 NearestY = FMath::RoundToInt(RelY / TileSize);
	//
	// 	OutGridCoords = FIntPoint(NextX, NearestY);
	// 	OutDistance = FMath::Abs(NextX * TileSize - RelX);
	// 	return true;
	// }
	//
	// if (FMath::Abs(CurrentMoveDirection.Y) > 0.5f)
	// {
	// 	const bool bPositive = CurrentMoveDirection.Y > 0.0f;
	// 	const int32 NextY = bPositive
	// 		? FMath::FloorToInt(RelY / TileSize + CenterEpsilon) + 1
	// 		: FMath::CeilToInt(RelY / TileSize - CenterEpsilon) - 1;
	// 	const int32 NearestX = FMath::RoundToInt(RelX / TileSize);
	//
	// 	OutGridCoords = FIntPoint(NearestX, NextY);
	// 	OutDistance = FMath::Abs(NextY * TileSize - RelY);
	// 	return true;
	// }
	//
	// return false;

	// 2026.10.06 Lee start（前提値の有限性検証。TileSize 無効・位置 NaN 時は安全に失敗させる）
	const FVector Origin = GetMapOrigin();
	const FVector CurrentLocation = GetActorLocation();
	const float RelX = CurrentLocation.X - Origin.X;
	const float RelY = CurrentLocation.Y - Origin.Y;

	if (!FMath::IsFinite(RelX) || !FMath::IsFinite(RelY) ||
		!FMath::IsFinite(TileSize) || TileSize <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// 2026.10.06 Lee end（前提値の有限性検証）

	// 進行軸上で「現在位置から到達許容の内側〜前方」にある最も近い中心を選ぶ。
	// これにより距離 0.5 の未読中心など、前方の中心を決して飛ばさない
	if (FMath::Abs(CurrentMoveDirection.X) > 0.5f)
	{
		const bool bPositive = CurrentMoveDirection.X > 0.0f;
		const int32 NextX = bPositive
			? FMath::CeilToInt((RelX - CenterArrivalTolerance) / TileSize)
			: FMath::FloorToInt((RelX + CenterArrivalTolerance) / TileSize);
		const int32 NearestY = FMath::RoundToInt(RelY / TileSize);

		FIntPoint Candidate(NextX, NearestY);

		// 実際に処理済みの中心だけを一度だけスキップする（未読の中心は飛ばさない）
		if (bProcessedLastGridCoords && Candidate == LastGridCoords)
		{
			Candidate = bPositive
				? Candidate + FIntPoint(1, 0)
				: Candidate + FIntPoint(-1, 0);
		}

		OutGridCoords = Candidate;
		OutDistance = FMath::Abs(static_cast<float>(Candidate.X) * TileSize - RelX);
		return true;
	}

	if (FMath::Abs(CurrentMoveDirection.Y) > 0.5f)
	{
		const bool bPositive = CurrentMoveDirection.Y > 0.0f;
		const int32 NextY = bPositive
			? FMath::CeilToInt((RelY - CenterArrivalTolerance) / TileSize)
			: FMath::FloorToInt((RelY + CenterArrivalTolerance) / TileSize);
		const int32 NearestX = FMath::RoundToInt(RelX / TileSize);

		FIntPoint Candidate(NearestX, NextY);

		// 実際に処理済みの中心だけを一度だけスキップする（未読の中心は飛ばさない）
		if (bProcessedLastGridCoords && Candidate == LastGridCoords)
		{
			Candidate = bPositive
				? Candidate + FIntPoint(0, 1)
				: Candidate + FIntPoint(0, -1);
		}

		OutGridCoords = Candidate;
		OutDistance = FMath::Abs(static_cast<float>(Candidate.Y) * TileSize - RelY);
		return true;
	}

	return false;
}
// 2026.10.06 Lee end（前方の最寄り中心を厳密に選ぶ方式へ修正）

/** @brief sweep 付きで Distance だけ移動し、実移動距離と衝突結果を検証する。
 *         受阻した場合は false を返し、呼び出し側は今フレームの残り移動と
 *         未到達セル中心の読み取りを打ち切る（強制传送は行わない） */
bool AAnimalBase::SweepMoveBy(const FVector& Direction, float Distance, FHitResult& OutHit)
{
	OutHit = FHitResult();

	const FVector StartLocation = GetActorLocation();
	const FVector MoveDelta = Direction * Distance;

	// sweep を有効化し、実際に動けた距離と Hit を検証する
	AddActorWorldOffset(MoveDelta, true, &OutHit);

	const float ActualDistance = FVector::Dist2D(GetActorLocation(), StartLocation);
	const bool bBlockedByHit = OutHit.bBlockingHit && OutHit.Time < (1.0f - KINDA_SMALL_NUMBER);
	const bool bShortMove = ActualDistance < (Distance - 0.01f);

	// 浮動小数の丸め程度の差は受阻とみなさない（0.01 cm 未満の不足は許容）
	return !(bBlockedByHit || bShortMove);
}

// 2026.10.06 Lee start（2D 到達確認と Goal 再入防止を追加。旧 Doxygen はコメントとして保持）
/** @brief 実際に2Dで到達したセル中心だけを処理する。同一セル中心は一度だけ読む。
 *         横ずれ状態・未到達時は読み取らず processed も記録しない。
 *         戻り値 true は Goal 到達などで今フレームの移動処理を停止することを意味する */
// /** @brief 到達済みセル中心を処理する。同一セル中心は一度だけ読む。
//  *         戻り値 true は Goal 到達などで今フレームの移動処理を停止することを意味する */
bool AAnimalBase::ProcessGridCenter(const FIntPoint& GridCoords)
{
	// 2026.10.06 Lee start（Goal 処理済み・破壊開始済みなら以降の読み取りを行わない）
	if (bGoalHandled || IsActorBeingDestroyed())
	{
		return true;
	}
	// 2026.10.06 Lee end（Goal 処理済み・破壊開始済みの遮断）

	// 2026.10.06 Lee start（実際に2Dで中心へ到達している場合のみ読み取る。
	// 横方向オフセットが残る状態での Goal 誤読を防ぐ）
	const FVector CenterLocation = GridCenterToWorld(GridCoords);
	const FVector CurrentLocation = GetActorLocation();
	const bool bArrivedAtCenter =
		FMath::Abs(CurrentLocation.X - CenterLocation.X) <= CenterArrivalTolerance &&
		FMath::Abs(CurrentLocation.Y - CenterLocation.Y) <= CenterArrivalTolerance;

	if (!bArrivedAtCenter)
	{
		// 未到達：読み取りも処理済み記録も行わない
		return false;
	}
	// 2026.10.06 Lee end（2D 到達確認）

	// 同一セル中心は一度だけ処理する（加速復帰時の再計分・再転換を防ぐ）
	if (bProcessedLastGridCoords && GridCoords == LastGridCoords)
	{
		return false;
	}

	// 中心に到達してから処理済みとして記録する（受阻時は更新しない）
	LastGridCoords = GridCoords;
	bProcessedLastGridCoords = true;

	const ETileType TileType = IGridInteractInterface::Execute_GetCellState(MapActor, GridCoords);

	// 方向 → 世界軸マッピングは旧実装を踏襲する（DirUp=+Y / DirDown=-Y / DirLeft=+X / DirRight=-X）
	switch (TileType)
	{
	case ETileType::DirUp:
		SetMoveDirection(FVector(0.0f, 1.0f, 0.0f));
		break;

	case ETileType::DirDown:
		SetMoveDirection(FVector(0.0f, -1.0f, 0.0f));
		break;

	case ETileType::DirLeft:
		SetMoveDirection(FVector(1.0f, 0.0f, 0.0f));
		break;

	case ETileType::DirRight:
		SetMoveDirection(FVector(-1.0f, 0.0f, 0.0f));
		break;

	case ETileType::GoalP1:
		HandleGoalTile(0);
		return true;

	case ETileType::GoalP2:
		HandleGoalTile(1);
		return true;

	default:
		break;
	}

	return false;
}

// 2026.10.06 Lee start（bGoalHandled による再入防止を追加。旧 Doxygen はコメントとして保持）
/** @brief ゴール処理（計分 → 効果音 → 破壊）は一度だけ行い、
 *         AddScore の委譲が発火する前に bGoalHandled を立てて再入を遮断する。
 *         戻り値 true は Goal 処理を確定した場合 */
// /** @brief ゴール処理（計分 → 効果音 → 破壊）は一度だけ行い、
//  *         呼び出し元が即座に今フレームを停止することで破壊後の移動を防ぐ */
bool AAnimalBase::HandleGoalTile(int32 ScoringPlayerId)
{
	// 2026.10.06 Lee start（二重 Goal 処理の遮断。委譲の再入に備えて先にフラグを立てる）
	if (bGoalHandled || IsActorBeingDestroyed())
	{
		return true;
	}

	// OnScoreChanged などの委譲がゲームコードを再入させる前に先に立てる
	bGoalHandled = true;
	// 2026.10.06 Lee end（二重 Goal 処理の遮断）

	// 試合截止以降は計分も演出も破壊も行わない（移動の停止のみ）
	if (!IsGoalScoringAllowed())
	{
		// 2026.10.06 Lee start（呼び出し元は Goal 処理で必ず今フレームを停止するため true へ統一）
		// return false;
		return true;
		// 2026.10.06 Lee end（呼び出し元は Goal 処理で必ず今フレームを停止するため true へ統一）
	}

	if (AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->AddScore(ScoringPlayerId, 1);
	}

	// 2026.07.31 Gu start
	// ゴールの効果音再生
	if (GoalSound)
	{
		UGameplayStatics::PlaySound2D(this, GoalSound);
	}
	// 2026.07.31 Gu end

	Destroy();
	return true;
}

// 2026.10.06 Lee start（許容・予計を厳守する整列へ修正。旧 Doxygen と旧本体はコメントとして保持）
/** @brief 初回のみ、出現位置の最寄りセル中心へ sweep 整列してセルを読む。
 *         許容は DirectionReadTolerance のみ（TileSize の半分などへ拡大しない）。
 *         整列移動は今フレームの残り予算以内で sweep で行い、partial / blocked 時は
 *         本フレームを停止して次フレームに継続する。実際に中心へ到達した場合のみ
 *         processed を記録し、unswept の SetActorLocation は行わない */
// /** @brief 初回のみ、出現位置の最寄りセル中心へ有限許容内で sweep 整列してセルを読む。
//  *         許容は DirectionReadTolerance と TileSize の半分（有限値）であり、
//  *         フレーム移動量に連動させない。受阻時は強制移動しない。
//  *         戻り値 true は今フレームの移動処理を停止（Goal 到達など） */
bool AAnimalBase::TryAlignToNearestCenter(float& InOutRemaining)
{
	// 【旧実装（保持）】許容の拡大（Max(DirectionReadTolerance, TileSize/2)）、予算外の
	// 一括整列移動、失敗時の processed 記録と unswept SetActorLocation が問題となっていた。
	// const FIntPoint NearestGridCoords = WorldToNearestGrid(GetActorLocation());
	// const FVector CenterLocation = GridCenterToWorld(NearestGridCoords);
	// const float DistanceToCenter = FVector::Dist2D(GetActorLocation(), CenterLocation);
	//
	// // 整列の試行は一度きり（失敗時も同一セルの再試行はしない）
	// LastGridCoords = NearestGridCoords;
	// bHasLastGridCoords = true;
	//
	// if (DistanceToCenter <= KINDA_SMALL_NUMBER)
	// {
	// 	// すでに中心にいる：整列移動は不要だが、このセル自体は読む（旧挙動を踏襲）
	// 	return ProcessGridCenter(NearestGridCoords);
	// }
	//
	// // 出現位置の整列は有限許容内でのみ行う（離れすぎる場合は直線移動に委ねる）
	// const float AlignTolerance = FMath::Max(DirectionReadTolerance, TileSize * 0.5f);
	// if (DistanceToCenter > AlignTolerance)
	// {
	// 	return false;
	// }
	//
	// const FVector AlignDirection = (CenterLocation - GetActorLocation()).GetSafeNormal2D();
	// if (AlignDirection.IsNearlyZero())
	// {
	// 	return false;
	// }
	//
	// FHitResult SweepHit;
	// if (!SweepMoveBy(AlignDirection, DistanceToCenter, SweepHit))
	// {
	// 	// 整列が受阻された場合は強制移動しない（セルも読まない）
	// 	return false;
	// }
	//
	// // 整列に要した距離は今フレームの移動量から消費する（残り位移動を壊さない）
	// InOutRemaining -= DistanceToCenter;
	//
	// // sweep で到達済みのため、中心座標への画一的な誤差修正のみ行う（衝突すり抜けの強制移動ではない）
	// SetActorLocation(CenterLocation, false);
	//
	// return ProcessGridCenter(NearestGridCoords);

	const FIntPoint NearestGridCoords = WorldToNearestGrid(GetActorLocation());
	const FVector CenterLocation = GridCenterToWorld(NearestGridCoords);
	const float DistanceToCenter = FVector::Dist2D(GetActorLocation(), CenterLocation);

	// すでに中心にいる：整列移動は不要、このセルを読む（旧挙動を踏襲）。
	// 実際に到達していない場合は processed にしない（ProcessGridCenter 側の2D到達確認による）
	if (DistanceToCenter <= CenterArrivalTolerance)
	{
		return ProcessGridCenter(NearestGridCoords);
	}

	// 整列は DirectionReadTolerance 以内でのみ試みる（旧の Max(Tolerance, TileSize/2) による
	// 許容拡大は行わない）。離れすぎる場合は整列せず直線移動に委ねる
	if (DistanceToCenter > DirectionReadTolerance)
	{
		return false;
	}

	const FVector AlignDirection = (CenterLocation - GetActorLocation()).GetSafeNormal2D();
	if (AlignDirection.IsNearlyZero())
	{
		return false;
	}

	// 今フレームの残り予算を超える整列移動は行わない（予算 1.67 で 40 動くことを防ぐ）
	const float AlignDistance = FMath::Min(DistanceToCenter, FMath::Max(InOutRemaining, 0.0f));
	if (AlignDistance <= 0.0f)
	{
		return false;
	}

	FHitResult SweepHit;
	const bool bSweepCompleted = SweepMoveBy(AlignDirection, AlignDistance, SweepHit);
	InOutRemaining = FMath::Max(0.0f, InOutRemaining - AlignDistance);

	if (!bSweepCompleted)
	{
		// blocked：強制移動せず、セルも読まず processed にもしない。本フレームは停止し、
		// 次フレームの整列試行で継続する
		return true;
	}

	if (AlignDistance >= DistanceToCenter - KINDA_SMALL_NUMBER)
	{
		// sweep で実際に中心へ到達した場合のみセルを読む（unswept の位置修正は行わない）
		return ProcessGridCenter(NearestGridCoords);
	}

	// partial：予算を消費したので本フレームは停止。次フレームの整列試行で継続する
	return true;
}
// 2026.10.06 Lee end（許容・予計を厳守する整列へ修正）

/** @brief グリッド非依存の直線移動。マップ無し / interface 未実装 / 非軸方向の保護経路。
 *         受阻した場合はそこで停止する */
void AAnimalBase::MoveStraight(float Distance)
{
	if (CurrentMoveDirection.IsNearlyZero() || Distance <= 0.0f)
	{
		return;
	}

	FHitResult SweepHit;
	SweepMoveBy(CurrentMoveDirection, Distance, SweepHit);
}

// 2026.10.06 Lee start（固定区間上限の廃止・前提値検証・横ずれ保護・進行保証へ修正。
// 旧 Doxygen と旧本体はコメントとして保持）
/** @brief 1 フレームの移動を、進行上のセル中心ごとの区間へ分割して処理する。
 *         中心に到達するたびに方向 / Goal を読み、方向転換後も残り距離を消費し続ける。
 *         固定の区間数上限は設けず、Goal / 受阻 / 試合終了 / 予算消費 / 無進行検出で抜ける */
// /** @brief 1 フレームの移動を、進行上のセル中心ごとの区間へ分割して処理する。
//  *         中心に到達するたびに方向 / Goal を読み、方向転換後も残り距離を消費し続ける */
void AAnimalBase::MoveAlongGrid(float DeltaTime)
{
	// 【旧実装（保持）】固定の 256 区間上限で残り距離を切り捨て、中心到達後の
	// unswept SetActorLocation（1.0cm 以内の強制整列）を行っていた。
	// const float EffectiveSpeed = GetEffectiveMoveSpeed();
	//
	// if (CurrentMoveDirection.IsNearlyZero() || EffectiveSpeed <= 0.0f)
	// {
	// 	return;
	// }
	//
	// // マップが無い / interface 未実装の場合は従来どおりの直線移動（旧機能の保護）
	// const bool bUseGrid = (MapActor != nullptr) &&
	// 	MapActor->GetClass()->ImplementsInterface(UGridInteractInterface::StaticClass());
	//
	// // BP から軸以外の方向を設定されたケースも直線移動へフォールバックする
	// const bool bAxisAligned =
	// 	FMath::IsNearlyEqual(FMath::Abs(CurrentMoveDirection.X) + FMath::Abs(CurrentMoveDirection.Y), 1.0f, 0.01f) &&
	// 	FMath::IsNearlyZero(CurrentMoveDirection.Z);
	//
	// if (!bUseGrid || !bAxisAligned)
	// {
	// 	MoveStraight(EffectiveSpeed * DeltaTime);
	// 	return;
	// }
	//
	// float Remaining = EffectiveSpeed * DeltaTime;
	// if (Remaining <= 0.0f)
	// {
	// 	return;
	// }
	//
	// // 初回のみ出現セル中心への整列とセル読み取りを試みる
	// if (!bHasLastGridCoords)
	// {
	// 	if (TryAlignToNearestCenter(Remaining))
	// 	{
	// 		// 出現セルが Goal だった場合などはここで今フレームを終了する
	// 		return;
	// 	}
	// }
	//
	// // 1 フレームの区間数上限（異常な低 FPS × 超高速の暴走保険。通常到達しない）
	// constexpr int32 MaxSegmentsPerFrame = 256;
	// int32 SegmentCount = 0;
	//
	// while (Remaining > KINDA_SMALL_NUMBER)
	// {
	// 	if (++SegmentCount > MaxSegmentsPerFrame)
	// 	{
	// 		UE_LOG(LogTemp, Warning, TEXT("AnimalBase: reached the segment limit in one frame. Remaining distance was dropped."));
	// 		break;
	// 	}
	//
	// 	// フレーム途中に試合が終了した場合は残りの移動と Goal 処理を行わない
	// 	if (!IsMatchMovementAllowed())
	// 	{
	// 		break;
	// 	}
	//
	// 	FIntPoint NextGridCoords = FIntPoint::ZeroValue;
	// 	float DistanceToCenter = 0.0f;
	// 	if (!ComputeNextCenterAhead(NextGridCoords, DistanceToCenter))
	// 	{
	// 		// 軸方向判定が崩れた場合は残り距離を直線移動で消費する
	// 		MoveStraight(Remaining);
	// 		break;
	// 	}
	//
	// 	if (DistanceToCenter <= KINDA_SMALL_NUMBER)
	// 	{
	// 		// 念のための保護（正常な計算では発生しない）：中心処理だけ進める
	// 		if (ProcessGridCenter(NextGridCoords))
	// 		{
	// 			return;
	// 		}
	// 		continue;
	// 	}
	//
	// 	// この区間で消費する距離（中心まで / フレーム残り距離の短い方）
	// 	const float SegmentDistance = FMath::Min(Remaining, DistanceToCenter);
	// 	const FVector SegmentDirection = CurrentMoveDirection;
	//
	// 	FHitResult SweepHit;
	// 	if (!SweepMoveBy(SegmentDirection, SegmentDistance, SweepHit))
	// 	{
	// 		// 受阻した場合は強制移動せず、未到達のセル中心も読まない
	// 		return;
	// 	}
	//
	// 	Remaining -= SegmentDistance;
	//
	// 	if (SegmentDistance >= DistanceToCenter - KINDA_SMALL_NUMBER)
	// 	{
	// 		// 区間がセル中心に到達した場合のみ、中心の読み取りと整列を行う
	// 		const FVector CenterLocation = GridCenterToWorld(NextGridCoords);
	// 		if (FVector::Dist2D(GetActorLocation(), CenterLocation) <= 1.0f)
	// 		{
	// 			// sweep 到達後の画一的な誤差修正（数 mm 未満。衝突を越える強制移動ではない）
	// 			SetActorLocation(CenterLocation, false);
	// 		}
	//
	// 		if (ProcessGridCenter(NextGridCoords))
	// 		{
	// 			return;
	// 		}
	// 	}
	// }

	// 2026.10.06 Lee start（DeltaTime / 有効速度の有限性・正値検証。NaN ループを防ぐ）
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	const float EffectiveSpeed = GetEffectiveMoveSpeed();
	if (!FMath::IsFinite(EffectiveSpeed) || EffectiveSpeed <= 0.0f)
	{
		return;
	}

	// 位置・方向の有限性検証（NaN は sweep と距離計算を壊す）
	const FVector ValidatedLocation = GetActorLocation();
	if (ValidatedLocation.ContainsNaN() || CurrentMoveDirection.ContainsNaN() ||
		CurrentMoveDirection.IsNearlyZero())
	{
		return;
	}

	// 2026.10.06 Lee start（フレーム移動予算の一元算出と検証。
	// 有効速度・DeltaTime が個別に有限正値でも積は桁あふれで +Inf になり得るため、
	// 直線・グリッドの全分岐へ渡す前に一度だけ算出・検証する）
	const float FrameDistance = EffectiveSpeed * DeltaTime;
	if (!FMath::IsFinite(FrameDistance) || FrameDistance <= 0.0f)
	{
		return;
	}
	// 2026.10.06 Lee end（フレーム移動予算の一元算出と検証）

	// TileSize が無効な場合はグリッド計算が不可能なため直線移動へフォールバックする
	if (!FMath::IsFinite(TileSize) || TileSize <= KINDA_SMALL_NUMBER)
	{
		// 2026.10.06 Lee start（検証済み予算 FrameDistance へ統一。旧引数はコメントとして保持）
		// MoveStraight(EffectiveSpeed * DeltaTime);
		MoveStraight(FrameDistance);
		// 2026.10.06 Lee end（検証済み予算 FrameDistance へ統一）
		return;
	}
	// 2026.10.06 Lee end（DeltaTime / 有効速度の有限性・正値検証）

	// マップが無い / interface 未実装の場合は従来どおりの直線移動（旧機能の保護）
	const bool bUseGrid = (MapActor != nullptr) &&
		MapActor->GetClass()->ImplementsInterface(UGridInteractInterface::StaticClass());

	// BP から軸以外の方向を設定されたケースも直線移動へフォールバックする
	const bool bAxisAligned =
		FMath::IsNearlyEqual(FMath::Abs(CurrentMoveDirection.X) + FMath::Abs(CurrentMoveDirection.Y), 1.0f, 0.01f) &&
		FMath::IsNearlyZero(CurrentMoveDirection.Z);

	if (!bUseGrid || !bAxisAligned)
	{
		// 2026.10.06 Lee start（検証済み予算 FrameDistance へ統一。旧引数はコメントとして保持）
		// MoveStraight(EffectiveSpeed * DeltaTime);
		MoveStraight(FrameDistance);
		// 2026.10.06 Lee end（検証済み予算 FrameDistance へ統一）
		return;
	}

	// 2026.10.06 Lee start（残り予算は検証済み FrameDistance から初期化。
	// 旧算出と再検証はコメントとして保持）
	// float Remaining = EffectiveSpeed * DeltaTime;
	// if (!FMath::IsFinite(Remaining) || Remaining <= 0.0f)
	// {
	// 	return;
	// }
	float Remaining = FrameDistance;
	// 2026.10.06 Lee end（残り予算は検証済み FrameDistance から初期化）

	// どのセル中心もまだ処理していない間は、出現セル中心への整列を試みる。
	// partial / blocked で停止した整列は次フレームの試行で継続される
	if (!bProcessedLastGridCoords)
	{
		if (TryAlignToNearestCenter(Remaining))
		{
			// Goal 到達・受阻・予算消費のいずれかで本フレームを終了する
			return;
		}

		if (Remaining <= KINDA_SMALL_NUMBER)
		{
			return;
		}
	}

	// 2026.10.06 Lee start（進行軸に垂直な中心線からのオフセット確認。
	// オフセットが到達許容を超える動物はどのセル中心にも実際には到達しないため、
	// セル読み取りなしの直線移動へ切り替える（横ずれ状態での Goal 誤読を防ぐ））
	const FVector Origin = GetMapOrigin();
	const FVector CurrentLocation = GetActorLocation();
	const float LateralRel = (FMath::Abs(CurrentMoveDirection.X) > 0.5f)
		? (CurrentLocation.Y - Origin.Y)
		: (CurrentLocation.X - Origin.X);
	const float LateralOffset =
		FMath::Abs(LateralRel - FMath::RoundToInt(LateralRel / TileSize) * TileSize);
	if (LateralOffset > CenterArrivalTolerance)
	{
		MoveStraight(Remaining);
		return;
	}
	// 2026.10.06 Lee end（進行軸に垂直な中心線からのオフセット確認）

	// 1 フレームの移動を、進行上のセル中心ごとの区間へ分割して処理する。
	// 固定の区間数上限は設けない。Goal / 受阻 / 試合終了 / 予算消費のいずれかで抜け、
	// 各反復では実進行（残り距離・位置・処理済み中心のいずれかの変化）を要求する
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		// Goal 処理・破壊後の再入を遮断する
		if (bGoalHandled || IsActorBeingDestroyed())
		{
			return;
		}

		// フレーム途中に試合が終了した場合は残りの移動と Goal 処理を行わない
		if (!IsMatchMovementAllowed())
		{
			return;
		}

		// 非有限・負の残り距離は異常として打ち切る（NaN ループ防止）
		if (!FMath::IsFinite(Remaining) || Remaining < 0.0f)
		{
			return;
		}

		// 進行保証の比較基準（反復開始時点のスナップショット）
		const float PrevRemaining = Remaining;
		const FVector PrevLocation = GetActorLocation();
		const FIntPoint PrevProcessedCenter = LastGridCoords;
		const bool bPrevProcessedFlag = bProcessedLastGridCoords;

		FIntPoint NextGridCoords = FIntPoint::ZeroValue;
		float DistanceToCenter = 0.0f;
		if (!ComputeNextCenterAhead(NextGridCoords, DistanceToCenter))
		{
			// 軸方向判定が崩れた場合は残り距離を直線移動で消費する
			MoveStraight(Remaining);
			return;
		}

		if (DistanceToCenter <= KINDA_SMALL_NUMBER)
		{
			// 中心上：移動せず読み取りだけ進める（2D 到達は ProcessGridCenter 側で再確認）。
			// 処理済み中心は ComputeNextCenterAhead が既にスキップするため進行は保証される
			if (ProcessGridCenter(NextGridCoords))
			{
				return;
			}

			// 進行保証：中心上の処理で状態が一切変わらなければ暴走とみなして打ち切る
			const bool bStateAdvanced =
				(bProcessedLastGridCoords && LastGridCoords != PrevProcessedCenter) ||
				(bProcessedLastGridCoords != bPrevProcessedFlag);
			if (!bStateAdvanced)
			{
				UE_LOG(LogTemp, Warning, TEXT("AnimalBase: on-center processing made no progress. Stopped consuming the remaining distance this frame."));
				return;
			}
			continue;
		}

		// この区間で消費する距離（中心まで / フレーム残り距離の短い方）
		const float SegmentDistance = FMath::Min(Remaining, DistanceToCenter);
		if (!FMath::IsFinite(SegmentDistance) || SegmentDistance <= 0.0f)
		{
			return;
		}

		FHitResult SweepHit;
		if (!SweepMoveBy(CurrentMoveDirection, SegmentDistance, SweepHit))
		{
			// 受阻した場合は強制移動せず、未到達のセル中心も読まない
			return;
		}

		Remaining -= SegmentDistance;

		if (SegmentDistance >= DistanceToCenter - KINDA_SMALL_NUMBER)
		{
			// 区間がセル中心に到達した場合のみ読み取る。unswept の強制整列は行わず、
			// sweep 検証済みの実移動に残る極小の世界誤差は ProcessGridCenter の
			// 到達許容が吸収する
			if (ProcessGridCenter(NextGridCoords))
			{
				return;
			}
		}

		// 進行保証：残り距離・位置・処理済み中心のいずれも変化しなければ打ち切る
		const bool bNoProgress =
			FMath::IsNearlyEqual(Remaining, PrevRemaining) &&
			GetActorLocation().Equals(PrevLocation, KINDA_SMALL_NUMBER) &&
			LastGridCoords == PrevProcessedCenter &&
			bProcessedLastGridCoords == bPrevProcessedFlag;
		if (bNoProgress)
		{
			UE_LOG(LogTemp, Warning, TEXT("AnimalBase: segment loop made no real progress. Stopped consuming the remaining distance this frame."));
			return;
		}
	}
}
// 2026.10.06 Lee end（固定区間上限の廃止・前提値検証・横ずれ保護・進行保証へ修正）
// 2026.10.06 Lee end
