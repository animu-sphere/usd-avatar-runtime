# MMD session end-to-end fixture

A small PMX and VMD that exercise morph, primary UV, IK, append and physics feedback through one
session, with a joint from a body to itself, a bone two simulated bodies name, and an after-physics bone
that appends the simulated bone. [`tools/make_mmd_session_fixture.py`](../../../tools/make_mmd_session_fixture.py) writes them
with the MMD owner's encoders, from its source archive at the tag and digest in `importers.json`, with
no third-party art. They are test inputs: one quad per moving bone, not a model anyone would draw.

Each file in `broken/` takes away one feature. [`scripts/check_mmd_session_fixture.py`](../../../scripts/check_mmd_session_fixture.py)
runs them through the owner's published importer and `avatarMmdSessionCheck`, and each must fail the
comparison on what it took away. `expected.json` holds the committed state at every 30th tick, on both platforms.
Change it only with `--update`, and review what moved.
