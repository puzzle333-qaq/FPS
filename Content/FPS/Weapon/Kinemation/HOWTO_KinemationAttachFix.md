# HOWTO — KINEMATION 1P attach (shared Bandits / RAR arms)

## Cause of "挂点怎么调都不对"
`EnsureAnimatedGunAttachSockets` used to **promote** bone `ik_hand_gun` to a mesh socket
parented to `ik_hand_root`. `AttachToComponent("ik_hand_gun")` then hit that **static**
socket, so the gun froze at rest while hands / RAR `SOCKET_Weapon` (on the real bone)
kept animating. Tweaking `Mesh1PAttachRotation/Location` could never catch up.

## Fix (C++)
- Strip shadow sockets instead of promoting.
- If BP asks for `ik_hand_gun`, remap to `SOCKET_Weapon` when present (same as RAR).
- Default `Mesh1PAttachRotation` = identity.

## BP Class Defaults (all KINEMATION guns)
| Property | Value |
|----------|--------|
| Mesh1PAttachSocket | `SOCKET_Weapon` |
| Mesh1PAttachRotation | 0,0,0 |
| Mesh1PAttachLocation | 0,0,0 |
| bMesh1PAttachUsePivotOffset | false |

Do **not** tune Mesh1P component RelativeTransform (ignored after attach).
Do **not** use Mesh1PAttachRelative* unless `bUseMeshAttachRelativeRotation` is on
(and that path is for non-custom GripPoints mounts).

## Retest
1. Live Coding compile (Ctrl+Alt+F11) or restart editor so C++ loads.
2. Output Log: look for `[GunAttach] removed shadow socket 'ik_hand_gun'` once, then
   `[WeaponAttach] ... remapped 'ik_hand_gun' -> SOCKET_Weapon` or `sock=SOCKET_Weapon`.
3. PIE Mk14 / AK105: gun should sit in the right hand like RAR; idle/reload should
   move the gun with the hand (not drift).
4. If one gun is still rotated wrong, only then nudge that BP's Mesh1PAttachRotation
   by small steps and re-equip (attach runs on equip).
