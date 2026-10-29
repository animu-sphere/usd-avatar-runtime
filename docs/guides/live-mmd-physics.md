# Live MMD physics in usdview

The native implementation is also available as the installed
[MMD session](../architecture/MMD_SESSION.md). The dedicated renderer host
consumes its retained final snapshots through the
[hydra-toon viewport](https://github.com/animu-sphere/hydra-toon/blob/main/docs/guides/BUILDING.md#live-mmd-physics-in-the-dedicated-viewport).

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
The installed owner's `ReadJointSprings` validates source spring declarations
and converts coefficients to SI. Independent default-OFF translation and rotation
opt-ins apply positive free/limited axes of enabled standard `spring6Dof` joints
as experimental Hookean and small-angle approximations, with zero equilibrium
and viscous damping. Negative/locked/format-only meaning remains unsupported.
`session.stats` reports `approximateTranslationSpringAxes` and
`approximateRotationSpringAxes`; the UI warns about approximation and remaining
unapplied attribute paths from `session.unappliedMetadata`. A partially applied
attribute stays in that list. Malformed spring declarations refuse even when
both opt-ins are OFF, including during Reset.
Standard joint frames/limits come from the installed physics reader. This
preview is not representative hair/skirt acceptance or native MMD solver
equivalence. The host binds canonical vertex morphs by the owner's mesh token,
target and source index, then publishes owner-resolved direct/group weights
alongside final joints. Primary UV leaves use the installed owner UV query
and immutable canonical bases, with absolute `st` publication in the same
temporary layer. Material morphs now publish canonical material inputs in that
layer and separate sampled tint coefficients through direct snapshots. USD has
no canonical input for the latter. Additional UV, flip and impulse leaves and hidden
outputs without bindings fail through the owner registration adapter.

The first configuration requires a directly opened PMX root layer with
`/Asset/skel/Skeleton`, one avatar, rigid default-time placement and a stage
admitted by the physics owner. Composed multi-avatar configuration, selective
unload, arbitrary seek, source edits during playback, timeline synchronization,
representative spring behavior remain open. This usdview route does not exercise
resident Toon matching; the native renderer host is linked above. Stop before
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
The installed MMD package must export `mmdPhysicsAdapter_BODY_PROPERTIES_VERSION=1`,
`mmdPhysicsAdapter_JOINT_SPRINGS_VERSION=1` and
`mmdPhysicsAdapter_INITIAL_POSE_VERSION=1`;
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

Initial world construction and Reset place all associated bodies at the
evaluated VMD frame-zero pose with zero velocity, before constraints or Step.
This prevents a rest-to-motion teleport from injecting initial velocity.
There is no warm-up or skipped motion time; Pause/resume retains solver state.

Add `-TranslationSprings` and/or `-RotationSprings` to the launcher for explicit
experimental opt-in. Both are OFF when omitted; the generated fixture has zero
coefficients until a host authors test opinions. Direct Python callers may pass
the two optional booleans as `Session(stage, pmx, vmd, translation, rotation)`.
Options persist across Reset and changed coefficients are reread. Pause/Stop
before editing source opinions; use Reset to reconstruct the world afterward.

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

For the generated keyed-input check, generate VMD fixtures from the MMD
checkout, then configure the optional paired input from the runtime checkout:

```powershell
python "$mmd/tests/fixtures/generate_vmd_fixtures.py" --out build/live-vmd-fixtures
$anchor = (Resolve-Path build/live-vmd-fixtures/live-physics-anchor.vmd).Path
cmake -S . -B build/usdview-mmd "-DAVATAR_MMD_PREVIEW_VMD=$anchor"
ctest --test-dir build/usdview-mmd -C Release -R '^hosts.usdview_mmd_motion$' --output-on-failure
```

Keep `AVATAR_MMD_PREVIEW_PMX` set to the generated pendulum. The VMD option
adds keyed-input, dynamic-feedback and startup native checks; it is not a
general real-asset acceptance configuration.
`hosts.usdview_mmd_motion` moves the kinematic pivot 10 cm, returns and holds,
checks each fixed tick against the keyed path and concrete solver-driven
skinning, and checks exact reset replay. Contact-free generated input does not
establish representative avatar acceptance. The
[keyed-input evidence](../reference/CAPABILITY_MATRIX.md#live-mmd-keyed-input-evidence-2026-10-24)
also records the separate private-asset intake boundary.

For direct and owner-resolved group vertex morph acceptance, generate the current VMD fixture set above
and configure the paired generated MMD model:

```powershell
$morphPmx = "$mmd/plugins/usdMmdFileFormat/tests/fixtures/live-physics-morph.pmx"
cmake -S . -B build/usdview-mmd "-DAVATAR_MMD_PREVIEW_MORPH_PMX=$morphPmx"
ctest --test-dir build/usdview-mmd -C Release -R '^hosts.usdview_mmd_(morph|uv)$' --output-on-failure
```

Keep the preceding PMX/VMD options configured. The VMD fixture directory must
contain `live-physics-morph.vmd`, `live-physics-uv.vmd`,
`live-physics-group.vmd`, `live-physics-mixed.vmd`, `live-physics-uv-mixed.vmd`
and `live-physics-material.vmd`. Install an MMD owner
package providing `Evaluator::ResolveMorphChannels` and `EvaluateUvMorphs` before rebuilding the host;
configuration explicitly refuses older/incomplete headers or libraries.
This native test checks direct, group-only and mixed direct/group weights,
including return to zero while the pivot moves and physics drives the final skeleton,
composed blend-shape deformation followed by skinning, pause at a nonzero weight,
exact reset replay and layer restoration. The empty material declaration is now
admitted with a complete material layout. Nonempty material modulation has its
own generated [session check](../architecture/MMD_SESSION.md).
The UV test checks direct and mixed group/direct UV plus vertex leaves, signed
and above-one weights, primary-V conversion, ignored primary Z/W, return to
immutable bases, pause, three-generation replay and Reset/Close cleanup.
Rebuild runtime providers/consumers with experimental ABI revision 5.
This numeric check does not open a window or establish rendered pixel acceptance.

`hosts.usdview_mmd_properties` additionally adapts the same generated stage in
memory to isolate admitted/rejected drops, restitution, inclined-floor friction,
linear damping and pendulum angular damping. Source files remain untouched.
Malformed/unit damping checks cover constructor and Reset refusal.
`hosts.usdview_mmd_springs` checks separate translation/rotation response,
positive free/limited activation, signed/locked/type/disabled gaps, live pose
agreement, exact reset replay, changed-stiffness reconstruction and owner refusal.
To exercise springs in the actual-window test, set
`$env:AVATAR_MMD_ROTATION_SPRINGS = '1'` before its command; the test supplies
a nonzero coefficient in the anonymous session layer. Leave it unset for the
original gravity-only window check.
`session.bodyStates` returns owned copies of current world translations,
quaternions (x,y,z,w) and linear velocities by body path for host inspection;
these remain usable after close. It returns an empty dictionary after close.

`hosts.usdview_mmd_dynamic` checks mode-1 position/rotation publication with the
same generated pendulum/anchor inputs. It changes the composed session's
swing mode to `dynamic` and Y translation limits to +/-0.2 m, uses both spring
options OFF, and compares the final
UsdSkel bone transform to the concrete body after removing its rest offset.
Install the corrected MMD owner and rebuild the host before running it:

```powershell
ctest --test-dir build/usdview-mmd -C Release -R '^hosts.usdview_mmd_dynamic$' --output-on-failure
```

The test includes a nonzero joint-local translation, pause, three exact
240-tick reset replays and persistent/session layer restoration. It does not
establish representative spring or full-motion acceptance.

Regenerate the VMD fixtures with the current MMD owner before configuring the
keyed-input tests. The directory must also contain
`live-physics-start-offset.vmd`, `live-physics-start-turn.vmd` and
`live-physics-start-bone.vmd`. Run the startup regression with:

```powershell
ctest --test-dir build/usdview-mmd -C Release -R '^hosts.usdview_mmd_startup$' --output-on-failure
```

It checks non-rest frame-zero translation, root rotation and child pose before
any Step, zero linear velocity, translation-invariant trajectories, Pause,
three exact reset replays per input and source/session layer restoration.
The [MMD report](https://github.com/animu-sphere/usd-mmd-plugins/blob/main/docs/reports/2026-10-25-phase8-start-pose-initialization.md)
owns measurements and the private-preview boundary. Initial contact overlap,
representative skirt recovery and spring tuning remain separate acceptance work.
