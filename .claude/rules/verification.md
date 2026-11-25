# Verification and performance evidence

## Correctness checks

- Discover runnable checks from the actual build and CI configuration. Run relevant
  checks on the final tree before committing; report commands, results, and checks
  that could not run. Never describe an unexecuted check as passing.
- For documentation-only changes, check links, fenced blocks, consistency with
  the source designs, and whitespace. Do not add implementation-mirroring tests
  for prose or claim runtime tests when no runnable suite exists.
- Preserve an independent CPU oracle and small licensed, versioned fixtures.
  Compare operators, layers, logits, and token output using explicit per-dtype
  tolerances; distinguish same-backend replay from cross-backend numerical parity.
- KV/lifecycle changes need invariant tests and fault injection for cancellation,
  stale completions, generation reuse, reservation rollback, partial launch, and
  asynchronous reclamation. Cover page boundaries, partial pages, masks, and
  non-contiguous layouts in attention tests.
- Exercise host memory and concurrency safety with suitable sanitizers and parser
  fuzzing. Keep target-board checks separate from CPU/mock checks; report device
  and SDK qualification gaps explicitly.

## Performance claims

- Performance changes include measurements or an explicit explanation of why
  measurements are unavailable. Record commit, hardware, SDK, model, precision,
  context/output lengths, batch/arrival profile, warmup, and run duration.
- Compare matched configurations. Report TTFT and inter-token latency distributions,
  SLO goodput, offered/admitted/completed/rejected demand, memory, and relevant
  thermal/power conditions; aggregate tokens/s alone is insufficient.
- Treat simulator estimates and design targets as such. Do not extrapolate local
  links to WAN collectives or compare unrelated NPU/GPU runs as an engine speedup.
- Keep optimizations gated until correctness, supported-resource bounds, and the
  intended workload benefit are demonstrated. Do not silently change production
  precision or cache semantics to improve benchmark numbers.

Sources: [architecture verification strategy](../../docs/init-draft/LUMEN-Architecture-Design.md)
and [roadmap acceptance gates](../../docs/init-draft/LUMEN-Implementation-Roadmap.md).
