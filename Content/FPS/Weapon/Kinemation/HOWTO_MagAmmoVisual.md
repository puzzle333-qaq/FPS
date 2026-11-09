MagCheck / magazine ammo visuals — 2026-11-02
=============================================

Compile
-------
1st (A-path already in tree): Saved\compile_magcheck_20261102.txt → Result: Succeeded
2nd (mag ammo visuals): Saved\compile_magcheck_ammo_20261102.txt → Result: Succeeded
No error C / error : lines. Warnings only from Engine deprecations.

Feasibility
-----------
YES for KINEMATION skel mags: SK_Mag_* / SKM_Mag_* have Ammo_001..Ammo_030 (+ Follower/Spring).
YES for RAR static: Default_Full / Default_Empty pair (DA_RAR_Magazine uses Full; code auto-swaps Empty when Ammo==0).
NOT wiring ABP_Magazine / BPI GetAmmo (conflicts with PlayAnimation reload path).

What code does
--------------
AWeapon::UpdateMagazineAmmoVisuals():
- Hide/UnHide Ammo_* bones from MagazineAmmoBonePrefix (default "Ammo_") by Weapon.Ammo
- Swap mag StaticMesh Full↔Empty (MagEmptyMesh or auto _Full→_Empty path)
Hooks: Local/Auth/Rep_Fire, ApplyMagazineCapacity, FinishMagazineReloadVisuals,
NotifyAmmoChanged, MagCheck (Initiate_VersionWeapon).

IMPORTANT for KIN BP_Weapon_AK105 / TR15
---------------------------------------
Those BPs currently have NO Magazine attachment slot; Ammo_* live on SKM_Mag_*, NOT on SKM_AK105.
Until you add Mag slot + DA with SkeletalMesh=SKM_Mag_AK105 (socket MagazineSocket / Magazine_Parent),
bone hide has nothing to drive on KIN guns. RAR MagCheck empty/full swap works now via DA_RAR_Magazine.

Backup
------
Saved\CodeBackup_20261102_035548_MagAmmoVisual

PIE
---
1. Cold-start editor (new DLL).
2. RAR: shoot to 0 → hold R MagCheck → mag mesh should be Default_Empty (no bullets).
   Reload / spare with ammo → MagCheck shows Default_Full.
3. KIN: after Mag DA+slot wired, same test; Output Log [MagCheck] playing ...
