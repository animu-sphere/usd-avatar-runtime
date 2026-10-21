# Live MMD physics in usdview

The opt-in `usdviewMmdPhysics` host drives installed MMD control/coupling,
avatar-runtime's fixed-tick barrier, Jolt, and
[live UsdSkel pose output](../architecture/USD_POSE_OUTPUT.md).
The usdview **MMD Physics** menu supplies Play, Pause, Reset, Stop and Choose VMD.
Use this menu for simulation; usdview's ordinary timeline playback is not the
physics clock. A VMD is optional; without one the authored rest pose drives the
kinematic bodies while gravity drives the dynamic bodies.

## First validated scope

The generated `live-physics-pendulum.pmx` in the MMD repository has a visible
skinned bar, a kinematic anchor, a dynamicWithBone box and a standard joint.
Gravity changes its body orientation, owner feedback changes the bone rotation,
and usdview consumes the resulting live animation. No precomputed samples or
simulation bake is involved. The
[capability matrix](../reference/CAPABILITY_MATRIX.md#live-usdskel-and-usdview-mmd-evidence-2026-10-21)
records native and actual-window checks.

The host consumes installed `mmdPhysicsAdapter::ReadBodyProperties` for MMD
collision admission masks, contact coefficients and damping. The single-format
world uses product friction/restitution mixing with friction capped at 10,
following the MMD owner's policy. Damping fractions convert to finite decay
rates; backend stepping approximates the source response. Unit damping refuses
explicitly, and malformed declarations report their owner code and body path,
including during Reset. Missing metadata pairs retain shared body defaults.
Joint springs remain unapplied; their attribute paths are exposed as
`session.unappliedMetadata` and the UI emits an experimental scope warning.
Standard joint frames/limits come from the installed physics reader. This
preview is not representative hair/skirt acceptance or native MMD solver
equivalence. Unsupported non-bone morph/hidden outputs fail through the owner
registration adapter. This host currently publishes joints only.

The first configuration requires a directly opened PMX root layer with
`/Asset/skel/Skeleton`, one avatar, rigid default-time placement and a stage
admitted by the physics owner. Composed multi-avatar configuration, selective
unload, arbitrary seek, source edits during playback, timeline synchronization,
spring integration and resident Toon matching remain open. Stop before
editing or replacing bindings. Hydra-toon may be selected separately by a host
with a compatible installed delegate; this evidence uses usdview's default
renderer and makes no Toon rendering/parity claim.

## Build

Run from the runtime checkout. Prepare the installed MMD 0.2, physics/Jolt,
Stage Runner `runtimeCore` 0.1, motion owner 0.5.4 and Python-enabled OpenUSD
26.08 packages first. Set `$prefixes` to their install prefixes and `$jolt` to
the matching Jolt SDK prefix. No sibling source is compiled into this host.
The generated PMX fixture is input data.

```powershell
$mmd = (Resolve-Path ../usd-mmd-plugins).Path
$pendulum = "$mmd/plugins/usdMmdFileFormat/tests/fixtures/live-physics-pendulum.pmx"
cmake -S . -B build/usdview-mmd -A x64 `
  -DAVATAR_BUILD_USDVIEW_MMD_PHYSICS=ON -DAVATAR_BUILD_MMD_ADAPTER=ON `
  -DAVATAR_BUILD_USD_BINDING=ON -DAVATAR_BUILD_STAGE_RUNNER_SESSION=ON `
  -DAVATAR_BUILD_PHYSICS_USD_RESOURCES=ON `
  "-DCMAKE_PREFIX_PATH=$prefixes" "-DJolt_DIR=$jolt/lib/cmake/Jolt" `
  "-DAVATAR_MMD_PREVIEW_PMX=$pendulum"
cmake --build build/usdview-mmd --config Release
```

The option is OFF by default and requires the four optional components above.
The installed MMD package must export `mmdPhysicsAdapter_BODY_PROPERTIES_VERSION=1`;
older packages refuse at configure time with a rebuild/install instruction.
Only this application module links Jolt/Python; reusable core and adapter package
dependencies are unchanged. The build stages the plugin and native module under
`build/usdview-mmd/tools/usdviewMmdPhysics/python`; installation places them under
`lib/usd-avatar-runtime/usdview`.

## Run

Set `$usd` to the Python-enabled OpenUSD prefix and `$mmdInstall` to the installed
MMD prefix. Use the matching Python 3.13 environment, with Qt available:

```powershell
./tools/usdviewMmdPhysics/launch.ps1 -UsdRoot $usd -MmdPrefix $mmdInstall `
  -BuildDirectory build/usdview-mmd -PmxPath $pendulum
```

The script supplies Python/plugin/DLL paths for this process and starts usdview
with ephemeral default UI settings. Frame the bar if necessary, then choose
**MMD Physics > Play**. Pause retains the last pose. Reset pauses, reconstructs
the complete world with an increasing generation and removes the displayed pose;
choose Play to resume. Stop closes the runtime world registration before solver
resources, removes the display layer and restores the authored pose. Choose VMD
stops the old session before changing motion. Replacing the stage stops and
releases its session. An update error pauses the timer and reports the refusal;
use Reset for simulation recovery or Stop/rebind after structural changes.

## Validation

With the same dependency DLL/plugin environment as the launcher:

```powershell
ctest --test-dir build/usdview-mmd -C Release --output-on-failure
python tools/usdviewMmdPhysics/test_usdview.py $pendulum --defaultsettings
```

The second command opens a real usdview window and runs its Qt timer lifecycle;
it is opt-in and closes the window when finished. Ordinary CTest uses the native
session without a display. The native test requires the generated pendulum path
above; omit that cache option to build the host without this asset-specific test.
No licensed third-party asset is needed.

`hosts.usdview_mmd_properties` additionally adapts the same generated stage in
memory to isolate admitted/rejected drops, restitution, inclined-floor friction,
linear damping and pendulum angular damping. Source files remain untouched.
Malformed/unit damping checks cover constructor and Reset refusal.
`session.bodyStates` returns owned copies of current world translations,
quaternions (x,y,z,w) and linear velocities by body path for host inspection;
these remain usable after close. It returns an empty dictionary after close.
