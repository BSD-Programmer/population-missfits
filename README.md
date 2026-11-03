# Population Misfits (v1.0.0-v1)

**Population Misfits** is a native `arm64-v8a` OpenXR 1.1 / OpenGL ES 3.2 VR Arena & Customs game prototype for **Meta Quest 2, Meta Quest Pro, Meta Quest 3, and Meta Quest 3S**, inspired by *POPULATION: ONE*'s Vertical Combat System (VCS) and UGC Sandbox / Customs ecosystem.

## Key Features in Version 1
- **Arena Battle Mode**: Free-For-All Arena Deathmatch and Squad Battle Arena with 3.5s respawns, live kill feed, damage popups, and scoreboard (no shrinking Battle Royale zone in v1).
- **Vertical Combat System (VCS)**:
  - **Climb Anything**: One-handed and two-handed climbing on any wall, pillar, or tower using Quest Touch Grip buttons with pull-up fling momentum.
  - **Fly / Glide Anywhere**: Extend both arms outward in T-pose while airborne (or hold Left Index Trigger) to unfold dorsal stabilizer wings and glide across the arena while firing one-handed.
  - **5 VR Weapon Classes**: `HX-4 Hex Carbine (AR)`, `V9 Vector Pulse (SMG)`, `M10-Hex Scattergun (Shotgun)`, `AWP-Hex50 Rail Sniper`, and `PX-Sidearm (Pistol)`, plus Shield Sodas, Health spawners, and AI Misfit Bots.
  - **Tactical Building on the v2 Roadmap**: Surfaced in the spatial HUD (`kTacticalCombatBuildingEnabledInV1 = false`).
- **Custom Maps & Customs System**:
  - In-VR **Map Editor** with 13 placeable hexagonal assets/gizmos, 8 material swatches, grid/angle snapping, duplicate/recolor/delete, and a `5000`-unit complexity budget meter.
  - 3 built-in maps (*Hex Citadel*, *Neon Spire Arena*, *Skybridge Clash*), shareable Map Codes (`PM1|...`), JSON persistence, and LAN/UDP Custom Room Hosting (`port 39770`).
- **Hexagonal Low-Poly Blue Base Models & Layered Graphical Skins**:
  - Distinct **Base Male** and **Base Female** articulated hexagonal low-poly character meshes in signature cobalt/cyan-blue faceted panels (`552` base tris), with 6 selectable layered graphical skins & armor overlays (`624` tris) in the Character Locker and 100% standardized combat hitboxes.

## Building & Installing on Meta Quest 2 / Quest 3

```bash
./build-quest-apk.sh
adb install -r build/population-misfits-v1.apk
```
