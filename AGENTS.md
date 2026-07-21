# CEngine Agent Guide

Before changing architecture or adding an engine feature, read
`docs/arch/STATUS.md`, then `docs/arch/IMPLEMENTATION.md`.

- `docs/arch/STATUS.md` identifies the active milestone, active vertical step,
  current implementation inventory, next deliverable, and next automated gate.
- `docs/arch/IMPLEMENTATION.md` defines the small active architecture and stable
  extension seams.
- `docs/arch/DELIVERY.md` owns milestones, promotion evidence, and performance
  gates.
- `docs/arch/CORE.md`, `SYSTEMS.md`, and `NETWORK.md` preserve the complete
  target design. They are lookup references, not permission to implement a
  deferred feature.

Prefer direct concrete code for the active milestone. Do not add placeholder
interfaces, registries, threads, queues, allocators, serialized fields, or
configuration switches for deferred features. Preserve stable IDs, ownership,
authority, phase, coordinate, and backend-boundary contracts while allowing
internal storage and scheduling to remain replaceable.

The worktree may contain user changes. Preserve unrelated modifications and
never treat an untracked architecture file as disposable.
