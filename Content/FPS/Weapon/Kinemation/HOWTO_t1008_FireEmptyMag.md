# KINEMATION t1008 — Fire / EmptyReload / TR15 Mag

## Runtime bridge (Live Coding OK 2026-11-02)
`AWeapon::EnsureKinemationSharedArmsAssets()` on BeginPlay:

1. **TR15 / SRM12 / Viper** — Mesh1P/3P `SKM_*` full → `*_Body` (stops baked mag + Mag DA double).
2. `ReserveMagazineSocketName = None` (no Infima dual-mag second mesh).
3. Clear `OverrideArmsAnimClass` / `OverrideArmsMesh1P` (drop ABP_FP_Arms / SK_Arms).
4. Soft-load `RT_A_FP_*` Idle/Draw/ReloadEmpty/ReloadTac (+ Fire where pack has Character Fire).
5. Soft-load `A_W_*` Weapon Fire / Reload Empty / Tac / Draw.
6. Rifles **without** pack Character Fire (AK105/TR15/Mk14/MKR9/MX68): ArmsFire = RAR `A_RAR_FP_PCH_AssaultRifle_Fire` (+ Aimed for ADS).

## Permanent BP wire (editor closed)
`Saved/kin_t1008_wire_only.py` via UnrealEditor-Cmd `-run=PythonScript`.

## PIE checklist
- TR15: only **one** mag on `Magazine`; fire — hands move (RAR fire fallback); empty R — empty reload arms+weapon.
- R08/SRM12/Herrington/Viper: pack RT Fire on arms.
- Log: `[KinRT] ... Mesh1P -> Body` and `arms fire=... empty=...`

## Backups
- `Saved/AssetBackup_20261102_052840_KinT1008`
- `Saved/CodeBackup_20261102_053727_KinT1008Ensure`
