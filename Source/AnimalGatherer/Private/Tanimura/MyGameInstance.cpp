// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MyGameInstance.h"

void UMyGameInstance::SetFinalResult(int32 InP1Score, int32 InP2Score)
{
    P1Score = InP1Score;
    P2Score = InP2Score;

    // 勝敗判定ロジック
    if (P1Score > P2Score) {
        MatchResult = EMatchResult::P1Win;
    }
    else if (P2Score > P1Score) {
        MatchResult = EMatchResult::P2Win;
    }
    else {
        MatchResult = EMatchResult::Draw;
    }
}