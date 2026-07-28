#include "Takeuchi/Pawn/AnimalBase.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/MapManager.h"
#include "Tanimura/GridInteractInterface.h"
#include "Tanimura/MainGameMode.h"

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
}

void AAnimalBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//現在いるタイルの情報を読み取り、必要なら移動方向を更新する
	UpdateMoveDirectionFromCurrentTile(DeltaTime);

	MoveAnimal(DeltaTime);
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
	MapActor = NewMapActor;
	bHasLastGridCoords = false;
}

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
		SetMoveDirection(FVector(-1.0f, 0.0f, 0.0f));
		break;

	case ETileType::DirRight:
		SetMoveDirection(FVector(1.0f, 0.0f, 0.0f));
		break;

	case ETileType::GoalP1:
	{
		if (AMainGameMode* GameMode =
			Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GameMode->AddScore(0, 1);
			Destroy();
		}
		break;
	}

	case ETileType::GoalP2:
	{
		if (AMainGameMode* GameMode =
			Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GameMode->AddScore(1, 1);
			Destroy();
		}
		break;
	}

	default:
		break;
	}
}

void AAnimalBase::MoveAnimal(float DeltaTime)
{
	//移動方向がない、または移動速度が0以下の場合は移動しない
	if (CurrentMoveDirection.IsNearlyZero() || MoveSpeed <= 0.0f)return;

	//現在の移動方向、移動速度、前フレームからの経過時間を使って移動量を計算する
	const FVector MoveDelta = CurrentMoveDirection * MoveSpeed * DeltaTime;

	//衝突を無視して、計算した移動量だけワールド座標で移動する
	AddActorWorldOffset(MoveDelta, true);
}
