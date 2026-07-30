// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tanimura/MiniGame/MiniGameTypes.h"
#include "GridTile.generated.h"

class UStaticMeshComponent;

UCLASS()
class ANIMALGATHERER_API AGridTile : public AActor
{
    GENERATED_BODY()

public:
    AGridTile();

protected:
    virtual void BeginPlay() override;

public:
    // タイルの見た目を担うメッシュ
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* TileMesh;

    // タイルの状態を変更し、必要に応じて見た目を更新する
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetTileState(ETileState NewState, float MoveInterval);

    // 現在のタイルの状態を取得する
    UFUNCTION(BlueprintPure, Category = "Tile")
    ETileState GetTileState() const;

protected:
    // 現在のタイルの状態
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Tile")
    ETileState CurrentTileState;

    // 状態が変わった際に Blueprint 側でマテリアルやアザラシの表示を切替えるイベント
    UFUNCTION(BlueprintImplementableEvent, Category = "Tile|Events")
    void OnTileStateChanged(ETileState NewState, float MoveInterval);

    // プレイヤー（ペンギン）がマスに乗った時の判定処理
    UFUNCTION()
    void OnTileBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};