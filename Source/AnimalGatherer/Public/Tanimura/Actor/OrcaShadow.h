#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "OrcaShadow.generated.h"

/**
 * 海面下をランダムに泳ぐシャチの影
 * メッシュは BP 側で構築し、このクラスは移動のみを担当する
 */
UCLASS()
class ANIMALGATHERER_API AOrcaShadow : public AActor
{
	GENERATED_BODY()

public:
	AOrcaShadow();

	// 泳ぐ速さ（cm/秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orca|Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 180.0f;

	// 速さのばらつき（±cm/秒）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orca|Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpeedVariation = 40.0f;

	// 徘徊範囲の半径（cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orca|Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WanderRadius = 1500.0f;

	// 目標地点に到達したと判定する距離（cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orca|Movement", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float ArriveRadius = 50.0f;

	// 旋回の追従速度。大きいほど機敏に曲がる
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orca|Movement", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TurnInterpSpeed = 1.2f;

	// 徘徊範囲をデバッグ表示するか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orca|Debug")
	bool bDrawDebugWander = false;

	// 徘徊範囲の中心を返す
	UFUNCTION(BlueprintPure, Category = "Orca")
	FVector GetHomeLocation() const
	{
		return HomeLocation;
	}

	// 次の目標地点を選び直す
	UFUNCTION(BlueprintCallable, Category = "Orca")
	void PickNewTarget();

	// 目標地点を変更したときに BP へ通知する
	UFUNCTION(BlueprintImplementableEvent, Category = "Orca")
	void OnTargetChanged();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// BP のメッシュをぶら下げるルート
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orca|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 徘徊範囲の中心。開始時の位置を保持する
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orca|Movement")
	FVector HomeLocation = FVector::ZeroVector;

	// 現在の目標地点。Z は開始時の高さを保持する
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orca|Movement")
	FVector TargetLocation = FVector::ZeroVector;

	// 目標地点へ向かう現在の速さ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orca|Movement")
	float CurrentSpeed = 0.0f;
};
