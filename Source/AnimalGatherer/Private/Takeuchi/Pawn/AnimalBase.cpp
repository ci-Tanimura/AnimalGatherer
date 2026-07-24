#include "Takeuchi/Pawn/AnimalBase.h"

#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
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

	SetMoveDirection(InitialMoveDirection);

	if (!MapActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimalBase: MapActor is not set."));
	}
}

void AAnimalBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//現在いるタイルの情報を読み取り、必要なら移動方向を更新する
	UpdateMoveDirectionFromCurrentTile();

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
void AAnimalBase::UpdateMoveDirectionFromCurrentTile()
{
	//マップ参照が設定されていない場合は、タイル情報を取得できないため処理しない
	if (!MapActor)
	{
		return;
	}

	//参照先が GridInteractInterface を実装していない場合は、タイル情報を取得できないため処理しない
	if (!MapActor->GetClass()->ImplementsInterface(UGridInteractInterface::StaticClass()))
	{
		return;
	}

	//動物の座標を取得
	const FVector AnimalLocation = GetActorLocation();

	//マップアクターの位置を、グリッド計算の原点として扱う
	const FVector MapOrigin = MapActor->GetActorLocation();

	//動物のワールド座標を、マップ上のグリッド座標に変換する
	const FIntPoint CurrentGridCoords(
		FMath::FloorToInt((AnimalLocation.X - MapOrigin.X) / TileSize),
		FMath::FloorToInt((AnimalLocation.Y - MapOrigin.Y) / TileSize)
	);

	//前回と同じマスにいる場合は、同じタイル処理を繰り返さない
	if (bHasLastGridCoords && CurrentGridCoords == LastGridCoords)
	{
		return;
	}

	//現在のマスを、最後に処理したマスとして記録する
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
	if (CurrentMoveDirection.IsNearlyZero() || MoveSpeed <= 0.0f)
	{
		return;
	}

	//現在の移動方向、移動速度、前フレームからの経過時間を使って移動量を計算する
	const FVector MoveDelta = CurrentMoveDirection * MoveSpeed * DeltaTime;

	//衝突を無視して、計算した移動量だけワールド座標で移動する
	AddActorWorldOffset(MoveDelta, true);
}
