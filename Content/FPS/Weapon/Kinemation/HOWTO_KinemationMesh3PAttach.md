# Kinemation third-person grip

The nine FPS Kinemation weapon Blueprints now have an independent mount under
`FPS | Weapon | Attach | ThirdPerson`:

- `Mesh3PAttachSocket`: `hand_r` on the character body.
- `Mesh3PAttachTransform`: the weapon origin relative to the animated right hand.

Each transform is baked from that weapon's authored idle animation:
`ik_hand_gun(component space).GetRelativeTransform(hand_r(component space))`.
This preserves both grip orientation and translation. Reference-pose bone
alignment and the weapon's `Trigger` bone are not grip calibration data.

The complete transform replaces the Mesh3P component's preview transform when
equipped. Changing first-person offsets does not change this third-person mount.
Weapons without `Mesh3PAttachSocket` keep their existing GripPoints behavior.
Right-hand grip notifies restore the calibrated mount; left-hand/holster notifies
retain their explicit destination. Re-equipping and body mesh swaps reapply it.

To recalibrate after changing a weapon's source idle pose, run
`Scripts/CalibrateKinemationThirdPerson.py` through Unreal's Python commandlet.
It validates all sources, backs up the nine Blueprints, saves their defaults,
and writes a reconstruction report under `Saved/Kin3PAttach/<timestamp>`.

Regression test: `Automation RunTests FPS.Weapons.ThirdPersonGrip`.
It covers all nine weapons on Manny Simple and Bandit Preset 13, including
re-equipping, right/left hand transfers, and first-person transform preservation.
