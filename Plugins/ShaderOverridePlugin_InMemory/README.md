# ShaderOverridePlugin_InMemory: tonemapper notes

## Choosing the tonemapper (project-wide)

**Project Settings > Plugins > Tonemap Override**

| Setting | Options |
|---|---|
| Tonemapper | Engine Default (ACES), AgX, Reinhard, Linear, Khronos PBR Neutral, Hable, Uchimura, Lottes, ACES Fitted |
| AgX Look | None, Golden, Punchy (AgX only) |
| Split Tone | None, Teal & Orange, Sickly Green, Cold Moonlight |
| Hue Preservation % | 0 = bright colors bleach to white, 100 = hues stay fixed (fire/flashlight stay orange) |

Changing a setting rewrites `Shaders/TonemapOverrideConfig.ush` and recompiles the LUT shader.
The values are saved in `Config/DefaultGame.ini`. Commit both files.

## Per-volume controls (blendable, animatable)

In every mode except Engine Default, these Post Process Volume settings are repurposed.
At their engine defaults they have no effect, so existing volumes look unchanged.
Tick the override checkbox next to a setting to use it.

| Post Process Volume setting | Default | New meaning |
|---|---|---|
| Film > Slope | 0.88 | Contrast. Lower = flatter, higher = punchier |
| Film > Toe | 0.55 | Shadows. Higher = crushed, lower = lifted |
| Film > Shoulder | 0.26 | Highlights. Higher = brighter/harder, lower = softer roll-off |
| Film > Black Clip | 0.0 | Night vision: dark areas lose color and shift blue (0 to 1) |
| Film > White Clip | 0.04 | Bleach bypass: desaturated, harsh film look (0.04 = off, 1 = full) |
| Color Grading > Misc > Blue Correction | 0.6 | AgX look strength (0 = none, 0.6 = 100%, 1.0 = 167%) |
| Color Grading > Misc > Expand Gamut | 1.0 | Split-tone strength (0 to 100% of the preset) |

The regular Color Grading controls (temperature, saturation, contrast, gamma, gain and offset for
shadows/midtones/highlights) work in every mode. They are applied before the tone curve.

Ideas: raise Toe and Black Clip in a basement volume. Animate White Clip for a flashback.
Blend Slope up during a chase.

## Editing the shader by hand

Edit `Shaders/PostProcessCombineLUTs.usf`, then run the console command `ShaderOverride.Reload`.
You no longer need to restart the editor.

## Known limitations

- **HDR output** (HDR enabled in game/Windows) uses the engine's ACES output transform.
  The custom tonemapper, its sliders and looks apply to SDR output only.
- `PostProcessCombineLUTs.usf` is a full copy of the engine file. After an engine upgrade,
  merge in the engine's new version of the file or the shader may fail to compile.
- The plugin only runs in the editor and during cooking (shaders are compiled at cook time).
  Packaged builds ship the compiled result and don't load the plugin's code.
