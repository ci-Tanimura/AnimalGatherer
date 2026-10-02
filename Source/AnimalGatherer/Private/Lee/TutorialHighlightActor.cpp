// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/TutorialHighlightActor.h"
#include "Lee/MapManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ATutorialHighlightActor::ATutorialHighlightActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// ハイライト表示用の薄い板（エンジン標準キューブを Z 方向に潰して使用）
	HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightMesh"));
	SetRootComponent(HighlightMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		HighlightMesh->SetStaticMesh(CubeMesh.Object);
	}

	// 当たり判定は不要（見た目だけのアクター）
	HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATutorialHighlightActor::Initialize(UMaterialInterface* ParentMaterial, const FLinearColor& InColor,
	FIntPoint Cell, const AMapManager* Map)
{
	if (!Map)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TutorialHighlightActor] Initialize: MapManager が null のため配置をスキップします"));
		return;
	}

	// セル中心 + 少し上（カーソルの Z=50 と干渉しない低めの位置）
	const float TileSize = Map->TileSize;
	const FVector TileCenter = Map->GetActorLocation()
		+ FVector(Cell.X * TileSize, Cell.Y * TileSize, 0.0f);
	SetActorLocation(TileCenter + FVector(0.0f, 0.0f, 8.0f));

	// 標準キューブ(100)をタイルサイズに合わせて薄い板に潰す
	BaseScale = FVector(0.9f, 0.9f, 0.08f) * (TileSize / 100.0f);
	SetActorScale3D(BaseScale);

	if (HighlightMesh && ParentMaterial)
	{
		MID = HighlightMesh->CreateDynamicMaterialInstance(0, ParentMaterial);
		SetHighlightColor(InColor);
	}

	TimeAccum = 0.0f;
}

void ATutorialHighlightActor::SetHighlightColor(const FLinearColor& InColor)
{
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("HighlightColor"), InColor);
	}
}

void ATutorialHighlightActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// スケール呼吸による脈動表現（±6%、周期約 1.6 秒）
	TimeAccum += DeltaTime;
	const float Pulse = 1.0f + 0.06f * FMath::Sin(TimeAccum * 4.0f);
	SetActorScale3D(BaseScale * Pulse);
}
