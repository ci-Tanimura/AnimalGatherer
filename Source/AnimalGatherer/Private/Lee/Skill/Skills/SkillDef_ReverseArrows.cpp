// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/Skill/Skills/SkillDef_ReverseArrows.h"

#include "Lee/AnimalGatherPlayerController.h"
#include "Lee/MapManager.h"
// 2026.10.06 Lee start（B範囲：同図対戦相手の特定にアクター反復を使用）
#include "Containers/Set.h"
#include "EngineUtils.h"
// 2026.10.06 Lee end

// 2026.10.06 Lee : 反転スキル定義 新規作成
//
// 2026.10.06 Lee start（B範囲：下記 Controller API 契約は本バッチで AnimalGatherPlayerController に実装済み）
// 【次バッチ（第三弾）で実装される Controller API 契約】
//   - TArray<FIntPoint> GetOwnedPlacedArrowCoords(uint8 OwnerPlayerId) const
//     （指定プレイヤーが現在所有する有効な配置座標のコピーを返す。重複除去済みの想定だが防御的に再除去する）
//   - AMapManager* GetMapManager() const
//   - uint8 GetSkillPlayerId() const（未設定は 255）
// これらの実装が無い間は本ファイルはリンク・コンパイル不可（許容済み）。
// 2026.10.06 Lee 最終審査小修正 : 上記 Controller API は B 範囲で実装済み（備考は原文のまま保持）。
// 2026.10.06 Lee end（B範囲：契約実装済み）

namespace
{
	/** @brief 反転適用計画の1エントリ（座標・反転後の方向・元の所有者）。 */
	struct FReversePlanEntry
	{
		FIntPoint Coord = FIntPoint::ZeroValue;
		ETileType NewType = ETileType::Empty;
		uint8 OriginalOwner = 0;
	};
}

/**
 * @brief 指定方向の180度反対方向を返す。
 *        列挙値の算術変換は行わず、方向ごとの明示的な switch のみで対応する。
 * @param Direction 変換元の方向。
 * @return 反対方向。方向タイル以外は Empty を返す。
 */
ETileType USkillDef_ReverseArrows::GetOppositeDirection(ETileType Direction)
{
	switch (Direction)
	{
		case ETileType::DirUp:    return ETileType::DirDown;
		case ETileType::DirDown:  return ETileType::DirUp;
		case ETileType::DirLeft:  return ETileType::DirRight;
		case ETileType::DirRight: return ETileType::DirLeft;
		default:                  return ETileType::Empty;
	}
}

/**
 * @brief スキル効果を同期実行する。
 *        手順：依存検証 → 対象候補の収集（対戦相手の有効座標）→ 全対象の事前検証 →
 *        一括適用（SetTileData で TileType のみ反転し、Owner は元の値を維持して返す）。
 *        履歴（PlaceHistory）・特殊タイル・マップ予約矢印には一切触れない。
 * @param Context 発動 Controller・プレイヤーID・バインド済みマップ・共有効果コンポーネントの文脈。
 * @return 実行結果（Success / NoTarget / MissingDependency）。
 */
ESkillUseResult USkillDef_ReverseArrows::ExecuteSkill(const FSkillContext& Context) const
{
	//==============================================================================
	// 1) 依存検証（欠落があれば地図には一切触れずに失敗を返す）
	//==============================================================================
	// 2026.10.06 Lee start（B範囲：Context 完全検証へ強化。空文脈の代替・身分の暗黙フォールバックを禁止）
	// 旧実装（null 許容・255 を未設定扱い）は下記の通り。元のコードは消さない。
	// const AAnimalGatherPlayerController* Caster = Cast<AAnimalGatherPlayerController>(Context.CasterController);
	// if (Caster == nullptr)
	// {
	// 	return ESkillUseResult::MissingDependency;
	// }
	//
	// AMapManager* BoundMap = Caster->GetMapManager();
	// if (BoundMap == nullptr)
	// {
	// 	return ESkillUseResult::MissingDependency;
	// }
	//
	// if (Context.MapManager != nullptr && Context.MapManager != BoundMap)
	// {
	// 	return ESkillUseResult::MissingDependency;
	// }
	//
	// const uint8 MyPlayerId = Caster->GetSkillPlayerId();
	// if (MyPlayerId != 0 && MyPlayerId != 1)
	// {
	// 	return ESkillUseResult::MissingDependency;
	// }
	//
	// if (Context.PlayerId != 255 && Context.PlayerId != MyPlayerId)
	// {
	// 	return ESkillUseResult::MissingDependency;
	// }
	//
	// const uint8 OpponentPlayerId = (MyPlayerId == 0) ? 1u : 0u;

	// Context の必須参照と身分は全て有効でなければならない。
	const AAnimalGatherPlayerController* Caster = Cast<AAnimalGatherPlayerController>(Context.CasterController);
	if (Caster == nullptr || !IsValid(Context.MapManager))
	{
		return ESkillUseResult::MissingDependency;
	}

	// 無効な身分（255 を含む）は P1 へ暗黙格上げしない。
	if (Context.PlayerId != 0 && Context.PlayerId != 1)
	{
		return ESkillUseResult::MissingDependency;
	}

	// 文脈の身分は発動 Controller の実身分と一致しなければならない。
	if (Caster->GetSkillPlayerId() != Context.PlayerId)
	{
		return ESkillUseResult::MissingDependency;
	}

	// 文脈の地図は発動 Controller のバインド地図と一致し、World も同一でなければならない
	// （「場の最初の MapManager」での代替は許可しない）。
	AMapManager* BoundMap = Caster->GetMapManager();
	if (BoundMap != Context.MapManager)
	{
		return ESkillUseResult::MissingDependency;
	}
	if (Caster->GetWorld() == nullptr || Caster->GetWorld() != Context.MapManager->GetWorld())
	{
		return ESkillUseResult::MissingDependency;
	}

	const uint8 MyPlayerId = Context.PlayerId;
	const uint8 OpponentPlayerId = (MyPlayerId == 0) ? 1u : 0u;

	// 同じ地図をバインドした対戦相手 Controller を必ず特定する（不在は NoTarget でなく依存不備）。
	bool bOpponentFound = false;
	for (TActorIterator<AAnimalGatherPlayerController> It(Caster->GetWorld()); It; ++It)
	{
		const AAnimalGatherPlayerController* Candidate = *It;
		if (Candidate == nullptr || Candidate == Caster)
		{
			continue;
		}
		if (Candidate->GetSkillPlayerId() != OpponentPlayerId)
		{
			continue;
		}
		if (Candidate->GetMapManager() != Context.MapManager)
		{
			continue; // 別マップの Controller は対戦相手とみなさない。
		}
		bOpponentFound = true;
		break;
	}
	if (!bOpponentFound)
	{
		return ESkillUseResult::MissingDependency;
	}
	// 2026.10.06 Lee end（B範囲：Context 完全検証へ強化）

	//==============================================================================
	// 2) 対象候補の収集：対戦相手の有効な配置座標（履歴ベース、座標コピー）
	//==============================================================================
	const TArray<FIntPoint> CandidateCoords = Caster->GetOwnedPlacedArrowCoords(OpponentPlayerId);

	//==============================================================================
	// 3) 全対象の事前検証（フィルター非該当は除外。ここではマップを変更しない）
	//==============================================================================
	TSet<FIntPoint> SeenCoords;
	TArray<FReversePlanEntry> Plan;
	SeenCoords.Reserve(CandidateCoords.Num());
	Plan.Reserve(FMath::Min(CandidateCoords.Num(), MaxReverseTargets));

	for (const FIntPoint& Coord : CandidateCoords)
	{
		if (Plan.Num() >= MaxReverseTargets)
		{
			break;
		}

		bool bAlreadySeen = false;
		SeenCoords.Add(Coord, &bAlreadySeen);
		if (bAlreadySeen)
		{
			continue;
		}

		// 地図範囲外の座標は対象外（履歴と地図のズレに対する防御）。
		if (Coord.X < 0 || Coord.Y < 0 || Coord.X >= BoundMap->MapWidth || Coord.Y >= BoundMap->MapHeight)
		{
			continue;
		}

		// 外周ボーダーは対象外。
		if (BoundMap->IsBorderTile(Coord.X, Coord.Y))
		{
			continue;
		}

		const int32 TileIndex = Coord.Y * BoundMap->MapWidth + Coord.X;
		if (!BoundMap->GridData.IsValidIndex(TileIndex))
		{
			continue;
		}

		const FMapTileData& Tile = BoundMap->GridData[TileIndex];

		// 現在も方向矢印で、かつ所有者がまだ対戦相手のマスのみ対象
		// （己方が後から上書きした・既に消滅した古い履歴は除外）。
		if (!AMapManager::IsDirectionTile(Tile.TileType) || Tile.OwnerPlayerId != OpponentPlayerId)
		{
			continue;
		}

		const ETileType OppositeType = GetOppositeDirection(Tile.TileType);
		if (OppositeType == ETileType::Empty)
		{
			continue;
		}

		FReversePlanEntry Entry;
		Entry.Coord = Coord;
		Entry.NewType = OppositeType;
		Entry.OriginalOwner = Tile.OwnerPlayerId;
		Plan.Add(Entry);
	}

	if (Plan.Num() == 0)
	{
		return ESkillUseResult::NoTarget;
	}

	//==============================================================================
	// 4) 一括適用：TileType のみ反転し、Owner は元の値を返して維持する。
	//    履歴（PlaceHistory）は変更しない。全層再構築（UpdateMapVisuals）は行わない。
	//==============================================================================
	for (const FReversePlanEntry& Entry : Plan)
	{
		BoundMap->SetTileData(Entry.Coord.X, Entry.Coord.Y, Entry.NewType, Entry.OriginalOwner);
	}

	return ESkillUseResult::Success;
}