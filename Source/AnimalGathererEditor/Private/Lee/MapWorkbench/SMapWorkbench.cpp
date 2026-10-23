// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/MapWorkbench/SMapWorkbench.h"

// 2026.10.22 Lee start（マップ工作台パネル実装：上半分＝キャンバス＋パネル骨格）

#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EditorUndoClient.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "GameTypes.h"
#include "IContentBrowserSingleton.h"
#include "Lee/MapManager.h"
#include "Lee/MapWorkbench/MapWorkbenchText.h"
#include "LevelUtils.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "Templates/Function.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

//==============================================================================
// 共有ヘルパー
//==============================================================================

namespace MapWorkbenchStyle
{
	/** @brief タイル種別ごとの表示色を返す。 */
	static FLinearColor GetTileColor(ETileType Type)
	{
		switch (Type)
		{
		// 2026.10.23 Lee start
		case ETileType::Hole:    return FLinearColor(0.025f, 0.03f, 0.04f);
		// 2026.10.23 Lee end
		case ETileType::DirUp:   return FLinearColor(0.25f, 0.77f, 1.00f);
		case ETileType::DirDown: return FLinearColor(1.00f, 0.65f, 0.30f);
		case ETileType::DirLeft: return FLinearColor(0.51f, 0.78f, 0.52f);
		case ETileType::DirRight:return FLinearColor(0.73f, 0.41f, 0.78f);
		case ETileType::Spawn:   return FLinearColor(1.00f, 0.93f, 0.34f);
		case ETileType::GoalP1:  return FLinearColor(0.26f, 0.65f, 0.96f);
		case ETileType::GoalP2:  return FLinearColor(0.94f, 0.33f, 0.31f);
		default:                 return FLinearColor(0.24f, 0.25f, 0.27f);
		}
	}

	/** @brief タイル種別ごとの表示名を返す（言語ヘルパへ委譲）。 */
	static FText GetTileLabel(ETileType Type)
	{
		switch (Type)
		{
		case ETileType::Empty:   return FMapWorkbenchText::Get(EWorkbenchText::TileEmpty);
		// 2026.10.23 Lee start
		case ETileType::Hole:    return FMapWorkbenchText::Get(EWorkbenchText::TileHole);
		// 2026.10.23 Lee end
		case ETileType::DirUp:   return FMapWorkbenchText::Get(EWorkbenchText::TileUp);
		case ETileType::DirDown: return FMapWorkbenchText::Get(EWorkbenchText::TileDown);
		case ETileType::DirLeft: return FMapWorkbenchText::Get(EWorkbenchText::TileLeft);
		case ETileType::DirRight:return FMapWorkbenchText::Get(EWorkbenchText::TileRight);
		case ETileType::Spawn:   return FMapWorkbenchText::Get(EWorkbenchText::TileSpawn);
		case ETileType::GoalP1:  return FMapWorkbenchText::Get(EWorkbenchText::TileGoalP1);
		case ETileType::GoalP2:  return FMapWorkbenchText::Get(EWorkbenchText::TileGoalP2);
		default:                 return FMapWorkbenchText::Get(EWorkbenchText::TileUnknown);
		}
	}

	/**
	 * @brief ペン種別を ETileType へ変換する（cpp 共通ヘルパ）。
	 *        キャンバス側からパネル private メソッドへアクセスできないため、両者これを使用する。
	 */
	static ETileType BrushToTile(EWorkbenchBrush InBrush)
	{
		switch (InBrush)
		{
		case EWorkbenchBrush::Empty:    return ETileType::Empty;
		// 2026.10.23 Lee start
		case EWorkbenchBrush::Hole:     return ETileType::Hole;
		// 2026.10.23 Lee end
		case EWorkbenchBrush::DirUp:    return ETileType::DirUp;
		case EWorkbenchBrush::DirDown:  return ETileType::DirDown;
		case EWorkbenchBrush::DirLeft:  return ETileType::DirLeft;
		case EWorkbenchBrush::DirRight: return ETileType::DirRight;
		case EWorkbenchBrush::Spawn:    return ETileType::Spawn;
		case EWorkbenchBrush::GoalP1:   return ETileType::GoalP1;
		case EWorkbenchBrush::GoalP2:   return ETileType::GoalP2;
		default:                        return ETileType::Empty;
		}
	}

	/** @brief 矩形を全方向へ一定量拡張した新しい矩形を返す（FSlateRect に ExpandBy は無い）。 */
	static FSlateRect ExpandedRect(const FSlateRect& InRect, float InAmount)
	{
		return FSlateRect(
			InRect.Left - InAmount,
			InRect.Top - InAmount,
			InRect.Right + InAmount,
			InRect.Bottom + InAmount);
	}

	/**
	 * @brief UE5.8 推奨の PaintGeometry を生成する（ローカルサイズ＋レイアウト変換）。
	 *        位置＝左上オフセット、サイズ＝矩形寸法（逆にしないこと）。
	 */
	static auto MakePaintGeometry(const FGeometry& InGeometry, const FVector2D& InPosition, const FVector2D& InSize)
	{
		return InGeometry.ToPaintGeometry(FVector2f(InSize), FSlateLayoutTransform(FVector2f(InPosition)));
	}
}

/**
 * @brief 盤外（越界）に退避済みのタイル1マス分の表示情報。
 */
struct FOutOfBoundsTileView
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	ETileType Type = ETileType::Empty;
};

/**
 * @brief ツール生成アクターのリスト行データ。
 *        実ワールド位置・投影マス・越界状態を保持する（パネルとキャンバスで共通の順序）。
 */
struct FToolActorRow
{
	/** @brief 追跡対象アクター（破棄済みは無効）。 */
	TWeakObjectPtr<AActor> Actor;

	/** @brief アクター表示名。 */
	FString Label;

	/** @brief 実ワールド位置。 */
	FVector WorldLocation = FVector::ZeroVector;

	/** @brief 実位置を最寄りマスへ投影した結果。 */
	FIntPoint Cell = FIntPoint::ZeroValue;

	/** @brief 投影マスが盤面外か。 */
	bool bOutOfBounds = false;

	/** @brief アクターが MapManager と別レベルに属するか（未認領扱いの表示用）。 */
	bool bOtherLevel = false;

	/** @brief 有効アクター順の表示番号。 */
	int32 Index = 0;
};

//==============================================================================
// キャンバスとパネルの橋渡し（関数オブジェクト束）
//==============================================================================

/**
 * @brief キャンバスがパネルへ委譲する操作の束。
 *        パネル Construct 時にメンバ関数へ束ねて渡す（cpp 内完結の結合）。
 */
struct FWorkbenchCanvasContext
{
	/** @brief 現在の綁定目標を取得する（無効時 nullptr）。 */
	TFunction<AMapManager*()> MapTarget;

	/** @brief 現在のペン種別を取得する。 */
	TFunction<EWorkbenchBrush()> GetBrush;

	/** @brief 現在の選択マスを取得する。 */
	TFunction<TOptional<FIntPoint>()> GetSelectedCell;

	/** @brief 書き込み可否の総合判定（理由付き）。 */
	TFunction<bool(FText&)> CanEdit;

	/** @brief 単一マスへの筆肋適用（取引管理はパネル側）。 */
	TFunction<bool(const FIntPoint&, ETileType, FText&)> PaintCell;

	/** @brief 座標選択ペンのクリック通知。 */
	TFunction<void(const FIntPoint&)> SelectCell;

	/** @brief ストローク確定通知。 */
	TFunction<void()> NotifyStrokeCommitted;

	/** @brief ステータスバーへのメッセージ表示。 */
	TFunction<void(const FText&)> ReportStatus;
};

//==============================================================================
// 二维盤面キャンバス（cpp 内部ウィジェット）
//==============================================================================

/**
 * @brief 盤面を自前描画する二维キャンバス。
 *        世界 X を画面右、世界 Y を画面上へ対応させる（DirUp=+Y を上矢印で表示）。
 *        左ドラッグで筆肋描画・右ドラッグで Empty 化・ホイールでズーム・中ドラッグで平移。
 *        一度の押下～解放を 1 ストロークとし、跨格移動は Bresenham 補間で中間マスを落とさない。
 *        同一ストローク内の同一マスは再書き込みしない。
 */
class SWorkbenchCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchCanvas) {}
		SLATE_ARGUMENT(TSharedPtr<FWorkbenchCanvasContext>, Context)
	SLATE_END_ARGS()

	/** @brief キャンバスを構築する。 */
	void Construct(const FArguments& InArgs)
	{
		Context = InArgs._Context;
		// 自前描画が平移・縮小時に他ウィジェットへはみ出さないようクリップする
		SetClipping(EWidgetClipping::ClipToBounds);
	}

	//--------------------------------------------------------------------------
	// パネル側からの操作
	//--------------------------------------------------------------------------

	/** @brief ストロークが進行中か。 */
	bool IsStrokeActive() const { return bStrokeActive; }

	/** @brief 進行中のストロークを強制確定する（PIE 開始・パネル破棄時）。 */
	void ForceEndStroke() { EndStrokeInternal(); }

	/** @brief 次回描画時に盤面へビューをフィットさせる。 */
	void RequestFitView() { bViewInitialized = false; }

	/** @brief キャンバスの再描画を要求する。 */
	void InvalidateCanvas() { Invalidate(EInvalidateWidgetReason::Paint); }

	//--------------------------------------------------------------------------
	// SWidget インターフェース
	//--------------------------------------------------------------------------

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float InLayoutScaleMultiplier) const override { return FVector2D(900.0f, 560.0f); }
	virtual FReply OnMouseButtonDown(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	//--------------------------------------------------------------------------
	// 座標変換
	//--------------------------------------------------------------------------

	/** @brief 盤面相対ワールド XY を画面座標へ変換する。 */
	FVector2D WorldToScreen(float WorldX, float WorldY, const FVector2D& LocalSize) const
	{
		return FVector2D(ViewPan.X + WorldX * ViewScale, LocalSize.Y - ViewPan.Y - WorldY * ViewScale);
	}

	/** @brief 画面座標を盤面相対ワールド XY へ変換する。 */
	FVector2D ScreenToWorld(const FVector2D& ScreenPos, const FVector2D& LocalSize) const
	{
		return FVector2D((ScreenPos.X - ViewPan.X) / ViewScale, (LocalSize.Y - ScreenPos.Y - ViewPan.Y) / ViewScale);
	}

	/** @brief 画面座標を最寄りマスへ変換する（マス中心 = X*TileSize）。 */
	FIntPoint CellAtScreen(const FVector2D& ScreenPos, const FVector2D& LocalSize) const
	{
		AMapManager* M = Context.IsValid() ? Context->MapTarget() : nullptr;
		const float TileSize = M ? M->TileSize : 100.0f;
		const FVector2D World = ScreenToWorld(ScreenPos, LocalSize);
		return FIntPoint(
			FMath::FloorToInt32(World.X / TileSize + 0.5f),
			FMath::FloorToInt32(World.Y / TileSize + 0.5f));
	}

	/** @brief 盤面全体が見えるようビューを初期化する（OnPaint 初回 / 再フィット要求時）。 */
	void FitView(const FVector2D& LocalSize) const;

	//--------------------------------------------------------------------------
	// ストローク処理
	//--------------------------------------------------------------------------

	/** @brief マス間を Bresenham 直線で補間したセル列を作る（両端を含む）。 */
	static void BuildLineCells(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutCells);

	/** @brief 現在位置へストロークを延長する（補間＋同一ストローク内の重複書き込み抑止）。 */
	void StrokeTo(const FIntPoint& InCell);

	/** @brief 単一マスへ書き込む（同一ストローク内の再訪は抑止）。 */
	void PaintCellOnce(const FIntPoint& InCell);

	/** @brief ストロークを確定する（多重呼び出し安全）。 */
	void EndStrokeInternal();

	//--------------------------------------------------------------------------
	// 描画ヘルパー
	//--------------------------------------------------------------------------

	/** @brief 盤面相対ワールド座標系の矩形を画面矩形へ変換する。 */
	FSlateRect TileScreenRect(int32 CellX, int32 CellY, float TileSize, const FVector2D& LocalSize) const;

	/** @brief 格子線群を描画する。 */
	void DrawGrid(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief 非空タイルと方向矢印を描画する。 */
	void DrawTiles(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief 盤外退避タイル（越界アーカイブ）を描画する。 */
	void DrawOutOfBoundsArchive(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief 出生タイル関連付けのマーカーを描画する。 */
	void DrawBindingMarkers(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief ツール生成アクターを実ワールド位置へ投影して描画する。 */
	void DrawToolActors(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief 選択マス・ホバー・ホバー情報を描画する。 */
	void DrawHoverAndSelection(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief 方向図例と軸説明を描画する。 */
	void DrawLegend(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const;

	/** @brief 中央へヒント文を描画する。 */
	void DrawCenteredText(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FString& InText) const;

	/** @brief マス中心に向き矢印を描画する（画面方向：Up=上, Down=下, Left=左, Right=右）。 */
	void DrawArrow(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FVector2D& Center, float HalfSize, const FVector2D& ScreenDir, const FLinearColor& Color) const;

	/** @brief 矩形の枠線を描画する。 */
	void DrawRectBorder(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FSlateRect& InRect, const FLinearColor& Color, float Thickness) const;

	/** @brief 指定矩形の位置・サイズで塗りBoxを描画する（全Canvasを塗らない）。 */
	void DrawBoxAt(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FSlateRect& InRect, const FLinearColor& Color) const;

	//--------------------------------------------------------------------------
	// 状態
	//--------------------------------------------------------------------------

	/** @brief パネル操作の委譲束。 */
	TSharedPtr<FWorkbenchCanvasContext> Context;

	/** @brief 表示倍率（画面ピクセル / ワールド単位）。初期フィットで更新。 */
	mutable float ViewScale = 0.5f;

	/** @brief 平移オフセット（画面ピクセル）。初期フィットで更新。 */
	mutable FVector2D ViewPan = FVector2D::ZeroVector;

	/** @brief 初回フィット済みか。 */
	mutable bool bViewInitialized = false;

	/** @brief ストローク（左筆肋 / 右消去）進行中か。 */
	bool bStrokeActive = false;

	/** @brief 現在のストロークが右ボタン消去か。 */
	bool bStrokeIsErase = false;

	/** @brief 中ボタン平移中か。 */
	bool bPanning = false;

	/** @brief ストロークで書き込むタイル種別。 */
	ETileType StrokeTile = ETileType::Empty;

	/** @brief ストローク最後に書き込んだマス（補間始点）。 */
	TOptional<FIntPoint> LastStrokeCell;

	/** @brief ホバー中マス。 */
	TOptional<FIntPoint> HoverCell;

	/** @brief ホバー画面位置（情報表示用）。 */
	FVector2D HoverScreenPos = FVector2D::ZeroVector;

	/** @brief 平移開始時のカーソル位置。 */
	FVector2D PanDragStart = FVector2D::ZeroVector;

	/** @brief 平移開始時のオフセット。 */
	FVector2D PanStart = FVector2D::ZeroVector;

	/** @brief 同一ストローク内で処理済みのマス（再書き込み抑止）。 */
	TSet<FIntPoint> VisitedCells;
};

//--------------------------------------------------------------------------
// 描画本体
//--------------------------------------------------------------------------

void SWorkbenchCanvas::FitView(const FVector2D& LocalSize) const
{
	AMapManager* M = Context.IsValid() ? Context->MapTarget() : nullptr;
	const float TileSize = M ? M->TileSize : 100.0f;
	const int32 GridW = M ? FMath::Max(1, M->MapWidth) : 10;
	const int32 GridH = M ? FMath::Max(1, M->MapHeight) : 10;

	const float TotalW = GridW * TileSize;
	const float TotalH = GridH * TileSize;

	float NewScale = 1.0f;
	if (TotalW > 0.0f && TotalH > 0.0f)
	{
		NewScale = FMath::Min((LocalSize.X - 60.0f) / TotalW, (LocalSize.Y - 60.0f) / TotalH);
	}
	// 1マスあたり 2px～300px に制限（256x256 対応）
	NewScale = FMath::Clamp(NewScale, 2.0f / TileSize, 300.0f / TileSize);

	ViewScale = NewScale;
	ViewPan = FVector2D((LocalSize.X - TotalW * NewScale) * 0.5f, (LocalSize.Y - TotalH * NewScale) * 0.5f);
	bViewInitialized = true;
}

int32 SWorkbenchCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 背景
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(),
		FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None,
		FLinearColor(0.07f, 0.075f, 0.085f));

	AMapManager* M = Context.IsValid() ? Context->MapTarget() : nullptr;
	if (!M)
	{
		DrawCenteredText(OutDrawElements, AllottedGeometry, LayerId,
			FMapWorkbenchText::Get(EWorkbenchText::CanvasNoTarget).ToString());
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (!bViewInitialized)
	{
		FitView(LocalSize);
	}

	// 盤面の底（盤面範囲をわずかに明るく）
	{
		const FVector2D BoardTopLeft = WorldToScreen(-0.5f * M->TileSize, -0.5f * M->TileSize, LocalSize);
		const FVector2D BoardBottomRight = WorldToScreen(
			(static_cast<float>(M->MapWidth) - 0.5f) * M->TileSize,
			(static_cast<float>(M->MapHeight) - 0.5f) * M->TileSize, LocalSize);
		const FSlateRect BoardRect(BoardTopLeft, BoardBottomRight);
		DrawBoxAt(OutDrawElements, AllottedGeometry, LayerId, BoardRect, FLinearColor(0.13f, 0.14f, 0.16f));
	}

	DrawGrid(M, OutDrawElements, AllottedGeometry, LayerId);
	DrawTiles(M, OutDrawElements, AllottedGeometry, LayerId);
	DrawOutOfBoundsArchive(M, OutDrawElements, AllottedGeometry, LayerId);
	DrawBindingMarkers(M, OutDrawElements, AllottedGeometry, LayerId);
	DrawToolActors(M, OutDrawElements, AllottedGeometry, LayerId);
	DrawHoverAndSelection(M, OutDrawElements, AllottedGeometry, LayerId);
	DrawLegend(M, OutDrawElements, AllottedGeometry, LayerId);

	// 編集不可状態の遮罩（副作用のない局所判定のみ：PIE/SIE・レベルロック）
	bool bReadOnly = false;
	FString ReadOnlyReason;
	if (GEditor && GEditor->PlayWorld != nullptr)
	{
		bReadOnly = true;
		ReadOnlyReason = FMapWorkbenchText::Get(EWorkbenchText::OverlayPie).ToString();
	}
	else if (FLevelUtils::IsLevelLocked(M))
	{
		bReadOnly = true;
		ReadOnlyReason = FMapWorkbenchText::Get(EWorkbenchText::OverlayLocked).ToString();
	}
	if (bReadOnly)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(),
			FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None,
			FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
		DrawCenteredText(OutDrawElements, AllottedGeometry, LayerId, ReadOnlyReason);
	}

	return LayerId;
}

FSlateRect SWorkbenchCanvas::TileScreenRect(int32 CellX, int32 CellY, float TileSize, const FVector2D& LocalSize) const
{
	const FVector2D Center = WorldToScreen(CellX * TileSize, CellY * TileSize, LocalSize);
	const float Half = TileSize * 0.5f * ViewScale;
	return FSlateRect(Center.X - Half, Center.Y - Half, Center.X + Half, Center.Y + Half);
}

void SWorkbenchCanvas::DrawGrid(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TileSize = M->TileSize;

	// 縦線：マス境界 = (x - 0.5) * TileSize、x = 0..MapWidth
	for (int32 X = 0; X <= M->MapWidth; ++X)
	{
		const float WorldX = (static_cast<float>(X) - 0.5f) * TileSize;
		const FVector2D Top = WorldToScreen(WorldX, -0.5f * TileSize, LocalSize);
		const FVector2D Bottom = WorldToScreen(WorldX, (static_cast<float>(M->MapHeight) - 0.5f) * TileSize, LocalSize);

		TArray<FVector2D> Points;
		Points.Add(Top);
		Points.Add(Bottom);

		const bool bMajor = (X % 8 == 0);
		FSlateDrawElement::MakeLines(
			OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None,
			bMajor ? FLinearColor(1.0f, 1.0f, 1.0f, 0.18f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.06f),
			true, bMajor ? 1.5f : 1.0f);

		// 8 マスごとに X 座標ラベル（盤面下端）
		if (bMajor && X < M->MapWidth)
		{
			const FVector2D LabelPos = WorldToScreen(WorldX + TileSize * 0.5f, (static_cast<float>(M->MapHeight) - 0.5f) * TileSize, LocalSize) + FVector2D(2.0f, 2.0f);
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, LabelPos, FVector2D(120.0f, 16.0f)),
				FText::AsNumber(X), FCoreStyle::GetDefaultFontStyle("Regular", 9),
				ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
		}
	}

	// 横線：y = 0..MapHeight
	for (int32 Y = 0; Y <= M->MapHeight; ++Y)
	{
		const float WorldY = (static_cast<float>(Y) - 0.5f) * TileSize;
		const FVector2D Left = WorldToScreen(-0.5f * TileSize, WorldY, LocalSize);
		const FVector2D Right = WorldToScreen((static_cast<float>(M->MapWidth) - 0.5f) * TileSize, WorldY, LocalSize);

		TArray<FVector2D> Points;
		Points.Add(Left);
		Points.Add(Right);

		const bool bMajor = (Y % 8 == 0);
		FSlateDrawElement::MakeLines(
			OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), Points,
			ESlateDrawEffect::None,
			bMajor ? FLinearColor(1.0f, 1.0f, 1.0f, 0.18f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.06f),
			true, bMajor ? 1.5f : 1.0f);

		// 8 マスごとに Y 座標ラベル（盤面左端）
		if (bMajor && Y < M->MapHeight)
		{
			const FVector2D LabelPos = WorldToScreen(-0.5f * TileSize, WorldY - TileSize * 0.5f, LocalSize) + FVector2D(2.0f, 2.0f);
			FSlateDrawElement::MakeText(
				OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, LabelPos, FVector2D(120.0f, 16.0f)),
				FText::AsNumber(Y), FCoreStyle::GetDefaultFontStyle("Regular", 9),
				ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.55f));
		}
	}
}

void SWorkbenchCanvas::DrawArrow(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FVector2D& Center, float HalfSize, const FVector2D& ScreenDir, const FLinearColor& Color) const
{
	const float ShaftLen = HalfSize * 0.55f;
	const float HeadLen = HalfSize * 0.35f;
	const float HeadHalfWidth = HalfSize * 0.22f;

	const FVector2D Tail = Center - ScreenDir * ShaftLen;
	const FVector2D Tip = Center + ScreenDir * ShaftLen;
	const FVector2D Perp(-ScreenDir.Y, ScreenDir.X);
	const FVector2D HeadBase = Tip - ScreenDir * HeadLen;

	// 軸
	TArray<FVector2D> Shaft;
	Shaft.Add(Tail);
	Shaft.Add(Tip);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), Shaft, ESlateDrawEffect::None, Color, true, 2.5f);

	// 矢頭の両翼
	TArray<FVector2D> HeadA;
	HeadA.Add(HeadBase + Perp * HeadHalfWidth);
	HeadA.Add(Tip);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), HeadA, ESlateDrawEffect::None, Color, true, 2.5f);

	TArray<FVector2D> HeadB;
	HeadB.Add(HeadBase - Perp * HeadHalfWidth);
	HeadB.Add(Tip);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), HeadB, ESlateDrawEffect::None, Color, true, 2.5f);
}

void SWorkbenchCanvas::DrawTiles(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TileSize = M->TileSize;

	for (int32 i = 0; i < M->GridData.Num(); ++i)
	{
		const ETileType Type = M->GridData[i].TileType;
		if (Type == ETileType::Empty)
		{
			continue;
		}

		const int32 CellX = i % M->MapWidth;
		const int32 CellY = i / M->MapWidth;
		const FLinearColor Color = MapWorkbenchStyle::GetTileColor(Type);

		// マス塗り（境界をわずかに残す）
		const FSlateRect Rect = TileScreenRect(CellX, CellY, TileSize, LocalSize);
		DrawBoxAt(OutDrawElements, AllottedGeometry, LayerId, Rect, FLinearColor(Color.R, Color.G, Color.B, 0.85f));

		// 2026.10.23 Lee start
		/** @brief 穴は編集画面だけで交差線を表示し、通常の空地と区別する。 */
		if (Type == ETileType::Hole)
		{
			TArray<FVector2D> Line;
			Line.Add(FVector2D(Rect.Left, Rect.Top));
			Line.Add(FVector2D(Rect.Right, Rect.Bottom));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(),
				Line, ESlateDrawEffect::None, FLinearColor(0.55f, 0.58f, 0.62f), true, 1.0f);
			Line[0] = FVector2D(Rect.Left, Rect.Bottom);
			Line[1] = FVector2D(Rect.Right, Rect.Top);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(),
				Line, ESlateDrawEffect::None, FLinearColor(0.55f, 0.58f, 0.62f), true, 1.0f);
		}
		// 2026.10.23 Lee end

		// 方向タイルは「世界軸」基準で矢印を描く（画布X=世界+Xが画面右、画布Y=世界+Yが画面上）：
		// DirUp=+Y → 上、DirDown=-Y → 下、DirLeft=+X → 画面右、DirRight=-X → 画面左
		if (AMapManager::IsDirectionTile(Type))
		{
			FVector2D ScreenDir = FVector2D::ZeroVector;
			switch (Type)
			{
			case ETileType::DirUp:   ScreenDir = FVector2D(0.0f, -1.0f); break;
			case ETileType::DirDown: ScreenDir = FVector2D(0.0f, 1.0f); break;
			case ETileType::DirLeft: ScreenDir = FVector2D(1.0f, 0.0f); break;
			case ETileType::DirRight:ScreenDir = FVector2D(-1.0f, 0.0f); break;
			default: break;
			}
			const FVector2D Center = WorldToScreen(CellX * TileSize, CellY * TileSize, LocalSize);
			DrawArrow(OutDrawElements, AllottedGeometry, LayerId, Center, TileSize * 0.5f * ViewScale, ScreenDir, FLinearColor(0.05f, 0.05f, 0.08f, 0.95f));
		}
	}
}

void SWorkbenchCanvas::DrawOutOfBoundsArchive(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TileSize = M->TileSize;

	for (const FOutOfBoundsTileRecord& Record : M->OutOfBoundsTiles)
	{
		if (Record.TileData.TileType == ETileType::Empty)
		{
			continue;
		}

		const FLinearColor Color = MapWorkbenchStyle::GetTileColor(Record.TileData.TileType);
		const FSlateRect Rect = TileScreenRect(Record.GridCoords.X, Record.GridCoords.Y, TileSize, LocalSize);

		// 退避タイルは半透明で表示（盤面データではなくアーカイブである印）
		DrawBoxAt(OutDrawElements, AllottedGeometry, LayerId, Rect, FLinearColor(Color.R, Color.G, Color.B, 0.25f));

		DrawRectBorder(OutDrawElements, AllottedGeometry, LayerId, Rect, FLinearColor(1.0f, 1.0f, 1.0f, 0.45f), 1.0f);
	}
}

void SWorkbenchCanvas::DrawRectBorder(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FSlateRect& InRect, const FLinearColor& Color, float Thickness) const
{
	TArray<FVector2D> Points;
	Points.Add(FVector2D(InRect.Left, InRect.Top));
	Points.Add(FVector2D(InRect.Right, InRect.Top));
	Points.Add(FVector2D(InRect.Right, InRect.Bottom));
	Points.Add(FVector2D(InRect.Left, InRect.Bottom));
	Points.Add(FVector2D(InRect.Left, InRect.Top));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, false, Thickness);
}

void SWorkbenchCanvas::DrawBoxAt(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FSlateRect& InRect, const FLinearColor& Color) const
{
	// 矩形そのものの位置・サイズで PaintGeometry を作る（全Canvasを塗らない）
	FSlateDrawElement::MakeBox(
		OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, InRect.GetTopLeft(), InRect.GetSize()),
		FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
}

void SWorkbenchCanvas::DrawBindingMarkers(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TileSize = M->TileSize;

	for (const FSpawnTileBinding& Binding : M->SpawnTileBindings)
	{
		if (Binding.Spawner == nullptr)
		{
			continue;
		}
		const FSlateRect Rect = TileScreenRect(Binding.GridCoords.X, Binding.GridCoords.Y, TileSize, LocalSize);
		// 関連付き出生マスは白枠で強調
		DrawRectBorder(OutDrawElements, AllottedGeometry, LayerId, MapWorkbenchStyle::ExpandedRect(Rect, 2.0f), FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), 2.0f);
	}
}

void SWorkbenchCanvas::DrawToolActors(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector MapOrigin = M->GetActorLocation();
	const float TileSize = M->TileSize;

	int32 ValidIndex = 0;
	for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
	{
		AActor* Actor = ActorPtr.Get();
		if (!Actor)
		{
			continue;
		}
		const int32 DrawIndex = ValidIndex++;

		const FVector Rel = Actor->GetActorLocation() - MapOrigin;
		const FVector2D ScreenPos = WorldToScreen(Rel.X, Rel.Y, LocalSize);
		const FIntPoint Cell(
			FMath::FloorToInt32(Rel.X / TileSize + 0.5f),
			FMath::FloorToInt32(Rel.Y / TileSize + 0.5f));
		const bool bOutOfBounds = Cell.X < 0 || Cell.X >= M->MapWidth || Cell.Y < 0 || Cell.Y >= M->MapHeight;
		const bool bOtherLevel = Actor->GetLevel() != M->GetLevel();

		// 実ワールド位置に小さい方塊を描く（越界=赤、別レベル=灰、通常=青緑）
		FLinearColor MarkerColor(0.30f, 0.90f, 0.70f);
		if (bOutOfBounds)
		{
			MarkerColor = FLinearColor(1.0f, 0.35f, 0.30f);
		}
		else if (bOtherLevel)
		{
			MarkerColor = FLinearColor(0.55f, 0.55f, 0.55f);
		}

		const float Half = 5.0f;
		const FSlateRect MarkerRect(ScreenPos.X - Half, ScreenPos.Y - Half, ScreenPos.X + Half, ScreenPos.Y + Half);
		DrawBoxAt(OutDrawElements, AllottedGeometry, LayerId, MarkerRect, FLinearColor(MarkerColor.R, MarkerColor.G, MarkerColor.B, 0.9f));
		DrawRectBorder(OutDrawElements, AllottedGeometry, LayerId, MarkerRect, FLinearColor(0.05f, 0.05f, 0.05f, 0.9f), 1.0f);

		// リストと共通の通し番号
		FSlateDrawElement::MakeText(
			OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, ScreenPos + FVector2D(6.0f, -14.0f), FVector2D(80.0f, 14.0f)),
			FText::AsNumber(DrawIndex), FCoreStyle::GetDefaultFontStyle("Bold", 8),
			ESlateDrawEffect::None, MarkerColor);
	}
}

void SWorkbenchCanvas::DrawHoverAndSelection(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float TileSize = M->TileSize;

	// 選択マス（黄枠）
	if (Context.IsValid())
	{
		const TOptional<FIntPoint> Selected = Context->GetSelectedCell();
		if (Selected.IsSet())
		{
			const FSlateRect Rect = TileScreenRect(Selected->X, Selected->Y, TileSize, LocalSize);
			DrawRectBorder(OutDrawElements, AllottedGeometry, LayerId, MapWorkbenchStyle::ExpandedRect(Rect, 1.5f), FLinearColor(1.0f, 0.9f, 0.2f, 1.0f), 2.5f);
		}
	}

	// ホバー（枠＋筆肋プレビュー）
	if (HoverCell.IsSet())
	{
		const FSlateRect Rect = TileScreenRect(HoverCell->X, HoverCell->Y, TileSize, LocalSize);
		const EWorkbenchBrush Brush = Context.IsValid() ? Context->GetBrush() : EWorkbenchBrush::Select;
		const ETileType PreviewTile = MapWorkbenchStyle::BrushToTile(Brush);
		if (PreviewTile != ETileType::Empty)
		{
			const FLinearColor PreviewColor = MapWorkbenchStyle::GetTileColor(PreviewTile);
			DrawBoxAt(OutDrawElements, AllottedGeometry, LayerId, Rect, FLinearColor(PreviewColor.R, PreviewColor.G, PreviewColor.B, 0.35f));
		}
		DrawRectBorder(OutDrawElements, AllottedGeometry, LayerId, Rect,
			Brush == EWorkbenchBrush::Select ? FLinearColor(1.0f, 0.9f, 0.2f, 0.95f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.85f), 1.5f);

		// ホバー情報（左上）
		if (HoverCell->X >= 0 && HoverCell->X < M->MapWidth && HoverCell->Y >= 0 && HoverCell->Y < M->MapHeight)
		{
			const int32 Index = HoverCell->Y * M->MapWidth + HoverCell->X;
			if (M->GridData.IsValidIndex(Index))
			{
				FFormatNamedArguments HoverArgs;
				HoverArgs.Add(TEXT("x"), FText::AsNumber(HoverCell->X));
				HoverArgs.Add(TEXT("y"), FText::AsNumber(HoverCell->Y));
				HoverArgs.Add(TEXT("type"), MapWorkbenchStyle::GetTileLabel(M->GridData[Index].TileType));
				FString HoverInfo = FMapWorkbenchText::Format(EWorkbenchText::HoverCellFmt, HoverArgs).ToString();
				if (M->SpawnTileBindings.FindByPredicate([&](const FSpawnTileBinding& B) { return B.GridCoords == HoverCell.GetValue() && B.Spawner != nullptr; }) != nullptr)
				{
					HoverInfo += FMapWorkbenchText::Get(EWorkbenchText::BoundMark).ToString();
				}
				FSlateDrawElement::MakeText(
					OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, FVector2D(10.0f, 6.0f), FVector2D(600.0f, 20.0f)),
					FText::FromString(HoverInfo), FCoreStyle::GetDefaultFontStyle("Bold", 10),
					ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.95f));
			}
		}
	}
}

void SWorkbenchCanvas::DrawLegend(AMapManager* M, FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId) const
{
	// 画面左下へ方向対応の図例を描く（世界軸の明示）
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	FVector2D Pos(10.0f, LocalSize.Y - 38.0f);

	const FString Line1 = FMapWorkbenchText::Get(EWorkbenchText::LegendLine1).ToString();
	const FString Line2 = FMapWorkbenchText::Get(EWorkbenchText::LegendLine2).ToString();

	FSlateDrawElement::MakeText(
		OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, Pos, FVector2D(760.0f, 16.0f)),
		FText::FromString(Line1), FCoreStyle::GetDefaultFontStyle("Regular", 9),
		ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.75f));
	Pos.Y += 16.0f;
	FSlateDrawElement::MakeText(
		OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, Pos, FVector2D(760.0f, 16.0f)),
		FText::FromString(Line2), FCoreStyle::GetDefaultFontStyle("Regular", 9),
		ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.75f));
}

void SWorkbenchCanvas::DrawCenteredText(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32& LayerId, const FString& InText) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FText Text = FText::FromString(InText);
	const FVector2D TextPos(LocalSize.X * 0.5f - 260.0f, LocalSize.Y * 0.5f - 10.0f);
	FSlateDrawElement::MakeText(
		OutDrawElements, LayerId++, MapWorkbenchStyle::MakePaintGeometry(AllottedGeometry, TextPos, FVector2D(520.0f, 40.0f)),
		Text, FCoreStyle::GetDefaultFontStyle("Bold", 11),
		ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f));
}

//--------------------------------------------------------------------------
// マウス操作
//--------------------------------------------------------------------------

FReply SWorkbenchCanvas::OnMouseButtonDown(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent)
{
	AMapManager* M = Context.IsValid() ? Context->MapTarget() : nullptr;
	if (!M)
	{
		if (Context.IsValid())
		{
			Context->ReportStatus(FMapWorkbenchText::Get(EWorkbenchText::CanvasNoTargetClick));
		}
		return FReply::Handled();
	}

	const FVector2D Local = AllottedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FIntPoint Cell = CellAtScreen(Local, AllottedGeometry.GetLocalSize());

	// 左：筆肋描画 / 右：Empty 化
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FText Reason;
		if (!Context.IsValid() || !Context->CanEdit(Reason))
		{
			if (Context.IsValid())
			{
				Context->ReportStatus(Reason);
			}
			return FReply::Handled();
		}

		const EWorkbenchBrush Brush = Context->GetBrush();
		const bool bRightErase = (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton);

		// 座標選択ペン＋左ボタンのみ：書き込まずに選択だけ行う（右ボタンは常に消去）
		if (Brush == EWorkbenchBrush::Select && !bRightErase)
		{
			Context->SelectCell(Cell);
			return FReply::Handled();
		}

		bStrokeActive = true;
		bStrokeIsErase = bRightErase || (Brush == EWorkbenchBrush::Empty);
		StrokeTile = bStrokeIsErase ? ETileType::Empty : MapWorkbenchStyle::BrushToTile(Brush);
		LastStrokeCell.Reset();
		VisitedCells.Reset();

		StrokeTo(Cell);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
	}

	// 中：平移
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bPanning = true;
		PanDragStart = MouseEvent.GetScreenSpacePosition();
		PanStart = ViewPan;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	return FReply::Unhandled();
}

FReply SWorkbenchCanvas::OnMouseButtonUp(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent)
{
	if (bStrokeActive && (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton))
	{
		EndStrokeInternal();
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (bPanning && MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SWorkbenchCanvas::OnMouseMove(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = AllottedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const TOptional<FIntPoint> NewHover = CellAtScreen(Local, AllottedGeometry.GetLocalSize());

	bool bHoverChanged = (!HoverCell.IsSet() != !NewHover.IsSet()) ||
		(HoverCell.IsSet() && NewHover.IsSet() && HoverCell.GetValue() != NewHover.GetValue());
	HoverCell = NewHover;

	if (bPanning)
	{
		ViewPan = PanStart + (MouseEvent.GetScreenSpacePosition() - PanDragStart);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	if (bStrokeActive)
	{
		StrokeTo(NewHover.GetValue());
	}

	if (bHoverChanged || bStrokeActive)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Unhandled();
}

FReply SWorkbenchCanvas::OnMouseWheel(const FGeometry& AllottedGeometry, const FPointerEvent& MouseEvent)
{
	AMapManager* M = Context.IsValid() ? Context->MapTarget() : nullptr;
	if (!M)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();

	// Shift + ホイール：垂直スクロール（平移）
	if (MouseEvent.GetModifierKeys().IsShiftDown())
	{
		ViewPan.Y += MouseEvent.GetWheelDelta() * 40.0f;
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	// 通常ホイール：カーソル位置を固定したズーム
	const FVector2D Local = AllottedGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D World = ScreenToWorld(Local, LocalSize);
	const float TileSize = M->TileSize;

	const float NewScale = FMath::Clamp(ViewScale * (MouseEvent.GetWheelDelta() > 0.0f ? 1.15f : 1.0f / 1.15f), 2.0f / TileSize, 300.0f / TileSize);
	ViewPan = FVector2D(Local.X - World.X * NewScale, (LocalSize.Y - Local.Y) - World.Y * NewScale);
	ViewScale = NewScale;

	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SWorkbenchCanvas::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	HoverCell.Reset();
	EndStrokeInternal();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SWorkbenchCanvas::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// 捕获丢失 → 結束ストローク・平移
	bPanning = false;
	EndStrokeInternal();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SWorkbenchCanvas::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	EndStrokeInternal();
}

//--------------------------------------------------------------------------
// ストローク
//--------------------------------------------------------------------------

void SWorkbenchCanvas::BuildLineCells(const FIntPoint& From, const FIntPoint& To, TArray<FIntPoint>& OutCells)
{
	int32 X = From.X;
	int32 Y = From.Y;
	const int32 DX = FMath::Abs(To.X - X);
	const int32 DY = -FMath::Abs(To.Y - Y);
	const int32 StepX = (X < To.X) ? 1 : -1;
	const int32 StepY = (Y < To.Y) ? 1 : -1;
	int32 Error = DX + DY;

	// 念のためのステップ数上限（盤面 256x256 + 余裕で十分）
	int32 SafetyCount = 0;
	while (true)
	{
		OutCells.Add(FIntPoint(X, Y));
		if (X == To.X && Y == To.Y)
		{
			break;
		}
		const int32 Error2 = 2 * Error;
		if (Error2 >= DY)
		{
			Error += DY;
			X += StepX;
		}
		if (Error2 <= DX)
		{
			Error += DX;
			Y += StepY;
		}
		if (++SafetyCount > 65536)
		{
			break;
		}
	}
}

void SWorkbenchCanvas::StrokeTo(const FIntPoint& InCell)
{
	if (!bStrokeActive || !Context.IsValid())
	{
		return;
	}

	// 画板外の極端な座標（巨大値）では盤面付近 ± 余裕域を超えた時点でストロークを終了し、
	// Bresenham が膨大なセル列を生成しないようにする
	{
		AMapManager* GuardMap = Context->MapTarget();
		const int32 LimitX = ((GuardMap ? GuardMap->MapWidth : 64) + 64);
		const int32 LimitY = ((GuardMap ? GuardMap->MapHeight : 64) + 64);
		if (FMath::Abs(InCell.X) > LimitX || FMath::Abs(InCell.Y) > LimitY)
		{
			EndStrokeInternal();
			return;
		}
	}

	if (!LastStrokeCell.IsSet())
	{
		PaintCellOnce(InCell);
		LastStrokeCell = InCell;
		return;
	}

	if (LastStrokeCell.GetValue() == InCell)
	{
		return;
	}

	// 跨格補間：Bresenham 直線（先頭は前回処理済みのためスキップ）
	TArray<FIntPoint> LineCells;
	BuildLineCells(LastStrokeCell.GetValue(), InCell, LineCells);
	for (int32 i = 1; i < LineCells.Num(); ++i)
	{
		PaintCellOnce(LineCells[i]);
	}
	LastStrokeCell = InCell;
}

void SWorkbenchCanvas::PaintCellOnce(const FIntPoint& InCell)
{
	if (VisitedCells.Contains(InCell))
	{
		// 同一ストローク内の再訪：再書き込みしない
		return;
	}
	VisitedCells.Add(InCell);

	FText Reason;
	Context->PaintCell(InCell, StrokeTile, Reason);
}

void SWorkbenchCanvas::EndStrokeInternal()
{
	if (!bStrokeActive)
	{
		return;
	}
	bStrokeActive = false;
	LastStrokeCell.Reset();
	VisitedCells.Reset();

	if (Context.IsValid())
	{
		Context->NotifyStrokeCommitted();
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

//==============================================================================
// パネル本体
//==============================================================================

void SMapWorkbench::Construct(const FArguments& InArgs)
{
	// ---- キャンバス文脈（メンバ関数へのブリッジ） ----
	TSharedRef<FWorkbenchCanvasContext> CanvasContext = MakeShared<FWorkbenchCanvasContext>();
	CanvasContext->MapTarget = [this]() -> AMapManager* { return GetValidTarget(); };
	CanvasContext->GetBrush = [this]() { return CurrentBrush; };
	CanvasContext->GetSelectedCell = [this]() { return SelectedCell; };
	CanvasContext->CanEdit = [this](FText& Reason) { return CanEditTarget(&Reason); };
	CanvasContext->PaintCell = [this](const FIntPoint& Cell, ETileType Type, FText& Reason) { return PaintCell(Cell, Type, Reason); };
	CanvasContext->SelectCell = [this](const FIntPoint& Cell) { HandleCellSelected(Cell); };
	CanvasContext->NotifyStrokeCommitted = [this]() { NotifyStrokeCommitted(); };
	CanvasContext->ReportStatus = [this](const FText& Message) { UpdateStatus(Message); };

	SAssignNew(Canvas, SWorkbenchCanvas).Context(CanvasContext);

	// ---- 筆肋ボタン生成ヘルパ（ラジオ風チェックボックス、ラベルは FText） ----
	auto MakeBrushCheck = [this](EWorkbenchBrush InBrush, const FText& InLabel) -> TSharedRef<SWidget>
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([this, InBrush]()
			{
				return CurrentBrush == InBrush ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, InBrush](ECheckBoxState InState)
			{
				if (InState == ECheckBoxState::Checked)
				{
					CurrentBrush = InBrush;
				}
			})
			[
				SNew(STextBlock).Text(InLabel)
			];
	};

	auto MakeColorSwatch = [](ETileType InType) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.WidthOverride(14.0f)
			.HeightOverride(14.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(MapWorkbenchStyle::GetTileColor(InType))
				.Padding(0.0f)
				[
					SNullWidget::NullWidget
				]
			];
	};

	auto MakeLegendEntry = [MakeColorSwatch](ETileType InType, const FText& InLabel) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f)
			[
				MakeColorSwatch(InType)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f)
			[
				SNew(STextBlock).Text(InLabel)
			];
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		//----------------------------------------------------------------------
		// 目標バインド行
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 6.0f, 6.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::BindSelection))
				.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BindSelectionTooltip))
				.OnClicked_Lambda([this]() { HandleBindToSelection(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::UnbindButton))
				.OnClicked_Lambda([this]() { HandleUnbindTarget(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 2.0f).VAlign(VAlign_Center)
			[
				SAssignNew(TargetInfoText, STextBlock)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::TargetNone))
			]
		]

		//----------------------------------------------------------------------
		// 筆肋選択行
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Select,  FMapWorkbenchText::Get(EWorkbenchText::BrushSelect)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Empty,   FMapWorkbenchText::Get(EWorkbenchText::BrushErase)) ]
			// 2026.10.23 Lee start
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Hole,    FMapWorkbenchText::Get(EWorkbenchText::BrushHole)) ]
			// 2026.10.23 Lee end
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirUp,   FMapWorkbenchText::Get(EWorkbenchText::BrushUp)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirDown, FMapWorkbenchText::Get(EWorkbenchText::BrushDown)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirLeft, FMapWorkbenchText::Get(EWorkbenchText::BrushLeft)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirRight,FMapWorkbenchText::Get(EWorkbenchText::BrushRight)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Spawn,   FMapWorkbenchText::Get(EWorkbenchText::BrushSpawn)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::GoalP1,  FMapWorkbenchText::Get(EWorkbenchText::BrushGoalP1)) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::GoalP2,  FMapWorkbenchText::Get(EWorkbenchText::BrushGoalP2)) ]
		]

		//----------------------------------------------------------------------
		// 二维キャンバス
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 2.0f)
		[
			SNew(SBox)
			.HeightOverride(560.0f)
			[
				Canvas.ToSharedRef()
			]
		]

		//----------------------------------------------------------------------
		// 図例行（色見本＋軸説明）
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::Empty,    FMapWorkbenchText::Get(EWorkbenchText::LegendItemEmpty)) ]
			// 2026.10.23 Lee start
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::Hole,     FMapWorkbenchText::Get(EWorkbenchText::LegendItemHole)) ]
			// 2026.10.23 Lee end
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirUp,    FMapWorkbenchText::Get(EWorkbenchText::LegendItemUp)) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirDown,  FMapWorkbenchText::Get(EWorkbenchText::LegendItemDown)) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirLeft,  FMapWorkbenchText::Get(EWorkbenchText::LegendItemLeft)) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirRight, FMapWorkbenchText::Get(EWorkbenchText::LegendItemRight)) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::Spawn,    FMapWorkbenchText::Get(EWorkbenchText::LegendItemSpawn)) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::GoalP1,   FMapWorkbenchText::Get(EWorkbenchText::LegendItemGoalP1)) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::GoalP2,   FMapWorkbenchText::Get(EWorkbenchText::LegendItemGoalP2)) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(10.0f, 0.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::LegendPanelAxis))
			]
		]

		//----------------------------------------------------------------------
		// 選択マス情報
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 2.0f)
		[
			SAssignNew(CellInfoText, STextBlock)
			.Text(FMapWorkbenchText::Get(EWorkbenchText::CellNoneHint))
		]

		//----------------------------------------------------------------------
		// ツール生成アクター区
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(FMapWorkbenchText::Get(EWorkbenchText::ToolSectionTitle))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnPickAsset))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnPickAssetTooltip))
						.OnClicked_Lambda([this]() { HandlePickAssetFromSelection(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnPlaceAsset))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnPlaceAssetTooltip))
						.OnClicked_Lambda([this]() { HandlePlaceAssetAtSelectedCell(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnDeleteToolActor))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnDeleteToolActorTooltip))
						.OnClicked_Lambda([this]() { HandleDeleteSelectedToolActor(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 2.0f).VAlign(VAlign_Center)
					[
						SAssignNew(AssetInfoText, STextBlock)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::AssetNone))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
				[
					SAssignNew(ToolActorList, SListView<TSharedPtr<FToolActorRow>>)
					.ListItemsSource(&ToolActorRows)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow_Lambda([this](TSharedPtr<FToolActorRow> InRow, const TSharedRef<STableViewBase>& OwnerTable)
					{
						return SNew(STableRow<TSharedPtr<FToolActorRow>>, OwnerTable)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 2.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([InRow]() { return InRow.IsValid() ? FText::AsNumber(InRow->Index) : FText::GetEmpty(); })
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 2.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this, InRow]()
								{
									if (!InRow.IsValid())
									{
										return FText::GetEmpty();
									}
									FFormatNamedArguments RowArgs;
									RowArgs.Add(TEXT("label"), FText::FromString(InRow->Label));
									RowArgs.Add(TEXT("x"), FText::AsNumber(FMath::RoundToInt32(InRow->WorldLocation.X)));
									RowArgs.Add(TEXT("y"), FText::AsNumber(FMath::RoundToInt32(InRow->WorldLocation.Y)));
									RowArgs.Add(TEXT("z"), FText::AsNumber(FMath::RoundToInt32(InRow->WorldLocation.Z)));
									RowArgs.Add(TEXT("cx"), FText::AsNumber(InRow->Cell.X));
									RowArgs.Add(TEXT("cy"), FText::AsNumber(InRow->Cell.Y));
									FString Marks;
									if (InRow->bOutOfBounds)
									{
										Marks += FMapWorkbenchText::Get(EWorkbenchText::RowMarkOob).ToString();
									}
									if (InRow->bOtherLevel)
									{
										Marks += FMapWorkbenchText::Get(EWorkbenchText::RowMarkOtherLevel).ToString();
									}
									RowArgs.Add(TEXT("marks"), FText::FromString(Marks));
									return FMapWorkbenchText::Format(EWorkbenchText::RowDescFmt, RowArgs);
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 2.0f)
							[
								SNew(SButton)
								.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnFocusActor))
								.OnClicked_Lambda([this, InRow]()
								{
									if (InRow.IsValid() && InRow->Actor.IsValid())
									{
										HandleFocusToolActor(InRow->Actor.Get());
									}
									return FReply::Handled();
								})
							]
						];
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FToolActorRow> InRow, ESelectInfo::Type SelectInfo) { RefreshCellInfo(); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(FMapWorkbenchText::Get(EWorkbenchText::ToolSectionHint))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]

		//----------------------------------------------------------------------
		// 生成器区
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(FMapWorkbenchText::Get(EWorkbenchText::SpawnerSectionTitle))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnPickSpawner))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnPickSpawnerTooltip))
						.OnClicked_Lambda([this]() { HandlePickSpawnerFromSelection(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnBindSpawn))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnBindSpawnTooltip))
						.OnClicked_Lambda([this]() { HandleBindSpawner(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnMoveSpawn))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnMoveSpawnTooltip))
						.OnClicked_Lambda([this]() { HandleMoveSpawnerSpawnTile(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::UnbindButton))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnUnbindSpawnTooltip))
						.OnClicked_Lambda([this]() { HandleUnbindSpawner(); return FReply::Handled(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
				[
					SAssignNew(SpawnerInfoText, STextBlock)
					.Text(FMapWorkbenchText::Get(EWorkbenchText::SpawnerNone))
				]
			]
		]

		//----------------------------------------------------------------------
		// 盤面パラメータ区（待機適用値）
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(FMapWorkbenchText::Get(EWorkbenchText::ParamsSectionTitle))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FMapWorkbenchText::Get(EWorkbenchText::LabelWidth))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SAssignNew(PendingWidthBox, SSpinBox<int32>)
						.MinValue(1)
						.MaxValue(AMapManager::MaxGridDimension)
						.Delta(1)
						.Value(10)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FMapWorkbenchText::Get(EWorkbenchText::LabelHeight))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SAssignNew(PendingHeightBox, SSpinBox<int32>)
						.MinValue(1)
						.MaxValue(AMapManager::MaxGridDimension)
						.Delta(1)
						.Value(10)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FMapWorkbenchText::Get(EWorkbenchText::LabelTileSize))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SAssignNew(PendingTileSizeBox, SSpinBox<float>)
						.MinValue(1.0f)
						.MaxValue(100000.0f)
						.Delta(10.0f)
						.Value(100.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnApplyParams))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnApplyParamsTooltip))
						.OnClicked_Lambda([this]() { HandleApplyParams(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
					[
						SNew(SButton)
						.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnSyncParams))
						.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnSyncParamsTooltip))
						.OnClicked_Lambda([this]() { SyncPendingParamsFromCurrent(); return FReply::Handled(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(FMapWorkbenchText::Get(EWorkbenchText::ParamsHintLine))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]

		//----------------------------------------------------------------------
		// 保存・取り消し行
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnSaveLevel))
				.ToolTipText(FMapWorkbenchText::Get(EWorkbenchText::BtnSaveLevelTooltip))
				.OnClicked_Lambda([this]() { HandleSaveTargetLevel(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnUndo))
				.OnClicked_Lambda([this]() { HandleUndo(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.Text(FMapWorkbenchText::Get(EWorkbenchText::BtnRedo))
				.OnClicked_Lambda([this]() { HandleRedo(); return FReply::Handled(); })
			]
		]

		//----------------------------------------------------------------------
		// ステータスバー
		//----------------------------------------------------------------------
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 2.0f, 6.0f, 6.0f)
		[
			SAssignNew(StatusText, STextBlock)
			.AutoWrapText(true)
			.Text(FMapWorkbenchText::Get(EWorkbenchText::StatusInitial))
		]
		];
}

SMapWorkbench::~SMapWorkbench()
{
	// 残存ストロークを確定してから破棄する（取引の後始末）
	if (Canvas.IsValid())
	{
		Canvas->ForceEndStroke();
	}
	NotifyStrokeCommitted();
}

//==============================================================================
// 縮小確認ダイアログ（cpp 内部）
//==============================================================================

/**
 * @brief 盤面縮小時の越界データ確認ダイアログ（Yes=保留 / No=移除 / Cancel=取消）。
 */
class SWorkbenchShrinkDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorkbenchShrinkDialog) {}
		SLATE_ARGUMENT(FText, BodyText)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, OwnerWindow)
		SLATE_ARGUMENT(TSharedPtr<int32>, OutChoice)
	SLATE_END_ARGS()

	/** @brief ダイアログ本体（本文スクロール＋三択ボタン）を構築する。 */
	void Construct(const FArguments& InArgs)
	{
		OwnerWindow = InArgs._OwnerWindow;
		OutChoice = InArgs._OutChoice;

		auto MakeChoiceButton = [this](int32 InValue, const FText& InLabel) -> TSharedRef<SWidget>
		{
			return SNew(SButton)
				.Text(InLabel)
				.OnClicked_Lambda([this, InValue]() { FinishChoice(InValue); return FReply::Handled(); });
		};

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(10.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(STextBlock).AutoWrapText(true).Text(InArgs._BodyText)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(10.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f)
				[
					MakeChoiceButton(0, FMapWorkbenchText::Get(EWorkbenchText::ShrinkKeep))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f)
				[
					MakeChoiceButton(1, FMapWorkbenchText::Get(EWorkbenchText::ShrinkRemove))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f)
				[
					MakeChoiceButton(2, FMapWorkbenchText::Get(EWorkbenchText::ShrinkCancel))
				]
			]
		];
	}

private:
	/** @brief 選択を記録してダイアログを閉じる（0=保留 1=移除 2=取消）。 */
	void FinishChoice(int32 InValue)
	{
		if (OutChoice.IsValid())
		{
			*OutChoice = InValue;
		}
		if (OwnerWindow.IsValid())
		{
			OwnerWindow->RequestDestroyWindow();
		}
	}

	/** @brief 閉じる対象のウィンドウ。 */
	TSharedPtr<SWindow> OwnerWindow;

	/** @brief 呼び出し側へ選択結果を返す共有変数。 */
	TSharedPtr<int32> OutChoice;
};

/**
 * @brief 縮小確認ダイアログをモーダル表示し、選択結果を返す。
 * @return 0=保留 1=移除 2=取消。
 */
static int32 ShowShrinkDialog(const TSharedRef<SWidget>& ParentWidget, const FText& BodyText)
{
	TSharedPtr<int32> Choice = MakeShared<int32>(2);
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(ParentWidget);
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FMapWorkbenchText::Get(EWorkbenchText::ShrinkDialogTitle))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMinimize(false)
		.SupportsMaximize(false);
	Window->SetContent(SNew(SWorkbenchShrinkDialog).BodyText(BodyText).OwnerWindow(Window).OutChoice(Choice));
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow);
	return Choice.IsValid() ? *Choice : 2;
}

//==============================================================================
// パネル実装：目標バインド
//==============================================================================

/** @brief float のビット列を署名用へ変換する。 */
static uint32 MapWorkbenchFloatBits(float InValue)
{
	uint32 Bits = 0;
	FMemory::Memcpy(&Bits, &InValue, sizeof(Bits));
	return Bits;
}

AMapManager* SMapWorkbench::GetValidTarget(FText* OutProblem) const
{
	AMapManager* M = BoundMap.Get();
	if (M == nullptr)
	{
		if (OutProblem)
		{
			*OutProblem = FMapWorkbenchText::Get(EWorkbenchText::ProblemNoTarget);
		}
		return nullptr;
	}

	UWorld* TargetWorld = M->GetWorld();
	UWorld* EditorWorld = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (TargetWorld == nullptr || EditorWorld == nullptr || TargetWorld != EditorWorld || TargetWorld->WorldType != EWorldType::Editor)
	{
		if (OutProblem)
		{
			*OutProblem = FMapWorkbenchText::Get(EWorkbenchText::ProblemWrongWorld);
		}
		return nullptr;
	}
	return M;
}

bool SMapWorkbench::CanEditTarget(FText* OutReason) const
{
	FText Problem;
	AMapManager* M = GetValidTarget(&Problem);
	if (M == nullptr)
	{
		// 実際のバインド解除は Tick（ForceUnbindIfNeeded）が行う
		if (OutReason)
		{
			*OutReason = Problem;
		}
		return false;
	}
	if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
	{
		if (OutReason)
		{
			*OutReason = FMapWorkbenchText::Get(EWorkbenchText::OverlayPie);
		}
		return false;
	}
	if (FLevelUtils::IsLevelLocked(M))
	{
		if (OutReason)
		{
			*OutReason = FMapWorkbenchText::Get(EWorkbenchText::OverlayLocked);
		}
		return false;
	}
	return true;
}

void SMapWorkbench::HandleBindToSelection()
{
	if (GEditor == nullptr)
	{
		return;
	}
	if (GEditor->PlayWorld != nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::CannotBindPie));
		return;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AMapManager*> Maps;
	if (SelectedActors != nullptr)
	{
		SelectedActors->GetSelectedObjects<AMapManager>(Maps);
	}
	// エディタ世界に属するインスタンスのみ対象（PIE 複製を除外）
	Maps.RemoveAll([](AMapManager* Candidate)
	{
		UWorld* W = Candidate ? Candidate->GetWorld() : nullptr;
		UWorld* EditorWorld = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
		return (W == nullptr || EditorWorld == nullptr || W != EditorWorld || W->WorldType != EWorldType::Editor);
	});

	if (Maps.Num() == 0)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NotSelectedMap));
		return;
	}
	if (Maps.Num() > 1)
	{
		FFormatNamedArguments CountArgs;
		CountArgs.Add(TEXT("count"), FText::AsNumber(Maps.Num()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::MultiSelectedMapsFmt, CountArgs));
		return;
	}

	AMapManager* M = Maps[0];
	BoundMap = M;
	SelectedCell.Reset();
	SelectedSpawner.Reset();
	bHasSelectedAsset = false;
	SelectedAssetData = FAssetData();
	SyncPendingParamsFromCurrent();
	if (Canvas.IsValid())
	{
		Canvas->RequestFitView();
	}
	RefreshPanel();

	const FString LevelName = (M->GetLevel() && M->GetLevel()->GetOutermost()) ? M->GetLevel()->GetOutermost()->GetName() : FMapWorkbenchText::Get(EWorkbenchText::TileUnknown).ToString();
	FFormatNamedArguments BoundArgs;
	BoundArgs.Add(TEXT("actor"), FText::FromString(M->GetActorLabel()));
	BoundArgs.Add(TEXT("level"), FText::FromString(LevelName));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::BoundOkFmt, BoundArgs));
}

void SMapWorkbench::HandleUnbindTarget()
{
	if (Canvas.IsValid())
	{
		Canvas->ForceEndStroke();
	}
	NotifyStrokeCommitted();
	BoundMap.Reset();
	SelectedSpawner.Reset();
	SelectedCell.Reset();
	RefreshPanel();
	UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::UnboundOk));
}

void SMapWorkbench::ForceUnbindIfNeeded()
{
	if (!BoundMap.IsValid())
	{
		return;
	}
	AMapManager* M = BoundMap.Get();
	UWorld* TargetWorld = M ? M->GetWorld() : nullptr;
	UWorld* EditorWorld = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
	const bool bInvalid = (M == nullptr || TargetWorld == nullptr || EditorWorld == nullptr || TargetWorld != EditorWorld);
	if (!bInvalid)
	{
		return;
	}
	if (Canvas.IsValid())
	{
		Canvas->ForceEndStroke();
	}
	NotifyStrokeCommitted();
	BoundMap.Reset();
	SelectedSpawner.Reset();
	SelectedCell.Reset();
	RefreshPanel();
	UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::TargetInvalidAutoUnbind));
}

void SMapWorkbench::SyncPendingParamsFromCurrent()
{
	AMapManager* M = GetValidTarget();
	if (M == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::SyncParamsNoTarget));
		return;
	}
	if (PendingWidthBox.IsValid())
	{
		PendingWidthBox->SetValue(M->MapWidth);
	}
	if (PendingHeightBox.IsValid())
	{
		PendingHeightBox->SetValue(M->MapHeight);
	}
	if (PendingTileSizeBox.IsValid())
	{
		PendingTileSizeBox->SetValue(M->TileSize);
	}
	UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::SyncParamsDone));
}

//==============================================================================
// パネル実装：盤面描画
//==============================================================================

ETileType SMapWorkbench::BrushToTileType(EWorkbenchBrush InBrush) const
{
	// キャンバスと同一の cpp 共通ヘルパへ委譲（表記ゆれ防止）
	return MapWorkbenchStyle::BrushToTile(InBrush);
}

bool SMapWorkbench::PaintCell(const FIntPoint& InCell, ETileType InType, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	AMapManager* M = GetValidTarget();
	if (M == nullptr)
	{
		OutReason = FText::Format(FTextFormat(FMapWorkbenchText::Get(EWorkbenchText::ProblemNoTarget)), FFormatNamedArguments());
		return false;
	}
	FText Reason;
	if (!CanEditTarget(&Reason))
	{
		OutReason = Reason;
		return false;
	}
	if (InCell.X < 0 || InCell.X >= M->MapWidth || InCell.Y < 0 || InCell.Y >= M->MapHeight)
	{
		FFormatNamedArguments CellArgs;
		CellArgs.Add(TEXT("x"), FText::AsNumber(InCell.X));
		CellArgs.Add(TEXT("y"), FText::AsNumber(InCell.Y));
		OutReason = FMapWorkbenchText::Format(EWorkbenchText::PaintOobFmt, CellArgs);
		return false;
	}
	// 関連付き出生マスは通常ペン・消去のいずれでも上書き禁止（Spawn 再指定のみ許可）
	if (InType != ETileType::Spawn && FindBindingAtCell(InCell) != nullptr)
	{
		FFormatNamedArguments BoundArgs;
		BoundArgs.Add(TEXT("x"), FText::AsNumber(InCell.X));
		BoundArgs.Add(TEXT("y"), FText::AsNumber(InCell.Y));
		OutReason = FMapWorkbenchText::Format(EWorkbenchText::PaintBoundCellFmt, BoundArgs);
		return false;
	}

	const int32 Index = InCell.Y * M->MapWidth + InCell.X;
	FMapTileData& Tile = M->GridData[Index];
	// エディタ配置はすべて 1P（Owner=0）扱い（ApplyTileEdit と同じ規約）
	const uint8 NewOwner = 0u;
	if (Tile.TileType == InType && Tile.OwnerPlayerId == NewOwner)
	{
		// 同一値：書き込まずに正常終了（盤面を汚さない）
		return true;
	}

	// 最初の実書き込み時のみ取引を開く
	if (!StrokeTransaction.IsValid())
	{
		StrokeTransaction = MakeUnique<FScopedTransaction>(FMapWorkbenchText::Get(EWorkbenchText::TxPaint));
	}
	M->Modify();
	// 2026.10.23 Lee start
	// const ETileType OldType = Tile.TileType;
	// Tile.TileType = InType;
	// Tile.OwnerPlayerId = NewOwner;
	// if (OldType != ETileType::Empty && OldType != InType)
	// {
	// 	M->RefreshStateVisual(OldType);
	// }
	// if (InType != ETileType::Empty)
	// {
	// 	M->RefreshStateVisual(InType);
	// }
	/** @brief 取引と編集制限は維持し、書き込みと床同期は共通処理へ委譲する。 */
	M->SetTileData(InCell.X, InCell.Y, InType, NewOwner);
	// 2026.10.23 Lee end
	bStrokeChanged = true;
	return true;
}

void SMapWorkbench::NotifyStrokeCommitted()
{
	if (StrokeTransaction.IsValid())
	{
		// TUniquePtr 破棄＝FScopedTransaction 確定（スコープ終了で EndTransaction）
		StrokeTransaction.Reset();
	}
	if (bStrokeChanged)
	{
		bStrokeChanged = false;
		RefreshPanel();
	}
}

void SMapWorkbench::HandleCellSelected(const FIntPoint& InCell)
{
	SelectedCell = InCell;
	RefreshCellInfo();
	if (Canvas.IsValid())
	{
		Canvas->InvalidateCanvas();
	}
}

void SMapWorkbench::RefreshCellInfo()
{
	FString Info = FMapWorkbenchText::Get(EWorkbenchText::CellNoneHint).ToString();
	AMapManager* M = GetValidTarget();
	if (SelectedCell.IsSet() && M != nullptr)
	{
		const FIntPoint Cell = SelectedCell.GetValue();
		const bool bInBounds = (Cell.X >= 0 && Cell.X < M->MapWidth && Cell.Y >= 0 && Cell.Y < M->MapHeight);
		if (!bInBounds)
		{
			FFormatNamedArguments OobArgs;
			OobArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
			OobArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
			Info = FMapWorkbenchText::Format(EWorkbenchText::CellOutOfBoundsFmt, OobArgs).ToString();
		}
		else
		{
			const int32 Index = Cell.Y * M->MapWidth + Cell.X;
			const ETileType Type = M->GridData.IsValidIndex(Index) ? M->GridData[Index].TileType : ETileType::Empty;
			FFormatNamedArguments CellArgs;
			CellArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
			CellArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
			CellArgs.Add(TEXT("type"), MapWorkbenchStyle::GetTileLabel(Type));
			Info = FMapWorkbenchText::Format(EWorkbenchText::CellInfoFmt, CellArgs).ToString();
			if (FindBindingAtCell(Cell) != nullptr)
			{
				Info += FMapWorkbenchText::Get(EWorkbenchText::BoundMark).ToString();
			}

			// このマスに投影される配置アクターの番号一覧（リスト番号と同一順序）
			FString ActorIndexes;
			int32 ValidIndex = 0;
			const FVector Origin = M->GetActorLocation();
			for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
			{
				AActor* Actor = ActorPtr.Get();
				if (Actor == nullptr)
				{
					continue;
				}
				const int32 CurrentIndex = ValidIndex++;
				const FVector Rel = Actor->GetActorLocation() - Origin;
				if (ProjectWorldToCell(Actor->GetActorLocation()) == Cell ||
					FMath::FloorToInt32(Rel.X / M->TileSize + 0.5f) == Cell.X && FMath::FloorToInt32(Rel.Y / M->TileSize + 0.5f) == Cell.Y)
				{
					ActorIndexes += (ActorIndexes.IsEmpty() ? FString::Printf(TEXT("%d"), CurrentIndex) : FString::Printf(TEXT(", %d"), CurrentIndex));
				}
			}
			if (!ActorIndexes.IsEmpty())
			{
				FFormatNamedArguments ListArgs;
				ListArgs.Add(TEXT("list"), FText::FromString(ActorIndexes));
				Info += FMapWorkbenchText::Format(EWorkbenchText::CellActorListFmt, ListArgs).ToString();
			}
		}
	}
	if (CellInfoText.IsValid())
	{
		CellInfoText->SetText(FText::FromString(Info));
	}
}

//==============================================================================
// パネル実装：座標換算
//==============================================================================

FVector2D SMapWorkbench::CellAnchorWorldXY(const FIntPoint& InCell) const
{
	AMapManager* M = GetValidTarget();
	if (M == nullptr)
	{
		return FVector2D::ZeroVector;
	}
	const FVector Origin = M->GetActorLocation();
	return FVector2D(Origin.X + InCell.X * M->TileSize, Origin.Y + InCell.Y * M->TileSize);
}

FIntPoint SMapWorkbench::ProjectWorldToCell(const FVector& InWorld) const
{
	AMapManager* M = GetValidTarget();
	if (M == nullptr)
	{
		return FIntPoint(INDEX_NONE, INDEX_NONE);
	}
	const FVector Origin = M->GetActorLocation();
	return FIntPoint(
		FMath::FloorToInt32((InWorld.X - Origin.X) / M->TileSize + 0.5f),
		FMath::FloorToInt32((InWorld.Y - Origin.Y) / M->TileSize + 0.5f));
}

//==============================================================================
// パネル実装：ツール生成アクター
//==============================================================================

UClass* SMapWorkbench::ValidateAssetForPlacement(UObject* InAsset, UStaticMesh*& OutMesh, FText& OutReason) const
{
	OutMesh = nullptr;
	if (InAsset == nullptr)
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::AssetEmpty);
		return nullptr;
	}
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(InAsset))
	{
		OutMesh = Mesh;
		return AStaticMeshActor::StaticClass();
	}

	UBlueprint* BP = Cast<UBlueprint>(InAsset);
	if (BP == nullptr)
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::OnlyMeshOrActorBp);
		return nullptr;
	}
	if (BP->BlueprintType != BPTYPE_Normal)
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::OnlyNormalBp);
		return nullptr;
	}
	if (BP->bDeprecate)
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::BpDeprecated);
		return nullptr;
	}
	UClass* ParentClass = BP->ParentClass.Get();
	if (ParentClass == nullptr || !ParentClass->IsChildOf<AActor>())
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::BpParentNotActor);
		return nullptr;
	}
	if (ParentClass->HasAnyClassFlags(CLASS_Abstract))
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::BpParentAbstract);
		return nullptr;
	}
	if (ParentClass->HasAnyClassFlags(CLASS_Deprecated))
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::BpParentDeprecated);
		return nullptr;
	}
	UClass* GeneratedClass = BP->GeneratedClass.Get();
	if (GeneratedClass == nullptr)
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::BpNoGeneratedClass);
		return nullptr;
	}
	if (GeneratedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		OutReason = FMapWorkbenchText::Get(EWorkbenchText::BpGeneratedClassInvalid);
		return nullptr;
	}
	return GeneratedClass;
}

void SMapWorkbench::HandlePickAssetFromSelection()
{
	// 実際のコンテンツブラウザ選択（FAssetData）を読み取る
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> Selected;
	ContentBrowserModule.Get().GetSelectedAssets(Selected);

	TArray<UObject*> Candidates;
	for (const FAssetData& AssetData : Selected)
	{
		UObject* LoadedObject = AssetData.GetAsset();
		if (LoadedObject != nullptr && (LoadedObject->IsA<UStaticMesh>() || LoadedObject->IsA<UBlueprint>()))
		{
			Candidates.Add(LoadedObject);
		}
	}

	if (Candidates.Num() == 0)
	{
		bHasSelectedAsset = false;
		if (AssetInfoText.IsValid())
		{
			AssetInfoText->SetText(FMapWorkbenchText::Get(EWorkbenchText::AssetNone));
		}
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::AssetPickerNone));
		return;
	}
	if (Candidates.Num() > 1)
	{
		FFormatNamedArguments CountArgs;
		CountArgs.Add(TEXT("count"), FText::AsNumber(Candidates.Num()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::MultiAssetsFmt, CountArgs));
		return;
	}

	UObject* Candidate = Candidates[0];
	UStaticMesh* Mesh = nullptr;
	FText Reason;
	UClass* SpawnClass = ValidateAssetForPlacement(Candidate, Mesh, Reason);
	if (SpawnClass == nullptr)
	{
		bHasSelectedAsset = false;
		if (AssetInfoText.IsValid())
		{
			FFormatNamedArguments TagArgs;
			TagArgs.Add(TEXT("name"), FText::FromString(Candidate->GetName()));
			TagArgs.Add(TEXT("reason"), Reason);
			AssetInfoText->SetText(FMapWorkbenchText::Format(EWorkbenchText::AssetTagFailFmt, TagArgs));
		}
		FFormatNamedArguments FailArgs;
		FailArgs.Add(TEXT("reason"), Reason);
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::AssetValidateFailFmt, FailArgs));
		return;
	}

	SelectedAssetData = FAssetData(Candidate);
	bHasSelectedAsset = true;
	if (AssetInfoText.IsValid())
	{
		FFormatNamedArguments InfoArgs;
		InfoArgs.Add(TEXT("name"), FText::FromString(Candidate->GetName()));
		InfoArgs.Add(TEXT("kind"), Mesh != nullptr ? FMapWorkbenchText::Get(EWorkbenchText::KindStaticMesh) : FMapWorkbenchText::Get(EWorkbenchText::KindBlueprint));
		AssetInfoText->SetText(FMapWorkbenchText::Format(EWorkbenchText::AssetInfoFmt, InfoArgs));
	}
	FFormatNamedArguments PickedArgs;
	PickedArgs.Add(TEXT("name"), FText::FromString(Candidate->GetName()));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::AssetPickedFmt, PickedArgs));
}

void SMapWorkbench::HandlePlaceAssetAtSelectedCell()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	if (!SelectedCell.IsSet())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoCellSelected));
		return;
	}
	if (!bHasSelectedAsset || !SelectedAssetData.IsValid())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoAssetPicked));
		return;
	}
	UObject* AssetObject = SelectedAssetData.GetAsset();
	if (AssetObject == nullptr)
	{
		bHasSelectedAsset = false;
		if (AssetInfoText.IsValid())
		{
			AssetInfoText->SetText(FMapWorkbenchText::Get(EWorkbenchText::AssetUnloadedLabel));
		}
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::AssetUnloadedStatus));
		return;
	}

	UStaticMesh* Mesh = nullptr;
	FText ValidateReason;
	UClass* SpawnClass = ValidateAssetForPlacement(AssetObject, Mesh, ValidateReason);
	if (SpawnClass == nullptr)
	{
		UpdateStatus(ValidateReason);
		return;
	}

	const FIntPoint Cell = SelectedCell.GetValue();
	const FVector2D Anchor = CellAnchorWorldXY(Cell);
	const FString AssetName = AssetObject->GetName();

	FScopedTransaction Transaction(FMapWorkbenchText::Get(EWorkbenchText::TxPlaceActor));
	M->Modify();
	// 蓝图保持默认缩放（恒等变换、仅指定位置）
	const FTransform SpawnTransform(FVector(Anchor.X, Anchor.Y, M->GetActorLocation().Z));
	AActor* NewActor = GEditor->AddActor(M->GetLevel(), SpawnClass, SpawnTransform, /*bSilent*/ true, RF_Transactional, /*bSelectActor*/ false);
	if (NewActor == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::PlaceFailed));
		return;
	}
	if (Mesh != nullptr)
	{
		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(NewActor))
		{
			if (UStaticMeshComponent* MeshComponent = StaticMeshActor->GetStaticMeshComponent())
			{
				MeshComponent->Modify();
				MeshComponent->SetStaticMesh(Mesh);
			}
		}
	}
	NewActor->SetActorLabel(FString::Printf(TEXT("WB_%s_%d_%d"), *AssetName, Cell.X, Cell.Y));
	M->ToolCreatedActors.Add(NewActor);

	RefreshToolActorRows();
	RefreshPanel();
	FFormatNamedArguments PlaceArgs;
	PlaceArgs.Add(TEXT("actor"), FText::FromString(NewActor->GetActorLabel()));
	PlaceArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
	PlaceArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::PlaceOkFmt, PlaceArgs));
}

void SMapWorkbench::HandleDeleteSelectedToolActor()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	TArray<TSharedPtr<FToolActorRow>> SelectedRows;
	if (ToolActorList.IsValid())
	{
		ToolActorList->GetSelectedItems(SelectedRows);
	}
	if (SelectedRows.Num() == 0 || !SelectedRows[0].IsValid())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoRowSelected));
		return;
	}
	AActor* Victim = SelectedRows[0]->Actor.Get();
	if (Victim == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::RowActorGone));
		RefreshToolActorRows();
		return;
	}
	// 本 Map の追跡対象かつ同一レベルの Actor のみ削除を許可
	if (Victim->GetLevel() != M->GetLevel() || !M->ToolCreatedActors.Contains(Victim))
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NotTrackedActor));
		return;
	}

	const FString VictimLabel = Victim->GetActorLabel();
	UEditorActorSubsystem* ActorSubsystem = (GEditor != nullptr) ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (ActorSubsystem == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::DeleteNoSubsystem));
		return;
	}

	// 取引前の準備：本ツールのストロークを確定させてから、他者の活動中取引を拒否する
	// （誤ってより古い取引を Undo しないための保証）
	if (Canvas.IsValid() && Canvas->IsStrokeActive())
	{
		Canvas->ForceEndStroke();
	}
	if (GEditor != nullptr && GEditor->IsTransactionActive())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::OtherTxActive));
		return;
	}

	// 取引スコープ：Map を本取引の最初の Modify 対象とし、削除成功後のみ追跡参照を除去する。
	// Modify が記録に失敗した場合は Cancel して何も変更せず終了（Undo は行わない）。
	// 削除失敗時は取引を確定した直後に Undo して自動復元する（手動撤销に頼らない）。
	bool bDeleted = false;
	bool bModifyRecorded = false;
	{
		FScopedTransaction Transaction(FMapWorkbenchText::Get(EWorkbenchText::TxDeleteActor));
		bModifyRecorded = M->Modify();
		if (!bModifyRecorded)
		{
			Transaction.Cancel();
		}
		else
		{
			bDeleted = ActorSubsystem->DestroyActor(Victim);
			if (bDeleted)
			{
				M->ToolCreatedActors.Remove(Victim);
			}
		}
	}
	if (!bModifyRecorded)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::DeleteAbortedNoTx));
		return;
	}
	if (!bDeleted)
	{
		// 直前に確定した本取引のみを Undo して、Level・Actor・追跡配列を自動復元する
		const bool bUndone = (GEditor != nullptr) && GEditor->UndoTransaction();
		RefreshToolActorRows();
		RefreshPanel();
		FFormatNamedArguments FailArgs;
		FailArgs.Add(TEXT("actor"), FText::FromString(VictimLabel));
		UpdateStatus(bUndone
			? FMapWorkbenchText::Format(EWorkbenchText::DeleteFailedRolledBackFmt, FailArgs)
			: FMapWorkbenchText::Format(EWorkbenchText::DeleteRollbackFailedFmt, FailArgs));
		return;
	}
	RefreshToolActorRows();
	RefreshPanel();
	FFormatNamedArguments OkArgs;
	OkArgs.Add(TEXT("actor"), FText::FromString(VictimLabel));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::DeleteOkFmt, OkArgs));
}

void SMapWorkbench::HandleFocusToolActor(AActor* InActor)
{
	if (InActor == nullptr || GEditor == nullptr)
	{
		return;
	}
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(InActor, true, true);
	TArray<AActor*> FocusActors;
	FocusActors.Add(InActor);
	FEditorDelegates::OnFocusViewportOnActors.Broadcast(FocusActors);
	FFormatNamedArguments FocusArgs;
	FocusArgs.Add(TEXT("actor"), FText::FromString(InActor->GetActorLabel()));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::FocusOkFmt, FocusArgs));
}

void SMapWorkbench::RefreshToolActorRows()
{
	AMapManager* M = GetValidTarget();
	ToolActorRows.Reset();
	if (M != nullptr)
	{
		const FVector Origin = M->GetActorLocation();
		int32 ValidIndex = 0;
		for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
		{
			AActor* Actor = ActorPtr.Get();
			if (Actor == nullptr)
			{
				continue;
			}
			TSharedPtr<FToolActorRow> Row = MakeShared<FToolActorRow>();
			Row->Actor = Actor;
			Row->Label = Actor->GetActorLabel();
			Row->WorldLocation = Actor->GetActorLocation();
			const FVector Rel = Row->WorldLocation - Origin;
			Row->Cell = FIntPoint(
				FMath::FloorToInt32(Rel.X / M->TileSize + 0.5f),
				FMath::FloorToInt32(Rel.Y / M->TileSize + 0.5f));
			Row->bOutOfBounds = (Row->Cell.X < 0 || Row->Cell.X >= M->MapWidth || Row->Cell.Y < 0 || Row->Cell.Y >= M->MapHeight);
			Row->bOtherLevel = (Actor->GetLevel() != M->GetLevel());
			Row->Index = ValidIndex++;
			ToolActorRows.Add(Row);
		}
	}
	if (ToolActorList.IsValid())
	{
		ToolActorList->RebuildList();
	}
	if (Canvas.IsValid())
	{
		Canvas->InvalidateCanvas();
	}
}

//==============================================================================
// パネル実装：生成器
//==============================================================================

const FSpawnTileBinding* SMapWorkbench::FindBindingForSpawner(AAnimalSpawner* InSpawner) const
{
	AMapManager* M = GetValidTarget();
	if (M == nullptr || InSpawner == nullptr)
	{
		return nullptr;
	}
	return M->SpawnTileBindings.FindByPredicate([InSpawner](const FSpawnTileBinding& Binding)
	{
		return Binding.Spawner == InSpawner;
	});
}

const FSpawnTileBinding* SMapWorkbench::FindBindingAtCell(const FIntPoint& InCell) const
{
	AMapManager* M = GetValidTarget();
	if (M == nullptr)
	{
		return nullptr;
	}
	return M->SpawnTileBindings.FindByPredicate([InCell](const FSpawnTileBinding& Binding)
	{
		return Binding.GridCoords == InCell && Binding.Spawner != nullptr;
	});
}

void SMapWorkbench::HandlePickSpawnerFromSelection()
{
	if (GEditor == nullptr)
	{
		return;
	}
	AMapManager* M = GetValidTarget();
	if (M == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoTargetPickMap));
		return;
	}
	if (GEditor->PlayWorld != nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::PieNoRead));
		return;
	}
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AAnimalSpawner*> Spawners;
	if (SelectedActors != nullptr)
	{
		SelectedActors->GetSelectedObjects<AAnimalSpawner>(Spawners);
	}
	Spawners.RemoveAll([](AAnimalSpawner* Candidate)
	{
		UWorld* W = Candidate ? Candidate->GetWorld() : nullptr;
		UWorld* EditorWorld = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
		return (W == nullptr || EditorWorld == nullptr || W != EditorWorld || W->WorldType != EWorldType::Editor);
	});

	if (Spawners.Num() == 0)
	{
		SelectedSpawner.Reset();
		RefreshSpawnerInfo();
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoSpawnerSelected));
		return;
	}
	if (Spawners.Num() > 1)
	{
		FFormatNamedArguments CountArgs;
		CountArgs.Add(TEXT("count"), FText::AsNumber(Spawners.Num()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::MultiSpawnersFmt, CountArgs));
		return;
	}

	AAnimalSpawner* Candidate = Spawners[0];
	if (Candidate->GetLevel() != M->GetLevel())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::SpawnerCrossLevel));
		return;
	}
	SelectedSpawner = Candidate;
	RefreshSpawnerInfo();
	const FSpawnTileBinding* ExistingBinding = FindBindingForSpawner(Candidate);
	if (ExistingBinding != nullptr)
	{
		FFormatNamedArguments PickedArgs;
		PickedArgs.Add(TEXT("actor"), FText::FromString(Candidate->GetActorLabel()));
		PickedArgs.Add(TEXT("x"), FText::AsNumber(ExistingBinding->GridCoords.X));
		PickedArgs.Add(TEXT("y"), FText::AsNumber(ExistingBinding->GridCoords.Y));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::PickedSpawnerBoundFmt, PickedArgs));
	}
	else
	{
		FFormatNamedArguments PickedArgs;
		PickedArgs.Add(TEXT("actor"), FText::FromString(Candidate->GetActorLabel()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::PickedSpawnerUnboundFmt, PickedArgs));
	}
}

void SMapWorkbench::HandleBindSpawner()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	AAnimalSpawner* Spawner = SelectedSpawner.Get();
	if (Spawner == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NeedPickSpawner));
		return;
	}
	if (!SelectedCell.IsSet())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoCellSelected));
		return;
	}
	if (Spawner->GetLevel() != M->GetLevel())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::CrossLevelBindRejected));
		return;
	}
	if (FindBindingForSpawner(Spawner) != nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::AlreadyBoundUnbindOrMove));
		return;
	}
	const FIntPoint Cell = SelectedCell.GetValue();
	if (Cell.X < 0 || Cell.X >= M->MapWidth || Cell.Y < 0 || Cell.Y >= M->MapHeight)
	{
		FFormatNamedArguments OobArgs;
		OobArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
		OobArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::SelectedCellOobFmt, OobArgs));
		return;
	}
	if (FindBindingAtCell(Cell) != nullptr)
	{
		FFormatNamedArguments OtherArgs;
		OtherArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
		OtherArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::CellBoundByOtherFmt, OtherArgs));
		return;
	}
	const int32 TileIndex = Cell.Y * M->MapWidth + Cell.X;
	if (!M->GridData.IsValidIndex(TileIndex) || M->GridData[TileIndex].TileType != ETileType::Spawn)
	{
		FFormatNamedArguments NotSpawnArgs;
		NotSpawnArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
		NotSpawnArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::NotSpawnTileFmt, NotSpawnArgs));
		return;
	}

	// 高度オフセット＝スポナー生成位置Z − マス中心Z（SpawnRotation・Transform は変更しない）
	const float HeightOffset = Spawner->SpawnLocation.Z - M->GetActorLocation().Z;
	FScopedTransaction Transaction(FMapWorkbenchText::Get(EWorkbenchText::TxBindSpawn));
	if (!M->BindSpawnTile(Spawner, Cell, HeightOffset))
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::DataLayerBindFailed));
		return;
	}
	RefreshSpawnerInfo();
	RefreshCellInfo();
	RefreshPanel();
	FFormatNamedArguments BindArgs;
	BindArgs.Add(TEXT("actor"), FText::FromString(Spawner->GetActorLabel()));
	BindArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
	BindArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
	BindArgs.Add(TEXT("offset"), FText::FromString(FString::Printf(TEXT("%.1f"), HeightOffset)));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::BindOkFmt, BindArgs));
}

void SMapWorkbench::HandleUnbindSpawner()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	AAnimalSpawner* Spawner = SelectedSpawner.Get();
	if (Spawner == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NeedPickSpawner));
		return;
	}
	if (FindBindingForSpawner(Spawner) == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoBinding));
		return;
	}
	FScopedTransaction Transaction(FMapWorkbenchText::Get(EWorkbenchText::TxUnbindSpawn));
	if (!M->UnbindSpawnTile(Spawner))
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::DataLayerUnbindFailed));
		return;
	}
	RefreshSpawnerInfo();
	RefreshCellInfo();
	RefreshPanel();
	FFormatNamedArguments UnbindArgs;
	UnbindArgs.Add(TEXT("actor"), FText::FromString(Spawner->GetActorLabel()));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::UnbindOkFmt, UnbindArgs));
}

void SMapWorkbench::HandleMoveSpawnerSpawnTile()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	AAnimalSpawner* Spawner = SelectedSpawner.Get();
	if (Spawner == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NeedPickSpawner));
		return;
	}
	if (FindBindingForSpawner(Spawner) == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::MoveNeedBinding));
		return;
	}
	if (!SelectedCell.IsSet())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoCellSelected));
		return;
	}
	const FIntPoint TargetCell = SelectedCell.GetValue();
	if (TargetCell.X < 0 || TargetCell.X >= M->MapWidth || TargetCell.Y < 0 || TargetCell.Y >= M->MapHeight)
	{
		FFormatNamedArguments OobArgs;
		OobArgs.Add(TEXT("x"), FText::AsNumber(TargetCell.X));
		OobArgs.Add(TEXT("y"), FText::AsNumber(TargetCell.Y));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::SelectedCellOobFmt, OobArgs));
		return;
	}
	const FSpawnTileBinding* OtherBinding = FindBindingAtCell(TargetCell);
	if (OtherBinding != nullptr && OtherBinding->Spawner != Spawner)
	{
		FFormatNamedArguments OtherArgs;
		OtherArgs.Add(TEXT("x"), FText::AsNumber(TargetCell.X));
		OtherArgs.Add(TEXT("y"), FText::AsNumber(TargetCell.Y));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::CellBoundByOtherFmt, OtherArgs));
		return;
	}
	const int32 TileIndex = TargetCell.Y * M->MapWidth + TargetCell.X;
	if (M->GridData.IsValidIndex(TileIndex))
	{
		const ETileType CurrentType = M->GridData[TileIndex].TileType;
		if (CurrentType != ETileType::Empty && CurrentType != ETileType::Spawn)
		{
			FFormatNamedArguments OccupiedArgs;
			OccupiedArgs.Add(TEXT("x"), FText::AsNumber(TargetCell.X));
			OccupiedArgs.Add(TEXT("y"), FText::AsNumber(TargetCell.Y));
			OccupiedArgs.Add(TEXT("type"), MapWorkbenchStyle::GetTileLabel(CurrentType));
			UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::TargetTileOccupiedFmt, OccupiedArgs));
			return;
		}
	}

	FScopedTransaction Transaction(FMapWorkbenchText::Get(EWorkbenchText::TxMoveSpawn));
	if (!M->MoveSpawnTile(Spawner, TargetCell))
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::MoveDataLayerFailed));
		return;
	}
	RefreshSpawnerInfo();
	RefreshCellInfo();
	RefreshPanel();
	FFormatNamedArguments MoveArgs;
	MoveArgs.Add(TEXT("actor"), FText::FromString(Spawner->GetActorLabel()));
	MoveArgs.Add(TEXT("x"), FText::AsNumber(TargetCell.X));
	MoveArgs.Add(TEXT("y"), FText::AsNumber(TargetCell.Y));
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::MoveOkFmt, MoveArgs));
}

void SMapWorkbench::RefreshSpawnerInfo()
{
	FString Info = FMapWorkbenchText::Get(EWorkbenchText::SpawnerNone).ToString();
	AAnimalSpawner* Spawner = SelectedSpawner.Get();
	if (Spawner != nullptr)
	{
		const FSpawnTileBinding* Binding = FindBindingForSpawner(Spawner);
		if (Binding != nullptr)
		{
			FFormatNamedArguments BoundArgs;
			BoundArgs.Add(TEXT("actor"), FText::FromString(Spawner->GetActorLabel()));
			BoundArgs.Add(TEXT("x"), FText::AsNumber(Binding->GridCoords.X));
			BoundArgs.Add(TEXT("y"), FText::AsNumber(Binding->GridCoords.Y));
			BoundArgs.Add(TEXT("offset"), FText::FromString(FString::Printf(TEXT("%.1f"), Binding->HeightOffset)));
			Info = FMapWorkbenchText::Format(EWorkbenchText::SpawnerInfoBoundFmt, BoundArgs).ToString();
		}
		else
		{
			FFormatNamedArguments UnboundArgs;
			UnboundArgs.Add(TEXT("actor"), FText::FromString(Spawner->GetActorLabel()));
			Info = FMapWorkbenchText::Format(EWorkbenchText::SpawnerInfoUnboundFmt, UnboundArgs).ToString();
		}
	}
	if (SpawnerInfoText.IsValid())
	{
		SpawnerInfoText->SetText(FText::FromString(Info));
	}
}

//==============================================================================
// パネル実装：盤面パラメータ
//==============================================================================

bool SMapWorkbench::BuildShrinkConflictReport(int32 NewWidth, int32 NewHeight, float NewTileSize, FText& OutReport) const
{
	AMapManager* M = GetValidTarget();
	if (M == nullptr || NewTileSize <= 0.0f || !FMath::IsFinite(NewTileSize))
	{
		return false;
	}

	TArray<FString> TileLines;
	TArray<FString> ActorLines;
	TArray<FString> BindingLines;

	for (int32 i = 0; i < M->GridData.Num(); ++i)
	{
		const int32 X = i % M->MapWidth;
		const int32 Y = i / M->MapWidth;
		if ((X >= NewWidth || Y >= NewHeight) && M->GridData[i].TileType != ETileType::Empty)
		{
			FFormatNamedArguments LineArgs;
			LineArgs.Add(TEXT("x"), FText::AsNumber(X));
			LineArgs.Add(TEXT("y"), FText::AsNumber(Y));
			LineArgs.Add(TEXT("type"), MapWorkbenchStyle::GetTileLabel(M->GridData[i].TileType));
			TileLines.Add(FMapWorkbenchText::Format(EWorkbenchText::ReportTileLineFmt, LineArgs).ToString());
		}
	}
	for (const FSpawnTileBinding& Binding : M->SpawnTileBindings)
	{
		if (Binding.Spawner != nullptr && (Binding.GridCoords.X >= NewWidth || Binding.GridCoords.Y >= NewHeight))
		{
			FFormatNamedArguments LineArgs;
			LineArgs.Add(TEXT("actor"), FText::FromString(Binding.Spawner->GetActorLabel()));
			LineArgs.Add(TEXT("x"), FText::AsNumber(Binding.GridCoords.X));
			LineArgs.Add(TEXT("y"), FText::AsNumber(Binding.GridCoords.Y));
			BindingLines.Add(FMapWorkbenchText::Format(EWorkbenchText::ReportBindingLineFmt, LineArgs).ToString());
		}
	}
	{
		// 投影は「待機適用の NewTileSize」基準（縮小後の格子寸法で実位置を判定）。負座標も越界扱い。
		const FVector Origin = M->GetActorLocation();
		for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
		{
			AActor* Actor = ActorPtr.Get();
			if (Actor == nullptr || Actor->GetLevel() != M->GetLevel())
			{
				continue;
			}
			const FVector Rel = Actor->GetActorLocation() - Origin;
			const FIntPoint Cell(
				FMath::FloorToInt32(Rel.X / NewTileSize + 0.5f),
				FMath::FloorToInt32(Rel.Y / NewTileSize + 0.5f));
			if (Cell.X < 0 || Cell.X >= NewWidth || Cell.Y < 0 || Cell.Y >= NewHeight)
			{
				FFormatNamedArguments LineArgs;
				LineArgs.Add(TEXT("actor"), FText::FromString(Actor->GetActorLabel()));
				LineArgs.Add(TEXT("x"), FText::AsNumber(Cell.X));
				LineArgs.Add(TEXT("y"), FText::AsNumber(Cell.Y));
				ActorLines.Add(FMapWorkbenchText::Format(EWorkbenchText::ReportActorLineFmt, LineArgs).ToString());
			}
		}
	}

	if (TileLines.Num() == 0 && ActorLines.Num() == 0 && BindingLines.Num() == 0)
	{
		return false;
	}

	auto AppendLines = [](FString& Out, const TArray<FString>& Lines, int32 MaxLines)
	{
		for (int32 i = 0; i < Lines.Num() && i < MaxLines; ++i)
		{
			Out += Lines[i];
			Out += TEXT("\n");
		}
		if (Lines.Num() > MaxLines)
		{
			FFormatNamedArguments MoreArgs;
			MoreArgs.Add(TEXT("count"), FText::AsNumber(Lines.Num()));
			Out += FMapWorkbenchText::Format(EWorkbenchText::ReportMoreFmt, MoreArgs).ToString();
		}
	};

	FFormatNamedArguments HeadArgs;
	HeadArgs.Add(TEXT("w"), FText::AsNumber(NewWidth));
	HeadArgs.Add(TEXT("h"), FText::AsNumber(NewHeight));
	HeadArgs.Add(TEXT("tiles"), FText::AsNumber(TileLines.Num()));
	FString Report = FMapWorkbenchText::Format(EWorkbenchText::ShrinkReportHeadFmt, HeadArgs).ToString();
	AppendLines(Report, TileLines, 24);
	FFormatNamedArguments ActorSegArgs;
	ActorSegArgs.Add(TEXT("actors"), FText::AsNumber(ActorLines.Num()));
	Report += FMapWorkbenchText::Format(EWorkbenchText::ShrinkReportActorsFmt, ActorSegArgs).ToString();
	AppendLines(Report, ActorLines, 24);
	FFormatNamedArguments BindSegArgs;
	BindSegArgs.Add(TEXT("bindings"), FText::AsNumber(BindingLines.Num()));
	Report += FMapWorkbenchText::Format(EWorkbenchText::ShrinkReportBindingsFmt, BindSegArgs).ToString();
	AppendLines(Report, BindingLines, 24);
	Report += FMapWorkbenchText::Get(EWorkbenchText::ShrinkReportKeep).ToString();
	Report += FMapWorkbenchText::Get(EWorkbenchText::ShrinkReportRemove).ToString();
	Report += FMapWorkbenchText::Get(EWorkbenchText::ShrinkReportCancel).ToString();
	OutReport = FText::FromString(Report);
	return true;
}

void SMapWorkbench::HandleApplyParams()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	if (!PendingWidthBox.IsValid() || !PendingHeightBox.IsValid() || !PendingTileSizeBox.IsValid())
	{
		return;
	}
	const int32 NewWidth = PendingWidthBox->GetValue();
	const int32 NewHeight = PendingHeightBox->GetValue();
	const float NewTileSize = PendingTileSizeBox->GetValue();
	if (NewWidth < 1 || NewWidth > AMapManager::MaxGridDimension || NewHeight < 1 || NewHeight > AMapManager::MaxGridDimension)
	{
		FFormatNamedArguments InvalidArgs;
		InvalidArgs.Add(TEXT("w"), FText::AsNumber(NewWidth));
		InvalidArgs.Add(TEXT("h"), FText::AsNumber(NewHeight));
		InvalidArgs.Add(TEXT("max"), FText::AsNumber(AMapManager::MaxGridDimension));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::InvalidWHFmt, InvalidArgs));
		return;
	}
	if (!FMath::IsFinite(NewTileSize) || NewTileSize <= 0.0f)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::InvalidTileSize));
		return;
	}

	const bool bSameSize = (NewWidth == M->MapWidth && NewHeight == M->MapHeight);
	const bool bSameTileSize = (NewTileSize == M->TileSize);
	if (bSameSize && bSameTileSize)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::NoChange));
		return;
	}

	const bool bShrink = (NewWidth < M->MapWidth || NewHeight < M->MapHeight);
	bool bKeepOutOfBounds = true;
	if (bShrink)
	{
		FText Report;
		if (BuildShrinkConflictReport(NewWidth, NewHeight, NewTileSize, Report))
		{
			const int32 Choice = ShowShrinkDialog(AsShared(), Report);
			if (Choice == 2)
			{
				UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::CancelledNoChange));
				return;
			}
			bKeepOutOfBounds = (Choice == 0);
		}
	}

	UEditorActorSubsystem* ActorSubsystem = (GEditor != nullptr) ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;

	// 取引前の準備：本ツールのストロークを確定させてから、他者の活動中取引を拒否する
	// （誤ってより古い取引を Undo しないための保証）
	if (Canvas.IsValid() && Canvas->IsStrokeActive())
	{
		Canvas->ForceEndStroke();
	}
	if (GEditor != nullptr && GEditor->IsTransactionActive())
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::OtherTxActive));
		return;
	}

	// 取引スコープ：Map を本取引の最初の Modify 対象とする。
	// Modify が記録に失敗した場合は Cancel して何も変更せず終了（Undo は行わない）。
	// どのステップが失敗しても、取引を確定した直後に Undo して本取引のみを自動ロールバックする
	// （部分状態を残さない。Undo 対象は直後に確定した本取引がスタック先頭のため、より古い取引には触れない）。
	bool bAllOk = false;
	bool bResizeRejected = false;
	bool bModifyRecorded = false;
	{
		FScopedTransaction Transaction(FMapWorkbenchText::Get(EWorkbenchText::TxApplyParams));
		bModifyRecorded = M->Modify();
		if (!bModifyRecorded)
		{
			Transaction.Cancel();
		}
		else
		{
			bool bStepFailed = false;
			// 移除分支：先删本工具跟踪的同 Level 越界物件（待機適用 NewTileSize 基準・負座標も越界），成功才移除跟踪项
			if (bShrink && !bKeepOutOfBounds)
			{
				TArray<AActor*> Victims;
				{
					const FVector Origin = M->GetActorLocation();
					for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
					{
						AActor* Actor = ActorPtr.Get();
						if (Actor == nullptr || Actor->GetLevel() != M->GetLevel() || !M->ToolCreatedActors.Contains(Actor))
						{
							continue;
						}
						const FVector Rel = Actor->GetActorLocation() - Origin;
						const FIntPoint Cell(
							FMath::FloorToInt32(Rel.X / NewTileSize + 0.5f),
							FMath::FloorToInt32(Rel.Y / NewTileSize + 0.5f));
						if (Cell.X < 0 || Cell.X >= NewWidth || Cell.Y < 0 || Cell.Y >= NewHeight)
						{
							Victims.Add(Actor);
						}
					}
				}
				// ActorSubsystem が取得できない場合は削除に入る前に失敗扱い（nullptr 解参照を避ける）
				if (ActorSubsystem == nullptr)
				{
					bStepFailed = true;
				}
				for (AActor* Victim : Victims)
				{
					if (!ActorSubsystem->DestroyActor(Victim))
					{
						bStepFailed = true;
						break;
					}
					M->ToolCreatedActors.Remove(Victim);
				}
			}
			// 越界生成器绑定的清理（负坐标同样按越界处理）
			if (!bStepFailed && bShrink && !bKeepOutOfBounds)
			{
				for (int32 i = M->SpawnTileBindings.Num() - 1; i >= 0; --i)
				{
					const FSpawnTileBinding& Binding = M->SpawnTileBindings[i];
					if (Binding.Spawner != nullptr && (Binding.GridCoords.X >= NewWidth || Binding.GridCoords.Y >= NewHeight))
					{
						if (!M->UnbindSpawnTile(Binding.Spawner))
						{
							bStepFailed = true;
							break;
						}
					}
				}
			}
			// 順序：先 Resize → 次に TileSize（SavedTileSize 同期） → 最後に Rebuild 同期（生成位置を含む）
			if (!bStepFailed && !bSameSize)
			{
				if (!M->ResizeGridPreservingLayout(NewWidth, NewHeight, bKeepOutOfBounds))
				{
					bResizeRejected = true;
					bStepFailed = true;
				}
			}
			if (!bStepFailed && !bSameTileSize)
			{
				M->Modify();
				M->TileSize = NewTileSize;
				M->SavedTileSize = NewTileSize;
			}
			if (!bStepFailed)
			{
				M->RebuildFloorAndPositions(true);
				bAllOk = true;
			}
		}
	}
	if (!bModifyRecorded)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::ApplyAbortedNoTx));
		return;
	}
	if (!bAllOk)
	{
		// 直前に確定した本取引のみを Undo して自動復元する（Undo 成否を検査する）
		const bool bUndone = (GEditor != nullptr) && GEditor->UndoTransaction();
		RefreshPanel();
		UpdateStatus(bUndone
			? (bResizeRejected
				? FMapWorkbenchText::Get(EWorkbenchText::ResizeRejectedRolledBack)
				: FMapWorkbenchText::Get(EWorkbenchText::ApplyFailedRolledBack))
			: FMapWorkbenchText::Get(EWorkbenchText::ApplyRollbackFailed));
		return;
	}

	if (Canvas.IsValid())
	{
		Canvas->RequestFitView();
	}
	RefreshPanel();
	FFormatNamedArguments ApplyArgs;
	ApplyArgs.Add(TEXT("w"), FText::AsNumber(NewWidth));
	ApplyArgs.Add(TEXT("h"), FText::AsNumber(NewHeight));
	ApplyArgs.Add(TEXT("ts"), FText::FromString(FString::Printf(TEXT("%.1f"), NewTileSize)));
	ApplyArgs.Add(TEXT("suffix"), bShrink
		? FMapWorkbenchText::Get(bKeepOutOfBounds ? EWorkbenchText::KeepOobSuffix : EWorkbenchText::RemoveOobSuffix)
		: FText::GetEmpty());
	UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::ApplyOkFmt, ApplyArgs));
}

//==============================================================================
// パネル実装：保存・取り消し
//==============================================================================

void SMapWorkbench::HandleSaveTargetLevel()
{
	FText Reason;
	AMapManager* M = GetValidTarget(&Reason);
	if (M == nullptr)
	{
		UpdateStatus(Reason);
		return;
	}
	if (!CanEditTarget(&Reason))
	{
		UpdateStatus(Reason);
		return;
	}
	ULevel* TargetLevel = M->GetLevel();
	UPackage* LevelPackage = TargetLevel ? TargetLevel->GetOutermost() : nullptr;
	if (LevelPackage == nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::SaveNoLevelPkg));
		return;
	}

	TArray<UPackage*> Packages;
	auto AddPackage = [&Packages](UPackage* InPackage)
	{
		if (InPackage != nullptr && !Packages.Contains(InPackage))
		{
			Packages.Add(InPackage);
		}
	};
	AddPackage(LevelPackage);
	// MapManager 自身のパッケージ（通常は Level パッケージと同一だが明示的に含める）
	AddPackage(M->GetOutermost());
	// 同一 Level に属する追跡アクター／バインド済みスポナーの外部パッケージのみ（他 Level・全工程は保存しない）
	auto AddRelatedActorPackage = [TargetLevel, &AddPackage](AActor* Actor)
	{
		if (Actor == nullptr || Actor->GetLevel() != TargetLevel)
		{
			return;
		}
		AddPackage(Actor->GetOutermost());
	};
	for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
	{
		AddRelatedActorPackage(ActorPtr.Get());
	}
	for (const FSpawnTileBinding& Binding : M->SpawnTileBindings)
	{
		AddRelatedActorPackage(Binding.Spawner.Get());
	}

	bool bAnyDirty = false;
	for (UPackage* Package : Packages)
	{
		if (Package->IsDirty())
		{
			bAnyDirty = true;
			break;
		}
	}
	if (!bAnyDirty)
	{
		FFormatNamedArguments DirtyArgs;
		DirtyArgs.Add(TEXT("pkg"), FText::FromString(LevelPackage->GetName()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::SaveNothingDirtyFmt, DirtyArgs));
		return;
	}

	TArray<UPackage*> FailedPackages;
	const FEditorFileUtils::EPromptReturnCode Result = FEditorFileUtils::PromptForCheckoutAndSave(
		Packages, /*bCheckDirty*/ true, /*bPromptToSave*/ false,
		FMapWorkbenchText::Get(EWorkbenchText::BtnSaveLevel),
		FMapWorkbenchText::Get(EWorkbenchText::BtnSaveLevelTooltip),
		&FailedPackages, /*bAlreadyCheckedOut*/ false, /*bCanBeDeclined*/ true);

	if (Result == FEditorFileUtils::PR_Success && FailedPackages.Num() == 0)
	{
		FFormatNamedArguments OkArgs;
		OkArgs.Add(TEXT("pkg"), FText::FromString(LevelPackage->GetName()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::SaveOkFmt, OkArgs));
	}
	else if (FailedPackages.Num() > 0)
	{
		FFormatNamedArguments FailArgs;
		FailArgs.Add(TEXT("count"), FText::AsNumber(FailedPackages.Num()));
		UpdateStatus(FMapWorkbenchText::Format(EWorkbenchText::SaveFailedFmt, FailArgs));
	}
	else
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::SaveCancelled));
	}
}

void SMapWorkbench::HandleUndo()
{
	if (GEditor == nullptr)
	{
		return;
	}
	if (GEditor->PlayWorld != nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::PieNoUndo));
		return;
	}
	if (Canvas.IsValid() && Canvas->IsStrokeActive())
	{
		Canvas->ForceEndStroke();
	}
	NotifyStrokeCommitted();
	GEditor->UndoTransaction();
	RefreshPanel();
	UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::UndoDone));
}

void SMapWorkbench::HandleRedo()
{
	if (GEditor == nullptr)
	{
		return;
	}
	if (GEditor->PlayWorld != nullptr)
	{
		UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::PieNoUndo));
		return;
	}
	if (Canvas.IsValid() && Canvas->IsStrokeActive())
	{
		Canvas->ForceEndStroke();
	}
	NotifyStrokeCommitted();
	GEditor->RedoTransaction();
	RefreshPanel();
	UpdateStatus(FMapWorkbenchText::Get(EWorkbenchText::RedoDone));
}

//==============================================================================
// パネル実装：表示更新
//==============================================================================

void SMapWorkbench::UpdateStatus(const FText& InMessage)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(InMessage);
	}
}

void SMapWorkbench::RefreshPanel()
{
	FString TargetInfo = FMapWorkbenchText::Get(EWorkbenchText::TargetNone).ToString();
	AMapManager* M = BoundMap.Get();
	if (M != nullptr)
	{
		const FString LevelName = (M->GetLevel() && M->GetLevel()->GetOutermost()) ? M->GetLevel()->GetOutermost()->GetName() : FMapWorkbenchText::Get(EWorkbenchText::TileUnknown).ToString();
		FFormatNamedArguments TargetArgs;
		TargetArgs.Add(TEXT("actor"), FText::FromString(M->GetActorLabel()));
		TargetArgs.Add(TEXT("level"), FText::FromString(LevelName));
		TargetInfo = FMapWorkbenchText::Format(EWorkbenchText::BoundOkFmt, TargetArgs).ToString();
	}
	if (TargetInfoText.IsValid())
	{
		TargetInfoText->SetText(FText::FromString(TargetInfo));
	}
	RefreshCellInfo();
	RefreshSpawnerInfo();
	RefreshToolActorRows();
	if (Canvas.IsValid())
	{
		Canvas->InvalidateCanvas();
	}
}

uint32 SMapWorkbench::ComputeSignature() const
{
	uint32 Hash = 2166136261u;
	auto Mix = [&Hash](uint32 Value)
	{
		Hash ^= Value;
		Hash *= 16777619u;
	};

	AMapManager* M = BoundMap.Get();
	Mix(M != nullptr ? 1u : 0u);
	if (M == nullptr)
	{
		return Hash;
	}

	Mix(::GetTypeHash(M->MapWidth));
	Mix(::GetTypeHash(M->MapHeight));
	Mix(MapWorkbenchFloatBits(M->TileSize));
	Mix(::GetTypeHash(M->GridData.Num()));
	for (const FMapTileData& Tile : M->GridData)
	{
		Mix(static_cast<uint32>(Tile.TileType));
		Mix(static_cast<uint32>(Tile.OwnerPlayerId));
	}
	Mix(::GetTypeHash(M->SpawnTileBindings.Num()));
	for (const FSpawnTileBinding& Binding : M->SpawnTileBindings)
	{
		// FIntPoint は ::GetTypeHash で ADL 前進しないため軸別のスカラー hash を使う
		Mix(::GetTypeHash(static_cast<int32>(Binding.GridCoords.X)));
		Mix(::GetTypeHash(static_cast<int32>(Binding.GridCoords.Y)));
		Mix(MapWorkbenchFloatBits(Binding.HeightOffset));
		Mix(::GetTypeHash(Binding.Spawner.Get()));
	}
	Mix(::GetTypeHash(M->ToolCreatedActors.Num()));
	{
		const FVector Origin = M->GetActorLocation();
		for (const TObjectPtr<AActor>& ActorPtr : M->ToolCreatedActors)
		{
			AActor* Actor = ActorPtr.Get();
			if (Actor == nullptr)
			{
				Mix(0u);
				continue;
			}
			Mix(::GetTypeHash(Actor));
			const FVector Location = Actor->GetActorLocation();
			Mix(::GetTypeHash(FMath::RoundToInt32((Location.X - Origin.X) * 10.0f)));
			Mix(::GetTypeHash(FMath::RoundToInt32((Location.Y - Origin.Y) * 10.0f)));
			Mix(::GetTypeHash(FMath::RoundToInt32((Location.Z - Origin.Z) * 10.0f)));
		}
	}
	Mix(::GetTypeHash(M->OutOfBoundsTiles.Num()));
	for (const FOutOfBoundsTileRecord& Record : M->OutOfBoundsTiles)
	{
		Mix(::GetTypeHash(static_cast<int32>(Record.GridCoords.X)));
		Mix(::GetTypeHash(static_cast<int32>(Record.GridCoords.Y)));
		Mix(static_cast<uint32>(Record.TileData.TileType));
	}
	return Hash;
}

bool SMapWorkbench::PollExternalChanges()
{
	const uint32 Signature = ComputeSignature();
	if (!bHasSignature)
	{
		LastSignature = Signature;
		bHasSignature = true;
		return false;
	}
	if (Signature == LastSignature)
	{
		return false;
	}
	LastSignature = Signature;

	// 盤面設定の変化はキャンバス再フィットを要求
	AMapManager* M = BoundMap.Get();
	if (M != nullptr && (M->MapWidth != LastFitWidth || M->MapHeight != LastFitHeight || M->TileSize != LastFitTileSize))
	{
		LastFitWidth = M->MapWidth;
		LastFitHeight = M->MapHeight;
		LastFitTileSize = M->TileSize;
		if (Canvas.IsValid())
		{
			Canvas->RequestFitView();
		}
	}
	return true;
}

//==============================================================================
// パネル実装：ライフサイクル
//==============================================================================

void SMapWorkbench::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	// 毎Tick：PIE/SIE・目標失効・レベルロックでは進行中ストロークを即時確定する（節流の影響を受けない）
	if (Canvas.IsValid() && Canvas->IsStrokeActive())
	{
		bool bMustEndStroke = false;
		if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
		{
			bMustEndStroke = true;
		}
		else if (BoundMap.IsValid())
		{
			AMapManager* BoundTarget = BoundMap.Get();
			if (BoundTarget == nullptr)
			{
				bMustEndStroke = true;
			}
			else
			{
				UWorld* TargetWorld = BoundTarget->GetWorld();
				UWorld* EditorWorld = (GEditor != nullptr) ? GEditor->GetEditorWorldContext().World() : nullptr;
				if (TargetWorld == nullptr || EditorWorld == nullptr || TargetWorld != EditorWorld || FLevelUtils::IsLevelLocked(BoundTarget))
				{
					bMustEndStroke = true;
				}
			}
		}
		if (bMustEndStroke)
		{
			Canvas->ForceEndStroke();
		}
	}

	// 低頻度ポーリング：リスト再構築・署名差分は節流する（毎フレームの HISM 再構築はしない）
	RefreshAccum += InDeltaTime;
	if (RefreshAccum < 0.4f)
	{
		return;
	}
	RefreshAccum = 0.0f;

	ForceUnbindIfNeeded();

	if (PollExternalChanges())
	{
		RefreshPanel();
	}
}

void SMapWorkbench::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshPanel();
	}
}

void SMapWorkbench::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshPanel();
	}
}

// 2026.10.22 Lee end（マップ工作台パネル実装一式）


// 2026.10.23 Lee start（元コードの集中留存）
/**
 * @brief 言語対応改修で置換された元コードのコメント留存。
 *        有効なコードではない。
 */
//	/** @brief タイル種別ごとの中国語表示名を返す。 */
//		case ETileType::Empty:   return FText::FromString(TEXT("空地"));
//		case ETileType::DirUp:   return FText::FromString(TEXT("上箭头 (+Y)"));
//		case ETileType::DirDown: return FText::FromString(TEXT("下箭头 (−Y)"));
//		case ETileType::DirLeft: return FText::FromString(TEXT("左箭头 (+X)"));
//		case ETileType::DirRight:return FText::FromString(TEXT("右箭头 (−X)"));
//		case ETileType::Spawn:   return FText::FromString(TEXT("出生点"));
//		case ETileType::GoalP1:  return FText::FromString(TEXT("1P 终点"));
//		case ETileType::GoalP2:  return FText::FromString(TEXT("2P 终点"));
//		default:                 return FText::FromString(TEXT("未知"));
//			TEXT("未绑定 MapManager：在关卡中选中一个 MapManager，然后点击『绑定选中的MapManager』"));
//		ReadOnlyReason = TEXT("PIE/SIE 运行中，禁止编辑");
//		ReadOnlyReason = TEXT("目标所在关卡已锁定，禁止编辑");
//				FString HoverInfo = FString::Printf(TEXT("格 (%d, %d)  %s"), HoverCell->X, HoverCell->Y, *MapWorkbenchStyle::GetTileLabel(M->GridData[Index].TileType).ToString());
//					HoverInfo += TEXT("  [已关联生成器]");
//	const FString Line1 = TEXT("图例：上笔刷 = +Y（画布向上）    下笔刷 = −Y（画布向下）    左笔刷 = +X（画布向右）    右笔刷 = −X（画布向左）");
//	const FString Line2 = TEXT("轴：画布 X 向右 = 世界 +X，画布 Y 向上 = 世界 +Y（箭头按世界方向绘制）");
//			Context->ReportStatus(FText::FromString(TEXT("未绑定 MapManager：请先绑定目标")));
//	// ---- 筆肋ボタン生成ヘルパ（ラジオ風チェックボックス） ----
//	auto MakeBrushCheck = [this](EWorkbenchBrush InBrush, const FString& InLabel) -> TSharedRef<SWidget>
//				SNew(STextBlock).Text(FText::FromString(InLabel))
//	auto MakeLegendEntry = [MakeColorSwatch](ETileType InType, const FString& InLabel) -> TSharedRef<SWidget>
//				SNew(STextBlock).Text(FText::FromString(InLabel))
//				.Text(FText::FromString(TEXT("绑定选中的MapManager")))
//				.ToolTipText(FText::FromString(TEXT("在关卡中恰好选中一个 MapManager 后点击绑定；无选中或多选将被拒绝")))
//				.Text(FText::FromString(TEXT("解除绑定")))
//				.Text(FText::FromString(TEXT("目标：未绑定")))
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Select,  TEXT("选择坐标")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Empty,   TEXT("清除")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirUp,   TEXT("上 (+Y)")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirDown, TEXT("下 (−Y)")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirLeft, TEXT("左 (+X)")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::DirRight,TEXT("右 (−X)")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::Spawn,   TEXT("出生点")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::GoalP1,  TEXT("1P 终点")) ]
//			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)[ MakeBrushCheck(EWorkbenchBrush::GoalP2,  TEXT("2P 终点")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::Empty,    TEXT("空地")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirUp,    TEXT("上 +Y")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirDown,  TEXT("下 −Y")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirLeft,  TEXT("左 +X")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::DirRight, TEXT("右 −X")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::Spawn,    TEXT("出生点")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::GoalP1,   TEXT("1P 终点")) ]
//			+ SHorizontalBox::Slot().AutoWidth()[ MakeLegendEntry(ETileType::GoalP2,   TEXT("2P 终点")) ]
//				.Text(FText::FromString(TEXT("画布方向：X 向右 = 世界 +X，Y 向上 = 世界 +Y")))
//			.Text(FText::FromString(TEXT("所选格：未选择（用『选择坐标』笔刷点击画布）")))
//					.Text(FText::FromString(TEXT("工具物件（仅跟踪工作台创建的 Actor）")))
//						.Text(FText::FromString(TEXT("读取内容浏览器选中素材")))
//						.ToolTipText(FText::FromString(TEXT("读取内容浏览器当前选中的静态网格体或可放置 Actor 蓝图并校验")))
//						.Text(FText::FromString(TEXT("放置到所选格")))
//						.ToolTipText(FText::FromString(TEXT("将已读取的素材放置到所选格（同格可多个；保留蓝图默认缩放）")))
//						.Text(FText::FromString(TEXT("删除选中列表物件")))
//						.ToolTipText(FText::FromString(TEXT("删除列表中选中的工作台物件（事务内删除并移除跟踪）")))
//						.Text(FText::FromString(TEXT("素材：未读取")))
//								.Text_Lambda([InRow]()
//									FString Desc = FString::Printf(TEXT("%s  |  位置 XYZ(%.0f, %.0f, %.0f)  |  投影格 (%d, %d)%s%s"),
//										*InRow->Label,
//										InRow->WorldLocation.X, InRow->WorldLocation.Y, InRow->WorldLocation.Z,
//										InRow->Cell.X, InRow->Cell.Y,
//										InRow->bOutOfBounds ? TEXT("  [越界]") : TEXT(""),
//										InRow->bOtherLevel ? TEXT("  [其他Level]") : TEXT(""));
//									return FText::FromString(Desc);
//								.Text(FText::FromString(TEXT("选中并聚焦")))
//					.Text(FText::FromString(TEXT("同格可放置多个物件；放置后可自由移动/旋转/缩放，工具按实际世界位置显示，不自动吸附。")))
//					.Text(FText::FromString(TEXT("生成器（AnimalSpawner 出生格关联）")))
//						.Text(FText::FromString(TEXT("读取选中的AnimalSpawner")))
//						.ToolTipText(FText::FromString(TEXT("读取关卡中恰好选中的一个 AnimalSpawner；未选或多选将被拒绝")))
//						.Text(FText::FromString(TEXT("绑定出生点到所选格")))
//						.ToolTipText(FText::FromString(TEXT("将已读取的生成器关联到所选格（该格必须是出生点类型）")))
//						.Text(FText::FromString(TEXT("移动绑定出生点到所选格")))
//						.ToolTipText(FText::FromString(TEXT("将已关联生成器的出生格移到所选格：旧格置空、新格置为出生点，并同步生成器位置")))
//						.Text(FText::FromString(TEXT("解除绑定")))
//						.ToolTipText(FText::FromString(TEXT("解除已读取生成器的出生格关联（生成器本身不移动）")))
//					.Text(FText::FromString(TEXT("生成器：未读取")))
//					.Text(FText::FromString(TEXT("地图参数（待应用值，点击应用才修改）")))
//						SNew(STextBlock).Text(FText::FromString(TEXT("宽")))
//						SNew(STextBlock).Text(FText::FromString(TEXT("高")))
//						SNew(STextBlock).Text(FText::FromString(TEXT("TileSize")))
//						.Text(FText::FromString(TEXT("应用参数")))
//						.ToolTipText(FText::FromString(TEXT("按先Resize后TileSize的顺序应用（一个事务）；缩图时将列出越界数据供选择保留或移除")))
//						.Text(FText::FromString(TEXT("从当前值刷新")))
//						.ToolTipText(FText::FromString(TEXT("将待应用值同步为当前盘面参数（不修改盘面）")))
//					.Text(FText::FromString(TEXT("宽高范围 1..256；TileSize 需为有限正值。缩放尺寸保留类型与归属，扩图补空地；缩图先列出越界非空格/物件/绑定供选择。")))
//				.Text(FText::FromString(TEXT("保存目标关卡")))
//				.ToolTipText(FText::FromString(TEXT("仅保存目标 Level 包（含跟踪物件的外部包，若存在）；不保存其他用户资源")))
//				.Text(FText::FromString(TEXT("撤销")))
//				.Text(FText::FromString(TEXT("重做")))
//			.Text(FText::FromString(TEXT("请先在关卡中选中一个 MapManager，然后点击『绑定选中的MapManager』。")))
//		auto MakeChoiceButton = [this](int32 InValue, const FString& InLabel) -> TSharedRef<SWidget>
//				.Text(FText::FromString(InLabel))
//					MakeChoiceButton(0, TEXT("保留越界数据并缩小"))
//					MakeChoiceButton(1, TEXT("移除越界数据并缩小"))
//					MakeChoiceButton(2, TEXT("取消"))
//		.Title(FText::FromString(TEXT("缩小地图确认")))
//			*OutProblem = FText::FromString(TEXT("未绑定目标或目标已销毁"));
//			*OutProblem = FText::FromString(TEXT("目标不在当前编辑器世界中（可能已切换关卡）"));
//			*OutReason = FText::FromString(TEXT("PIE/SIE 运行中，禁止编辑"));
//			*OutReason = FText::FromString(TEXT("目标所在关卡已锁定，禁止编辑"));
//		UpdateStatus(FText::FromString(TEXT("PIE/SIE 运行中，禁止切换绑定")));
//		UpdateStatus(FText::FromString(TEXT("未选中 MapManager：请在关卡中恰好选中一个后重试")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("选中了 %d 个 MapManager：请仅选中一个"), Maps.Num())));
//	const FString LevelName = (M->GetLevel() && M->GetLevel()->GetOutermost()) ? M->GetLevel()->GetOutermost()->GetName() : TEXT("未知");
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已绑定：%s（Level：%s）"), *M->GetActorLabel(), *LevelName)));
//	UpdateStatus(FText::FromString(TEXT("已解除绑定")));
//	UpdateStatus(FText::FromString(TEXT("目标已销毁或所在世界已切换，绑定已解除")));
//		UpdateStatus(FText::FromString(TEXT("未绑定目标：无法同步参数")));
//	UpdateStatus(FText::FromString(TEXT("待应用参数已同步为当前盘面值（未修改盘面）")));
//		OutReason = FText::FromString(TEXT("未绑定有效目标，未写入"));
//		OutReason = FText::FromString(FString::Printf(TEXT("坐标 (%d, %d) 越界，未写入"), InCell.X, InCell.Y));
//		OutReason = FText::FromString(FString::Printf(TEXT("格 (%d, %d) 已关联生成器：请先解绑或使用『移动绑定出生点到所选格』"), InCell.X, InCell.Y));
//		StrokeTransaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("地图工作台：绘制格")));
//	FString Info = TEXT("所选格：未选择（用『选择坐标』笔刷左键点击画布）");
//			Info = FString::Printf(TEXT("所选格：(%d, %d)（盘面外）"), Cell.X, Cell.Y);
//			Info = FString::Printf(TEXT("所选格：(%d, %d)  类型：%s"), Cell.X, Cell.Y, *MapWorkbenchStyle::GetTileLabel(Type).ToString());
//				Info += TEXT("  [已关联生成器]");
//			// このマスに投影される工具物件の番号一覧（リスト番号と同一順序）
//				Info += FString::Printf(TEXT("  本格工具物件编号：%s"), *ActorIndexes);
//		OutReason = FText::FromString(TEXT("素材为空"));
//		OutReason = FText::FromString(TEXT("仅支持静态网格体或可放置 Actor 蓝图"));
//		OutReason = FText::FromString(TEXT("仅支持普通蓝图类（拒绝函数库/宏库/接口蓝图）"));
//		OutReason = FText::FromString(TEXT("该蓝图已标记弃用，拒绝放置"));
//		OutReason = FText::FromString(TEXT("蓝图父类不是 Actor，拒绝放置"));
//		OutReason = FText::FromString(TEXT("蓝图父类为抽象类，无法实例化"));
//		OutReason = FText::FromString(TEXT("蓝图父类已弃用，拒绝放置"));
//		OutReason = FText::FromString(TEXT("蓝图未生成可用类（可能未编译）"));
//		OutReason = FText::FromString(TEXT("蓝图生成类为抽象/弃用，拒绝放置"));
//			AssetInfoText->SetText(FText::FromString(TEXT("素材：未读取")));
//		UpdateStatus(FText::FromString(TEXT("未读到素材：请先在内容浏览器选中一个静态网格体或 Actor 蓝图")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("读到 %d 个素材：请仅选中一个"), Candidates.Num())));
//			AssetInfoText->SetText(FText::FromString(FString::Printf(TEXT("素材：%s  [校验失败] %s"), *Candidate->GetName(), *Reason.ToString())));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("素材校验失败：%s"), *Reason.ToString())));
//		AssetInfoText->SetText(FText::FromString(FString::Printf(TEXT("素材：%s（%s）"),
//			*Candidate->GetName(), Mesh != nullptr ? TEXT("静态网格体") : TEXT("Actor 蓝图"))));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已读取素材：%s，可用『放置到所选格』"), *Candidate->GetName())));
//		UpdateStatus(FText::FromString(TEXT("未选择格：请用『选择坐标』笔刷左键点击画布")));
//		UpdateStatus(FText::FromString(TEXT("未读取素材：请先点击『读取内容浏览器选中素材』")));
//			AssetInfoText->SetText(FText::FromString(TEXT("素材：已卸载，请重新读取")));
//		UpdateStatus(FText::FromString(TEXT("素材已卸载：请重新读取")));
//	FScopedTransaction Transaction(FText::FromString(TEXT("地图工作台：放置物件")));
//		UpdateStatus(FText::FromString(TEXT("放置失败：Actor 生成被拒绝（详见输出日志）")));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已放置 %s 于格 (%d, %d)"), *NewActor->GetActorLabel(), Cell.X, Cell.Y)));
//		UpdateStatus(FText::FromString(TEXT("未在列表中选中物件行")));
//		UpdateStatus(FText::FromString(TEXT("所选行物件已失效，正在刷新列表")));
//		UpdateStatus(FText::FromString(TEXT("该 Actor 不是本 Map 跟踪的物件（或其他 Level），拒绝删除")));
//		UpdateStatus(FText::FromString(TEXT("删除失败：无法取得 EditorActorSubsystem")));
//		UpdateStatus(FText::FromString(TEXT("其他编辑事务进行中：为避免误撤销，请稍后重试")));
//		FScopedTransaction Transaction(FText::FromString(TEXT("地图工作台：删除工具物件")));
//		UpdateStatus(FText::FromString(TEXT("删除中止：Map 未能记录到事务（无任何状态变更）")));
//			? FText::FromString(FString::Printf(TEXT("删除失败：%s —— 已自动回滚，未残留部分状态"), *VictimLabel))
//			: FText::FromString(FString::Printf(TEXT("删除失败：%s —— 回滚失败，请立即手动撤销（Ctrl+Z）"), *VictimLabel)));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已删除工具物件：%s"), *VictimLabel)));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已在场景中选中并聚焦：%s"), *InActor->GetActorLabel())));
//		UpdateStatus(FText::FromString(TEXT("未绑定目标：请先绑定 MapManager")));
//		UpdateStatus(FText::FromString(TEXT("PIE/SIE 运行中，禁止读取")));
//		UpdateStatus(FText::FromString(TEXT("未读到 AnimalSpawner：请在场景中恰好选中一个")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("读到 %d 个 AnimalSpawner：请仅选中一个"), Spawners.Num())));
//		UpdateStatus(FText::FromString(TEXT("生成器不在目标 MapManager 所在 Level：跨关卡关联被拒绝")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("已读取生成器：%s（已关联格 (%d, %d)）"),
//			*Candidate->GetActorLabel(), ExistingBinding->GridCoords.X, ExistingBinding->GridCoords.Y)));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("已读取生成器：%s（未关联）"), *Candidate->GetActorLabel())));
//		UpdateStatus(FText::FromString(TEXT("未读取生成器：请先点击『读取选中的AnimalSpawner』")));
//		UpdateStatus(FText::FromString(TEXT("未选择格：请用『选择坐标』笔刷左键点击画布")));
//		UpdateStatus(FText::FromString(TEXT("跨关卡关联被拒绝：生成器与 MapManager 不在同一 Level")));
//		UpdateStatus(FText::FromString(TEXT("该生成器已关联出生格：请先解除绑定，或使用『移动绑定出生点到所选格』")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("所选格 (%d, %d) 越界"), Cell.X, Cell.Y)));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("格 (%d, %d) 已关联其他生成器"), Cell.X, Cell.Y)));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("格 (%d, %d) 不是出生点（Spawn）类型：请先用出生点笔刷绘制该格"), Cell.X, Cell.Y)));
//	FScopedTransaction Transaction(FText::FromString(TEXT("地图工作台：绑定出生格")));
//		UpdateStatus(FText::FromString(TEXT("绑定失败：数据层拒绝（详见输出日志）")));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已绑定 %s 到格 (%d, %d)，高度偏移 %.1f"),
//		*Spawner->GetActorLabel(), Cell.X, Cell.Y, HeightOffset)));
//		UpdateStatus(FText::FromString(TEXT("未读取生成器：请先点击『读取选中的AnimalSpawner』")));
//		UpdateStatus(FText::FromString(TEXT("该生成器未关联出生格")));
//	FScopedTransaction Transaction(FText::FromString(TEXT("地图工作台：解绑出生格")));
//		UpdateStatus(FText::FromString(TEXT("解绑失败：数据层拒绝（详见输出日志）")));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已解除 %s 的出生格关联"), *Spawner->GetActorLabel())));
//		UpdateStatus(FText::FromString(TEXT("未读取生成器：请先点击『读取选中的AnimalSpawner』")));
//		UpdateStatus(FText::FromString(TEXT("该生成器未关联出生格：请先绑定")));
//		UpdateStatus(FText::FromString(TEXT("未选择格：请用『选择坐标』笔刷左键点击画布")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("所选格 (%d, %d) 越界"), TargetCell.X, TargetCell.Y)));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("格 (%d, %d) 已关联其他生成器"), TargetCell.X, TargetCell.Y)));
//			UpdateStatus(FText::FromString(FString::Printf(TEXT("格 (%d, %d) 已有 %s：请先清除该格再移动"),
//				TargetCell.X, TargetCell.Y, *MapWorkbenchStyle::GetTileLabel(CurrentType).ToString())));
//	FScopedTransaction Transaction(FText::FromString(TEXT("地图工作台：移动出生格")));
//		UpdateStatus(FText::FromString(TEXT("移动失败：数据层拒绝（详见输出日志）")));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已将 %s 的出生格移动到 (%d, %d)"),
//		*Spawner->GetActorLabel(), TargetCell.X, TargetCell.Y)));
//	FString Info = TEXT("生成器：未读取");
//			Info = FString::Printf(TEXT("生成器：%s  已关联格 (%d, %d)  高度偏移 %.1f"),
//				*Spawner->GetActorLabel(), Binding->GridCoords.X, Binding->GridCoords.Y, Binding->HeightOffset);
//			Info = FString::Printf(TEXT("生成器：%s  未关联出生格"), *Spawner->GetActorLabel());
//			TileLines.Add(FString::Printf(TEXT("格 (%d, %d) %s"), X, Y, *MapWorkbenchStyle::GetTileLabel(M->GridData[i].TileType).ToString()));
//			BindingLines.Add(FString::Printf(TEXT("%s -> (%d, %d)"), *Binding.Spawner->GetActorLabel(), Binding.GridCoords.X, Binding.GridCoords.Y));
//				ActorLines.Add(FString::Printf(TEXT("%s @( %d, %d )"), *Actor->GetActorLabel(), Cell.X, Cell.Y));
//			Out += FString::Printf(TEXT("  %s\n"), *Lines[i]);
//			Out += FString::Printf(TEXT("  …共 %d 项\n"), Lines.Num());
//	FString Report = FString::Printf(TEXT("缩小到 %d x %d 将有越界数据：\n\n◆ 越界非空格 %d 个：\n"), NewWidth, NewHeight, TileLines.Num());
//	Report += FString::Printf(TEXT("\n◆ 越界工具物件 %d 个（仅统计本工具跟踪、同 Level）：\n"), ActorLines.Num());
//	Report += FString::Printf(TEXT("\n◆ 越界生成器绑定 %d 个：\n"), BindingLines.Num());
//	Report += TEXT("\n【保留越界数据并缩小】越界格存档保存，扩图后恢复；物件留在原处并显示为越界。\n");
//	Report += TEXT("【移除越界数据并缩小】删除越界格/仅限本工具跟踪的越界物件，并清理相应绑定（不删资产或文件）。\n");
//	Report += TEXT("【取消】放弃本次应用，盘面不做任何修改。");
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("宽高无效：%d x %d（需 1..%d）"), NewWidth, NewHeight, AMapManager::MaxGridDimension)));
//		UpdateStatus(FText::FromString(TEXT("TileSize 无效：需为有限正值")));
//		UpdateStatus(FText::FromString(TEXT("参数无变化：未创建事务")));
//				UpdateStatus(FText::FromString(TEXT("已取消：盘面参数未修改")));
//		UpdateStatus(FText::FromString(TEXT("其他编辑事务进行中：为避免误撤销，请稍后重试")));
//		FScopedTransaction Transaction(FText::FromString(TEXT("地图工作台：应用地图参数")));
//		UpdateStatus(FText::FromString(TEXT("应用中止：Map 未能记录到事务（无任何状态变更）")));
//				? FText::FromString(TEXT("尺寸调整被数据层拒绝：已自动回滚，盘面保持原状"))
//				: FText::FromString(TEXT("应用参数失败：已自动回滚，盘面保持原状")))
//			: FText::FromString(TEXT("应用参数失败且回滚失败：请立即手动撤销（Ctrl+Z）")));
//	UpdateStatus(FText::FromString(FString::Printf(TEXT("已应用参数：%d x %d，TileSize %.1f%s"),
//		NewWidth, NewHeight, NewTileSize,
//		bShrink ? (bKeepOutOfBounds ? TEXT("（越界数据已保留为档案）") : TEXT("（越界数据已移除）")) : TEXT(""))));
//		UpdateStatus(FText::FromString(TEXT("无法取得目标 Level 的包：保存中止")));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("目标关卡 %s 没有未保存的改动"), *LevelPackage->GetName())));
//		FText::FromString(TEXT("保存目标关卡")),
//		FText::FromString(TEXT("仅保存工作台绑定的目标 Level 及相关包")),
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("已保存：%s"), *LevelPackage->GetName())));
//		UpdateStatus(FText::FromString(FString::Printf(TEXT("保存失败：%d 个包未能保存（详见输出日志）"), FailedPackages.Num())));
//		UpdateStatus(FText::FromString(TEXT("保存被取消或未完成（详见输出日志）")));
//		UpdateStatus(FText::FromString(TEXT("PIE/SIE 运行中，禁止撤销/重做")));
//	UpdateStatus(FText::FromString(TEXT("已执行撤销，面板已刷新")));
//		UpdateStatus(FText::FromString(TEXT("PIE/SIE 运行中，禁止撤销/重做")));
//	UpdateStatus(FText::FromString(TEXT("已执行重做，面板已刷新")));
//	FString TargetInfo = TEXT("目标：未绑定");
//		const FString LevelName = (M->GetLevel() && M->GetLevel()->GetOutermost()) ? M->GetLevel()->GetOutermost()->GetName() : TEXT("未知");
//		TargetInfo = FString::Printf(TEXT("目标：%s   Level：%s"), *M->GetActorLabel(), *LevelName);
// 2026.10.23 Lee end
// PRESERVATION-MARKER: MAPWORKBENCH-PRESERVED-ORIGINAL-PANEL-2026.10.23-LEE
