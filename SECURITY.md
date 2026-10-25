# Security Policy

Please do not report a suspected vulnerability in a public issue or pull
request. Avatar assets, motion data and external inputs should be treated as
untrusted. Memory-safety problems, invalid handle or snapshot lifetime handling,
unsafe path handling and unsafe build or package behavior are worth reporting.

## Report privately

Use GitHub's private [Report a vulnerability](https://github.com/animu-sphere/usd-avatar-runtime/security/advisories/new)
form when available. Include only what is needed to reproduce the problem. A
useful report usually contains:

- the affected commit, release or component;
- the impact you observed;
- reproduction steps or a small generated fixture;
- the operating system, compiler, build mode and enabled CMake options;
- relevant runtime ABI, OpenUSD and optional provider or consumer versions.

Do not upload an avatar, texture, motion, capture, credential or private file
unless you have permission to share it. Remove secrets and personal information
from logs. If private vulnerability reporting is unavailable, open a minimal
issue asking for a private contact and do not include technical details.

Maintainers will acknowledge reports when able, investigate the impact, and
coordinate a fix or release as appropriate. Credit is given when requested and
when it does not create a safety or privacy concern.

## Supported versions

The runtime is experimental and its ABI is not frozen. Security fixes target
the latest development revision; older commits and releases do not have a
guaranteed backport policy. Reports affecting older versions are still welcome.

## Scope

The common C contracts and their implementation, frame lifecycle, instance and
snapshot storage, scheduling, adapter integration, host tools, build scripts
and packaging in this repository are in scope. Issues in OpenUSD, format
parsers, motion algorithms, physics backends or renderers may also need to be
reported to their owning project. See the
[dependency boundary](docs/architecture/DEPENDENCIES.md); link upstream reports
once disclosure is safe.
