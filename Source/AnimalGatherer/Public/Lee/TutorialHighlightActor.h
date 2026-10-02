// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialHighlightActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AMapManager;

/**
 * @brief チュートリアルの対象タイルを脈動表示するハイライトアクター。
 *        対象セルの中心に薄い板を重ね、スケール呼吸で強調する。
 *        P1 = 青 / P2 = 赤 の色分けは ATutorialGameMode が指定する。
 */
UCLASS()
class ANIMALGATHERER_API ATutorialHighlightActor : public AActor
{
	GENERATED_BODY()

public:
	ATutorialHighlightActor();

	/**
	 * @brief ハイライトを初期化し、指定セルの中心へ配置する。
	 * @param ParentMaterial 親マテリアル（Vector パラメータ HighlightColor を持つ半透明材）。
	 * @param InColor ハイライト色（P1 = 青 / P2 = 赤）。
	 * @param Cell 対象セルのグリッド座標。
	 * @param Map セル → ワールド座標の変換に使うマップマネージャ。
	 */
	void Initialize(UMaterialInterface* ParentMaterial, const FLinearColor& InColor,
		FIntPoint Cell, const AMapManager* Map);

	/** @brief ハイライト色を後から変更する。 */
	void SetHighlightColor(const FLinearColor& InColor);

protected:
	virtual void Tick(float DeltaTime) override;

	/** @brief ハイライト表示用メッシュ（薄い板）。 */
	UPROPERTY(VisibleAnywhere, Category = "Tutorial")
	UStaticMeshComponent* HighlightMesh;

	/** @brief 色変更用の動的マテリアルインスタンス。 */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;

	/** @brief 呼吸脈動の基準スケール（Initialize 時にタイルサイズから算出）。 */
	FVector BaseScale = FVector(0.9f, 0.9f, 0.08f);

	/** @brief 脈動用の経過時間。 */
	float TimeAccum = 0.0f;
};
