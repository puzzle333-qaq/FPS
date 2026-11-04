# HOWTO — KINEMATION attachments (Grip / Muzzle / Optic / Tactical)

Custom slots only: **Grip, Muzzle, Optic, Laser+Flashlight (via Tactical)**. Magazines are fixed (required single DA, or body mag with no Mag slot).

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
| DA_Kinemation_Laser | Tactical | SM_LaserLM from CGrPCC (pack has no laser body; bProvidesLaser) |

Runtime `EnsureDeviceSlots` splits one **Tactical** whitelist into **Laser** + **Flashlight** slots.

## Per-gun (what works)

| Gun | Optic | Muzzle | Grip | Tactical (laser+light) | Mag |
|-----|-------|--------|------|------------------------|-----|
| AK105 | OpticSocket | MuzzleSocket | GripSocket | needs UnderbarrelSocket | fixed Mag DA |
| TR15 | OpticSocket | MuzzleSocket | GripSocket | needs UnderbarrelSocket | body mag (no Mag slot) |
| Mk14 | OpticSocket | MuzzleSocket | — | needs UnderbarrelSocket | fixed Mag DA |
| MKR9 | OpticSocket | MuzzleSocket (+own suppressor) | — | needs UnderbarrelSocket | fixed Mag DA |
| MX68 | OpticSocket | MuzzleSocket | GripSocket | needs UnderbarrelSocket | body mag |
| R08 | OpticSocket | — (no muzzle sock) | — | UnderbarrelSocket OK | body/speedloader |
| SRM12 | OpticSocket | MuzzleSocket | — | needs UnderbarrelSocket | fixed Mag DA |
| Herrington | — | Suppressor bone | — | needs UnderbarrelSocket | body/shell |
| Viper | — | — | — | — | fixed Mag DA only |

## Test

1. Open Gunsmith on AK105 / TR15 / R08.
2. Optic: Micro Red Dot / DMCO. Muzzle: MCS (MKR9 also own suppressor). Grip where listed.
3. R08: Tactical → LS3 + Laser (should appear as separate Laser/Flashlight after spawn).
4. Mag should not be user-swappable (one fixed option or absent).
5. Backup: `Saved/AssetBackup_20261103_025050_KinAttachFill`
