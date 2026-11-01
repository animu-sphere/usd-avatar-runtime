---
status: proposed
owner: usd-avatar-runtime
---

# Cross-package ABI

The direct implementation exposes experimental revision 5 through
[`api.h`](../../include/avatarRuntime/api.h). `arGetApi` fills the caller's
`ArRuntimeApi` core table. The additive `arGetPhysicsApi` entry point supplies
the separate experimental [`physics.h`](../../include/avatarRuntime/physics.h)
table; both tables require current core descriptor headers.
This is a tested prototype, not an ecosystem ABI freeze; see the
[freeze gates](../roadmap/current.md#abi-freeze).

The optional physics table has its own query version, currently
`AR_PHYSICS_API_VERSION == 1`; its descriptors/results use core revision-5
headers. `ArPhysicsWorld` is a scoped non-recycled handle. No provider, USD or
backend types cross this table. The existing installed C consumer also checks
the installed physics header and query symbol. Pre/post boundaries select
existing evaluator IDs; no new `AR_PHASE_*` constants are introduced.
Display provenance and checkpoint recovery remain RT-O4/RT-O5 work, and neither table is frozen.

## 1. Representation

Implemented revision-5 rules:

- Windows uses `__cdecl` and default compiler packing. Descriptors/views begin
  with `uint32_t struct_size` and `abi_version`; fixed value records and writer
  tables are versioned by their enclosing contract. All required fields of
  revision 5 must fit. Unknown larger tails are ignored/left untouched; any
  different revision is rejected before reading the descriptor body.
- Initialize descriptors to zero, then set `AR_HEADER(Type)`. `arGetApi` takes
  an explicit revision and output capacity. No packed structs, STL, USD types,
  exceptions or renderer handles occur in the public headers.
- Arrays use fixed-width counts and borrowed pointers. Counts above 1,048,576
  are rejected. Identifiers are nonempty UTF-8 strings, at most 1,024 bytes,
  without ASCII control characters. Strings are NUL terminated. Callers must
  supply readable memory of the declared size; these checks are not a memory
  sandbox for a malformed pointer.
- Runtime, instance and snapshot handles are nonzero `uint64_t` IDs, never
  recycled during the loaded library's lifetime. Wrong-kind, released, stale
  and cross-runtime instance handles return `AR_INVALID_HANDLE`. A process-wide
  handle table manages lifetime; mutable avatar/provider state is per instance.
- Registration copies descriptor identities, dependencies and capabilities;
  instance creation copies the authored output layout and initial values.
  Provider `user_data` and callback code remain borrowed.

Revision 2 introduced fields in `ArInstanceDesc`, `ArInputFrame` and `ArStateView` for
required binding layout ID/version, input revision and snapshot-associated
active capabilities. Revision 3 appended typed gaze observations to `ArInputFrame`.
Revision 4 appends retained `ArSourceSample` provenance to `ArStateView`
(which also grows the embedded `ArInstanceDesc.initial_state`), appends
`report_sample` to `ArStateWriter` and accepts `AR_VALUE_VEC2` material values.
Revision 5 appends primary per-vertex `ArVertexUv` values to `ArStateView`
(also growing embedded initial state) and `set_vertex_uv` to `ArStateWriter`.
The [state contract](EVALUATED_STATE.md#primary-vertex-uvs) defines fixed
layout, absolute values and consumer admission. Revisions 1–4 are rejected at
table and descriptor/view negotiation before
reading their bodies. This is an experimental breaking change, not a compatible
tail extension or an ABI freeze. Providers/consumers must rebuild with
revision-5 headers and supply nonempty layout ID/nonzero version on instance
creation. Installed providers and consumers, including renderer adapters,
must rebuild together before reading revision-5 snapshots. Rebuilding alone
does not admit UV outputs: a consumer must implement or explicitly refuse them.
The function table has the same operations. ABI revision is independent of
the experimental CMake package version.

Before freeze, review the selected layout/alignment, calling convention and
minimum-size rules with real provider adapters. C++ conveniences remain a layer
above this table, never an exported binary class boundary.

`ArGazeInput` is a fixed value record versioned by its enclosing input frame,
like `ArScalarInput`; it has no nested header. Its array, identity/reference
strings and values are borrowed for the same synchronous evaluation lifetime.
All known revision-5 fields must fit even when `gaze_count` is zero. No gaze
input pointer is retained by the runtime or snapshot. The
[input contract](INPUT_FRAME.md#revision-3-gaze-observations) owns its space,
validity, direction and absence rules.

`ArSourceSample` is likewise a fixed value record versioned by the enclosing
state view. Its records, identity strings and evaluator IDs are runtime-owned
copies with snapshot lifetime; a provider's `report_sample` argument is
borrowed only for the call. The
[evaluated-state contract](EVALUATED_STATE.md#source-sample-provenance) owns
its meaning.

Existing motion and format libraries may use C++/OpenUSD values internally.
Provider adapters marshal those values into versioned views; the ABI must
preserve the owner's semantics without exposing its binary class layout or
inventing a second motion model.

## 2. Lifetime and calls

The prototype implements the following lifetime model:

| Boundary | Prototype rule |
| --- | --- |
| input views | borrowed synchronously through `evaluate_frame`; no retention or asynchronous intake |
| evaluated outputs | successful evaluation returns one owned snapshot reference; `retain_snapshot`/`release_snapshot` manage additional references; views last until the last reference is released |
| callbacks | synchronous on the caller's thread; runtime API calls from provider/diagnostic callbacks return `AR_BUSY`; writer/diagnostic hooks are callback scoped |
| allocation | runtime buffers are released by runtime functions; providers create/destroy their private state using their own allocator |
| provider unload | code and `user_data` stay valid until runtime destruction completes; there is no unregister/unload API yet |
| instance shutdown | API calls serialize; destruction waits for earlier evaluation, destroys private state and preserves independently retained output snapshots |

Snapshot storage contains copied values/identities and no provider-private
pointers. It survives instance/runtime destruction; keep the runtime library
loaded until the final snapshot release. Capability views are borrowed until
instance destruction and remain unchanged by reset. Capabilities obtained
through a retained snapshot instead share that snapshot's lifetime, as do
layout identity/version and input revision. The snapshot never borrows its
capability strings from a provider or destroyed instance. Hosts must coordinate
release/destruction with readers. All API calls are serialized in this initial
implementation, including calls for different runtime objects; parallel
scheduling and cancellation are future work. A callback must not wait for a
different thread to call the runtime API. The stateless `arGetApi` table query
does not take this lock and may be used from callbacks.

No allocation may require freeing with another package's incompatible allocator.
No exception may cross a C boundary. Validation failures and evaluator refusals
return defined statuses with structured diagnostics.

Runtime entry points catch allocation/implementation exceptions. Provider and
diagnostic callbacks must not throw. Unexpected evaluator exceptions are caught
as a defensive measure and abort the transaction. `end_frame` and destruction
are contractually infallible; an exception during commit/abort poisons the
instance and requires reset. Revision 5 does not promise recovery from an
arbitrary provider violating its lifecycle contract.

## 3. Compatibility and portability

Define ABI compatibility independently from provider semantic versions and
capability versions. Negotiation must reject incompatible mandatory layouts
before accessing data. Optional extensions are discoverable, not assumed.

The contract tests compile the runtime, a C11 provider DLL and C11 consumer
separately. An additional external CMake build uses only installed runtime
headers, the exported package target and its shared library. A C++20 test also
includes the public headers. Hosted CI runs these on Windows, Linux and macOS with
one toolchain each; that does not establish cross-toolchain or cross-platform compatibility (`RT-O7`).
Optional features are not inferred from ABI success. Unknown evaluator flags,
state domains, typed material kinds and source-sample kinds/resolutions are rejected. Semantic capability
versions are independent from `AR_ABI_VERSION` and use exact matching.

The experimental `ArPhysicsMembershipApi` version 1 is negotiated through
`arGetPhysicsMembershipApi` with its own version and table size. It adds no field
to `ArRuntimeApi`, `ArPhysicsApi` or their descriptors. Headers retain the core
revision in `abi_version`, as with the existing physics table; the query's
version argument selects the extension revision. Its sole operation follows the
[membership lifecycle](../architecture/FRAME_LIFECYCLE.md#experimental-fixed-tick-requests).
The optional Stage Runner session requires this extension from its linked
runtime. No frozen C++ session layout or provider-unload capability is implied.

WASM-friendly representation is a design objective. It still needs decisions
about pointer width, host memory, optional dependencies and provider loading;
native dynamic loading cannot be presumed to work unchanged in a browser.

## 4. Change candidates, reviewed for v0.3.0

**v0.4.0 also keeps revision 5.** It changed no table or layout. A second format is now in a package, and install-tree CI
covers both sessions. Tests of failure recovery still cover the VRM session only, and the rest of the list below still does
not hold.

**The freeze stays deferred, and v0.3.0 keeps revision 5.** Work since 0.2.0 changed no table or
layout. Against the [freeze gates](../roadmap/current.md#abi-freeze): failure recovery and install-tree
CI exist for the VRM session only, and these do not hold:

- a second format in a package, and a second kind of external consumer; `hydra-toon` is the only one
- provider checkpoint restore
- output capability negotiation
- compatibility evidence across toolchains
- a stated compatibility policy per surface

A change costs one of three things, because any different revision is rejected:

- **Revision:** a new core revision, so every provider and consumer rebuilds, and a new minor version.
- **Extension:** a separately queried table, as `ArPhysicsMembershipApi` is, so nothing else rebuilds.
- **None:** a contract or source-level change with no binary layout change.

| Candidate | Open decision | Cost | Needed by |
| --- | --- | --- | --- |
| Typed gaze results and resolved expression records in `ArStateView` | RT-O2, RT-O4 | Revision | freeze |
| Retained diagnostics and snapshot provenance | RT-O4 | Revision | freeze |
| Additional UV channels and other deformation in `ArStateView` | MMD session | Revision | a consumer that needs them |
| Gaze space descriptors and clock mapping | RT-O1 | Revision if `ArInputFrame` changes; otherwise None | freeze |
| Checkpoint, restore and abort callbacks for stateful evaluators | RT-O5 | Revision, or Extension if registered separately | freeze |
| Output capability negotiation | RT-O4, RT-O7 | undecided: Extension fits if it only adds queries | freeze |
| Shared-world handles and callbacks | [RT-O6](../architecture/FRAME_LIFECYCLE.md#rt-o6-one-world-one-session), for one session; not frozen | Extension: the physics tables have their own versions | freeze |
| Provider unload | RT-O7 | Extension | freeze |
| Compatible tail growth in place of rejecting other revisions | RT-O7 | Revision once, then later additions need no rebuild | freeze |
| Layout, alignment and calling convention review with real providers | RT-O7 | Revision if anything changes | freeze |
| Parallel scheduling and cancellation, in place of serialized calls | [execution](../roadmap/current.md#execution-and-scheduling) | None for layout; a contract change callers must adopt | after freeze is acceptable |
| `avatarVrmSession::Session` and `FixedTickSession` | compatibility policy | None: C++ source API, not part of this ABI | freeze, as a stated policy |

Decide compatible tail growth first of these. Until it lands, each state or input addition rebuilds
every provider and consumer, and doing it first makes the later rows cheaper.
