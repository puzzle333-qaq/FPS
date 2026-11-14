**Language / 语言：** **English** ｜ [中文](README.zh-CN.md)

---

# FPS — Co-op CQB FPS (Unreal Engine 5.8, C++)

A **4-player co-op close-quarters-battle (CQB) FPS** built with **Unreal Engine 5.8 + C++**, benchmarked against *Ready or Not*. A squad deploys from a range hub into mission maps, clears rooms against a morale-driven suspect AI, completes randomized objectives, extracts, and gets a graded debrief.

The project grew out of an earlier tactical PvP framework (character, weapon and combat core). The current game — the co-op mission loop — lives on the default branch `cqb/strip-arena-modes`. The legacy arena modes (TDM / Search & Destroy / BDM) survive only on the `master` branch and are **no longer part of the game**.

> This document was rewritten on 2026-11-14 against the current code (`cqb/strip-arena-modes` @ `925df4f`).
> For the systems-level deep dive (mission flow, AI, voice, networking), see [DOCUMENTATION.md](DOCUMENTATION.md).
> Chinese version: [README.zh-CN.md](README.zh-CN.md) ｜ [DOCUMENTATION.zh-CN.md](DOCUMENTATION.zh-CN.md)

---

## 1. Project Overview

| Item | Value |
| --- | --- |
| Engine version | Unreal Engine 5.8 (`EngineAssociation: "5.8"` in [FPS.uproject](FPS.uproject)) |
| Primary module | `FPS`, type Runtime ([FPS.Build.cs](Source/FPS/FPS.Build.cs)) |
| Language | C++ (189 source files, ≈33,600 lines), gameplay exposed to Blueprints where needed |
| Input | Enhanced Input (`UEnhancedInputComponent` + `UInputMappingContext`) |
| Game default map | `/Game/FPS/Level/MainMenu/MainMenu` — the main menu level |
| GameInstance | `BP_ShooterGameInstance` (`UShooterGameInstance`) — owns squad travel between maps |
| Global default GameMode | `/Script/FPS.ShooterGameModeBase` (menu); missions run `ACoopCqbGameMode` |
| Online | Epic Online Services (EOS) party + sessions; listen-server host is authoritative |
| Build targets | `FPS` (Game), `FPSEditor` (Editor) |

Module dependencies ([FPS.Build.cs](Source/FPS/FPS.Build.cs)):

- Public: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `GameplayTags`, `PhysicsCore`, `UMG`, `Niagara`, `AnimGraphRuntime`, `AIModule`, `NavigationSystem`
- Private: `Slate`, `SlateCore`, `MoviePlayer`, `AssetRegistry`, `LevelSequence`, `MovieScene`, `OnlineSubsystem`, `OnlineSubsystemUtils`, `Sockets`

Enabled plugins (relevant subset, [FPS.uproject](FPS.uproject)): `OnlineSubsystemEOS`, `EOSShared`, `SocketSubsystemEOS`, `OnlineServicesEOS`, `GameplayStateTree`, `AnimationWarping`, `ModelContextProtocol` (editor tooling).

## 2. Feature Overview

| System | What is implemented | Main code |
| --- | --- | --- |
| Mission loop | Main menu → range hub → mission select (host) → squad travel → deploy gates → objectives → extraction → graded debrief → return to hub | [CoopCqbGameMode.cpp](Source/FPS/Private/Game/CoopCqbGameMode.cpp), [MissionDeployPoint.cpp](Source/FPS/Private/World/MissionDeployPoint.cpp) |
| Deploy chain | Host picks a mission at the deploy point, squad readies up, `UShooterGameInstance::TravelSquadTo` server-travels everyone with `ExpectedOperators` in the travel URL; destination waits for all operators to load before briefing | [ShooterGameInstance.cpp](Source/FPS/Private/ShooterGameInstance.cpp), [MissionFlowController → ShooterPlayerController_MissionFlow.cpp](Source/FPS/Private/ShooterPlayerController_MissionFlow.cpp) |
| Factory mission ("Rust Belt Workshop") | Night raid on an abandoned factory's underground arms workshop: ledger hidden randomly in one of 3 offices, 3 workbenches to seize, extraction; 4 hidden objectives (silent entry, breaker, shipping note, boiler-room cache) | [FactoryMissionObjective.cpp](Source/FPS/Private/World/FactoryMissionObjective.cpp) |
| Suspect AI | Brain states Idle / Investigate / Combat / TakeCover / Retreat / Flee / Surrender; hearing (gunshot 4500, footsteps 900), cover search, 3.5 s lose-sight grace, first-hit morale rolls (surrender 0.30 / last stand 0.25 / flee 0.45), squad roles (suppressor, flanker), aim zones (leg 70% / chest 25% / head 5%) | [HostileAIController.cpp](Source/FPS/Private/Game/HostileAIController.cpp), [HostileSpawner.cpp](Source/FPS/Private/Game/HostileSpawner.cpp), [HostileSquadSubsystem.cpp](Source/FPS/Private/Game/HostileSquadSubsystem.cpp) |
| Voice system | 19 English radio lines (commander + squadmate): briefing, opening, 4 hidden-objective triggers, 7 reports, 6 compliance shouts; cue enum + queued radio component | [MissionVoiceTypes.h](Source/FPS/Public/Game/MissionVoiceTypes.h), [MissionRadioComponent.cpp](Source/FPS/Private/Game/MissionRadioComponent.cpp) |
| Character & combat | First-person arms + third-person body, lean, crouch/slide/prone, stamina, per-body-part damage, hit reactions — carried over from the framework era and still the live core | [ShooterCharacter.cpp](Source/FPS/Private/ShooterCharacter.cpp), [CombatComponent.cpp](Source/FPS/Private/CombatComponent.cpp) |
| Weapons & attachments | Data-driven weapons (`UWeaponData`), attachment system (muzzle / optic / grip / stock / tactical / magazine…) with a gunsmith screen, pouch inventory (13 slots), warehouse loadout save, loadout preview rig | [Weapon.cpp](Source/FPS/Private/Weapon/Weapon.cpp), [RangeLoadoutWidget.cpp](Source/FPS/Private/Ui/RangeLoadoutWidget.cpp) |
| Doors & world | Breakable doors with handle / push / kick open modes, glowsticks, ladders, tripwire traps, gas cans, hostages | [BreakableDoor](Source/FPS/Public/World/BreakableDoor.h), [MissionObjective.cpp](Source/FPS/Private/World/MissionObjective.cpp) |
| HUD & UI | Minimal CQB HUD (device status strip; magazine queue shown only while long-pressing R to inspect), mission tablet (Tab), mission-flow / debrief screens, drone-recon loading presentation | [MissionFlowWidget.cpp](Source/FPS/Private/Ui/MissionFlowWidget.cpp), [MissionTabletWidget](Source/FPS/Public/Ui/MissionTabletWidget.h) |
| Party & sessions | EOS-based party (up to 4), host-authoritative listen server, cross-map squad integrity | [PartySubsystem.cpp](Source/FPS/Private/Online/PartySubsystem.cpp) |

## 3. Directory Layout

```
FPS/
├─ Config/                 Engine/game ini (defaults, EOS, input)
├─ Content/
│  ├─ FPS/
│  │  ├─ Level/            MainMenu, Lvl_Range (hub), Lvl_Boat, Testmap
│  │  ├─ Coop/             Co-op HOWTO notes
│  │  ├─ Range/            Range-era HOWTO notes (working logs)
│  │  ├─ Weapon/           Weapon BPs, Kinemation anim sets
│  │  ├─ Data/             WeaponData / attachment data assets
│  │  └─ Input/            Input actions & mapping contexts
│  └─ Maps/                Lvl_Factory_Whitebox (factory mission)
├─ Docs/                   Design & handoff notes (Chinese)
├─ Source/FPS/
│  ├─ Public/  Private/    Game, World, Ui, Weapon, Loadout, Online, …
│  └─ Private/Tests/       Automation tests (mission flow, factory voice)
├─ DOCUMENTATION.md        Systems reference (this rewrite)
└─ README.md
```

## 4. Getting Started

1. Install **Unreal Engine 5.8**.
2. Clone this repository and switch to `cqb/strip-arena-modes` (the default branch).
3. Right-click `FPS.uproject` → **Generate Visual Studio project files** (the project is developed with Cursor/VS + the UE build tools).
4. Build the `FPSEditor` target, then open `FPS.uproject`.
5. Play starts at the main menu; enter the **range hub** (`Lvl_Range`) to pick loadout and missions. The deploy point (mission station) is host-only and only works in the range hub.

For a fast solo check of the deploy chain there is an opt-in QA shortcut: launch with `-UIQA -UIQATravel` and the game auto-travels from the range to the factory whitebox with 1 expected operator.

## 5. Controls

Default bindings (Enhanced Input; see the mapping contexts under `Content/FPS/Input` for the authoritative list):

| Input | Action |
| --- | --- |
| WASD / Mouse | Move / look |
| Left Mouse | Fire |
| Right Mouse (hold) | Aim down sights |
| R | Reload — **long-press R** inspects the magazine and shows the magazine queue |
| Shift | Sprint |
| Ctrl (hold) / C (toggle) | Crouch (slide when pressed at speed) |
| Space | Jump |
| Q / E | Lean left / right |
| F | Interact (deploy point, doors, custody…) |
| Y | Ready up at the deploy point |
| Tab | Mission tablet |
| 1 / 2 / 3 … | Weapon slots (main / secondary / pistol / melee) |
| Fire-mode key | Switch fire mode |
| Device keys | Toggle flashlight / laser / night vision (states shown on the HUD strip) |

## 6. Key Maps and Assets

| Map | Role |
| --- | --- |
| `/Game/FPS/Level/MainMenu/MainMenu` (+ `Lvl_MainMenu`) | Main menu |
| `/Game/FPS/Level/Lvl_Range` | Range hub: loadout bench, deploy point, party staging (**the** pre-mission map) |
| `/Game/Maps/Lvl_Factory_Whitebox` | Mission 1 — factory ("Rust Belt Workshop") |
| `/Game/FPS/Level/Lvl_Boat` | Earlier test mission map |
| `/Game/FPS/Level/Testmap` | Sandbox / legacy test map |

Key blueprints: `BP_ShooterGameInstance`, `BP_MainMenuGM` (child of `AMainMenuGameMode`), `BP_ShooterCharacter`, `WBP_ShooterOverlay` (HUD), mission and loadout widgets under `Content/FPS/UI`.

## 7. Architecture at a Glance

The spine of the game is one chain (read it in this order):

```
MissionDeployPoint::Auth_SelectMission   host picks a mission (range hub only)
  → squad ready-up + roster re-check
  → MissionDeployPoint::DeploySquad
  → UShooterGameInstance::TravelSquadTo  builds the travel URL (?listen?game=…?ExpectedOperators=N)
  → World::ServerTravel                  every client is pulled along
  → ACoopCqbGameMode::InitGame           reads the URL: bRangeHub? ExpectedOperators?
  → InitGameState                        mirrors both into ACoopCqbGameState (replicated)
  → readiness gate                       all operators connected + loaded (0.25 s poll)
  → briefing gate                        briefing timer — factory maps instead wait for deployment-view ready
  → ACoopCqbGameMode::StartMission
  → SpawnHostilesFromLevel               every AHostileSpawner in the level spawns its row
```

Two design rules hold the whole thing together: **the GameInstance is the only object that survives a map change**, so cross-map data rides in the travel URL; and **the GameMode exists only on the server**, so anything clients must see is mirrored into the GameState.

## 8. Networking Model

- Listen-server, host-authoritative: gameplay decisions (mission select, spawn, morale, scoring) are server-only (`HasAuthority` guards throughout).
- Party/sessions via EOS (`UPartySubsystem`); the squad travels together via `ServerTravel` (non-seamless, deliberately, so fresh controllers acknowledge possession/streaming in the destination).
- Readiness is explicit: clients report `Server_MissionLoaded`; the mission starts only when connected ≥ expected and everyone is ready.

## 9. Known Limitations (2026-11-14)

- The game is in a testing/validation phase, not yet packaged for distribution; Development/Shipping packaging verification is pending.
- Texture streaming pool is over budget on current content — a performance pass is still owed.
- Suspect AI is functional but behavior tuning in the factory map is ongoing.
- A few legacy systems remain in the tree (e.g. `UTraversalComponent`, a leftover from a removed vault feature).
- Two hidden-objective voice lines still use earlier script wording; re-recording is pending.

## 10. Weapon Attachments and Loadout

- Loadout is managed at the **range bench** (`ARangeLoadoutBench` / `RangeLoadoutWidget`): weapons, attachments, gear.
- Attachments are data-driven (`AttachmentData`, `Gunsmith` definitions) and applied to the live weapon; a preview rig shows the result.
- Carried magazines form a **queue**: reloading takes the next magazine and returns the removed (partial) magazine to the tail of the queue; the HUD shows this queue, in order, only while inspecting (long-press R).
- Loadout presets persist in `UWarehouseLoadoutSaveGame`; best mission results persist in `UMissionBestSaveGame`.

## 11. Further Reading

- [DOCUMENTATION.md](DOCUMENTATION.md) — systems reference: mission flow, AI, voice, networking, data.
- [DOCUMENTATION.zh-CN.md](DOCUMENTATION.zh-CN.md) — Chinese edition.
- `Docs/` — design and handoff notes (Chinese).
- `Content/FPS/Coop/*_HOWTO.txt` — co-op systems working notes; `Content/FPS/Range/*_HOWTO.txt` — earlier working logs (partly historical).
