# t1017 — All KIN guns use TR15 body-mag path

## Goal
Remaining KINEMATION guns behave like TR15 for magazines: keep Full mesh body-integrated mag, drop Mag DA, MagCheck/reload stay RAR-like (anim on Mesh1P, no hand-attach Mag DA).

## Code (`Weapon.cpp`)
- Removed BodySwaps (SRM-12 / Viper no longer swap to *Body).
- `EnsureKinemationSharedArmsAssets`: table `BodyMagGuns` forces Full SKM for AK105, TR15, Mk14, MKR9, MX68, R08, SRM12, Herrington, Viper; clears Mag slot `DefaultAttachment` / `bRequired=false`; `RemoveAttachment(Magazine)`; unhides mag bones.
- `ResolveMagazineVisualDuplicates`: `bKinKeepBodyMag` for all those classes — unhide body mag, strip Mag DA / orphan Mag comps (same as TR15).

## BP assets
Cleared Mag `DefaultAttachment` (verified None) on:
- AK105, TR15, Mk14, MKR9, SRM12, Viper
MX68 / R08 / Herrington had no Mag DA already.
Full meshes already pointed at SKM_* Full (not Body).

## Backups
- Code: `Saved\CodeBackup_20261102_075308_KinBodyMagAll`
- Assets (pre-edit): `Saved\AssetBackup_20261102_KinBodyMagAll`
- Build log: `Saved\KinBodyMagAll_BuildLog.txt` (Succeeded)
- BP clear log: `Saved\kin_bodymag_clear_mag4.txt`

## PIE
1. Cold-start editor (DLL rebuilt while editor was closed).
2. Equip AK105 / Mk14 / MKR9 / SRM12 / Viper — **one** body mag only. Log: `[KinMag] ... forced Full ... (body mag)` and `keep body mag; Mag DA dropped`.
3. MagCheck (hold R) / reload — mag motion from weapon body anim (RAR-like), not Mag DA.
4. TR15 unchanged (still body mag).
5. MX68 / R08 / Herrington — no Mag DA path; Full mesh only.
