# LUMEN architecture decision register

The entries below are Proposed, dated 2026-11-25. They record the direction used by the phase designs; acceptance requires the stated evidence. They do not report implemented behavior.

| ADR | Decision area | Acceptance dependency |
|---|---|---|
| [0001](0001-engine-state-and-completion-ownership.md) | Engine state, immutable plans, completion authority | P0/P1 lifecycle and fault tests |
| [0002](0002-versioned-backend-abi.md) | Backend ABI and async handle lifetimes | CPU/mock ABI checks and target SDK probes |
| [0003](0003-logical-kv-layout-and-page-size.md) | Logical KV namespace, physical layouts, page tokens | P2 equivalence/safety; P3 layout measurements |
| [0004](0004-scheduler-and-admission-policy.md) | Admission and decode-first scheduling | P1 bounds, P2 fairness/replay/capacity curves |
| [0005](0005-model-pack-and-precision.md) | Native pack format and qualified precision | Fixture/license, parser, parity, device qualification |
| [0006](0006-distributed-execution-authority.md) | Replica authority, group fences, remote ownership | P4/P5 topology and failure evidence |

Accept or supersede each ADR explicitly before freezing its interface or enabling its feature. Append evidence links and the accepted supported envelope; preserve the rationale when superseding a decision.
