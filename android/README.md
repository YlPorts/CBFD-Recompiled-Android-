# Conker Recompiled Android — direct PC 0.1.2 port

This Android branch ports **sciaschi/CBFD-Recompiled PC 0.1.2**, upstream commit
`152cc380386481f008b40fb8c2059fd9a6fddedc`.

The goal of this branch is deliberately different from the older Android experiments:
**shared game and rendering behavior must remain PC 0.1.2.** Android only supplies the
platform code required to run that build on ARM64/Android.

## PC 0.1.2 source parity

The direct-port CI pins the exact upstream Git blob IDs for:

- `conker.toml`
- `host/src/widescreen.cpp`
- `recomp/conker.us.syms.toml`
- `recomp/rt64.patch`
- `recomp/n64modernruntime.patch`
- `recomp/n64recomp.patch`

If any of those files is edited independently on Android, CI fails.

PC 0.1.2's two widened-world culling fixes are therefore used directly:

1. `conker_widen_frustum` — widens the camera side planes.
2. `conker_widen_cull_scale` — widens the level/cell/triangle culling scale.

The PC interpolation, extended GBI widescreen work, pause background, iris transition,
ROM-version handling and mod exports are also built from the shared PC source.

## Android-only platform layer

Android keeps only what the desktop build cannot provide directly:

- Android Activity / SDL lifecycle and an ARM64 NDK target.
- Touch controls mapped to **normal N64 inputs**. The right touch/controller stick maps to C-buttons.
- Storage Access Framework ROM/mod import and app-private saves/config.
- Vulkan SDL/Android Surface negotiation in `android/patches/vulkan-surface.patch`.
- APK packaging, 16 KiB alignment and signing.

The dependency patcher intentionally does **not** apply Android-specific shader,
filtering, culling, blend, fixed-resolution or performance modifications.

Removed from this direct port:

- GLideN64 / OpenGL ES renderer.
- Mali-G57 blend override.
- Android custom three-tap filter.
- fixed-1080 render policy.
- Android frustum wrapper/bounds policies.
- orbital-camera game hook.
- custom RT64 coverage/matching/shader-worker changes.

## Launcher and Settings

The Android launcher follows the PC 0.1.2 menu flow:

- **Start Game / Load ROM**
- **Version**
- **Add ROM**
- **Controls**
- **Settings**
- **Mods**
- **Exit**

The visible graphics settings use RecompFrontend's PC semantics and defaults:

| PC option | Android choices | PC-style default |
| --- | --- | --- |
| Resolution | Original / Original 2x / Auto | Auto |
| Downsampling Quality | Off / 2x / 4x | Off |
| Aspect Ratio | Original / Expand | Expand |
| Framerate | Original / Display / Manual 20–240 | Display |
| MS Anti-Aliasing | None / 2X / 4X | 2X |
| HUD Placement | Original / 16:9 / Expand | 16:9 |
| High Precision Framebuffer | Auto / On / Off | Off |
| Graphics API | Vulkan | Vulkan (Android platform constraint) |
| Window Mode | Fullscreen | Fullscreen (Android platform constraint) |

These settings are mapped using the same `GraphicsConfig -> RT64::UserConfiguration`
logic as RecompFrontend. Changes made while the game is already running take effect
on the next launch in the current Android UI.

Mods are imported into `state/mods`, the same directory scanned by librecomp.
Existing `mods.json` enable/order state is preserved; a newly discovered mod follows
its manifest's `enabled_by_default` value.

## Version identity

- PC runtime/project version: **0.1.2**
- Android package version: **0.1.2-android-alpha**
- versionCode: **17**
- package: `com.ylports.cbfd`
- native build marker: `pc-012-direct`

Keep the original Android signing key if the APK must install over an existing build.

## Building

The repository still contains no game data. Supply your own legally obtained US ROM.

After applying the normal PC patches and regenerating the PC 0.1.2 recompiled sources:

```sh
python3 recomp/recompile.py
python3 android/tools/prepare.py --dependencies
python3 android/tools/build_apk.py \
  --sdk "$ANDROID_HOME" \
  --keystore /private/conker-android.jks \
  --alias conker-android
```

`build_apk.py` refuses to package an older native engine under this version label.

## Verification

Public CI can verify source parity, Java/touch behavior and the Android Vulkan WSI
policy without a ROM:

```sh
python3 android/tools/test.py
python3 android/tools/test_storage.py
python3 android/tools/test_mobile.py
python3 android/tools/test_pc_sync.py
python3 android/tools/test_vulkan_surface.py
```

A complete native APK build still needs the private regenerated game sources and
the signing key.

## SM-A155M / water issue

This branch intentionally discards the previous Android rendering experiments so
the Samsung SM-A155M can be compared against **PC 0.1.2 behavior itself**.

PC 0.1.2 adds the second world-culling correction (`conker_widen_cull_scale`) that
was missing from the older PC 0.1.1 base. That may affect disappearing level pieces,
but this document does **not** claim that the river/water defect is fixed.

The next meaningful validation is a real Vulkan run on the SM-A155M, especially the
same river angles that previously made the water disappear, plus a performance log.
