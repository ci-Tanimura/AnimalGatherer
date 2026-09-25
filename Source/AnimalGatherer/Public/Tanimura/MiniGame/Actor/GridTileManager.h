// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tanimura/MiniGame/MiniGameTypes.h"
#include "GridTileManager.generated.h"

// 前方宣言
class AGridTile;

/**
 * 盤面上に存在する個々の敵（アザラシ等）の進行状態および内部タイマーを保持する構造体
 */
USTRUCT()
struct FActiveEnemyData
{
    GENERATED_BODY()

    // 現在敵が位置しているグリッドのX座標 (0 〜 GridWidth - 1)
    int32 CurrentX = 0;

    // 現在敵が位置しているグリッドのY座標 (0 〜 GridHeight - 1)
    int32 CurrentY = 0;

    // 現在進行している8方向の移動ベクトル
    EGridDirection8 Direction = EGridDirection8::None;

    // 現在の移動マス数
    int32 CurrentMoveCount = 0;

    // 次のマスへ移動するまでの経過時間を計測するタイマー (秒)
    float MoveTimer = 0.0f;

    // 予兆状態 (Warning) から危険状態 (ActiveHazard) へ遷移するまでの残り時間 (秒)
    float WarningTimer = 0.0f;

    // 予兆時間が終了し、実際に攻撃判定を持っているかどうかのフラグ
    bool bIsActive = false;
};

/**
 * グリッド盤面の自動生成と、その上を移動する敵（捕食者）の発生・挙動を一括管理するマネージャークラス
 */
UCLASS()
class ANIMALGATHERER_API AGridTileManager : public AActor
{
    GENERATED_BODY()

public:
    AGridTileManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 盤面設定 (Grid Settings)

    // 盤面の横方向（X軸）のマス数
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 GridWidth = 16;

    // 盤面の縦方向（Y軸）のマス数
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    int32 GridHeight = 9;

    // タイル同士の配置間隔 (cm)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    float TileSpacing = 100.0f;

    // 盤面生成時にスポーンさせるタイルのBlueprintクラス参照
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Settings")
    TSubclassOf<AGridTile> TileClass;

    // ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 敵（捕食者）の設定パラメータ (Enemy Settings)

    // 敵の移動速度、予兆時間、移動数上限などの調整パラメータ群
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Settings")
    FEnemyParameters EnemyParameters;

protected:
    // 生成された全タイルのデータ（座標、状態、Actor参照）を保持する一次元配列
    // インデックスの計算式: Index = Y × GridWidth + X
    UPROPERTY(Transient)
    TArray<FGridTileData> GridTiles;

    // 現在盤面上を移動・存在している敵データの動的配列
    TArray<FActiveEnemyData> ActiveEnemies;

    // 一定間隔で新たな敵を自動生成するためのタイマーハンドラ
    FTimerHandle SpawnTimerHandle;

	// 盤面（GridWidth x GridHeight）分のタイルActorを格子状に生成する
    void GenerateGrid();

    // ランダムなマスと方向を選定し、新しい敵の予兆を生成する
    void SpawnEnemy();

    // 生存している全敵のタイマーを進め、移動およびタイルの危険状態を更新する
    void UpdateEnemies(float DeltaTime);

    // 2次元グリッド座標 (X, Y) から、1次元配列 (GridTiles) のインデックスを算出する
    int32 GetTileIndex(int32 X, int32 Y) const;

    // 指定された (X, Y) 座標が盤面の有効範囲内か判定する
    bool IsValidCoordinate(int32 X, int32 Y) const;

    // 8方向の列挙型 (EGridDirection8) から、X軸・Y軸それぞれの移動オフセット (dX, dY) を算出する
    void GetDirectionOffset(EGridDirection8 Direction, int32& OutDX, int32& OutDY) const;

    // 盤面の端に達した際、物理的な入射角・反射角に基づいた反転方向を計算する
    EGridDirection8 GetReflectedDirection(int32 CurrentX, int32 CurrentY, EGridDirection8 CurrentDir) const;

    // 指定した (X, Y) 座標のタイルステートを安全に変更し、見た目の更新を呼び出す
    void SetTileStateAt(int32 X, int32 Y, ETileState NewState, EGridDirection8 Direction = EGridDirection8::None);
};