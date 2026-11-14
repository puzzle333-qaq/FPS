# FPS Project — Code Reference (Co-op CQB edition)

## 1. About this document

This document describes the code **as it currently is**: the co-op CQB game on branch `cqb/strip-arena-modes` @ `925df4f` (2026-11-14). It replaces the previous edition, which documented the retired arena-PvP codebase (TDM / Search & Destroy / BDM, room lobby, buy shop) — that code now lives only on the `master` branch and is not covered here.

Reading order: §2 gives the map of the code and the one data flow that matters. §§3–5 cover the framework core inherited from the earlier game (character, combat, weapons). §§6–9 are the co-op game itself: mission framework, the deploy chain end-to-end, suspect AI, and the voice system. §§10–13 cover party/online, loadout, world interactables and UI. §§14–16 are configuration, tests, and honest caveats.

Conventions: class names are exact. Line numbers are intentionally rare (they rot); function names are the stable anchors. "Server" means the listen-server host; gameplay authority is always server-side.

## 2. Code map and data flow

Source layout (`Source/FPS`, 189 files ≈ 33.6k lines):

| Folder | Contents |
| --- | --- |
| `Game/` | `ACoopCqbGameMode`, `ACoopCqbGameState`, hostile AI (`AHostileAIController`, `AHostileAICharacter`, `AHostileSpawner`, `UHostileSquadSubsystem`), `UMissionRadioComponent`, `MissionVoiceTypes`, hostages, noise events |
| `World/` | `AMissionDeployPoint`, `AMissionObjective`, `AFactoryMissionObjective`, `ABreakableDoor`, glowsticks, ladders, traps, gas cans |
| `Ui/` | 26 widget classes: HUD family, `UMissionFlowWidget`, debrief slate, loadout, tablet, loading screen |
| `Weapon/` | `AWeapon`, `UWeaponData`, attachments |
| `Loadout/` | `UPouchInventoryComponent`, `UWarehouseLoadoutSaveGame` |
| `Online/` | `UPartySubsystem` (EOS party/session glue) |
| `Character/`, `Combat/`, `Stats/` | `AShooterCharacter` support, `UCombatComponent`, `UStatsComponent` |
| `Menu/`, `Range/`, `Settings/`, `Vision/` | Main-menu mode, range bench/preview rig, settings, NVG |
| `Tests/` (Private) | Automation tests: mission flow, factory voice |

The one flow to learn first is the deploy chain (§7). Almost everything else hangs off it: the GameMode it selects, the GameState it fills, the readiness it waits for, and the spawners it triggers.

## 3. Module and build layer

- Single runtime module `FPS`. Public deps: Core, CoreUObject, Engine, InputCore, EnhancedInput, GameplayTags, PhysicsCore, UMG, Niagara, AnimGraphRuntime, AIModule, NavigationSystem. Private deps include MoviePlayer (loading screens), LevelSequence/MovieScene (recon cinematics), OnlineSubsystem(Utils), Sockets.
- Engine association 5.8. Game targets `FPS` / `FPSEditor`.
- Plugins of note: OnlineSubsystemEOS + EOSShared + SocketSubsystemEOS + OnlineServicesEOS (online), GameplayStateTree, AnimationWarping, ModelContextProtocol (editor).

## 4. The inherited framework core

These systems predate the co-op game and remain its foundation. They are documented here in brief; their internal details changed less than everything around them.

### 4.1 AShooterCharacter — the player character

First-person arms mesh + third-person body, camera on a zero-length spring arm with lag. Owns movement modes (walk / sprint with stamina, crouch via Ctrl-hold or C-toggle, speed-triggered slide, prone crawl), Q/E lean (replicated, wall-probed, weapon trace follows the leaned camera), and per-frame locomotion data pushed into the AnimBPs. Largest file in the project (≈5,540 lines) — navigate it via your IDE's symbol search, not by scrolling.

### 4.2 UCombatComponent — the combat core

Fire/reload/ADS orchestration, fire modes, equip slots, interaction entry points. Server-validated actions with client prediction/rejection patterns carried from the framework era. ≈3,930 lines.

### 4.3 AWeapon + UWeaponData — weapons

`AWeapon` (≈4,370 lines) is the weapon actor: firing with hit detection, recoil pattern with view-kick recovery, ADS, scopes (including picture-in-picture magnified optics), flashlight/laser devices, melee sweep. All tuning lives in `UWeaponData` assets — swapping a weapon's model, animations or stats is a data change, not a code change. Attachments (§11) modify the data at runtime.

### 4.4 Damage, hit zones, health

Damage is resolved per body part (the same zone model the suspect AI aims with: legs/chest/head). `UStatsComponent` owns health and the server-authoritative stamina model (drain/regen, jump cost, depletion event). `ShooterPlayerController` (with its mission-flow partial in `ShooterPlayerController_MissionFlow.cpp`, §7.5) owns input routing and movement-mode arbitration.

## 5. From framework to co-op: what changed

For readers of the previous edition: the arena generation was **retired, not ported**. TDM / Search & Destroy / BDM game modes, the buy shop, the scoreboard, the room lobby and its session browser, and the old loadout save exist only on `master`. What carried over is the §4 core (character, combat, weapons, damage, stats) and the general server-authoritative style. Everything in §§6–13 — mission framework, deploy chain, suspect AI, voice, party, tablet, debrief — is new code written for the co-op game. The range survives, transformed: the old free-play range map became the squad hub (`Lvl_Range`) where loadout and mission selection live.

## 6. Co-op mission framework

### 6.1 ACoopCqbGameMode / ACoopCqbGameState

One GameMode runs **both** the range hub and mission maps; which role it is playing is decided per map in `InitGame`:

```cpp
bRangeHub = MapName.Contains(TEXT("Lvl_Range")) || UGameplayStatics::HasOption(Options, TEXT("RangeHub"));
ExpectedOperators = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("ExpectedOperators"), 1), 1, MaxOperators);
```

Known fragility (documented deliberately): hub detection partly *guesses from the map name*. The `RangeHub` travel option is the explicit override — the return-to-hub travel always sets it. Renaming the range map without updating this check breaks mission selection **silently**.

`InitGameState` mirrors `bRangeHub` and `ExpectedOperators` into `ACoopCqbGameState`, because the GameMode exists only on the server while the GameState is replicated to everyone (deploy point checks, HUD counters and readiness all read the GameState copy).

GameState also carries the mission phase (`ECoopCqbPhase`: Waiting / Briefing / InProgress / …), match state, primary objective, and the end-of-mission `FMissionReport` (§6.3).

### 6.2 Mission phases

`Waiting` (operators loading) → `Briefing` (gates in §7.4) → `InProgress` (objectives live) → debrief (`bDebriefOpen` on the GameState; the debrief UI reads the report) → return travel to the hub.

### 6.3 Scoring and the debrief report

`FMissionReport` (on the GameState) accumulates everything the debrief screen shows: objective counts, per-group points, `Score` / `MaximumScore`, the letter `Grade` string, hidden-objective completions, and penalties (e.g. surrendered suspects killed). The debrief needs **no** extra data pipeline — it renders this struct. A local `UMissionBestSaveGame` persists personal bests.

## 7. The deploy chain, end to end

### 7.1 AMissionDeployPoint — the mission station (range hub)

Placed in `Lvl_Range`. Interaction is host-only and hub-only. `Auth_SelectMission(PC, Index)` guards, in order: `HasAuthority()`, `IsRoomHost(PC)`, valid PC/pawn, index in range, **distance ≤ 500 units from the station**, `GameState->bRangeHub == true`, and no other deploy point already has a mission selected. It then sets `SelectedMission`, broadcasts the countdown UI (`Multicast_ShowCountdown` → clients open mission selection), starts a 0.25 s roster service timer, and — if the destination map name contains "Factory" — plays the briefing voice cue immediately via its `BriefingRadio` component (§9).

Squad members ready up (Y). `DeploySquad` re-verifies the roster (any change aborts with "队伍发生变化，请重新按 Y 就绪"), counts current PlayerControllers as `Operators`, resolves the destination's `FMissionDestination` (map + GameMode from the station's `Missions` array; the legacy single-map fallback field is unused when the array is non-empty), checks the map package exists, and calls travel.

### 7.2 UShooterGameInstance::TravelSquadTo — the travel

The GameInstance is the only object that survives a map change, so it owns travel. Standalone games take a simple `TravelTo` path. Networked games build a **travel URL**:

```
<LongPackageName>?listen?game=<GameModePath>?ExpectedOperators=<N>
```

- `listen` is added on listen servers; `game=` pins the destination GameMode.
- Before travelling it forces `bUseSeamlessTravel = false` (fresh controllers deliberately re-acknowledge possession/streaming) and **cleans the travel base URL** so options from the previous map — above all `RangeHub` — cannot leak into the next one.
- `World->ServerTravel(url)` pulls every connected client along.

An opt-in QA path exists in `UShooterGameInstance::Init`: with `-UIQA -UIQATravel` on the command line it waits 8 s in the range and travels to the factory whitebox with `ExpectedOperators=1`, so packaged builds can self-test this chain.

### 7.3 Arrival — InitGame / InitGameState

Covered in §6.1. One extra duty in `InitGameState`: older factory art maps have no `PlayerStart`, so if the map name contains "Factory" and no operator start exists, the GameMode spawns up to 4 transient deployment starts (ground-traced, teleport-spot checked) before login.

### 7.4 The two gates before StartMission

1. **Readiness gate.** On non-hub maps the GameMode starts a 0.25 s looping `ReadinessTimer` (`CheckOperatorReadiness`). Clients report loading completion (`AShooterPlayerController::Server_MissionLoaded`, serviced by `ServiceMissionReadiness` in the PlayerController's mission-flow partial). When connected ≥ `ExpectedOperators` and all are ready, the phase advances and the HUD status line shows "等待全员加载：x / y" (ready / expected).
2. **Briefing gate.** `StartBriefing` sets phase `Briefing`. Non-factory maps run a `BriefingTimer` (`BriefingDuration`) into `StartMission`. **Factory maps deliberately skip the timer**: they wait for every operator's deployment-view-ready report (`Auth_DeploymentViewReady`) — the hook the drone-recon deployment presentation hangs on — and only then call `StartMission`.

### 7.5 AShooterPlayerController's mission-flow partial

`Private/ShooterPlayerController_MissionFlow.cpp` (renamed 2026-11-14 from the misleading `MissionFlowController.cpp` — there is no such class) contains the PlayerController's mission half: `Server_MissionLoaded`, `ServiceMissionReadiness`, `InitializeMissionInput`, debrief actions (`Server_DebriefAction` — host retry / return to hub), loadout-protection RPCs, and readiness cleanup on `EndPlay`.

### 7.6 Spawning the opposition

`StartMission` → `SpawnHostilesFromLevel`: the GameMode iterates every `AHostileSpawner` placed in the level and calls `Auth_SpawnHostiles(HostileClass)`. A spawner is a **configuration point, not a random scatter**: it spawns `Count` hostiles in a row along its own right vector, spaced `SpacingCm` apart, facing its rotation, using deferred spawn so class, squad role (§8.3), body mesh (random from `LookPresets`) and weapon (random from `WeaponClasses`) are set before `FinishSpawning`.

### 7.7 Objectives and extraction

`AMissionObjective` is the base interactable objective; `AFactoryMissionObjective` specializes it for the factory mission's task types. Objectives only go live when the mission is `InProgress` and the map is not the hub (`IsMissionLive` / `bRangeHub` checks). The factory mission ("Rust Belt Workshop"):

- **Ledger**: one `AFactoryMissionObjective` of task type Ledger is chosen among the three office placements (财务室 / 档案室 / 经理室） at mission start.
- **Workbenches**: three seize objectives （改装间 / 组装间 / 弹药间）.
- **Hidden objectives** (not shown on the objective panel; announced by voice on trigger): silent entry (no alarm before the ledger), pulling the main breaker in the basement power room, the handwritten shipping note in the dispatch office, the finished-gun crate in the boiler room.
- Extraction completes the mission and opens the debrief (§6.3).

## 8. Suspect AI

### 8.1 AHostileAIController — the brain

A proper `AAIController` with explicit brain states: **Idle / Investigate / Combat / TakeCover / Retreat / Flee / Surrender**. Perception is sight plus hearing via `UAINoiseEvent`s: gunshots are audible at 4500, footsteps at 900. Losing sight starts a 3.5 s grace timer before de-escalation. In combat it searches cover, and on **first hit** rolls morale: surrender 0.30, last stand 0.25, flee 0.45 (values as tuned in code). Surrendered suspects can be taken into custody by players (a scored objective category, and killing a surrendered suspect is a report penalty).

### 8.2 AHostileAICharacter — the body

The pawn the controller drives: health/zones, weapon handling, surrender/incapacitated states, custody interaction. Spawned exclusively through spawners (§7.6) in missions.

### 8.3 Squads and aim

`UHostileSquadSubsystem` (world subsystem) hands out squad roles at spawn: a **suppressor** holds position and keeps fire up within a leash range; a **flanker** routes to the player's side. Aiming is zone-weighted — legs 70%, chest 25%, head 5% — which is why suppressive fire wounds more often than it kills.

### 8.4 Tuning surface

Spawner placement + `Count`/`SpacingCm`/class pools (level side), perception radii and morale probabilities (controller), and the squad-role mix (subsystem) are the three tuning surfaces. Behavior problems are usually placement (spawner facing, cover availability, NavMesh), not missing architecture.

## 9. Voice system

### 9.1 Cues

All mission voice lines are addressed by the `EMissionVoiceCue` enum (`MissionVoiceTypes.h`): `Briefing`, `Opening`, `HiddenSilent`, `HiddenShipping`, `HiddenBreaker`, `HiddenCrates`, `BenchSecured`, `CivilianSecured`, `LedgerFound`, `RoomClear`, `SuspectDown`, `SuspectIncap`, `TargetSecured`, and six shouts (`ShoutHandsUp`, `ShoutShowHands`, `ShoutSeeHands`, `ShoutDontMove`, `ShoutDropWeapon`, `ShoutGround`). The `MissionVoice` namespace maps cue → asset name (`AssetName`), expected `Duration`, and `LoadSound`; `NextWarning` rotates shout variants without immediate repeats.

### 9.2 UMissionRadioComponent — playback

A queued radio: commander lines and reports play as transmissions (with priority/interrupt rules — e.g. a new briefing cuts the current transmission), while shouts are played on-site by squadmate characters, not over the radio. Owners: the deploy point (briefing), objectives (reports/hidden triggers), hostile/hostage interactions (shouts and secured/down reports), the GameMode.

### 9.3 Assets

English is the working language. Cast (locked for the whole game): commander — deep/stoic male, radio-processed (300–3200 Hz bandpass, compression, squelch); squadmate — forceful middle-aged male, shouts recorded with a driven, gritty chain, reports on the radio chain. WAVs import directly into UE. Known debt: `HiddenShipping` and `HiddenCrates` audio still uses superseded script wording; re-recording is pending (asset swap only — no code change needed).

## 10. Party and online

`UPartySubsystem` (game-instance subsystem) wraps EOS sessions/lobbies for the up-to-4 squad: create/join, roster, and the travel coordination that keeps the party together across `ServerTravel`. Internet play is EOS-based (no Steam dependency); `OnlineSubsystemSteam` remains enabled in the project file for legacy reasons but is not the play path. LAN/local testing uses the same listen-server flow.

## 11. Loadout and attachments

- **Range bench** (`ARangeLoadoutBench` + `URangeLoadoutWidget`): full-screen loadout in the hub. `ALoadoutPreviewRig` shows the operator with the selected gear.
- **Pouch inventory**: `UPouchInventoryComponent` — exactly 13 slots.
- **Magazines are a queue.** Reload takes the next magazine in queue order; the removed (possibly partial) magazine returns to the tail; fully-empty magazines leave the queue. The HUD renders the queue in exact order, current magazine first, and only during magazine inspection (long-press R).
- **Attachments**: data-driven (`UAttachmentData` / gunsmith definitions): barrel, handguard, muzzle, optic, grip, stock, tactical device, magazine. Weapon behavior derives from `UWeaponData` + applied attachments.
- **Persistence**: `UWarehouseLoadoutSaveGame` (presets), `UMissionBestSaveGame` (per-mission bests). (The framework-era `UShooterLoadoutSave` exists only on `master`.)

## 12. World interactables

- **ABreakableDoor**: zone-based open modes — handle, push, kick — with breakable panels. (Known bug on the wrap-up list: kick currently swings the door toward the player; the open-direction sign in `ResolveOpenDirectionAwayFrom` is the suspect.)
- **AMissionDeployPoint** (§7.1), **AMissionObjective / AFactoryMissionObjective** (§7.7), **AHostageActor** (custody/rescue), glowsticks (chem-lights, counted on the HUD), ladders, tripwire traps, gas cans, blood pools.
- `UTraversalComponent` is a leftover from a removed vault/mantle feature — unreferenced, scheduled for deletion; do not build on it.

## 13. UI system

Design language (locked): near-black translucent panels, off-white text `#EDEEF0`, single warning red, Barlow Condensed / Rajdhani + Source Han Sans SC, squared corners, no gold in persistent UI.

- **HUD**: bottom-left device strip only (chem-light count, NVG / flashlight / laser state by icon brightness + dot); magazine queue appears only while inspecting (§11). There is deliberately **no** persistent numeric ammo counter.
- **MissionFlowWidget**: mission selection, status line （等待全员加载 x/y), objectives, debrief (§6.3).
- **Tablet** (Tab): a 3D tablet whose screen is a Widget Component running the briefing layout (light theme, full-bleed inside the bezel; LED feel via screen brightness + bloom, not a dot matrix).
- **Loading**: two stages — a per-map drone-recon video (Sequencer-rendered, played through the MoviePlayer loading framework with a widget recon-HUD frame; factory recon sequence asset `LS_Factory_Recon_V3`) gated to a 10 s loop ending overhead of the spawn, then in-level gating with a minimal status line while the §7.4 gates run.
- **Debrief & loadout screens**: restyles of existing data flows (§6.3, §11) — no parallel data pipelines.

## 14. Configuration layer

- `Config/DefaultEngine.ini`: `GameDefaultMap` = MainMenu map; `GameInstanceClass` = `BP_ShooterGameInstance`; `GlobalDefaultGameMode` = `/Script/FPS.ShooterGameModeBase` (the menu/base mode — missions override per travel). Custom collision channels from the framework era are retained.
- EOS credentials/product ids live in the project's EOS config (not in this document).
- **Maps to Cook**: any mission map offered by a deploy point (including the factory map) must be in the packaging cook list, or selection fails the `DoesPackageExist` check at runtime.

## 15. Tests

Automation tests live in `Source/FPS/Private/Tests`, notably `MissionFlowTests` (deploy/readiness flow, including `ExpectedOperators` handling) and `FactoryVoiceTests` (cue/asset wiring). Run them from the editor's Automation window after touching the deploy chain or the voice tables.

## 16. Caveats and known issues (2026-11-14)

- **Not yet packaged**: no Development/Shipping packaging validation yet; texture streaming pool is over budget on current content (performance pass pending).
- **bRangeHub name-guessing** (§6.1) is the sharpest known fragility in the mission flow.
- **Door kick direction** (§12) opens toward the player; fix is diagnosed but not yet applied.
- **AI tuning in the factory map** (spawner facing, cover, NavMesh) is in progress; the architecture (§8) is complete.
- **Untracked EOS plugin**: the OnlineSubsystemEOS plugin files are present locally but not yet committed to the repo — a fresh clone cannot build the online path until they are.
- **Two voice lines** (§9.3) await re-recording with final wording.
- The `master` branch is a read-only archive of the arena game; do not merge it into the co-op branch.
