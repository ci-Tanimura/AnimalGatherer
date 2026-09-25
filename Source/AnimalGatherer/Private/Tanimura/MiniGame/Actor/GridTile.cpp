// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MiniGame/Actor/GridTile.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Tanimura/MiniGame/DamageableInterface.h"

AGridTile::AGridTile()
{
    // Tickは不要なためオフにし、パフォーマンスを最適化
    PrimaryActorTick.bCanEverTick = false;

    // メッシュコンポーネントの初期化
    TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
    RootComponent = TileMesh;

    // コリジョンの設定（Overlapイベントを有効化）
    TileMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    TileMesh->SetGenerateOverlapEvents(true);

    CurrentTileState = ETileState::Safe;
}

void AGridTile::BeginPlay()
{
    Super::BeginPlay();

    // 接触判定イベントのバインド
    if (TileMesh) {
        TileMesh->OnComponentBeginOverlap.AddDynamic(this, &AGridTile::OnTileBeginOverlap);
    }
}

void AGridTile::SetTileState(ETileState NewState, float MoveInterval, EGridDirection8 Direction)
{
    // 状態が変更された場合のみ処理を実行
    if (CurrentTileState != NewState) {
        CurrentTileState = NewState;

        // BP側へ通知（色を変える、アザラシを出す等）
        OnTileStateChanged(CurrentTileState, MoveInterval, Direction);
    }
}

ETileState AGridTile::GetTileState() const
{
    return CurrentTileState;
}

void AGridTile::OnTileBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 自身との衝突、または無効なアクターは除外
    if (!OtherActor || OtherActor == this) {
        return;
    }

    // 危険、または点滅状態の時にダメージを受けられる対象が乗った場合
    if (CurrentTileState == ETileState::ActiveHazard || CurrentTileState == ETileState::ExpiringHazard) {
        if (OtherActor && OtherActor->Implements<UDamageableInterface>()) {
            // ダメージ処理をインターフェース経由で委譲する
            IDamageableInterface::Execute_ReceiveDamage(OtherActor);
        }
    }
}