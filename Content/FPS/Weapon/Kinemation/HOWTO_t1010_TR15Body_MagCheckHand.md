# t1010/t1011 — TR15 keep body mag + KIN MagCheck mag-in-hand

## Flipped (TR15)
- Previous wrong fix hid **body** mag bones and kept Mag DA.
- Now: force Full `SKM_TR-15`, clear Mag slot `DefaultAttachment` / `bRequired=false`, `RemoveAttachment(Magazine)`, unhide Magazine bones, destroy any Mag DA comps.
- TR15 removed from BodySwaps (no more Body-only mesh).

## MagCheck (KIN Mag-DA guns: AK105/Mk14/MKR9/SRM12/Viper/…)
- Idle still: Mag DA reparented off mag bone + body mag bones hidden (single mag at well).
- MagCheck start: `BeginMagCheckHandAttach` snaps Mag DA skel to arms `hand_l`, then `UpdateMagazineAmmoVisuals` (Ammo_* from Ammo count).
- MagCheck end / CancelInspect / tick when montage ends: `EndMagCheckHandAttach` restores Mag DA to gun.

## Logs to watch
- `[KinMag] TR15 forced Full SKM_TR-15 (body mag)`
- `[KinMag] ... keep body mag; Mag DA dropped`
- `[MagCheck] ... Mag DA attached to hand_l (ammo=N)`
- `[MagCheck] playing AM_RT_A_FP_*_Mag_Check`

## Backup / build
- Backup: `Saved\CodeBackup_20261102_055828_TR15BodyMag_MagCheckHand`
- Build: Succeeded (`Saved\compile_tr15_magcheckhand_20261102.txt`)

## PIE
1. Cold-start editor (DLL rebuilt while editor was closed).
2. TR15: one body mag only; MagCheck uses weapon MagCheck anim (no Mag DA in hand).
3. AK105/Mk14/etc: MagCheck should show Mag DA in left hand with ammo bones matching Ammo.
4. If hand offset wrong, tune `BeginMagCheckHandAttach` relative Loc/Rot.
