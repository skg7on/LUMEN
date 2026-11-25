# ADR 0003 Logical KV layout and page size

Status: Proposed | Date: 2026-11-25

## Context

The MVP can use contiguous KV, while later batching and reuse need logical pages. NPU alignment, DMA, head grouping, and quantization determine actual physical storage.

## Proposed decision

Keep engine block tables logical and resolve physical addresses within workers. Version layout descriptors including dtype, page tokens, alignment, strides/packing, model traits, and shard mapping. Start P2 experiments at 16 tokens/page; select supported sizes through P3 measurement. Publish only full immutable committed pages. Use generation handles, references, leases, and transactional reservations.

## Alternatives and consequences

Hard-coding a GPU layout or page size simplifies the first kernel but prevents honest capability negotiation. Multiple layouts increase qualification cost; enable only measured combinations. P1's fixed contiguous store preserves the logical interface without claiming native paging.

## Acceptance evidence

Dense/paged attention equivalence, checked layout sizing, randomized reclamation tests, target gather/native comparison, and page-size/padding/thermal sweeps. See [P2](../architecture/P2-Continuous-Batching-and-Paged-KV.md) and [P3](../architecture/P3-NPU-Optimization-and-Prefix-Reuse.md).
