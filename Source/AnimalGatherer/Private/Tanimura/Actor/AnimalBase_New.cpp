#include "Tanimura/Actor/AnimalBase_New.h"

#include "Components/SceneComponent.h"

AAnimalBase_New::AAnimalBase_New()
{
	PrimaryActorTick.bCanEverTick = true;

	// ルートを作成
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
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

	// 落下中は移動ではなく落下だけを進める
	if (bFalling) {
		Fall(DeltaSeconds);
		return;
	}

	Advance(DeltaSeconds);
}

// 補間の進捗を進め、マスへ到達したら到達処理へ引き渡す
void AAnimalBase_New::Advance(float DeltaSeconds)
{
	if (SecondsPerCell <= KINDA_SMALL_NUMBER) {
		return;
	}

	MoveAlpha = FMath::Clamp(MoveAlpha + DeltaSeconds / SecondsPerCell, 0.0f, 1.0f);

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

// 到達マスの矢印を読み、向きを変えてから前へ進む
void AAnimalBase_New::EnterCell()
{
	// 矢印は向きを変えるだけ。矢印以外はNoneが返り向きは維持される
	const EGridDirection ArrowDirection = ReadDirection(CurrentCell);
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

// 指定マスのタイル種別から向きを読む
EGridDirection AAnimalBase_New::ReadDirection(const FIntPoint& Cell) const
{
	if (!HasGridMap()) {
		return EGridDirection::None;
	}

	return ToDirection(IGridInteractInterface::Execute_GetCellState(GridMap.GetObject(), Cell));
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

// 向きをグリッドの移動量へ対応付ける
FIntPoint AAnimalBase_New::ToDelta(EGridDirection Direction)
{
	switch (Direction) {
	case EGridDirection::Up:
		return FIntPoint(0, 1);

	case EGridDirection::Down:
		return FIntPoint(0, -1);

	case EGridDirection::Left:
		return FIntPoint(-1, 0);

	case EGridDirection::Right:
		return FIntPoint(1, 0);

	default:
		return FIntPoint::ZeroValue;
	}
}
