# HOWTO — Data-driven KINEMATION pilot (AK105 + TR15)

Path A: pack guns use the **shared RAR/AK arms ABP**. No more `bKinemationAttach`, path sniff, or forced `SK_Arms` / `ABP_FP_Arms`.

Set these on the **weapon Blueprint Class Defaults** (C++ cannot reliably edit `.uasset`).

---

## Shared for both BP_Weapon_AK105 and BP_Weapon_TR15

### Attach (FPS|Weapon|Attach)
| Property | Value |
|----------|--------|
| **Mesh1PAttachSocket** | `ik_hand_gun` |
| **Mesh1PAttachRotation** | Pitch `0`, Yaw `90`, Roll `-90` |
| **Mesh1PAttachLocation** | `(0, 0, 0)` |
| **bMesh1PAttachUsePivotOffset** | `true` (if arms have `VB ik_hand_gun_pivot`) |
| **OverrideArmsMesh1P** | None (keep shared Bandits / RAR arms) |
| **OverrideArmsAnimClass** | None (keep shared ABP, e.g. ABP_FirstPerson) |

If `ik_hand_gun` is missing on your shared arms mesh, either:
- promote/add that socket in the arms asset, or
- set **Mesh1PAttachSocket** to an existing grip (`weapon_r` / `SOCKET_Weapon`) and tune rotation/location.

### Sockets (FPS|Weapon|Sockets)
| Property | Value |
|----------|--------|
| **AimSocketName** (FPS\|Aiming) | `AimPoint` |
| **MuzzleSocketName** | `MuzzleSocket` |
| **FabrikSocketName** | `GripSocket` |
| **MagazineSocketName** | see per-gun below |
| **ReserveMagazineSocketName** | leave `SOCKET_Magazine_Reserve` (dual-mag only runs if socket exists — pack guns skip) |

### AttachmentSlots (fill rows)
| Slot tag | SocketName |
|----------|------------|
| Optic | `OpticSocket` |
| Muzzle | `MuzzleSocket` |
| Grip | `GripSocket` |
| Magazine | per-gun Mag* below |

### First-person sequences (FPS|FirstPerson|Sequences)
Assign on the BP (do **not** rely on C++ path tables anymore):

**AK105** (under `/Game/KINEMATION/TacticalShooterPack/Animations/UE5/AK105/Character/`):
- ArmsIdleSequence → `A_FP_AK105_Idle`
- ArmsAimIdleSequence → same idle (or dedicated aim if you author one)
- ArmsSprintSequence → `/Game/.../General/Character/A_FP_Sprint`
- ArmsEquipSequence → `A_FP_AK105_Draw`
- ArmsReloadEmptySequence → `A_FP_AK105_Reload_Empty`
- ArmsReloadTacticalSequence → `A_FP_AK105_Reload_Tac`
- WeaponFireSequence → `.../AK105/Weapon/A_W_AK105_Fire`

**TR15** (`.../UE5/TR15/Character/`):
- ArmsIdleSequence → `A_FP_TR15_Idle_Pose_Non_Grip` (or Grip variant when a vertical grip DA is equipped)
- ArmsEquipSequence → `A_FP_TR15_Draw`
- ArmsReloadEmptySequence → `A_FP_TR15_Reload_Empty`
- ArmsReloadTacticalSequence → `A_FP_TR15_Reload_Tactical`
- WeaponFireSequence → `.../TR15/Weapon/A_W_TR15_Fire`
- ArmsSprintSequence → same General `A_FP_Sprint`

Montage slots on the shared ABP stay as today (`DefaultSlot` / your existing reload/fire slots). Point WeaponData / montage rows at these sequences or play via the Arms*Sequence fallbacks.

---

## Per-gun MagazineSocketName

| Blueprint | MagazineSocketName | Notes |
|-----------|--------------------|-------|
| **BP_Weapon_AK105** | `Magazine_Parent` | Confirm in SK_AK105 Socket Manager |
| **BP_Weapon_TR15** | leave empty **or** add a Mag socket in editor | Mesh dump had no Mag* string — set AttachmentSlots Mag only after you add/confirm a socket |

Also set the Magazine row’s **SocketName** in AttachmentSlots to the same name (slot wins when filled; MagazineSocketName is the fallback).

---

## Optic DataAssets

For pack optics (MicroRedDot, XPS2, Phantom_TAC, DMCO, etc.):

| Property | Value |
|----------|--------|
| **SightSocketName** | `AimPoint` |
| **MountSocketName** | usually empty (weapon Optic slot `OpticSocket` mounts it) |
| **bSnapToMountSocket** | `true` if the pack expects identity snap |

Iron-sight ADS without optic uses weapon **AimSocketName = AimPoint**.

---

## Grip DAs (optional pilot)

- Mount via AttachmentSlots Grip → `GripSocket`
- **FabrikSocketName** on the grip DA: `GripSocket` (or the grip mesh’s hand socket)
- Shared-arms FABRIK only snaps when a grip is equipped (unchanged)

---

## After setting BPs

1. Compile C++ (Editor was not force-killed; build when Editor is closed or via Live Coding).
2. Open `BP_Weapon_AK105` / `BP_Weapon_TR15`, apply the table above, Compile & Save (clear any stale `bKinemationAttach` if it still shows as orphan).
3. PIE: equip AK105 → idle on shared arms, no ShooterComponent / ABP_FP_Arms error.
4. Attach optic → sits on `OpticSocket`; ADS aligns to optic `AimPoint`.
5. Fire / reload; reserve dual-mag should stay quiet (no `SOCKET_Magazine_Reserve` on pack meshes).
6. Switch to RAR/AK → still GripPoints path (Mesh1PAttachSocket empty).

---

## What C++ no longer does

- Path Contains `"KINEMATION"`
- Embedded `/Game/KINEMATION/...` anim tables
- Force `SK_Arms` + `ABP_FP_Arms`
- `EnsureKinemationShooterComponent` workaround
