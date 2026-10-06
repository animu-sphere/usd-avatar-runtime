# Documentation guidelines

One concept, one owning repository, one canonical document. Documents are
part of the implementation boundary and must remain consistent with the tree.

## Categories

| Category | Owns |
| --- | --- |
| root README | short scope, conceptual architecture, components and entry points |
| docs index | subject ownership and navigation |
| design | intended policy, rationale and unresolved proposals |
| contracts | focused public input/output/evaluation/ABI boundaries |
| architecture | components, responsibilities, dependencies and lifecycle |
| reference | implemented capabilities and measured support in the current tree |
| roadmap | incomplete runtime work and acceptance gates |
| contributing | repository maintenance practices |

Add guides only with runnable, verified procedures; releases only for actual
releases; reports for dated evidence; archives for completed/superseded plans.
Create their indexes when those categories first acquire content, rather than
publishing empty sections or unverified commands now.

## Authority and status

The adopted [design policy](../design/DESIGN_POLICY.md) preserves the original
22 section numbers. Its conceptual structs and layout are not implemented
interfaces. Focused proposals develop them and become binding only with
reviewed decisions and implementation evidence.

The adopted [near-term direction](../design/NEAR_TERM_PLAN.md) refines the
original phase sequence. The adopted
[boundary cleanup policy](../design/BOUNDARY_POLICY.md) preserves its supplied
21 sections and refines ownership and migration priorities. Keep both source
documents' numbering stable. Cleanup workstreams A–F are distinct from Runtime
Phases A–F and evidence milestones A/B/C. Keep integration priorities and freeze
gates aligned without treating planned channels or migrations as implemented support.

Design, architecture and contract documents carry front matter with `owner`
and `status`. Use `proposed` for unresolved target designs, `accepted` for
adopted direction, and `binding` only for an implemented/validated contract.
Superseded pages must link to their current owner/replacement. Status is scoped
to the document's subject; accepted ownership is not evidence of working code.

Current implementation facts live in the capability matrix. Plans live in the
roadmap. Do not maintain release/status histories independently in the README
or contract prose.

## Cross-repository references

Link to a sibling's owning document for motion values, format semantics,
connector behavior, imaging or rendering. Describe how this runtime consumes
that boundary without copying the sibling's API, capability table or roadmap.
Keep evidence with its producing repository and preserve provider diagnostic
identities.

Where an integration conflicts with a sibling boundary, record an open question
and resolve it with the owner. Do not silently redefine its semantics or claim
that a future runtime API is already available upstream.

## Form and validation

Repository documents are in English. Use relative links inside this repository
and canonical repository links for siblings. Keep code names in code spans.
Do not commit machine-local paths, unverified build commands or assets without
redistribution rights.

For every documentation change, check:

1. New pages appear in the docs index and relevant category index.
2. Relative paths and section fragments resolve, including case/spelling.
3. Proposal/implementation statements and metadata agree with the tree.
4. Ownership and phase names are consistent; phase names use `Runtime Phase`.
5. Contract changes update their open questions and acceptance criteria.
6. Source-policy section numbers and existing cross-repository citations stay
   stable when editing the adopted policy.
7. Binding adapter pages retain current behavior and evidence while clearly
   labeling planned owner migrations; conceptual API names are not advertised
   as available upstream or in installed headers.

When code lands, update implementation status and evidence in the same change.
Keep completed work out of the active roadmap, preserving its decision or
release/report provenance elsewhere as appropriate.
