// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MiniGame/Actor/GridTileManager.h"
#include "Tanimura/MiniGame/Actor/GridTile.h"
#include "Engine/World.h"
#include "TimerManager.h"

AGridTileManager::AGridTileManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AGridTileManager::BeginPlay()
{
    Super::BeginPlay();

    GenerateGrid();

    // 設定した発生周期に基づいてSpawnEnemyを自動実行するタイマーをセット
    if (GetWorld() && EnemyParameters.SpawnInterval > 0.0f) {
        GetWorld()->GetTimerManager().SetTimer(
            SpawnTimerHandle,
            this,
            &AGridTileManager::SpawnEnemy,
            EnemyParameters.SpawnInterval,
            true // ループ実行を有効化
        );
    }
}

void AGridTileManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateEnemies(DeltaTime);
}

void AGridTileManager::GenerateGrid()
{
    // タイルクラスが指定されていない場合は抜ける
    if (!TileClass) {
        return;
    }

    const FVector ManagerLocation = GetActorLocation();

    // メモリ再確保によるオーバーヘッドを減らすため、あらかじめ配列容量を確保
    GridTiles.Reserve(GridWidth * GridHeight);

    // 二重ループにより、X方向・Y方向へ格子状にタイルを配置
    for (int32 Y = 0; Y < GridHeight; ++Y) {
        for (int32 X = 0; X < GridWidth; ++X) {
            // マネージャーの位置を原点（基準）として、TileSpacing分だけオフセットした位置を計算
            FVector TileOffset = FVector(X * TileSpacing, Y * TileSpacing, 0.0f);
            FVector SpawnLocation = ManagerLocation + TileOffset;
            FRotator SpawnRotation = GetActorRotation();

            // スポーンパラメータの設定（マネージャーをOwnerとして設定）
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;

            // ワールド上にタイルActorを動的スポーン
            AGridTile* SpawnedTile = GetWorld()->SpawnActor<AGridTile>(TileClass, SpawnLocation, SpawnRotation, SpawnParams);

            // 盤面管理構造体の作成と初期化
            FGridTileData TileData;
            TileData.WorldLocation = SpawnLocation;
            TileData.TileState = ETileState::Safe; // 初期状態はすべて「安全」
            TileData.TileActor = SpawnedTile;

            // 配列へ格納
            GridTiles.Add(TileData);
        }
    }
}

void AGridTileManager::SpawnEnemy()
{
    // 盤面が存在しない場合は処理しない
    if (GridTiles.Num() == 0) {
        return;
    }

    // 盤面内のランダムなX座標・Y座標を算出
    int32 SpawnX = FMath::RandRange(0, GridWidth - 1);
    int32 SpawnY = FMath::RandRange(0, GridHeight - 1);

    // 8方向（1:Up ～ 8:DownRight）の中からランダムに進行方向を決定
    EGridDirection8 SpawnDir = static_cast<EGridDirection8>(FMath::RandRange(1, 8));

    // 新しい敵データの構築
    FActiveEnemyData NewEnemy;
    NewEnemy.CurrentX = SpawnX;
    NewEnemy.CurrentY = SpawnY;
    NewEnemy.Direction = SpawnDir;
    NewEnemy.CurrentMoveCount = 0;
    NewEnemy.MoveTimer = 0.0f;
    NewEnemy.WarningTimer = EnemyParameters.WarningDuration; // パラメータから予兆時間を設定
    NewEnemy.bIsActive = false;

    // 管理リストへ追加
    ActiveEnemies.Add(NewEnemy);

    // 敵が出現したマスの状態を「予兆 (Warning)」に切り替えてプレイヤーに警告
    SetTileStateAt(SpawnX, SpawnY, ETileState::Warning);
}

void AGridTileManager::UpdateEnemies(float DeltaTime)
{
    // 途中削除（RemoveAt）に伴うインデックスズレを防ぐため、配列の末尾から逆順ループ処理
    for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index) {
        FActiveEnemyData& Enemy = ActiveEnemies[Index];

        // ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
        // 予兆状態 (Warning) のタイマー処理
        if (!Enemy.bIsActive) {
            Enemy.WarningTimer -= DeltaTime;

            // 予兆時間が経過したら、危険領域 (ActiveHazard) へ
            if (Enemy.WarningTimer <= 0.0f) {
                Enemy.bIsActive = true;
                SetTileStateAt(Enemy.CurrentX, Enemy.CurrentY, ETileState::ActiveHazard);
            }
            // 予兆中はまだ次のマスへ移動しないためループをスキップ
            continue;
        }

        // ーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
        // 移動と消滅処理

        Enemy.MoveTimer += DeltaTime;

        // 設定された周期に達したら1マス移動
        if (Enemy.MoveTimer >= EnemyParameters.MoveInterval) {
            // 誤差が累積しないよう、端数を残してリセット
            Enemy.MoveTimer -= EnemyParameters.MoveInterval;

            // 直前まで存在していたマスは「安全 (Safe)」状態に戻す
            SetTileStateAt(Enemy.CurrentX, Enemy.CurrentY, ETileState::Safe);

            Enemy.CurrentMoveCount++;

            // 移動回数の上限に達した場合は、移動を行わずに敵を消滅させる
            if (Enemy.CurrentMoveCount >= EnemyParameters.MaxTravelMoves) {
                ActiveEnemies.RemoveAt(Index);
                continue;
            }

            // 現在の進行方向から、移動先の相対オフセット (DX, DY) を取得
            int32 DX = 0;
            int32 DY = 0;
            GetDirectionOffset(Enemy.Direction, DX, DY);

            // 移動予定の座標を計算
            int32 NextX = Enemy.CurrentX + DX;
            int32 NextY = Enemy.CurrentY + DY;

            // 移動先が端を超えてしまう場合、反射ベクトルを計算して進行方向を更新
            if (!IsValidCoordinate(NextX, NextY)) {
                Enemy.Direction = GetReflectedDirection(Enemy.CurrentX, Enemy.CurrentY, Enemy.Direction);

                // 新しく反射した方向に基づいて再度移動先座標を計算
                GetDirectionOffset(Enemy.Direction, DX, DY);
                NextX = Enemy.CurrentX + DX;
                NextY = Enemy.CurrentY + DY;
            }

            // 確定した移動先座標に更新
            Enemy.CurrentX = NextX;
            Enemy.CurrentY = NextY;

            // 残り移動ステップ数を計算
            int32 RemainingMoves = EnemyParameters.MaxTravelMoves - Enemy.CurrentMoveCount;

            // 残り移動回数が閾値以下なら「消滅直前 (ExpiringHazard)」、それ以外なら「危険 (ActiveHazard)」を適用
            ETileState NewState;
            if (RemainingMoves <= EnemyParameters.ExpirationWarningMoves) {
                NewState = ETileState::ExpiringHazard;
            }
            else {
                NewState = ETileState::ActiveHazard;
            }

            // 新しいマス目を危険状態へ切り替え
            SetTileStateAt(Enemy.CurrentX, Enemy.CurrentY, NewState);
        }
    }
}

int32 AGridTileManager::GetTileIndex(int32 X, int32 Y) const
{
    // 2次元の行・列番号を1次元配列の要素番号に変換
    return Y * GridWidth + X;
}

bool AGridTileManager::IsValidCoordinate(int32 X, int32 Y) const
{
    // 座標が盤面の境界線内部に収まっているかチェック
    return X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight;
}

void AGridTileManager::GetDirectionOffset(EGridDirection8 Direction, int32& OutDX, int32& OutDY) const
{
    OutDX = 0;
    OutDY = 0;

    // 8方向それぞれに対するX軸・Y軸の移動量を定義
    switch (Direction) {
    case EGridDirection8::Up:
        OutDY = 1;
        break;
    case EGridDirection8::Down:
        OutDY = -1;
        break;
    case EGridDirection8::Left:
        OutDX = -1;
        break;
    case EGridDirection8::Right:
        OutDX = 1;
        break;
    case EGridDirection8::UpLeft:
        OutDX = -1;
        OutDY = 1;
        break;
    case EGridDirection8::UpRight:
        OutDX = 1;
        OutDY = 1;
        break;
    case EGridDirection8::DownLeft:
        OutDX = -1;
        OutDY = -1;
        break;
    case EGridDirection8::DownRight:
        OutDX = 1;
        OutDY = -1;
        break;
    default:
        break;
    }
}

EGridDirection8 AGridTileManager::GetReflectedDirection(int32 CurrentX, int32 CurrentY, EGridDirection8 CurrentDir) const
{
    int32 DX = 0;
    int32 DY = 0;
    GetDirectionOffset(CurrentDir, DX, DY);

    // 進行予定の座標を仮計算
    int32 TargetX = CurrentX + DX;
    int32 TargetY = CurrentY + DY;

    // 左右の外壁に衝突しているか判定
    bool bReflectX = (TargetX < 0 || TargetX >= GridWidth);

    // 上下の外壁に衝突しているか判定
    bool bReflectY = (TargetY < 0 || TargetY >= GridHeight);

    // X軸（左右壁）に衝突した場合は横方向のベクトルを反転
    if (bReflectX) {
        DX = -DX;
    }

    // Y軸（上下壁）に衝突した場合は縦方向のベクトルを反転
    if (bReflectY) {
        DY = -DY;
    }

    // 反転後のベクトル (DX, DY) に対応する方向を判定して返す
    if (DX == 0 && DY == 1) { return EGridDirection8::Up; }
    if (DX == 0 && DY == -1) { return EGridDirection8::Down; }
    if (DX == -1 && DY == 0) { return EGridDirection8::Left; }
    if (DX == 1 && DY == 0) { return EGridDirection8::Right; }
    if (DX == -1 && DY == 1) { return EGridDirection8::UpLeft; }
    if (DX == 1 && DY == 1) { return EGridDirection8::UpRight; }
    if (DX == -1 && DY == -1) { return EGridDirection8::DownLeft; }
    if (DX == 1 && DY == -1) { return EGridDirection8::DownRight; }

    return EGridDirection8::None;
}

void AGridTileManager::SetTileStateAt(int32 X, int32 Y, ETileState NewState)
{
    if (!IsValidCoordinate(X, Y)) {
        return;
    }

    int32 Index = GetTileIndex(X, Y);
    if (GridTiles.IsValidIndex(Index)) {
        // マネージャー側の内部データ状態を更新
        GridTiles[Index].TileState = NewState;

        // タイルのインスタンスが存在していれば、Actor側の状態・見た目更新関数を呼び出し
        if (AGridTile* TileActor = GridTiles[Index].TileActor) {
            TileActor->SetTileState(NewState);
        }
    }
}