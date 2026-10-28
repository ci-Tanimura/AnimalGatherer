#include "Tanimura/Actor/OrcaShadow.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

AOrcaShadow::AOrcaShadow()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// BP のメッシュをぶら下げるルート
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AOrcaShadow::BeginPlay()
{
	Super::BeginPlay();

	// 配置位置を徘徊の中心とする
	HomeLocation = GetActorLocation();
	PickNewTarget();
}

void AOrcaShadow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector CurrentLocation = GetActorLocation();
	const FVector Target = FVector(TargetLocation.X, TargetLocation.Y, CurrentLocation.Z);
	FVector ToTarget = Target - CurrentLocation;
	ToTarget.Z = 0.0f;

	const float Distance = ToTarget.Size();

	// 目標に到達したら次の地点を選ぶ
	if (Distance <= ArriveRadius) {
		PickNewTarget();
		return;
	}

	const FVector Direction = ToTarget / Distance;

	// 1フレームで目標を越えない距離に抑える
	const float StepDistance = FMath::Min(CurrentSpeed * DeltaSeconds, Distance);
	SetActorLocation(CurrentLocation + Direction * StepDistance);

	// 進行方向へ滑らかに旋回する
	const FRotator DesiredRotation = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaSeconds, TurnInterpSpeed);
	SetActorRotation(NewRotation);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugWander) {
		// 深度テストを無効にして最前面へ描き、水面や地形に隠れないようにする
		DrawDebugCircle(GetWorld(), HomeLocation, WanderRadius, 64, FColor::Cyan, false, -1.0f, SDPG_Foreground, 8.0f, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);
		DrawDebugPoint(GetWorld(), HomeLocation, 40.0f, FColor::Yellow, false, -1.0f, SDPG_Foreground);
		DrawDebugPoint(GetWorld(), TargetLocation, 40.0f, FColor::Red, false, -1.0f, SDPG_Foreground);
		DrawDebugLine(GetWorld(), CurrentLocation, TargetLocation, FColor::Green, false, -1.0f, SDPG_Foreground, 6.0f);

		// Tick が動いているかを画面上で確認する
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("%s  Speed=%.0f  Dist=%.0f"), *GetName(), CurrentSpeed, Distance));
		}
	}
#endif
}

void AOrcaShadow::PickNewTarget()
{
	// 配置位置を中心とした円内から次の目標を選ぶ
	const FVector2D Offset = FMath::RandPointInCircle(WanderRadius);
	TargetLocation = HomeLocation + FVector(Offset.X, Offset.Y, 0.0f);

	// 目標ごとに速さを変えて泳ぎに緩急をつける
	CurrentSpeed = FMath::Max(10.0f, MoveSpeed + FMath::FRandRange(-SpeedVariation, SpeedVariation));

	OnTargetChanged();
}
