// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialGamepadWidget.generated.h"

// 2026.10.08 Lee : 教程用ゲームパッド早見 Widget 新規作成
// 2026.10.08 Lee 返工 start（SizeBox による設計 DesiredSize・十字キー十字形・ABXY の方向表示・LB/RB 文字を追加）

class UImage;
class UTextBlock;
class UTexture2D;

/**
 * @brief チュートリアル HUD 用のゲームパッド早見 Widget。
 *        底図（輪郭テクスチャ）の上に十字キー / Y,A,X,B / LB,RB の指示レイヤを重ね、
 *        SetHighlightedControl で指定ボタンを明色強調する。
 *        レイアウトは 1536x1024 の輪郭源図を半分にした 768x512 の設計座標で組み、
 *        根に USizeBox（768x512）を置いて DesiredSize を確定し、親側 ScaleBox で等比縮小される。
 *        摇杆は機能表示の対象外（輪郭のみ）。ABXY は文字と対応方向（↑↓←→）を併記する。
 */
UCLASS()
class ANIMALGATHERER_API UTutorialGamepadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief 底図テクスチャを設定する（チュートリアル HUD の EditDefaultsOnly から渡される）。
	 * @param InTexture パッド輪郭の Texture2D。nullptr の場合は既定のまま。
	 */
	void SetBodyTexture(UTexture2D* InTexture);

	/**
	 * @brief 強調するボタンを指定する。
	 *        指定値: "None"(強調なし) / "DPad" / "Y" / "A" / "X" / "B" / "LB" / "RB" / "All"(全点灯)。
	 * @param ControlName 強調するボタン名（大文字固定）。
	 */
	void SetHighlightedControl(FName ControlName);

protected:
	// 2026.10.08 Lee start（Slate 構築前に Root を組み替えるため RebuildWidget を使用）
	// 2026.10.08 Lee 返工 start（RebuildWidget の実際の署名は TSharedRef<SWidget> を返す）
	/** @brief Slate 取得前に設計座標レイアウトを構築してから Super の戻り値をそのまま返す。 */
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// 2026.10.08 Lee 返工 end
	// 2026.10.08 Lee end

private:
	/** @brief 設計座標 768x512 のレイアウトを構築する（冪等。二重構築しない）。 */
	void BuildNativeLayout();

	/** @brief 現在の HighlightedControl に従って指示レイヤと文字色を反映する。 */
	void ApplyHighlight();

	/** @brief 指示レイヤ（角丸なし矩形）をキャンバスへ追加する。 */
	UImage* AddMarkerToCanvas(class UCanvasPanel* Canvas, const FString& Name,
		const FVector2D& Center, const FVector2D& Size);

	/**
	 * @brief 中央寄せの文字ブロックをキャンバスへ追加する。
	 * @param Canvas 追加先キャンバス。
	 * @param Name Widget 名。
	 * @param Text 表示文字（LB / RB / Y / A / X / B / ↑ など）。
	 * @param FontSize フォントサイズ（設計単位）。
	 * @param Center 配置中心（設計座標）。
	 * @param Size 文字枠サイズ（設計座標）。
	 * @return 生成した TextBlock。
	 */
	UTextBlock* AddTextToCanvas(class UCanvasPanel* Canvas, const FString& Name,
		const TCHAR* Text, float FontSize, const FVector2D& Center, const FVector2D& Size);

	/** @brief レイアウト構築済みか。 */
	bool bNativeLayoutBuilt = false;

	/** @brief 現在強調中のボタン名（既定 "None"）。 */
	FName HighlightedControl = TEXT("None");

	/** @brief 底図（パッド輪郭）。 */
	UPROPERTY()
	TObjectPtr<UImage> BodyImage = nullptr;

	/** @brief 親から渡された底図テクスチャ（BuildNativeLayout の完了時に適用する）。 */
	UPROPERTY()
	TObjectPtr<UTexture2D> CachedBodyTexture = nullptr;

	/** @brief 十字キーの指示レイヤ（十字形の横腕）。 */
	UPROPERTY()
	TObjectPtr<UImage> DPadHBar = nullptr;

	/** @brief 十字キーの指示レイヤ（十字形の縦腕）。 */
	UPROPERTY()
	TObjectPtr<UImage> DPadVBar = nullptr;

	/** @brief Y ボタンの指示レイヤ。 */
	UPROPERTY()
	TObjectPtr<UImage> YMarker = nullptr;

	/** @brief A ボタンの指示レイヤ。 */
	UPROPERTY()
	TObjectPtr<UImage> AMarker = nullptr;

	/** @brief X ボタンの指示レイヤ。 */
	UPROPERTY()
	TObjectPtr<UImage> XMarker = nullptr;

	/** @brief B ボタンの指示レイヤ。 */
	UPROPERTY()
	TObjectPtr<UImage> BMarker = nullptr;

	/** @brief LB（左肩）の指示レイヤ。 */
	UPROPERTY()
	TObjectPtr<UImage> LBMarker = nullptr;

	/** @brief RB（右肩）の指示レイヤ。 */
	UPROPERTY()
	TObjectPtr<UImage> RBMarker = nullptr;

	// 2026.10.08 Lee 返工 start（高力度对比のための文字参照の保存）
	/** @brief LB の文字表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> LBText = nullptr;

	/** @brief RB の文字表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> RBText = nullptr;

	/** @brief Y の字母表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> YLetter = nullptr;

	/** @brief A の字母表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> ALetter = nullptr;

	/** @brief X の字母表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> XLetter = nullptr;

	/** @brief B の字母表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> BLetter = nullptr;

	/** @brief Y（上向き）の方向表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> YArrow = nullptr;

	/** @brief A（下向き）の方向表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> AArrow = nullptr;

	/** @brief X（左向き）の方向表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> XArrow = nullptr;

	/** @brief B（右向き）の方向表示。 */
	UPROPERTY()
	TObjectPtr<UTextBlock> BArrow = nullptr;
	// 2026.10.08 Lee 返工 end
};
// 2026.10.08 Lee 返工 end
