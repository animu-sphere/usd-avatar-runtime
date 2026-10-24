---
status: binding
owner: usd-avatar-runtime
---

# Live UsdSkel pose output

`avatarUsd::SkeletonPoseOutput`, installed with the optional `avatarUsdBinding`
target and `usd` component, transports completed runtime joints and optional
single-mesh blend-shape weights to a composed
`UsdSkelAnimation`. It consumes a stage and an existing owner-validated
[`SkeletonBinding`](USD_BINDING.md); it runs no motion, control or physics.
The [live MMD host guide](../guides/live-mmd-physics.md) describes the first
application consumer. Current evidence belongs to the
[capability matrix](../reference/CAPABILITY_MATRIX.md#live-usdskel-and-usdview-mmd-evidence-2026-10-21).

## Publication and lifetime

`Apply(state)` copies the selected skeleton's joints by exact skeleton/joint
identity, validating layout, parent mapping and finite positive-scale transforms.
Additional unrelated runtime joints are allowed. The optional third constructor
argument names one mesh bound to this skeleton. Its standard `skel:blendShapes`
and target relationship define the output order; runtime weights are selected by
exact mesh-path/token identity. Every bound shape needs one finite,
float-representable weight; values outside 0–1 remain unclamped. Unrelated
runtime shapes are allowed. The consumer interprets no format-specific morphs.
`Apply(api, snapshot)` borrows a retained snapshot only during the
call; no producer or snapshot resource is retained.

Root joints arrive in runtime world space, in metres. Publication removes the
binding's rigid skeleton placement once, converts translations to stage units,
and delegates decomposition into USD's translation/quaternion/half-scale arrays
to OpenUSD. Child joints stay parent-local. USD narrowing overflow/underflow
rejects. Joint order comes from the owner binding, not runtime array order.

Each output owns an anonymous layer, attached as the first sublayer of the
stage's session layer on first successful publication. It writes a unique
animation prim and the skeleton's `skel:animationSource` relationship there.
Animation arrays use default values without time samples; rendering any timeline
position reads the current final pose. This is live display transport, not a
recording, clip, bake or export API. Persistent layers and the current edit
target are untouched. Existing session sublayers and their offsets are preserved.
A session-root animation relationship would mask this output and rejects.
When configured, the same animation carries blend-shape names and weights.
Mesh/skeleton binding, names and targets must remain unchanged, with valid
unique shape identities. Invalid or missing weights reject before publication.

Preparation happens in a separate scratch layer before live content is replaced.
Invalid layout, joint identity/parents, transforms, topology, units or placement
leave the previous displayable layer unchanged. Instance identity stays fixed;
older generations/frames reject, while duplicate publication is permitted.
Structural edits require closing and rebinding. The caller supplies a current
binding and one exclusive synchronous writer; this is not a notice-driven binder.

`Close()` removes only the owned sublayer and restores the preceding composition.
It is idempotent; later Apply calls reject. Destruction closes automatically.
The output holds the stage alive until close and owns no runtime or solver.
Retained CPU state remains usable after output teardown.

## Scope

This USD publication path supplies joints and optional single-mesh blend-shape
weights for usdview's Hydra consumer. It supplies no material or visibility publication, resident Toon matching,
renderer resources, complete Scene Index, Hydra/direct parity or pixel guarantee.
Those broader [output-path gates](OUTPUT_PATHS.md) remain open.
