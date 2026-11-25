# ADR 0006 Distributed execution authority

Status: Proposed | Date: 2026-11-25

## Context

Replicas, model-parallel ranks, and remote session handoff have different authority requirements. Network timeouts cannot prove that an old owner stopped executing.

## Proposed decision

Keep independent replicas locally scheduled and route requests before model parallelism. One logical engine controls a fixed, epoch-fenced TP/PP group. Remote handoff requires a durable authoritative grant, immutable checkpoint, target preparation fence, and source execution revocation; target execution waits for the grant. Fail closed when authority is uncertain. Retry replica submissions before output only under idempotent reconciliation rules.

## Alternatives and consequences

A global decode scheduler for all replicas increases coordination cost without a model-sharing requirement. Timeout-only handoff permits split-brain. Selecting an authority implementation and deployment/failure model remains a blocking P5 decision; an in-memory coordinator is suitable for fault simulation, not durable production fencing.

## Acceptance evidence

Replica/rank failures, duplicate submissions, stale adverts, group epoch rejection, ownership partitions, coordinator restart, lost acknowledgements, and no duplicate output offsets. See [P4](../architecture/P4-Replica-Mesh-and-Local-TP.md) and [P5](../architecture/P5-Advanced-Distribution.md).
