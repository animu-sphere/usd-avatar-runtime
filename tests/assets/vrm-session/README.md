# Session end-to-end fixture

A small VRM 1.0 avatar, with bone and expression LookAt, and a VRMA clip.
[`tools/make_session_fixture.py`](../../../tools/make_session_fixture.py) writes them, with no
third-party art, so they are redistributable under this repository's license. They are test
inputs: one quad per joint, not an avatar anyone would draw.

[`scripts/check_session_fixture.py`](../../../scripts/check_session_fixture.py) runs them through
the owner's published importers, pinned by SHA-256 in `importers.json`, and through
`avatarVrmSessionCheck`, on Windows and Linux. `expected.json` holds the committed state at every
30th tick, with instance identity and an explicit empty visibility layout. Native handles
are process-local, so the comparison checks one-to-one instance correspondence across
runs. Change it only with `--update`, and review what moved.
