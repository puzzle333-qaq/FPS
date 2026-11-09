#include "Game/MissionTypes.h"

void FMissionReport::Calculate(const FMissionScoreRules& R, bool bWon, bool bFinal)
{
 PrimaryPoints = bPrimaryComplete ? FMath::Max(0, R.Primary) : 0;
 MarkedPoints = MarkedCompleted * FMath::Max(0, R.Marked);
 NeutralizedPoints = Neutralized * FMath::Max(0, R.Neutralized);
 ArrestedPoints = Arrested * FMath::Max(0, R.Arrested);
 HostagePoints = Rescued * FMath::Max(0, R.Hostage);
 SurvivalPoints = Survivors * FMath::Max(0, R.Survivor);
 HiddenPoints = HiddenCompleted * FMath::Max(0, R.Hidden);
 Penalties = HostagesLost * FMath::Max(0, R.HostageLostPenalty) + SurrenderedKilled * FMath::Max(0, R.SurrenderedKilledPenalty);
 Score = FMath::Max(0, PrimaryPoints + MarkedPoints + NeutralizedPoints + ArrestedPoints + HostagePoints + SurvivalPoints + HiddenPoints - Penalties);
 // Hidden objectives are bonus points, never part of the required denominator.
 MaximumScore = FMath::Max(1, FMath::Max(0, R.Primary) + MarkedTotal * FMath::Max(0, R.Marked)
  + SuspectsTotal * FMath::Max(R.Neutralized, R.Arrested) + HostagesTotal * FMath::Max(0, R.Hostage)
  + OperatorsTotal * FMath::Max(0, R.Survivor));
 if (!bFinal) { Grade = TEXT("—"); return; }
 if (!bWon) { Grade = TEXT("F"); return; }
 const float Ratio = float(Score) / MaximumScore;
 Grade = Ratio >= R.SThreshold && HostagesLost == 0 && SurrenderedKilled == 0 && Survivors == OperatorsTotal ? TEXT("S")
  : Ratio >= R.AThreshold ? TEXT("A") : Ratio >= R.BThreshold ? TEXT("B") : Ratio >= R.CThreshold ? TEXT("C") : TEXT("D");
}
