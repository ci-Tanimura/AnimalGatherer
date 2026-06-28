// Fill out your copyright notice in the Description page of Project Settings.


#include "Lee/MapManager.h"

// Sets default values
AMapManager::AMapManager()
{
	// このアクターでは Tick は不要なので無効にする（パフォーマンス向上）
	PrimaryActorTick.bCanEverTick = false;

	// --- ベースフロア（常時表示される床メッシュ） ---
	HISM_BaseFloor = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_BaseFloor"));
	RootComponent = HISM_BaseFloor;

	// --- タイルの状態ごとに専用の HISM レイヤーを作成 ---
	// 各レイヤーはベースフロアにアタッチされ、Empty(0) を除く全状態をカバーする
	for (uint8 i = 1; i <= 7; ++i)
	{
        ETileType Type = static_cast<ETileType>(i);
        FString CompName = FString::Printf(TEXT("HISM_State_%d"), i);

        UHierarchicalInstancedStaticMeshComponent* NewHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*CompName);
        NewHISM->SetupAttachment(RootComponent);

        // TMap に保存し、後で状態ごとにインスタンスを追加できるようにする
        StateVisuals.Add(Type, NewHISM);
    }
}

// Called when the game starts or when spawned
void AMapManager::BeginPlay()
{
	Super::BeginPlay();
	
}


void AMapManager::UpdateMapVisuals()
{
    // 1. すべての状態レイヤーをクリアする（BaseFloor は常に表示するのでそのまま）
    for (auto& Pair : StateVisuals)
    {
        Pair.Value->ClearInstances();
    }

    // 2. 全タイルのデータを走査し、現在の状態に応じたメッシュを配置する
    for (const FMapTileData& Tile : GridData)
    {
        // Empty 以外のタイルのみ処理する
        if (Tile.TileType != ETileType::Empty)
        {
            if (UHierarchicalInstancedStaticMeshComponent** TargetHISM = StateVisuals.Find(Tile.TileType))
            {
                FTransform InstanceTransform;
                // Z 方向に 1cm 浮かせてベース床との Z ファイティングを防止
                FVector LocalLocation = Tile.WorldLocation - GetActorLocation();
                InstanceTransform.SetLocation(LocalLocation + FVector(0, 0, 1.0f));
                (*TargetHISM)->AddInstance(InstanceTransform);
            }
        }
    }
}

void AMapManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // GridData が未初期化、またはマップサイズが変更された場合は全データを再生成する
    if (GridData.Num() != MapWidth * MapHeight)
    {
        ResetAndGenerateBlankMap();
    }
    else
    {
        // データが既に存在する場合はビジュアルのみ更新する
        UpdateMapVisuals();
    }
}

void AMapManager::ApplyTileEdit()
{
    // 指定された座標がマップ範囲内かを検証する
    if (Edit_X >= 0 && Edit_X < MapWidth && Edit_Y >= 0 && Edit_Y < MapHeight)
    {
        // 1次元配列で2次元グリッドを表現するためのインデックス計算
        int32 Index = Edit_Y * MapWidth + Edit_X;

        // 選択されたタイルの種類を変更する
        GridData[Index].TileType = Edit_TileType;

        // 変更を即座に画面に反映する
        UpdateMapVisuals();
    }
}

void AMapManager::ResetAndGenerateBlankMap()
{
    // 既存のデータをクリアし、新しいサイズでメモリを事前確保する
    GridData.Empty();
    GridData.Reserve(MapWidth * MapHeight);

    // ベースフロアの既存インスタンスを削除して重複を防ぐ
    if (HISM_BaseFloor)
    {
        HISM_BaseFloor->ClearInstances();
    }

    FVector StartLocation = GetActorLocation();

    // 全タイルを走査して初期化する
    for (int32 Y = 0; Y < MapHeight; Y++)
    {
        for (int32 X = 0; X < MapWidth; X++)
        {
            FMapTileData NewTile;
            NewTile.WorldLocation = StartLocation + FVector(X * TileSize, Y * TileSize, 0.0f);
            NewTile.TileType = ETileType::Empty;
            GridData.Add(NewTile);

            // ベースフロア用のインスタンスを追加する
            if (HISM_BaseFloor)
            {
                FTransform InstanceTransform;
                InstanceTransform.SetLocation(FVector(X * TileSize, Y * TileSize, 0.0f));
                HISM_BaseFloor->AddInstance(InstanceTransform);
            }
        }
    }

    // 全タイルのビジュアルを最新の状態に更新する
    UpdateMapVisuals();
}



