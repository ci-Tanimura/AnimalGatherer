// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AnimalBase.generated.h"

UCLASS()
class ANIMALGATHERER_API AAnimalBase : public APawn
{
	GENERATED_BODY()

public:
	AAnimalBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	//動物を動かすための基準コンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Components")
	TObjectPtr<class USceneComponent> SceneRoot;

	//1秒あたりの移動速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Settings", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 100.0f;

	//初期移動方向
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animal Movement")
	FVector InitialMoveDirection = FVector::ForwardVector;

	//現在の移動方向
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animal Movement")
	FVector CurrentMoveDirection = FVector::ZeroVector;

	//移動方向を変更する
	UFUNCTION(BlueprintCallable, Category = "Animal Movement")
	void SetMoveDirection(FVector NewMoveDirection);

protected:
	//足元のマスを読み取り、移動方向を変更するための入口
	virtual void UpdateMoveDirectionFromCurrentTile();

	//現在の方向へ移動する
	void MoveAnimal(float DeltaTime);
};
