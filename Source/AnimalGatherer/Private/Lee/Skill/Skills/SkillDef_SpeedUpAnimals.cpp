// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/Skill/Skills/SkillDef_SpeedUpAnimals.h"

// 2026.10.06 Lee : 全場加速スキル定義 新規作成
//
// 2026.10.06 Lee start（B範囲：下記 API 契約（bool 戻り値含む）は実装済み）
// 【次バッチ（第三弾）で実装される API 契約】
//   - void UMatchSkillEffectComponent::ApplySpeedEffect(AMapManager* BoundMap, float Multiplier, float DurationSeconds)
//     （指定マップに紐づく本試合の共有加速状態を確立 / 終了時刻を now + DurationSeconds へ刷新する）
// このヘッダーが無い間は本ファイルはコンパイル不可（許容済み）。実装は存在しない機能を偽らない。
// 2026.10.06 Lee end（B範囲：契約実装済み）
#include "Lee/Skill/MatchSkillEffectComponent.h"
// 2026.10.06 Lee start（B範囲：Context の身分・地図・World 検証のため）
#include "Lee/AnimalGatherPlayerController.h"
// 2026.10.06 Lee end
// 2026.10.06 Lee start（B範囲：Context.MapManager の完全型利用 — GetWorld() / IsValid() 呼び出しのため）
// Controller・Effect ヘッダーは AMapManager を前方宣言しているのみのため、
// 完全クラス定義は本ファイルで直接 include して取得する
// （ユニティビルドの偶発的な間接 include には依存しない）。
#include "Lee/MapManager.h"
// 2026.10.06 Lee end（B範囲：Context.MapManager の完全型利用 — GetWorld() / IsValid() 呼び出しのため）

/**
 * @brief 設定値の整合性を検証する。
 *        基底クラスの検証（回数・クールダウン）に加え、倍率・持続時間について
 *        有限値（NaN / Inf 不許可）かつ正の値であることを確認する。
 * @return 設定が有効な場合 true。
 */
bool USkillDef_SpeedUpAnimals::IsConfigurationValid() const
{
	if (!Super::IsConfigurationValid())
	{
		return false;
	}

	// 非有限値（NaN / Inf）は無効設定として扱う。
	if (!FMath::IsFinite(SpeedMultiplier) || SpeedMultiplier <= 0.0f)
	{
		return false;
	}
	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
	{
		return false;
	}

	return true;
}

/**
 * @brief スキル効果を同期実行する。
 *        実行時にもう一度設定検証を行い、問題がある場合は何も適用せず失敗を返す。
 *        依存（バインド済みマップ・共有効果コンポーネント）が揃っている場合のみ、
 *        ApplySpeedEffect に処理を委譲して Success を返す。
 * @param Context 発動 Controller・プレイヤーID・バインド済みマップ・共有効果コンポーネントの文脈。
 * @return 実行結果（Success / MissingDependency）。
 */
ESkillUseResult USkillDef_SpeedUpAnimals::ExecuteSkill(const FSkillContext& Context) const
{
	// 有効な数値設定は実行の前提。欠落している場合は依存不備扱いで失敗とする。
	if (!IsConfigurationValid())
	{
		return ESkillUseResult::MissingDependency;
	}

	// 2026.10.06 Lee start（B範囲：Context 完全検証へ強化。空文脈の代替・身分の暗黙フォールバックを禁止）
	// 旧実装（null 判定のみ）は下記の通り。元のコードは消さない。
	// if (Context.MapManager == nullptr || Context.MatchSkillEffect == nullptr)
	// {
	// 	return ESkillUseResult::MissingDependency;
	// }

	// Context の必須参照は全て有効でなければならない。
	const AAnimalGatherPlayerController* Caster = Cast<AAnimalGatherPlayerController>(Context.CasterController);
	if (Caster == nullptr || !IsValid(Context.MapManager) || !IsValid(Context.MatchSkillEffect))
	{
		return ESkillUseResult::MissingDependency;
	}

	// 無効な身分（255 を含む）は P1 へ暗黙格上げしない。
	if (Context.PlayerId != 0 && Context.PlayerId != 1)
	{
		return ESkillUseResult::MissingDependency;
	}

	// 文脈の身分・地図・World は発動 Controller の実参照と一致しなければならない。
	if (Caster->GetSkillPlayerId() != Context.PlayerId)
	{
		return ESkillUseResult::MissingDependency;
	}
	if (Caster->GetMapManager() != Context.MapManager)
	{
		return ESkillUseResult::MissingDependency;
	}
	if (Caster->GetWorld() == nullptr || Caster->GetWorld() != Context.MapManager->GetWorld())
	{
		return ESkillUseResult::MissingDependency;
	}
	// 2026.10.06 Lee end（B範囲：Context 完全検証へ強化）

	// 2026.10.06 Lee start（ApplySpeedEffect の戻り値 bool を反映し Success / MissingDependency を返す）
	// Context.MatchSkillEffect->ApplySpeedEffect(Context.MapManager, SpeedMultiplier, DurationSeconds);
	//
	// return ESkillUseResult::Success;
	if (Context.MatchSkillEffect->ApplySpeedEffect(Context.MapManager, SpeedMultiplier, DurationSeconds))
	{
		return ESkillUseResult::Success;
	}
	return ESkillUseResult::MissingDependency;
	// 2026.10.06 Lee end（ApplySpeedEffect の戻り値 bool を反映し Success / MissingDependency を返す）
}