# t1009 — TR15 double mag + MagCheck

## Cause
1) **Double mag**: Mesh1P already swaps to `SKM_TR15_Body` at BeginPlay (`[KinRT] ... -> Body`), but Body can still skin a mag shell, and Mag DA (`SKM_Mag_TR15`) stacks on `Magazine`. Also Mag3P does not inherit Mesh3P `bOwnerNoSee`, so the owner can see a second mag.
2) **MagCheck "no anim"**: Hold-R *was* playing, but always `AM_RAR_FP_PCH_AssaultRifle_MagCheck` (log). That RAR montage does not match KIN/shared-arms Mag_Check, so it looks like nothing / wrong slot. TR15 already has `AM_RT_A_FP_TR15_Mag_Check` / `RT_A_FP_TR15_Mag_Check` on DefaultSlot — they were never selected.

## Fix (Live Coding succeeded 2026-11-02)
- `AWeapon::HideBuiltinMagazineKeepAttachment()` after RebuildAttachmentVisuals:
  - Reparent Mag DA off mag bone (KeepWorld) then hide builtin mag bones on gun mesh
  - `SetOwnerNoSee(true)` on Mag3P
  - Destroy orphan Mag_* skel comps
- `ResolveKinMagCheckMontage()` — prefer KIN RT Mag_Check (TR15/AK105/Mk14/MKR9/SRM12/Viper) on **DefaultSlot**; fallback RAR
- Log `Montage_Play failed` if slot/skeleton rejects

## Backup
`Saved\CodeBackup_20261102_054932_TR15MagMagCheck`

## PIE
1. Stop PIE if running, start again (need new BeginPlay).
2. Equip TR15 — expect **one** mag. Log: `[KinMag] ... builtin mag bones hidden` and `[KinRT] ... Body`.
3. Hold R — expect MagCheck anim. Log: `[MagCheck] playing AM_RT_A_FP_TR15_Mag_Check` (or dynamic from `RT_A_FP_TR15_Mag_Check`), **not** `AM_RAR_FP_PCH_AssaultRifle_MagCheck`.
