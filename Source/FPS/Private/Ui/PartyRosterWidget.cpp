#include "Ui/PartyRosterWidget.h"

#include "Game/ShooterPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Online/PartySubsystem.h"
#include "Styling/CoreStyle.h"
#include "Ui/TacticalUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FLinearColor PortraitColor(const FString& Name)
	{
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(GetTypeHash(Name) & 255), 150, 210);
	}
}

TSharedRef<SWidget> UPartyRosterWidget::RebuildWidget()
{
	PanelBrush = FTacticalUIStyle::MakePanelBrush(FTacticalUIStyle::Panel, 8.f, 1.f);
	ButtonStyle = FTacticalUIStyle::MakeButtonStyle();
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 28.f, 28.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(260.f)
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(FMargin(12.f, 10.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(Rows, SVerticalBox)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 6.f)
					[
						SAssignNew(StatusText, STextBlock)
						.Font(FTacticalUIStyle::MakeFont(12))
						.ColorAndOpacity(FTacticalUIStyle::TextMuted)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
						[
							SAssignNew(InviteButton, SButton)
							.ButtonStyle(&ButtonStyle)
							.OnClicked(FOnClicked::CreateUObject(this, &UPartyRosterWidget::OnInviteClicked))
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Party", "Invite", "邀请好友"))
								.Font(FTacticalUIStyle::MakeFont(16, true))
								.ColorAndOpacity(FTacticalUIStyle::Text)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SAssignNew(LeaveButton, SButton)
							.ButtonStyle(&ButtonStyle)
							.OnClicked(FOnClicked::CreateUObject(this, &UPartyRosterWidget::OnLeaveClicked))
							.Visibility(EVisibility::Collapsed)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Party", "Leave", "离开"))
								.Font(FTacticalUIStyle::MakeFont(16, true))
								.ColorAndOpacity(FTacticalUIStyle::Text)
							]
						]
					]
				]
			]
		];
}

void UPartyRosterWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Rows.Reset();
	StatusText.Reset();
	InviteButton.Reset();
	LeaveButton.Reset();
}

FReply UPartyRosterWidget::OnInviteClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPartySubsystem* Party = GI->GetSubsystem<UPartySubsystem>()) Party->OpenInvite();
	}
	return FReply::Handled();
}

FReply UPartyRosterWidget::OnLeaveClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPartySubsystem* Party = GI->GetSubsystem<UPartySubsystem>()) Party->LeaveRoom();
	}
	return FReply::Handled();
}

void UPartyRosterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshWait += InDeltaTime;
	if (RefreshWait < 0.4f) return;
	RefreshWait = 0.f;
	RefreshRoster();
}

void UPartyRosterWidget::RefreshRoster()
{
	if (!Rows || !StatusText) return;
	UGameInstance* GI = GetGameInstance();
	UPartySubsystem* Party = GI ? GI->GetSubsystem<UPartySubsystem>() : nullptr;
	StatusText->SetText(FText::FromString(Party ? Party->GetStatusText() : FString(TEXT("房间未连接"))));
	const bool bInRoom = Party && Party->IsInRoom();
	const bool bHost = !Party || !bInRoom || Party->IsRoomHost();
	if (InviteButton) InviteButton->SetVisibility(bHost ? EVisibility::Visible : EVisibility::Collapsed);
	if (LeaveButton) LeaveButton->SetVisibility(bInRoom ? EVisibility::Visible : EVisibility::Collapsed);

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	TArray<const APlayerState*> Members;
	if (GS)
	{
		for (const APlayerState* Member : GS->PlayerArray)
		{
			if (IsValid(Member)) Members.Add(Member);
		}
	}
	Members.Sort([](const APlayerState& A, const APlayerState& B)
	{
		const AShooterPlayerState* Left = Cast<AShooterPlayerState>(&A);
		const AShooterPlayerState* Right = Cast<AShooterPlayerState>(&B);
		const bool bLeftHost = Left && Left->IsPartyHost();
		const bool bRightHost = Right && Right->IsPartyHost();
		if (bLeftHost != bRightHost) return bLeftHost;
		return A.GetPlayerId() < B.GetPlayerId();
	});

	FString Signature = Party ? Party->GetStatusText() : FString();
	Signature += bHost ? TEXT("|H") : TEXT("|G");
	Signature += bInRoom ? TEXT("|R") : TEXT("|S");
	for (const APlayerState* Member : Members)
	{
		const AShooterPlayerState* State = Cast<AShooterPlayerState>(Member);
		Signature += FString::Printf(TEXT(";%d|%s|%d"), Member->GetPlayerId(), *Member->GetPlayerName(), State && State->IsPartyHost() ? 1 : 0);
	}
	if (Signature == LastSignature) return;
	LastSignature = Signature;
	Rows->ClearChildren();

	auto AddMember = [this](const FString& Name, bool bHostMark)
	{
		const FString Letter = Name.IsEmpty() ? FString(TEXT("?")) : Name.Left(1).ToUpper();
		Rows->AddSlot().AutoHeight().Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(PortraitColor(Name))
				.Padding(0.f)
				[
					SNew(SBox).WidthOverride(36.f).HeightOverride(36.f)
					[
						SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Letter))
							.Font(FTacticalUIStyle::MakeFont(16, true))
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Name.IsEmpty() ? TEXT("…") : Name))
					.Font(FTacticalUIStyle::MakeFont(15, true))
					.ColorAndOpacity(FTacticalUIStyle::Text)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(bHostMark ? TEXT("房主") : TEXT("")))
					.Font(FTacticalUIStyle::MakeFont(11, true))
					.ColorAndOpacity(FTacticalUIStyle::TextGold)
					.Visibility(bHostMark ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
				]
			]
		];
	};

	if (Members.IsEmpty())
	{
		AddMember(Party ? Party->GetLocalDisplayName() : FString(TEXT("Operator")), false);
		return;
	}
	for (const APlayerState* Member : Members)
	{
		const AShooterPlayerState* State = Cast<AShooterPlayerState>(Member);
		AddMember(Member->GetPlayerName(), State && State->IsPartyHost());
	}
}
