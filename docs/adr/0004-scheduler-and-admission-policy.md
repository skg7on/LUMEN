# ADR 0004 Scheduler and admission policy

Status: Proposed | Date: 2026-11-25

## Context

Edge capacity is finite; long prefills, growing decode contexts, and slow clients compete for memory and latency. A policy must not bypass allocator correctness.

## Proposed decision

Use pure policy over immutable snapshots. P1 uses strict admission and simple FCFS/static or qualified iteration batching. P2 adds decode-first deadline/slack order, stable tie-breaking, fair prefill aging/service quotas, and bounded token/sequence/time budgets. Cost estimates come from measured supported buckets. P3 may add separately qualified elastic admission and adaptive policy.

## Alternatives and consequences

FCFS remains a comparison/fallback policy. Decode-first without prefill service protection risks starvation. Fully elastic reservations may improve utilization but cannot claim the strict memory envelope and require explicit preemption/recompute behavior.

## Acceptance evidence

Overload/output-credit tests, no OOM within the declared strict envelope, fairness and long-prefill tests, fixed-trace replay, held-out cost prediction, and matched goodput/tail-latency comparisons. See [P2](../architecture/P2-Continuous-Batching-and-Paged-KV.md).
