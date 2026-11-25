# Project scope and design

LUMEN is the Lightweight Unified Mesh Engine for NPU: a native inference serving
engine for resource-constrained edge systems.

## Design sources

- Read the [architecture design](../../docs/init-draft/LUMEN-Architecture-Design.md)
  and [implementation roadmap](../../docs/init-draft/LUMEN-Implementation-Roadmap.md)
  before changing module boundaries, execution contracts, or delivery scope.
- Distinguish proposed designs from implemented behavior. Verify the current tree
  before referring to a module, command, test, backend, or capability as available.
- Keep numerical examples, release dates, and performance goals labeled as
  provisional until supported by measurements and a qualified workload.
- Record decisions about the backend ABI, state ownership, KV layout, model format,
  precision, and distributed authority in ADRs under `docs/adr/` when made. Update
  affected design documents when an implementation intentionally departs from them.

## Delivery scope

- Build a CPU reference and mock execution path, qualify one target NPU, then
  deliver one dense model on one NPU before adding advanced serving features.
- Preserve the separation between serving policy, logical KV ownership, device
  execution, and mesh routing. Keep vendor-specific details behind the backend.
- Keep Python out of the shipped runtime and mandatory model packer. Development
  scripts and optional external fixture generation may use Python.
- Prefer replica routing before model parallelism. Add TP, PP, remote KV, or phase
  separation only with explicit hardware support, correctness evidence, and a
  demonstrated capacity or workload benefit.
- Keep unqualified features disabled. Expose supported model, precision, context,
  and API combinations explicitly; do not imply full API parity or hard real-time
  guarantees.
