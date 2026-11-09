# HOWTO — KINEMATION attachments (Grip / Muzzle / Optic / Tactical)

Custom slots: **Grip, Muzzle, Optic, combined Laser / Flashlight**. Magazines are fixed (required single DA, or body mag with no Mag slot).

## DataAssets (`/Game/FPS/Data/Kinemation/`)

| DA | Slot | Mesh |
|----|------|------|
| DA_Kinemation_VerticalGrip | Grip | SM_Zetco_VerticalGrip |
| DA_Kinemation_AngledGrip | Grip | SM_AngledForegrip |
| DA_Kinemation_MCS_Suppressor | Muzzle | SM_MCS |
| DA_Kinemation_MKR9_Suppressor | Muzzle | SM_MKR9_Suppressor (MKR9 only) |
| DA_Kinemation_MicroRedDot | Optic | SM_MicroRedDot (SightSocket=AimPoint) |
| DA_Kinemation_DMCO | Optic | SKM_DMCO skeletal (AimPoint) |
| DA_Kinemation_LS3_Light | Tactical | SM_LS3 (bProvidesFlashlight) |
| DA_Kinemation_Laser | Laser | SM_LaserLM from CGrPCC; provides both laser and flashlight |

`GetAttachmentSlots` and runtime `EnsureDeviceSlots` use the same normalization: former Tactical / Laser / Flashlight device entries become one **Laser** rail. There is no independent Flashlight slot. Non-device Tactical attachments remain separate. LS3 remains an alternative flashlight-only attachment on the combined rail.

TR15, Mk14, SRM12, MKR9 and MX68 use the authored **TacticleSocket** (spelling intentional) on both weapon meshes. `ResolveAttachmentSocket` looks up the resolved equipment slot, so a legacy Tactical-tagged DA still follows that rail instead of falling back to the weapon root. `MountSocketName` can remain empty to use each weapon's own socket.

The combined Laser DA emits light from **Light_Sckt** and laser from **Laser_Sckt**, matching the SM_LaserLM model's actual sockets.

## Per-gun (what works)

| Gun | Optic | Muzzle | Grip | Tactical (laser+light) | Mag |
|-----|-------|--------|------|------------------------|-----|
| AK105 | OpticSocket | MuzzleSocket | GripSocket | needs UnderbarrelSocket | fixed Mag DA |
| TR15 | OpticSocket | MuzzleSocket | GripSocket | TacticleSocket | body mag (no Mag slot) |
| Mk14 | OpticSocket | MuzzleSocket | — | TacticleSocket | fixed Mag DA |
| MKR9 | OpticSocket | MuzzleSocket (+own suppressor) | — | TacticleSocket | fixed Mag DA |
| MX68 | OpticSocket | MuzzleSocket | GripSocket | TacticleSocket | body mag |
| R08 | OpticSocket | — (no muzzle sock) | — | UnderbarrelSocket OK | body/speedloader |
| SRM12 | OpticSocket | MuzzleSocket | — | TacticleSocket | fixed Mag DA |
| Herrington | — | Suppressor bone | — | needs UnderbarrelSocket | body/shell |
| Viper | — | — | — | — | fixed Mag DA only |

## Test

1. Open Gunsmith on AK105 / TR15 / R08.
2. Optic: Micro Red Dot / DMCO. Muzzle: MCS (MKR9 also own suppressor). Grip where listed.
3. On the five TacticleSocket guns, equip Laser in the combined Laser / Flashlight row. Check 1P and 3P placement.
4. Mag should not be user-swappable (one fixed option or absent).
5. SideMouse2 (`ThumbMouseButton2`): short press toggles the selected function; hold 0.35s switches laser / flashlight. Their on/off states are independent, so both may be enabled.
6. Old loadouts containing both devices are reduced to one physical device, preferring the combined Laser.
7. Current asset backups: `Saved/FlashIdleFix/CombinedTacticalBackup`. Earlier attachment-fill backup: `Saved/AssetBackup_20261103_025050_KinAttachFill`.
