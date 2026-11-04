# t1016 — KIN Mag DA reload / MagCheck follow RAR (anim on Mesh1P)

## Root cause
1. Mag DA attached to Mag bone, then `HideBoneByName` zeroed that bone's scale → Mag DA collapsed / looked glued.
2. MagCheck used raw `Montage_Play` on weapon AnimInstance — no AnimBP ⇒ weapon MagCheck never played.
3. Reload preferred shared RAR WeaponData montages over KIN `ArmsReload*` sequences → wrong WEP skeleton on Mesh1P.
4. `PlayMagazineReload` only played Empty body sequence; Tactical Mag-DA guns had no body Mag-bone drive (no MagReloadAnim on KIN Mag DA).

## Fix (RAR-like, no hand-attach)
- Mag DA: `SetUsingAbsoluteScale(true)` after hide so it keeps size and follows Mag bone loc/rot.
- MagCheck: `PlayWeaponMontageOrSequence` (falls back to PlayAnimation).
- Prefer KIN ArmsReload sequences over RAR montages when present.
- `PlayMagazineReload` plays Empty **and** Tactical `WeaponReload*Sequence` on Mesh1P/3P.

## Backup / build
- Backup: `Saved\CodeBackup_20261102_074029_KinMagAnimFix`
- Build: Succeeded (`Saved\KinMagAnimFix_BuildLog.txt`)

## PIE
1. If editor was open during link: Ctrl+Alt+F11 or soft restart PIE.
2. AK105 / Mk14 / MKR9: R reload — Mag DA should leave the well with the gun Mag bone.
3. Long-press R MagCheck — same; logs: `[KinMag] PlayMagazineReload body=A_W_*`, `[MagCheck] weapon mesh playing`, `[KinMag] Mag DA absolute-scale`.
4. TR15 unchanged (body mag, Mag DA dropped).
