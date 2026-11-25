# ADR 0001 Engine state and completion ownership

Status: Proposed | Date: 2026-11-25

## Context

Asynchronous execution can complete after cancellation, model replacement, or slot reuse. Multiple state writers would make token accounting and logical KV publication difficult to fence.

## Proposed decision

Use one engine owner thread for lifecycle and logical allocation commits. Workers consume immutable plans and return epoch/generation-tagged results. Permit one uncommitted step per sequence and commit a group iteration only after every required fence and detokenizer/stop-state proposal. CPU-pool jobs compute proposals from immutable state; only the engine installs them, before another step may be scheduled. Reserve completion capacity before accepting dispatch. Keep sampled tokens distinct from committed KV positions.

## Alternatives and consequences

Worker-owned request mutation reduces event traffic but introduces concurrent authority; reject it for the baseline. Multiple speculative steps could improve utilization but require a separate rollback/visibility protocol. The baseline favors auditability and deterministic replay; device parallelism still exists inside a plan/rank group.

## Acceptance evidence

Cancellation, stale/duplicate completion, slot reuse, partial submission, group failure, token-cursor, and deterministic event tests. See [shared contracts](../architecture/Shared-Runtime-Contracts.md) and [P1](../architecture/P1-Native-Single-NPU-MVP.md).
