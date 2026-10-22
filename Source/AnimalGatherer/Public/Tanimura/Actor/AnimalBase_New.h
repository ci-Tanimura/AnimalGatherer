#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameTypes.h"
#include "Tanimura/GridInteractInterface.h"
#include "AnimalBase_New.generated.h"

// 動物がグリッド上で進む方向（盤面のETileTypeとは別の概念として扱う）
UENUM(BlueprintType)
enum class EGridDirection : uint8
{
	None = 0	UMETA(DisplayName = "移動なし"),
	Up			UMETA(DisplayName = "上"),
	Down		UMETA(DisplayName = "下"),
	Left		UMETA(DisplayName = "左"),
	Right		UMETA(DisplayName = "右")
};

/**
 * 盤面を矢印の向きに進む動物（盤外へ出たら海へ落下する）
 */
UCLASS()
class ANIMALGATHERER_API AAnimalBase_New : public AActor
{
	GENERATED_BODY()

public:
	AAnimalBase_New();

	virtual void Tick(float DeltaSeconds) override;

	// 1マス進むのに必要な秒数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float SecondsPerCell = 1.0f;

	// 落下時の重力加速度（cm/s^2）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FallGravity = 980.0f;

	// 落ち始めてから消えるまでの深さ（cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FallKillDepth = 2000.0f;

	// 盤面と初期の向きを受け取る
	UFUNCTION(BlueprintCallable, Category = "Animal")
	void Initialize(const TScriptInterface<IGridInteractInterface>& InGridMap, const EGridDirection InInitialFacing);

protected:
	virtual void BeginPlay() override;

private:
	// 位置と向きの基準となるルート
	UPROPERTY(VisibleAnywhere, Category = "Animal")
	TObjectPtr<class USceneComponent> Root;

	// 盤面情報を提供するアクター
	TScriptInterface<IGridInteractInterface> GridMap;

	// 出現時の向き
	EGridDirection InitialFacing = EGridDirection::Right;

	// 現在いるマス
	FIntPoint CurrentCell = FIntPoint::ZeroValue;

	// 移動中の到達先マス
	FIntPoint TargetCell = FIntPoint::ZeroValue;

	// 現在向いている方向
	EGridDirection Facing = EGridDirection::Right;

	// 補間の進捗（0.0 ～ 1.0）
	float MoveAlpha = 1.0f;

	// 補間区間の始点と終点
	FVector SegmentStart = FVector::ZeroVector;
	FVector SegmentEnd = FVector::ZeroVector;

	// 盤外へ出て落下中か
	bool bFalling = false;

	// 落下中の落下速度（cm/s）
	float FallSpeed = 0.0f;

	// 落下を始めた高さ
	float FallStartZ = 0.0f;

	// 補間を進め、完了したら到達処理へ引き渡す
	void Advance(float DeltaSeconds);

	// 到達マスの矢印を読み、向きを変えてから前へ進む
	void EnterCell();

	// 向きを変え、体をその方向へ回す
	void TurnTo(EGridDirection NewFacing);

	// 今向いている方向へ1マス進む区間を開始する
	void StepForward();

	// 盤外へ出た動物の落下を開始する
	void StartFalling();

	// 重力に従って落下し、規定の深さで消える
	void Fall(float DeltaSeconds);

	// 自分の座標から現在いるマスを求める
	FIntPoint ResolveCurrentCell() const;

	// 指定マスのタイルから向きを読む（矢印以外はNone）
	EGridDirection ReadDirection(const FIntPoint& Cell) const;

	// 盤面のタイルサイズを測る
	float MeasureTileSize() const;

	// グリッド座標をマス中心のワールド座標へ変換する
	FVector CellToWorld(const FIntPoint& Cell) const;

	// 盤面が注入されているか
	bool HasGridMap() const;

	// グリッド座標が盤面内か
	bool IsInside(const FIntPoint& Cell) const;

	// タイル種別を向きへ変換する
	static EGridDirection ToDirection(ETileType TileType);

	// 向きをグリッドの移動量へ変換する
	static FIntPoint ToDelta(EGridDirection Direction);
};
