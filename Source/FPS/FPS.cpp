// Fill out your copyright notice in the Description page of Project Settings.

#include "FPS.h"
#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"

class FFPSGameModule : public FDefaultGameModuleImpl
{
public:
 virtual void StartupModule() override
 {
  FDefaultGameModuleImpl::StartupModule();
  // The Police pack was moved without asset migration; repair its original package paths.
  FCoreRedirects::AddRedirectList({FCoreRedirect(ECoreRedirectFlags::Type_Package|ECoreRedirectFlags::Option_MatchPrefix,
   TEXT("/Game/MMC/"),TEXT("/Game/Police/MMC/"))},TEXT("FPS.PoliceAssetMigration"));
 }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FFPSGameModule, FPS, "FPS");
