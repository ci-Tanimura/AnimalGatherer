#include "Tanimura/AnimalTypes.h"

// 盤面の矢印タイルを移動方向へ対応付ける
EGridDirection ToGridDirection(ETileType TileType)
{
	switch (TileType) {
	case ETileType::DirUp:
		return EGridDirection::Up;

	case ETileType::DirDown:
		return EGridDirection::Down;

	case ETileType::DirLeft:
		return EGridDirection::Left;

	case ETileType::DirRight:
		return EGridDirection::Right;

	default:
		return EGridDirection::None;
	}
}

// 移動方向をグリッド座標の移動量へ対応付ける
FIntPoint ToGridDelta(EGridDirection Direction)
{
	switch (Direction) {
	case EGridDirection::Up:
		return FIntPoint(0, 1);

	case EGridDirection::Down:
		return FIntPoint(0, -1);

	case EGridDirection::Left:
		return FIntPoint(-1, 0);

	case EGridDirection::Right:
		return FIntPoint(1, 0);

	default:
		return FIntPoint::ZeroValue;
	}
}
