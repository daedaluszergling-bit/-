# Z Lab AnA

Z Lab AnA is a VST3 modulation reverb with two linked audio/visual worlds:

- ZAPIS: clear, pencil-like, forming space.
- ZRENJE: dark, unstable, self-spinning field.

The plugin is built with JUCE 8 through CMake. The GitHub Actions workflow included in this package builds the Windows VST3 artifact.

## V19 Notes

- Restored the original v0.10 package grid structure: a 30 by 18 point mesh drawn as connected line segments.
- Restored the original visual field force layer: five local visual field terms, deterministic point jitter, and edge fade.
- Kept the current main-object gravity model for the mass source: small local radius with subtle breathing.
- Removed the V18 flag-wave/geodesic rendering path from the active grid.

## V18 Notes

- Matched black and white deformation strength: same pull, same radius, same frame-drag and shear math.
- Replaced whole-grid scale breathing with a very slow flag-like travelling wave across the grid.
- Restored local breathing around the main object by gently pulsing the small gravity radius and pull strength.
- Kept black/white differences limited to material, colour, and stroke treatment.

## V17 Notes

- Optimized field drawing for smoother dragging: fewer samples per line and no duplicate path generation for dark glow strokes.
- Changed breathing to a subtle whole-grid scale motion instead of modulating the local gravity around the main object.
- Reduced the gravity disturbance volume to a small local region around the mass source.
- Matched the black and white worlds to the same gravity radius while keeping black slightly stronger inside that radius.

## V16 Notes

- Restored the visual grid density to the original 18 horizontal by 30 vertical field lines.
- Kept the V15 gravitational lensing logic, but made the black-world pull and frame-drag more exaggerated.
- Moved the white-world field closer to the black-world logic: same gravity/curvature behavior, lighter material.
- Removed sketch jitter and extra ring/glow decoration from the main object so the mass marker stays clean.

## V15 Notes

- Replaced the dense spider-web style UI grid with sparse gravitational geodesic curves.
- Reworked the center object as a mass/lens source, with stronger curvature near the body.
- ZAPIS now draws sketch-like graphite lines and paper grain.
- ZRENJE now draws thin luminous space-curvature lines, a dark core, and an Einstein-ring style lens accent.
- UI breathing now modulates curvature/mass strength instead of looking like water-surface waves.
- Repaired source lines that had been swallowed by malformed comments in the previous package.

## Build On GitHub

1. Create a new GitHub repository or replace the files in an existing one.
2. Upload all files from this package, including `.github/workflows/build.yml`.
3. Run the `Build Windows VST3` workflow, or push to `main`/`master`.
4. Download the `AnA-Windows-VST3` artifact.

The built plugin will be under the uploaded artifact as `Z Lab AnA.vst3`.

## Local Windows Build

If CMake and Visual Studio 2022 are installed locally:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DANA_COPY_AFTER_BUILD=FALSE
cmake --build build --config Release --target AnA_VST3
```
