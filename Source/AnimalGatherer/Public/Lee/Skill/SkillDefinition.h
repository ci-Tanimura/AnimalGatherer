// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameTypes.h"
#include "SkillDefinition.generated.h"

// 2026.10.06 Lee : スキル定義（DataAsset 基底クラス）新規作成

class UTexture2D;

/**
 * @brief スキルの静的設定を保持するステートレス DataAsset 基底クラス。
 *        表示名・カードテクスチャ・使用回数上限・クールダウン秒数のみを保持し、
 *        残回数・タイマー・World 参照などの実行時状態は一切持たない。
 *        実効果はサブクラスが ExecuteSkill() を同期処理として上書き実装する。
 *        設定は定義資産（DA_*）を優先し、資産がない場合に C++ 既定オブジェクトを
 *        定義のフォールバックとして使用する想定。
 */
UCLASS(BlueprintType)
class ANIMALGATHERER_API USkillDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @brief HUD のスキルカード等に表示する名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Display")
	FText DisplayName;

	/** @brief スキルカードに表示するテクスチャ。未設定は表示フォールバックの対象（設定不備には含めない）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Display")
	TObjectPtr<UTexture2D> DisplayTexture = nullptr;

	/** @brief 使用回数上限（プレイヤー・スキル・試合ごとに独立）。既定 2 回。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Config", meta = (ClampMin = "1"))
	int32 MaxUses = 2;

	/** @brief クールダウン秒数。既定 5 秒。0 はクールダウンなしとして扱う。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Config", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownSeconds = 5.0f;

	/**
	 * @brief 設定値の整合性を検証する。
	 *        定義資産・C++ 既定定義のいずれを使う場合でも、使用開始前に呼ばれることを想定。
	 *        名前・テクスチャは表示用のため検証対象外（数値設定のみ判定）。
	 * @return MaxUses が 1 以上かつ CooldownSeconds が 0 以上の有限値の場合 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill")
	virtual bool IsConfigurationValid() const;

	/**
	 * @brief スキル効果を同期実行する。
	 *        基底クラスは具体的効果を持たないため、常に MissingDependency を返す。
	 *        World やタイマーは所有せず、Context で渡された参照のみを使用する。
	 * @param Context 発動 Controller・プレイヤーID・バインド済みマップ・共有効果コンポーネントの文脈。
	 * @return 実行結果。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill")
	virtual ESkillUseResult ExecuteSkill(const FSkillContext& Context) const;
};