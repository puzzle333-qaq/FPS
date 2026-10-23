#include "Ui/SettingsScreen.h"

#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Menu/MainMenuPlayerController.h"
#include "ShooterPlayerController.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Components/PanelWidget.h"
#include "InputCoreTypes.h"

namespace FPSSettings
{
	static const TCHAR* Section = TEXT("/Script/FPS.SettingsScreen");
	static const TArray<FString>& QualityNames()
	{
		static const TArray<FString> N = { TEXT("低"), TEXT("中"), TEXT("高"), TEXT("极致") };
		return N;
	}
	static const FString Custom = TEXT("自定义");
	static const FString On = TEXT("开");
	static const FString Off = TEXT("关");
	static const TArray<int32> FrameRates = { 30, 60, 120, 144, 165, 240, 0 };
	static const TArray<int32> Scales = { 50, 60, 67, 75, 80, 90, 100 };
	static const TArray<FString>& DLSSNames()
	{
		static const TArray<FString> N = { TEXT("关"), TEXT("质量"), TEXT("平衡"), TEXT("性能"), TEXT("超级性能"), TEXT("DLAA") };
		return N;
	}
	static FString WindowModeName(EWindowMode::Type M)
	{
		switch (M)
		{
		case EWindowMode::Fullscreen: return TEXT("全屏");
		case EWindowMode::WindowedFullscreen: return TEXT("无边框");
		default: return TEXT("窗口");
		}
	}
	static const TArray<float> SensSteps = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f };
	static const TArray<float> ADSSteps = { 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f };
	static const TArray<float> FOVSteps = { 70.f, 75.f, 80.f, 85.f, 90.f, 95.f, 100.f, 105.f, 110.f };
	static FString NumName(float V) { return FString::Printf(TEXT("%g"), V); }
	static int32 Nearest(const TArray<float>& Steps, float V)
	{
		int32 Best = 0;
		for (int32 i = 1; i < Steps.Num(); ++i) if (FMath::Abs(Steps[i] - V) < FMath::Abs(Steps[Best] - V)) Best = i;
		return Best;
	}
	static FString FrameName(int32 Fps) { return Fps <= 0 ? FString(TEXT("无限制")) : FString::FromInt(Fps); }
	static FString ResName(const FIntPoint& P) { return FString::Printf(TEXT("%d x %d"), P.X, P.Y); }

	static void SetCVarInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	}
	static bool GetSavedBool(const TCHAR* Key, bool bDefault)
	{
		bool b = bDefault;
		if (GConfig) GConfig->GetBool(Section, Key, b, GGameUserSettingsIni);
		return b;
	}
	static int32 GetSavedInt(const TCHAR* Key, int32 Default)
	{
		int32 V = Default;
		if (GConfig) GConfig->GetInt(Section, Key, V, GGameUserSettingsIni);
		return V;
	}
	static void ApplyRayTracing(bool bOn)
	{
		SetCVarInt(TEXT("r.Lumen.HardwareRayTracing"), bOn ? 1 : 0);
		SetCVarInt(TEXT("r.RayTracing.Shadows"), bOn ? 1 : 0);
	}
	static void ApplyDLSS(int32 Mode)
	{
		// 0 关 1 质量 2 平衡 3 性能 4 超级性能 5 DLAA。插件不在时这些变量不存在，SetCVarInt 什么也不做
		SetCVarInt(TEXT("r.NGX.DLSS.Enable"), (Mode >= 1 && Mode <= 4) ? 1 : 0);
		SetCVarInt(TEXT("r.NGX.DLAA.Enable"), Mode == 5 ? 1 : 0);
		static const int32 QualityMap[] = { 0, 1, 0, -1, -2, 0 };
		if (Mode >= 1 && Mode <= 4) SetCVarInt(TEXT("r.NGX.DLSS.Quality"), QualityMap[Mode]);
	}
}

using namespace FPSSettings;

bool USettingsScreen::IsDLSSAvailable()
{
	return IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.DLSS.Enable")) != nullptr;
}

void USettingsScreen::ApplySavedExtraSettings()
{
	ApplyRayTracing(GetSavedBool(TEXT("bRayTracing"), false));
	if (IsDLSSAvailable()) ApplyDLSS(GetSavedInt(TEXT("DLSSMode"), 0));
}

void USettingsScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Apply) Button_Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApply);
	if (Button_Reset) Button_Reset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReset);
	if (Button_Back) Button_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
	if (Combo_OverallQuality) Combo_OverallQuality->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleOverallChanged);
	for (UComboBoxString* C : GetQualityCombos())
	{
		C->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleIndividualChanged);
	}
	SetIsFocusable(true);
	if (Combo_RayTracing) Combo_RayTracing->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleRestartOptionChanged);
	if (Combo_DLSS && !IsDLSSAvailable()) Combo_DLSS->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_RestartHint) Text_RestartHint->SetVisibility(ESlateVisibility::Collapsed);
	RefreshFromCurrent();
}

TArray<UComboBoxString*> USettingsScreen::GetQualityCombos() const
{
	TArray<UComboBoxString*> Out;
	for (UComboBoxString* C : { Combo_Shadows.Get(), Combo_AntiAliasing.Get(), Combo_PostProcess.Get(), Combo_Textures.Get(),
		Combo_Effects.Get(), Combo_ViewDistance.Get(), Combo_Foliage.Get(), Combo_Shading.Get(),
		Combo_GlobalIllumination.Get(), Combo_Reflections.Get() })
	{
		if (C) Out.Add(C);
	}
	return Out;
}

void USettingsScreen::Fill(UComboBoxString* Combo, const TArray<FString>& Options)
{
	if (!Combo) return;
	Combo->ClearOptions();
	for (const FString& O : Options) Combo->AddOption(O);
}

void USettingsScreen::Select(UComboBoxString* Combo, const FString& Option)
{
	if (!Combo) return;
	if (Combo->FindOptionIndex(Option) == INDEX_NONE) Combo->AddOption(Option);
	Combo->SetSelectedOption(Option);
}

void USettingsScreen::SelectQuality(UComboBoxString* Combo, int32 Level)
{
	Select(Combo, QualityNames()[FMath::Clamp(Level, 0, 3)]);
}

int32 USettingsScreen::ReadQuality(const UComboBoxString* Combo, int32 Fallback) const
{
	if (!Combo) return Fallback;
	const int32 I = QualityNames().IndexOfByKey(Combo->GetSelectedOption());
	return I == INDEX_NONE ? Fallback : I;
}

void USettingsScreen::BuildOptions()
{
	if (bBuilt) return;
	bBuilt = true;

	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	for (const FIntPoint& P : Supported) Resolutions.AddUnique(P);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X > B.X : A.Y > B.Y; });
	TArray<FString> ResNames;
	for (const FIntPoint& P : Resolutions) ResNames.Add(ResName(P));
	Fill(Combo_Resolution, ResNames);

	Fill(Combo_WindowMode, { WindowModeName(EWindowMode::Fullscreen), WindowModeName(EWindowMode::WindowedFullscreen), WindowModeName(EWindowMode::Windowed) });
	TArray<FString> Fps;
	for (int32 F : FrameRates) Fps.Add(FrameName(F));
	Fill(Combo_FrameRate, Fps);
	Fill(Combo_VSync, { On, Off });
	TArray<FString> Overall = QualityNames();
	Overall.Add(Custom);
	Fill(Combo_OverallQuality, Overall);
	for (UComboBoxString* C : GetQualityCombos()) Fill(C, QualityNames());
	Fill(Combo_RayTracing, { On, Off });
	TArray<FString> ScaleNames;
	for (int32 S : Scales) ScaleNames.Add(FString::Printf(TEXT("%d%%"), S));
	Fill(Combo_ResolutionScale, ScaleNames);
	Fill(Combo_DLSS, DLSSNames());

	auto FillSteps = [this](UComboBoxString* Combo, const TArray<float>& Steps)
	{
		TArray<FString> Names;
		for (float V : Steps) Names.Add(NumName(V));
		Fill(Combo, Names);
	};
	FillSteps(Combo_Sensitivity, SensSteps);
	FillSteps(Combo_ADSSensitivity, ADSSteps);
	FillSteps(Combo_FOV, FOVSteps);
	Fill(Combo_InvertY, { On, Off });
}

void USettingsScreen::RefreshFromCurrent()
{
	UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
	if (!S) return;
	BuildOptions();
	TGuardValue<bool> Guard(bUpdating, true);

	Select(Combo_Resolution, ResName(S->GetScreenResolution()));
	Select(Combo_WindowMode, WindowModeName(S->GetFullscreenMode()));
	const int32 Fps = FMath::RoundToInt(S->GetFrameRateLimit());
	Select(Combo_FrameRate, FrameName(Fps));
	Select(Combo_VSync, S->IsVSyncEnabled() ? On : Off);

	SelectQuality(Combo_Shadows, S->GetShadowQuality());
	SelectQuality(Combo_AntiAliasing, S->GetAntiAliasingQuality());
	SelectQuality(Combo_PostProcess, S->GetPostProcessingQuality());
	SelectQuality(Combo_Textures, S->GetTextureQuality());
	SelectQuality(Combo_Effects, S->GetVisualEffectQuality());
	SelectQuality(Combo_ViewDistance, S->GetViewDistanceQuality());
	SelectQuality(Combo_Foliage, S->GetFoliageQuality());
	SelectQuality(Combo_Shading, S->GetShadingQuality());
	SelectQuality(Combo_GlobalIllumination, S->GetGlobalIlluminationQuality());
	SelectQuality(Combo_Reflections, S->GetReflectionQuality());
	UpdateOverallFromIndividual();

	Select(Combo_RayTracing, GetSavedBool(TEXT("bRayTracing"), false) ? On : Off);

	float Normalized = 0.f, Current = 100.f, Min = 50.f, Max = 100.f;
	S->GetResolutionScaleInformationEx(Normalized, Current, Min, Max);
	int32 Best = Scales.Last();
	for (int32 V : Scales) if (FMath::Abs(V - Current) < FMath::Abs(Best - Current)) Best = V;
	Select(Combo_ResolutionScale, FString::Printf(TEXT("%d%%"), Best));

	Select(Combo_DLSS, DLSSNames()[FMath::Clamp(GetSavedInt(TEXT("DLSSMode"), 0), 0, 5)]);
	if (Text_RestartHint) Text_RestartHint->SetVisibility(ESlateVisibility::Collapsed);

	if (const UShooterGameUserSettings* SS = Cast<UShooterGameUserSettings>(S))
	{
		Select(Combo_Sensitivity, NumName(SensSteps[Nearest(SensSteps, SS->MouseSensitivity)]));
		Select(Combo_ADSSensitivity, NumName(ADSSteps[Nearest(ADSSteps, SS->ADSSensitivityMultiplier)]));
		Select(Combo_FOV, NumName(FOVSteps[Nearest(FOVSteps, SS->FieldOfView)]));
		Select(Combo_InvertY, SS->bInvertY ? On : Off);
	}
	ApplyGraphicsLockVisibility();
}

void USettingsScreen::UpdateOverallFromIndividual()
{
	if (!Combo_OverallQuality) return;
	TGuardValue<bool> Guard(bUpdating, true);
	int32 Level = -1;
	bool bSame = true;
	for (UComboBoxString* C : GetQualityCombos())
	{
		const int32 Q = ReadQuality(C, -1);
		if (Level < 0) Level = Q;
		else if (Q != Level) { bSame = false; break; }
	}
	Select(Combo_OverallQuality, (bSame && Level >= 0) ? QualityNames()[Level] : Custom);
}

void USettingsScreen::HandleOverallChanged(FString Selected, ESelectInfo::Type Type)
{
	if (bUpdating) return;
	const int32 Level = QualityNames().IndexOfByKey(Selected);
	if (Level == INDEX_NONE) return; // 选了“自定义”：不动
	TGuardValue<bool> Guard(bUpdating, true);
	for (UComboBoxString* C : GetQualityCombos()) SelectQuality(C, Level);
}

void USettingsScreen::HandleIndividualChanged(FString Selected, ESelectInfo::Type Type)
{
	if (bUpdating) return;
	UpdateOverallFromIndividual();
}

void USettingsScreen::HandleRestartOptionChanged(FString Selected, ESelectInfo::Type Type)
{
	if (bUpdating || !Text_RestartHint) return;
	Text_RestartHint->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USettingsScreen::ApplySettings()
{
	UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
	if (!S) return;

	ApplyControls();
	if (bGraphicsLocked)
	{
		// 局内：只保存操作设置（画面相关保持文件里的值）
		S->SaveSettings();
		UE_LOG(LogTemp, Log, TEXT("[Settings] Controls applied (graphics locked)"));
		return;
	}

	if (Combo_Resolution)
	{
		const int32 I = Combo_Resolution->GetSelectedIndex();
		if (Resolutions.IsValidIndex(I)) S->SetScreenResolution(Resolutions[I]);
	}
	if (Combo_WindowMode)
	{
		const int32 I = Combo_WindowMode->GetSelectedIndex();
		S->SetFullscreenMode(I == 0 ? EWindowMode::Fullscreen : I == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
	}
	if (Combo_FrameRate)
	{
		const int32 I = Combo_FrameRate->GetSelectedIndex();
		if (FrameRates.IsValidIndex(I)) S->SetFrameRateLimit(float(FrameRates[I]));
	}
	if (Combo_VSync) S->SetVSyncEnabled(Combo_VSync->GetSelectedOption() == On);

	S->SetShadowQuality(ReadQuality(Combo_Shadows, S->GetShadowQuality()));
	S->SetAntiAliasingQuality(ReadQuality(Combo_AntiAliasing, S->GetAntiAliasingQuality()));
	S->SetPostProcessingQuality(ReadQuality(Combo_PostProcess, S->GetPostProcessingQuality()));
	S->SetTextureQuality(ReadQuality(Combo_Textures, S->GetTextureQuality()));
	S->SetVisualEffectQuality(ReadQuality(Combo_Effects, S->GetVisualEffectQuality()));
	S->SetViewDistanceQuality(ReadQuality(Combo_ViewDistance, S->GetViewDistanceQuality()));
	S->SetFoliageQuality(ReadQuality(Combo_Foliage, S->GetFoliageQuality()));
	S->SetShadingQuality(ReadQuality(Combo_Shading, S->GetShadingQuality()));
	S->SetGlobalIlluminationQuality(ReadQuality(Combo_GlobalIllumination, S->GetGlobalIlluminationQuality()));
	S->SetReflectionQuality(ReadQuality(Combo_Reflections, S->GetReflectionQuality()));

	if (Combo_ResolutionScale)
	{
		const int32 I = Combo_ResolutionScale->GetSelectedIndex();
		if (Scales.IsValidIndex(I)) S->SetResolutionScaleValueEx(float(Scales[I]));
	}

	S->ApplySettings(false);
	S->SaveSettings();

	if (Combo_RayTracing)
	{
		const bool bOn = Combo_RayTracing->GetSelectedOption() == On;
		ApplyRayTracing(bOn);
		if (GConfig) GConfig->SetBool(Section, TEXT("bRayTracing"), bOn, GGameUserSettingsIni);
	}
	if (Combo_DLSS && IsDLSSAvailable())
	{
		const int32 Mode = FMath::Max(0, DLSSNames().IndexOfByKey(Combo_DLSS->GetSelectedOption()));
		ApplyDLSS(Mode);
		if (GConfig) GConfig->SetInt(Section, TEXT("DLSSMode"), Mode, GGameUserSettingsIni);
	}
	if (GConfig) GConfig->Flush(false, GGameUserSettingsIni);
	UE_LOG(LogTemp, Log, TEXT("[Settings] Applied and saved"));
}

void USettingsScreen::ResetToDefaults(bool bBenchmark)
{
	UGameUserSettings* S = UGameUserSettings::GetGameUserSettings();
	if (!S) return;
	S->SetToDefaults();
	if (bBenchmark)
	{
		S->RunHardwareBenchmark();
		S->ApplyHardwareBenchmarkResults();
	}
	RefreshFromCurrent();
	TGuardValue<bool> Guard(bUpdating, true);
	Select(Combo_RayTracing, Off);
	Select(Combo_DLSS, DLSSNames()[0]);
}

void USettingsScreen::Back()
{
	// 丢掉没应用的改动
	if (UGameUserSettings* S = UGameUserSettings::GetGameUserSettings())
	{
		S->LoadSettings(true);
	}
	if (AMainMenuPlayerController* MenuPC = Cast<AMainMenuPlayerController>(GetOwningPlayer()))
	{
		MenuPC->HideSettings();
	}
	else if (AShooterPlayerController* GamePC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		GamePC->HideSettings();
	}
	else
	{
		RemoveFromParent();
	}
	OnBackRequested.Broadcast();
}

void USettingsScreen::HandleApply() { ApplySettings(); }
void USettingsScreen::HandleReset() { ResetToDefaults(false); }
void USettingsScreen::HandleBack() { Back(); }

FReply USettingsScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		Back();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TArray<UWidget*> USettingsScreen::GetGraphicsWidgets() const
{
	TArray<UWidget*> Out;
	for (UWidget* W : { (UWidget*)Combo_Resolution.Get(), (UWidget*)Combo_WindowMode.Get(), (UWidget*)Combo_FrameRate.Get(),
		(UWidget*)Combo_VSync.Get(), (UWidget*)Combo_OverallQuality.Get(), (UWidget*)Combo_RayTracing.Get(),
		(UWidget*)Combo_ResolutionScale.Get(), (UWidget*)Combo_DLSS.Get(), (UWidget*)Text_RestartHint.Get() })
	{
		if (W) Out.Add(W);
	}
	for (UComboBoxString* C : GetQualityCombos()) Out.Add(C);
	return Out;
}

void USettingsScreen::SetGraphicsLocked(bool bLocked)
{
	bGraphicsLocked = bLocked;
	ApplyGraphicsLockVisibility();
}

void USettingsScreen::ApplyGraphicsLockVisibility()
{
	if (Panel_Graphics)
	{
		Panel_Graphics->SetVisibility(bGraphicsLocked ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	// 没放进 Panel_Graphics 的画面选项：禁用（不能点）
	for (UWidget* W : GetGraphicsWidgets())
	{
		W->SetIsEnabled(!bGraphicsLocked);
	}
	if (Combo_DLSS && !IsDLSSAvailable()) Combo_DLSS->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_RestartHint && bGraphicsLocked) Text_RestartHint->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_GraphicsLockedHint)
	{
		Text_GraphicsLockedHint->SetVisibility(bGraphicsLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USettingsScreen::ApplyControls()
{
	UShooterGameUserSettings* SS = UShooterGameUserSettings::GetShooterSettings();
	if (!SS) return;
	auto Read = [](const UComboBoxString* Combo, const TArray<float>& Steps, float Current)
	{
		if (!Combo) return Current;
		const int32 I = Combo->GetSelectedIndex();
		return Steps.IsValidIndex(I) ? Steps[I] : Current;
	};
	SS->MouseSensitivity = Read(Combo_Sensitivity, SensSteps, SS->MouseSensitivity);
	SS->ADSSensitivityMultiplier = Read(Combo_ADSSensitivity, ADSSteps, SS->ADSSensitivityMultiplier);
	SS->FieldOfView = Read(Combo_FOV, FOVSteps, SS->FieldOfView);
	if (Combo_InvertY) SS->bInvertY = Combo_InvertY->GetSelectedOption() == On;
	SS->SaveSettings();
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->ApplyControlSettings();
	}
}
