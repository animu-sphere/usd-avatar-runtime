# Documentation guidelines

One concept, one owning repository, one canonical document. Documents are part of the
implementation boundary and must stay consistent with the tree.

## Categories

| Category | Owns | Does not hold |
| --- | --- | --- |
| root README | short scope, status, entry points | adapter narratives, evidence |
| docs index | navigation | status |
| design | intent and rules | status, schedule, API shapes that contracts own |
| contracts | public input, output, evaluation and ABI boundaries | measurements, schedule |
| architecture | components, responsibilities, build and lifetime | measurements, schedule |
| reference | current state, in the capability matrix | history, narratives |
| reports | dated measurements, in the evidence log | current status |
| roadmap | incomplete work and acceptance | completed work, measurements |
| releases | one immutable record per actual release | later edits except fixing links |
| guides | runnable, verified procedures | unverified commands |
| contributing | maintenance practice | |

## Keeping pages current

- **When work lands,** remove it from the roadmap, update its capability matrix row and add
  the measurement to the evidence log, in the same change. Do not leave "now implemented"
  narratives anywhere. Say what a thing is, not how it came to be.
- **When a plan is replaced,** replace it. Delete superseded pages rather than adding a
  refinement document above them, and rely on git history for the old text. Keep one set of
  labels: the roadmap is organized by release and gates.
- **Implemented means implemented.** Accepted ownership is not evidence of working code, and
  a conceptual API name is not an available one.
- **The evidence log is append-only.** Fix links, not history. Its terms belong to its date.

## Metadata

Design, architecture and contract documents carry front matter with `owner` and `status`.
Use `proposed` for unresolved target designs, `accepted` for adopted direction, and `binding`
only for an implemented, validated contract. Status is scoped to the document's subject.

## Cross-repository references

Link to a sibling's owning document for motion values, format semantics, connector behavior,
imaging or rendering. Describe how this runtime consumes that boundary without copying the
sibling's API, capability table or roadmap. Keep evidence with its producing repository and
preserve provider diagnostic identities.

Where an integration conflicts with a sibling boundary, record an open question and resolve it
with the owner. Do not silently redefine its semantics or claim that a future runtime API is
already available upstream.

## Form and validation

Repository documents are in English. Use relative links inside this repository and canonical
repository links for siblings. Keep code names in code spans. Do not commit machine-local paths,
unverified build commands or assets without redistribution rights.

For every documentation change, check:

1. New pages appear in the docs index.
2. Relative paths and section fragments resolve, including case and spelling.
3. Statements of intent, implementation and metadata agree with the tree.
4. Contract changes update their open questions and the roadmap's open decisions.
