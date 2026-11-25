# P3 NPU optimization and prefix reuse

Document ID: LUMEN-P3-DESIGN | Version: 0.1 | Status: Proposed design

Date: 2026-11-25 | Release: R0.3 | Source: roadmap §6

## 1. Outcome and entry gate

Turn the accepted [P2 engine](P2-Continuous-Batching-and-Paged-KV.md) into a measured NPU-specific single-node product with automatic prefix caching (APC), qualified kernel/graph improvements, and adaptive SLO policy. Preserve the [shared ownership and completion contracts](Shared-Runtime-Contracts.md).

Entry requires a safe paged/gather path, reproducible R0.2 benchmark, calibrated supported shapes, and an instrumented allocation baseline. Optimize measured bottlenecks. No precision, kernel, layout, or admission-mode change becomes a silent production experiment.

## 2. Kernel and graph architecture

The model runner resolves a finite implementation registry at load time. A registry key includes operator semantics, model traits, shape bucket, input/output/KV precision, layout version, device/SDK, and required capabilities. Each entry declares workspace, alignment, supported masks/lengths, numeric tolerances, and fallback. Unsupported combinations fail startup or request validation.

| Optimization track | Design and qualification |
|---|---|
| Fusion | Compare RMSNorm/residual, QKV/RoPE/KV write, SwiGLU/MLP, and logits/sampling against unfused semantic equivalents |
| Decode attention | Optimize KV bandwidth, GQA head reuse, page gathers, DMA bursts and SRAM buffers for one/few query tokens |
| Prefill attention | Tile matrix work and causal masks for compute utilization; compare separate versus mixed phase programs |
| Long context | Split KV into partial online-softmax states; merge with correct rescaling and explicit all-masked handling |
| Graph variants | Precompile/prewarm finite phase/token/batch/context/precision buckets; expose padding and variant-switch costs |
| DMA and overlap | Double-buffer only where dependencies and target measurements prove overlap; keep serialized fallback |
| Sampling | Prefer small device token/logprob results when qualified; retain measured bounded CPU fallback |

For online softmax, each partial reduction carries `(m,l,o)` where `o` is the unnormalized weighted value sum. Merge using `m=max(m_a,m_b)`, rescaling each `l` and `o` by `exp(m_i-m)`, then normalize by the merged `l`. Handle empty partials without evaluating invalid infinity differences. Test independent dense reference, split counts, tail tiles, padding, and extreme scores.

Measure page tokens 8/16/32/64 against real aligned KV bytes, DMA, tail waste, reusable-prefix granularity, scratch, and latency. A selected layout is immutable for active requests. Switching layout requires a drained/reloaded pool or separately qualified conversion; cache pages cannot be reinterpreted.

## 3. Prefix identity and index

APC indexes only full, committed, immutable pages. The engine owns the bounded index; worker completion merely makes a page eligible for publication.

```text
h_i = H(canonical(domain, model_digest, layout_version, precision,
                  adapter_id, tokenizer/template_identity,
                  attention/position/RoPE_config, tenant_scope,
                  h_previous, token_ids_in_page))
```

Use length-prefixed canonical encoding and a collision-resistant digest such as SHA-256. Verify the candidate's identity and exact token metadata; hash equality alone is insufficient. Include layout/shard compatibility where pages are physically reused. Default tenant scope is isolated; cross-tenant sharing remains off until explicitly approved and security-qualified.

An index entry references pool/slot/generation, identity, predecessor digest, and token metadata. The slot retains generation, valid length, refs/leases, cache membership, and eviction links. Preallocate entry/token metadata storage to a declared capacity and report its footprint. On an identical publication, retain either safely referenced existing data or the new committed page; dedup/remap only at a commit boundary after leases retire.

## 4. Lookup, pinning, and full hits

1. Compute the longest contiguous compatible full-page chain for a validated request.
2. Atomically pin matching generations with admission and future capacity reservation. Eviction cannot interleave between lookup and reference acquisition.
3. Determine the executable prefix and the residual prompt that must produce logits. A complete KV hit still lacks next-token logits.
4. Execute the residual prompt into private writable storage, commit it, then sample under the request's RNG/sampling settings. Cached pages never carry another request's sampling state.

The proposed baseline full-hit strategy reuses only full pages strictly before the final prompt token and recomputes the residual page. For a 64-token prompt and P=16, an index lookup can find all 64 tokens, but execution reuses 48 and recomputes 16 in a private page. This avoids overwriting a sealed page and requires no partial-tail sharing. A later qualified read-only last-token or cached-logits path may improve reuse; it is a separate feature with semantic tests. Report lookup-hit tokens and actually reused tokens separately.

Only full immutable pages are shared. Appending after a sealed page allocates a new page. Branching/partial-tail sharing, if selected, adds COW: reserve a private destination on every rank, copy only valid KV, fence, then transactionally remap before writing. An interrupted copy never exposes the target. Baseline APC does not require sharing mutable tails.

## 5. Eviction, preemption, and optional host tier

Cache membership does not pin a page. Evict cold entries with zero sequence references and zero leases, remove the index before reuse, and advance generation. Favor simple bounded LRU first; cost/reuse weighting needs workload evidence. Active prefixes and transfer sources remain ineligible.

Strict admission remains qualified independently. Optional elastic mode reserves a finite horizon and chooses step-boundary victims by measured restore/recompute cost and fairness. It must preserve committed token/RNG/output offsets and never re-emit output. When safe progress cannot be guaranteed, reject/cancel explicitly rather than allocating beyond the cap.

Host swap requires a real useful tier. Before enabling it on a unified-memory SoC, demonstrate additional usable capacity or net benefit. Bound pinned bytes, DMA descriptors, and in-flight transfers; lease sources and hide targets until copy/integrity fences complete. On timeout, quarantine uncertain destinations. Flash KV is outside this baseline.

## 6. Precision and sampling extensions

Qualify one hardware-native low-bit weight format at a time, such as W8A8 or INT4 weight-only, using explicit group/scale/zero-point/packing descriptors. Do not materialize an unbudgeted full-precision weight copy. Fused dequantization is preferred only where measured. KV INT8/FP8 and activation FP8 require separate device support, scale-index tests, and model/task evidence.

Each registry entry includes a quality report: operator/logit error, non-ambiguous token parity where applicable, selected task accuracy/perplexity, long-context retrieval, multilingual prompts, memory, latency, and sustained energy/temperature. The roadmap's example ≤1 percentage-point task accuracy loss or ≤2% relative perplexity increase is a candidate criterion; the model owner must select applicable thresholds before testing.

Add Top-P/Min-P/logprobs/repetition penalties only with documented combined-filter order and tie semantics. Counter-based per-request RNG keys seed/output position, independent of batch order; commit advancement with the step. Optional guided JSON/regex decoding uses bounded compiled state and an explicit no-valid-token failure. Cap grammar compilation, mask work, vocabulary/logprob output, and response buffers.

## 7. Adaptive scheduling and thermal control

Extend the P2 snapshot with service classes, tenant deficits/quotas, observed model error, thermal/frequency state, and supported variants. Adapt prefill chunks using conservative measured bucket costs; enforce prefill service protection and deadline margins. Unknown thermal states choose conservative qualified limits. Apply hysteresis when reducing/recovering admission so throttling does not oscillate.

Store policy version and cost snapshot in traces. Replay of scheduling decisions and reproducibility of numeric outputs remain separate claims. New modes default off until their feature-matrix evidence passes; fallback must preserve the request's declared quality and cache identity.

## 8. Verification and exit gate

| Roadmap track | Acceptance artifact |
|---|---|
| P3.1 kernels/graphs | Per-shape independent differential tests, scratch bounds, page-size/variant/padding sweeps, target profiling and fallback coverage |
| P3.2 APC | Cached/uncached outputs under qualified rules; full-hit residual recomputation, collision candidates, eviction races, tenant/model/layout changes and partial final page |
| P3.2 optional COW/tiering | Branch/copy/restore cancellation and partial DMA fault tests; no target visibility before fence; measured net benefit |
| P3.3 policy/sampling | Fair class/tenant traffic, thermal transitions, batch-independent RNG, filter/constraint boundary tests and bounded output |

Require zero steady-state allocations in instrumented LUMEN scheduler/allocator/runner paths after warmup except declared bounded cold paths. Measure vendor allocations separately. Run a 24-hour soak with no monotonic LUMEN memory growth. Validate 100% lookup of a retained known compatible full prefix in a controlled warm-cache test while reporting effective reuse after logits recomputation.

Compare R0.3 with R0.2 on identical requests and precision; report a changed precision as a separate experiment. Suggested performance goals are ≥15% goodput improvement or ≥20% device-step latency reduction for targeted shapes, with no unacceptable tail/quality regression. Report maximum declared context/load/temperature envelope, p50/p95/p99/p99.9 latency, energy, padding, and capacity/rejection curves. Targets are provisional, not measured outcomes.

## 9. Deliverables and open decisions

Deliver qualified kernel/graph registry, bounded APC index and eviction, effective-reuse metrics, precision manifests/reports, adaptive policy and sampler extensions, allocation instrumentation, and R0.3 evidence. Ship optional COW/host tier/elastic/guided modes only when individually qualified.

Target evidence must settle supported page/layout sizes, fusion/overlap benefit, thermal limits, first quantization format, and model-owner quality thresholds. Tenant sharing and service-class SLOs are product/security decisions. See [ADR 0003](../adr/0003-logical-kv-layout-and-page-size.md), [ADR 0004](../adr/0004-scheduler-and-admission-policy.md), and [ADR 0005](../adr/0005-model-pack-and-precision.md).
