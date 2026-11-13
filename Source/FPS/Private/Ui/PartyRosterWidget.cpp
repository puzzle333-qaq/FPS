#include "Ui/PartyRosterWidget.h"

#include "Game/ShooterPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Online/PartySubsystem.h"
#include "Styling/CoreStyle.h"
#include "Ui/TacticalUIStyle.h"
#include "Ui/OperationUIStyle.h"
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
		return FOperationUIStyle::Dim;
	}
}

TSharedRef<SWidget> UPartyRosterWidget::RebuildWidget()
{
	LastSignature.Reset();
	PanelBrush = FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel, 2.f, 1.f);
	ButtonStyle = FTacticalUIStyle::MakeButtonStyle();
 ButtonStyle.Normal=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel,2,1);
 ButtonStyle.Hovered=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Dim,2,1);
 ButtonStyle.Pressed=FOperationUIStyle::MakePanelBrush(FOperationUIStyle::Panel,2,1);
	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 40.f, 48.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(260.f)
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.Padding(FMargin(16.f, 8.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(Rows, SVerticalBox)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 6.f)
					[
						SAssignNew(StatusText, STextBlock)
						.Font(FOperationUIStyle::MakeFont(12))
						.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
						.ColorAndOpacity(FOperationUIStyle::TextMuted)
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
								.Font(FOperationUIStyle::MakeFont(16, true))
								.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
								.ColorAndOpacity(FOperationUIStyle::Text)
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
								.Font(FOperationUIStyle::MakeFont(16, true))
								.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
								.ColorAndOpacity(FOperationUIStyle::Text)
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
	FString Status=Party?Party->GetStatusText():FString(TEXT("房间未连接"));
 if(Status.Contains(TEXT("failed"),ESearchCase::IgnoreCase)||Status.Contains(TEXT("登录失败"))) Status=TEXT("好友服务暂不可用");
 StatusText->SetText(FText::FromString(Status));
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
							.Font(FOperationUIStyle::MakeFont(16, true))
							.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
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
					.Font(FOperationUIStyle::MakeFont(16))
					.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
					.ColorAndOpacity(FOperationUIStyle::Text)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(bHostMark ? TEXT("房主") : TEXT("")))
					.Font(FOperationUIStyle::MakeFont(12))
					.ShadowOffset(FVector2D(0,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.6f))
					.ColorAndOpacity(FOperationUIStyle::Text)
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
