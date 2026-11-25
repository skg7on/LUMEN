# ADR 0005 Model pack and precision

Status: Proposed | Date: 2026-11-25

## Context

The mandatory loading/conversion path must be native and reproducible. Weights, tokenizer/template, graph variants, precision, and KV layout jointly define compatible execution.

## Proposed decision

Implement a native Safetensors-first packer with bounds/integrity validation and a versioned immutable manifest. Record semantic tensor traits, checksums, tokenizer/template digests, backend/SDK requirements, graph/workspace bounds, precision, and partition descriptors. Never execute arbitrary model repository code. Qualify one precision first; add low-bit weights and KV formats as separate feature combinations.

## Alternatives and consequences

GGUF is deferred until useful for a selected model. Runtime Python conversion conflicts with the product constraint. A new pack schema adds maintenance; its exact encoding remains open pending loader/fixture review. Production manifest signing is completed during qualification.

## Acceptance evidence

First model/license selection, malformed/overlapping/overflow parser tests, reproducible pack digests, independent layer/logit/token parity, and per-format quality/memory/performance results. See [P1](../architecture/P1-Native-Single-NPU-MVP.md) and [P3](../architecture/P3-NPU-Optimization-and-Prefix-Reuse.md).
