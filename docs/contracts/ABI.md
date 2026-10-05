---
status: proposed
owner: usd-avatar-runtime
---

# Cross-package ABI

This document develops [design policy section 9](../design/DESIGN_POLICY.md).
No header, ABI version or exported function currently exists. The following
items are freeze requirements, not available API declarations.

## 1. Representation

- Use a small C-compatible function table for registration and frame execution.
  C++ conveniences are layered above it.
- Version descriptor structs with size/version fields. Specify initialization,
  minimum accepted size, unknown-tail handling and mandatory feature rejection.
- Use fixed-width scalars, explicit counts and pointer/view arrays. Specify
  layout/alignment, string encoding and semantic identifier rules.
- Use opaque scoped handles for runtime-owned objects. Invalid, stale and
  cross-runtime handles return explicit status.
- Keep STL containers, C++ classes, exceptions, OpenUSD value types and
  renderer objects out of the exported C boundary.

Existing motion and format libraries may use C++/OpenUSD values internally.
Provider adapters marshal those values into versioned views; the ABI must
preserve the owner's semantics without exposing its binary class layout or
inventing a second motion model.

## 2. Lifetime and calls

Freeze these rules with the first implementation:

| Boundary | Required decision |
| --- | --- |
| input views | borrowing duration, copying and asynchronous handoff |
| evaluated outputs | retention/release mechanism, frame invalidation and immutable ownership |
| callbacks | invocation thread, reentrancy, diagnostic lifetime and cancellation |
| allocation | allocating/freeing side, allocator callbacks if needed |
| provider unload | when instance callbacks/code/state may be released |
| instance shutdown | drain/cancel policy and retained-output lifetime |

No allocation may require freeing with another package's incompatible allocator.
No exception may cross a C boundary. Validation failures and evaluator refusals
return defined statuses with structured diagnostics.

## 3. Compatibility and portability

Define ABI compatibility independently from provider semantic versions and
capability versions. Negotiation must reject incompatible mandatory layouts
before accessing data. Optional extensions are discoverable, not assumed.

Do not freeze function names or calling conventions from the policy's ellipsis
examples. Specify those with compilable C/C++ headers and separately built
provider/consumer evidence (`RT-O7`). Platform support needs evidence from each
actual target.

WASM-friendly representation is a design objective. It still needs decisions
about pointer width, host memory, optional dependencies and provider loading;
native dynamic loading cannot be presumed to work unchanged in a browser.
