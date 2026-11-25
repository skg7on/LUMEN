# P6 Industrial qualification

Document ID: LUMEN-P6-DESIGN | Version: 0.1 | Status: Proposed design

Date: 2026-11-25 | Release: R1.0 | Source: roadmap §9–13

## 1. Release scope and entry gate

Qualify a bounded, supportable deployment on named edge hardware, models, SDKs and workloads. R1.0 may consist of [P3 single-node](P3-NPU-Optimization-and-Prefix-Reuse.md) and [P4 replica mesh](P4-Replica-Mesh-and-Local-TP.md). Include [P5 features](P5-Advanced-Distribution.md) only if individually accepted. Reliability, security, build reproducibility and observability have already started in P0/P1; this phase closes their production acceptance gates.

Entry requires accepted [shared contracts](Shared-Runtime-Contracts.md), phase evidence for every enabled feature, no unresolved fundamental ownership issue, and a proposed supported matrix. The release owner freezes the acceptance workload and thresholds before the qualification run; failures cannot be hidden by silently shrinking the evaluated population.

## 2. Supported envelope and configuration

Publish a machine-readable compatibility matrix keyed by hardware/board, firmware/OS/driver/SDK, build/compiler, model/tokenizer/template digest, precision/KV layout, graph buckets, topology/ranks, API features, maximum context/concurrency, admission mode, SLO class, and thermal/power limits. Every shipped combination links to its correctness and resource evidence. Unknown combinations remain disabled.

Validate configuration schema/version, checked limits, usable memory, graph coverage, model integrity, backend capabilities, queue/transfer budgets and credentials before READY. Immutable configuration generations are pinned to requests. Changing precision/model/layout/topology requires drain or a separate generation/replica; safe policy adjustments record their generation and preserve existing promises. Invalid reload preserves the last valid configuration and reports a reason.

## 3. Reliability control plane

| State | Entry and allowed activity |
|---|---|
| STARTING | Validate config/artifacts, open device, allocate pools, load/prewarm/self-test; no request admission |
| READY | Admit only within qualified capacity |
| DEGRADED | Reduce or stop admission with reason; retain safe local progress/retirement |
| DRAINING | Reject new work, complete or cancel existing work within deadline |
| FAILED | Stop dispatch, fence affected epochs, preserve diagnostics and quarantine; supervisor handles reset/restart |

A watchdog observes bounded progress timestamps for engine, workers, transport and readiness, rather than relying on a single responsive HTTP thread. Classify driver/device timeout, bad plan/result, exhausted budget, transport loss, corruption and operator shutdown separately. The watchdog never frees a buffer merely because a deadline passed.

Supervisor policy uses bounded retries/backoff and qualified context/device reset or process restart. Readiness returns only after safe teardown and model self-test. TP/PP failure fences its group; unrelated replicas remain routable. If the hardware cannot establish safe retirement after reset, keep the device unavailable and escalate to the documented recovery path. Crash reports contain versioned metadata and reason codes, with sensitive content excluded by default.

Drain/upgrade stages are: remove new routing assignments, reject new local admissions, await existing completion until deadline, cancel leftovers, fence work, checkpoint diagnostics, terminate, install verified immutable artifact, self-test, then restore routing. Rollback reinstates a known compatible binary/config/model generation; it does not reinterpret live KV. Interrupted installation cannot leave a partial model selected.

## 4. Security and supply chain

Require authenticated service access, tenant quotas, request/model/config size and time limits, encrypted remote transport, scoped identities and least-privilege device access. Public and administrative listeners have explicit trust boundaries. TLS/HTTP/tokenizer/format libraries are selected through license/footprint/security review rather than reimplemented simply to remove dependencies.

Fuzz model headers/tensor offsets, tokenizer assets, API parsing, config, IPC plan slabs and any remote protocol. Check arithmetic/alignment before allocation or DMA. Preserve tenant-scoped cache identities, retention/deletion rules and sensitive-buffer reuse policy. Cross-tenant prefix sharing remains opt-in with a documented side-channel decision. Do not log prompts/token contents by default; protect diagnostic dumps and fixtures.

Pin dependencies and target toolchains, generate SBOM/license records, verify production manifest signatures/checksums, and install atomically. Define signing-key trust/rotation and artifact provenance. Audit runtime privileges, writable paths, secrets injection, outbound control channels and dependency vulnerabilities. The release blocks unresolved vulnerabilities that meet the agreed blocking severity/exploitability policy; the policy and scan date accompany the evidence.

## 5. Observability and support

Bound telemetry storage and export work; expose drop counters. Keep request IDs and reason codes stable across frontend/engine/executor/router traces. Measure ingress/tokenization/queue/device/fence/commit/output durations, KV and future commitments, scratch/weights/runtime memory, output pressure, padding, rejection, thermal throttling, reset and recovery. Avoid high-cardinality prompt/session labels in metrics.

Use node-local monotonic durations. Cross-node clock comparisons require a measured synchronization error; otherwise retain per-node durations and causal IDs. Report TTFT and per-token ITL distributions separately from average TPOT. Publish total host RSS/PSS and device usage beside LUMEN-owned category totals, vendor reservations and configured staging pools.

Support artifacts include install/run/rollback/drain guides, API schema and supported fields, model/precision/topology matrix, known limits, diagnostics collection/redaction, incident classification and escalation procedure. `lumen-inspect` reports actual capabilities, artifact compatibility and disabled-feature reasons without launching unsupported work.

## 6. Verification layers and build matrix

| Layer | Required coverage |
|---|---|
| Native host CI | CPU/mock tests without vendor SDK; C ABI C/C++ header/version compatibility; parser fuzz corpus; sanitizer configurations appropriate to host concurrency |
| Numerical qualification | Independent operator/layer/logit fixtures, greedy non-ambiguous cases, per-dtype errors, tokenizer/template parity and selected task quality |
| Lifecycle/property | Allocation, cancel, stale/duplicate completion, generation exhaustion/reuse, partial submission, reservation rollback, tail/COW and eviction |
| Board/device CI | Qualified SDK/driver, graph/shape/precision matrix, DMA/event/reset behavior, sustained thermal/shared-memory interference |
| Distribution if enabled | Router retries/partitions, rank/collective loss, transfer/grant/output fencing and no duplicate output |
| Deployment | Clean install, startup failure, upgrade, interrupted install, drain, rollback and reinstall on supported images |

Cross-build ARM64/x86-64 where supported with CMake presets/toolchains and pinned dependency manifests. Record toolchain/build flags and artifact digests. Reproduce a clean build and installation from the documented inputs; explicitly record vendor toolchain components that limit bit-for-bit reproducibility. Debug/test dependencies do not become mandatory runtime Python dependencies.

The simulator reuses pure scheduling interfaces where practical while retaining an independent correctness checker. Calibrate with held-out real traces. Roadmap candidate goals are ≤15% relative p50 and ≤25% p99 latency prediction error within a qualified range; disclose error near saturation and outside measured buckets. Simulation cannot replace board performance or fault qualification.

## 7. Release acceptance matrix

The numeric thresholds below are roadmap candidates. Product/model owners must approve the actual named envelope and thresholds before execution; this document reports no achieved results.

| Area | Candidate acceptance gate | Required evidence |
|---|---|---|
| Correctness | All required fixtures and selected quality thresholds pass; no known critical sequence/KV consistency defect | Independent/differential report with predeclared tolerances and model digests |
| Memory safety | No critical sanitizer finding, reproducible UAF/double-free/OOB, or OOM within strict envelope | Host sanitizer/fuzz/property/fault results; board memory diagnostics |
| Stability | 72-hour target-board soak with no unexpected crash/hang or unbounded LUMEN growth | Open-loop mixed traffic, cancels, thermal cycles; driver memory tracked separately |
| Host overhead | p99 scheduler + KV planning + dispatch CPU time ≤5% of representative device-step duration | Absolute microseconds, batch/shape and trace attribution; revise with evidence |
| Interactive SLO | Example p99 TTFT ≤500 ms and p99 ITL ≤80 ms | Named prompt/output/load/cache/thermal profile and client-observed timings |
| Goodput | ≥99% of admitted requests satisfy the declared per-request SLO at nominal qualified load | Offered/admitted/completed/rejected rates, failures/timeouts counted as misses, saturation curves |
| Host footprint | Candidate ≤64 MiB engine-owned metadata/scratch | Exclude and separately report tokenizer, weights, vendor/OS, configured token/output/KV staging pools; include full RSS/PSS |
| Recovery | Example healthy-replica routing restored ≤10 s after detectable node loss | Repeated injected failures; detection and restore intervals; worker readiness separately measured |
| Security | No unresolved release-blocking vulnerabilities; config/model integrity verified | Threat model, fuzz/scan results, signed artifact/install verification |
| Deployment | Reproducible clean install/upgrade/drain/rollback/reinstall | Automated acceptance logs and supported image matrix |

Define the observation window, sample count and population for every percentile. Specify whether ITL percentiles aggregate tokens or request maxima; publish both where tail gaps affect the user. A request's SLO predicate explicitly defines its TTFT and token-gap limits, EOS/length handling, and timeout. Goodput counts only requests/generated tokens from requests meeting that predicate. Failed admitted requests remain misses. Report rejected offered demand so rejection cannot inflate the apparent capacity.

Run cold/warm and sustained workloads, prompt/output/context sweeps, active-sequence/load saturation, cache hit/miss/tenant mixes, bursty/open-loop arrivals and shared memory/NIC interference. Match hardware/model/precision/workload against R0.3/R0.5 or another fairly runnable baseline. If an external baseline is unavailable, disclose the limitation; an unrelated GPU result is not an engine speedup comparison.

## 8. Release evidence and decision

Proposed evidence bundle:

```text
release-manifest.json       build, dependencies, hardware, SDK and artifact digests
support-matrix.json         enabled combinations, limits and qualification links
correctness/                fixture, numeric, task-quality and ABI results
safety/                     property, sanitizer, fuzz and fault results
performance/                workload/config/seed, raw timestamps, curves and deltas
soak/                       allocation, thermal, crash/hang and recovery timelines
security/                   SBOM, licenses, scan policy/results and trust configuration
deployment/                 install, upgrade, drain, rollback and runbook evidence
release-report.md           gate-by-gate outcome, limitations and accepted scope
```

Encoding and filenames are author-proposed builder choices; required contents and traceability are the contract. Every result identifies commit/build flags, model/precision/layout, backend/SDK, limits/policy, offered load, seed, warmup/duration, quality, latency, goodput, resources, failures, matched baseline and raw evidence.

The release owner accepts only the combinations whose gates pass. An unqualified optional feature is disabled and excluded from the support matrix. A failed required gate blocks the release until corrected or until an explicit product/design revision is approved and requalified; it is not silently relabeled as success. A soak is evidence, not proof of zero future failures. Hard real-time certification requires a separate platform program.

## 9. Deliverables and decisions

Deliver signed/versioned binaries and model artifacts, deployment templates, support/API/runbook documentation, machine-readable capability/support matrix, diagnostic tooling, complete release evidence and accepted ADRs for enabled features. Final acceptance settles named target images, model-owner quality/SLO criteria, security policy, key management, recovery budget and optional feature scope.

See the [ADR register](../adr/README.md) and [roadmap release gates](../init-draft/LUMEN-Implementation-Roadmap.md#9-p6--industrial-product-qualification-r10). R1.0 qualification must reflect the actual shipping configuration rather than the full set of architectural possibilities.
