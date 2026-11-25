# P3 NPU Optimization and Prefix Reuse Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Qualify NPU-specific performance improvements, safe automatic prefix reuse and explicit precision/policy extensions.

**Architecture:** Resolve finite operator/graph implementations at load time, then optimize against independent semantic oracles. Add an engine-owned bounded prefix index with atomic admission pins and conservative full-hit recomputation; keep optional modes individually gated.

**Tech Stack:** C++20, versioned C backend ABI, CMake/Ninja/CTest, CPU/mock adapters and a qualified NPU adapter.

**Spec:** [P3-NPU-Optimization-and-Prefix-Reuse.md](../../architecture/P3-NPU-Optimization-and-Prefix-Reuse.md) and [shared runtime contracts](../../architecture/Shared-Runtime-Contracts.md).

**Status:** Proposed implementation plan | Date: 2026-11-25 | Release: R0.3

## Global Constraints

- “APC indexes only full, committed, immutable pages.”
- “Default tenant scope is isolated; cross-tenant sharing remains off until explicitly approved and security-qualified.”
- “For a 64-token prompt and P=16, an index lookup can find all 64 tokens, but execution reuses 48 and recomputes 16 in a private page.”
- “Strict admission remains qualified independently.”
- “No precision, kernel, layout, or admission-mode change becomes a silent production experiment.”
- Follow the [execution conventions and prerequisite contract](README.md); commands and source paths below are planned interfaces, not available commands in this design-only checkout.
- Keep all writes in an isolated worktree under `.claude/worktrees/`; use conventional commit subjects and preserve CPU/mock checks without a vendor SDK.

## Review Focus

- Stale layout/graph identity must not select a fast but incompatible implementation — Tasks 1–2.
- A full prefix hit must recompute logits without overwriting sealed shared KV — Task 3.
- Eviction racing pin/COW must not reclaim a referenced or copying page — Task 4.
- Quantization scale indexing and batch-reordered RNG must preserve qualified semantics — Tasks 5–6.
- Thermal transitions, elastic pressure and host-copy timeout must preserve bounded progress and ownership — Tasks 7–8.

## Entry gate and task order

Require accepted P2 safety and R0.2 baseline, measured supported variants and allocation instrumentation. Record per-feature enabled/disabled state; optional COW, host swap, elastic admission and guided decoding cannot inherit acceptance from APC.

Tasks 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9. Implement a selected optional branch only after its quality/capacity/benefit profile is accepted; otherwise commit its disabled reason and test that configuration rejects it.

## File structure

| Paths | Responsibility |
|---|---|
| `src/model/implementation_registry.*`, `src/kernels/target_npu/` | Qualified variant dispatch and measured kernels |
| `src/memory/{prefix_index,prefix_reuse,cow,host_tier}.*` | Cache identity, pins, private writes and optional transfers |
| `src/model/precision_registry.*`, `src/model/sampler.*` | Qualified formats and request-scoped sampling |
| `src/scheduler/adaptive_policy.*` | Measured class/tenant/thermal policy |
| `tests/`, `bench/workloads/`, `docs/qualification/p3/` | Independent quality and sustained performance evidence |

---

### Task 1: Build finite qualified implementation dispatch

**Files:**

- Create: `src/model/implementation_registry.h`, `src/model/implementation_registry.cpp`, `tests/unit/implementation_registry_test.cpp`
- Modify: `src/model/model_runner.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P2 layouts/costs and P1 immutable pack/capabilities.
- Produces: `Result<Implementation> resolve_implementation(const ImplementationKey&, const QualifiedRegistry&)`; key includes semantics/traits/shapes/precision/layout/device/SDK; entry declares scratch/alignment/masks/fallback and evidence digest.

- [ ] **Step 1: Write `implementation_lookup_rejects_incompatible_fast_paths` assertions in `tests/unit/implementation_registry_test.cpp` and register CTest `p3-registry`.** Register one measured entry and mismatches for SDK, layout, masks and scratch.

```cpp
CHECK(valid_lookup.ok());
CHECK(wrong_layout.status() == Status::unsupported);
CHECK(unqualified_sdk.status() == Status::unsupported);
CHECK(excess_scratch.status() == Status::resource_exhausted);
CHECK(unknown_mask.silent_fallbacks == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-registry$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Bind variants at load/warmup, validate stable metadata addresses and finite memory, and keep qualified semantic fallbacks. Expose selected implementation and disabled reasons through inspect/trace.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run startup rejection and allocation tracing; selecting an unqualified registry entry must be impossible.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(model): dispatch only qualified npu implementations"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 2: Optimize measured attention and fusion bottlenecks

**Files:**

- Create: `src/kernels/target_npu/attention.cpp`, `src/kernels/target_npu/fused_ops.cpp`, `tests/oracle/optimized_attention_test.cpp`, `bench/workloads/p3-kernels.json`
- Modify: `src/model/implementation_registry.cpp`, `src/kernels/reference/paged_attention.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 registry, P2 independent dense oracle and target timing profiles.
- Produces: `Status merge_softmax_partials(std::span<const SoftmaxPartial>, std::span<float> output)`; SoftmaxPartial contains m/l/unnormalized o; qualified registry entries for selected decode/prefill/fused paths.

- [ ] **Step 1: Write `split_softmax_and_fused_ops_match_independent_reference` assertions in `tests/oracle/optimized_attention_test.cpp` and register CTest `p3-kernels`.** Include empty/all-masked partials, extreme scores, tail tiles, GQA and noncontiguous pages; keep reference expectations independent.

```cpp
CHECK(split_output_error <= qualified_tolerance);
CHECK(all_empty_output == defined_zero_output);
CHECK(nonfinite_merge_values == 0);
CHECK(fused_operator_error <= operator_tolerance);
CHECK(padding_reads == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-kernels$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Merge partial states with max-rescaling and explicit empty-state handling. Profile then implement only beneficial RMSNorm/residual, QKV/RoPE/write, MLP or attention fusion. Prewarm finite variants; fence double buffers and use overlap only when measured.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run device-labeled differential tests and page-token sweeps 8/16/32/64; report bytes, padding, scratch, thermal and latency per shape before enabling entries.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "perf(kernels): qualify measured npu attention optimizations"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 3: Implement bounded prefix lookup and full-hit reuse

**Files:**

- Create: `src/memory/prefix_index.h`, `src/memory/prefix_index.cpp`, `src/memory/prefix_reuse.h`, `src/memory/prefix_reuse.cpp`, `tests/fault/prefix_reuse_test.cpp`
- Modify: `src/scheduler/admission.cpp`, `src/engine/completion_commit.cpp`, `tests/CMakeLists.txt`
- Modify: `src/scheduler/admission.h`

**Interfaces:**

- Consumes: P2 PagePool/KvTxn; P1 pack/token identities and final-prompt emission rules.
- Produces: `Result<PrefixHit> lookup_and_pin(const PrefixIdentity&, std::span<const TokenId>, AdmissionTicket&)`; `ReusePlan plan_prefix_reuse(const PrefixHit&, uint32_t prompt_tokens, uint32_t page_tokens)`. PrefixIdentity covers canonical chained model/layout/precision/tokenizer/attention/tenant metadata; hit carries generation pins.

- [ ] **Step 1: Write `full_hit_recomputes_private_residual_and_checks_identity` assertions in `tests/fault/prefix_reuse_test.cpp` and register CTest `p3-prefix`.** Retain a 64-token prefix at P=16, create incompatible tenant/RoPE/layout candidates and force a digest collision with unequal token metadata.

```cpp
CHECK(full_hit.lookup_tokens == 64);
CHECK(reuse.reused_tokens == 48);
CHECK(reuse.recompute_tokens == 16);
CHECK(writes_to_cached_pages == 0);
CHECK(other_tenant_hit.lookup_tokens == 0);
CHECK(collision_candidate_reused == false);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-prefix$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use length-prefixed canonical chained hashing and exact identity/token verification. Publish only after engine commit; pin with admission atomically. Bound index/token metadata and keep lookup-hit versus effectively reused tokens separate. Extend the existing AdmissionTicket to own tentative prefix pins and roll them back with failed admission; do not add a second admission transaction type.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Compare cached/uncached qualified output, complete/partial prompt pages, model changes and a controlled retained warm prefix with 100% compatible lookup.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(memory): add safe bounded automatic prefix reuse"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 4: Fence cache eviction and optional copy on write

**Files:**

- Create: `src/memory/cow.h`, `src/memory/cow.cpp`, `tests/fault/prefix_eviction_cow_test.cpp`
- Modify: `src/memory/prefix_index.cpp`, `src/memory/page_pool.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 3 pins/index and P2 generation/ref/lease lifecycle.
- Produces: `Status evict_one(PrefixIndex&, PagePool&)`; optional `Result<CowTxn> begin_cow(BlockHandle, uint32_t valid_tokens)` and `Status finish_cow(CowTxn&, const CommitProof&)`.

- [ ] **Step 1: Write `eviction_and_cow_never_publish_unfenced_reuse` assertions in `tests/fault/prefix_eviction_cow_test.cpp` and register CTest `p3-cache-lifetime`.** Race logical eviction with a pin, delay copy completion and cancel the branching request; baseline partial-tail sharing stays disabled unless selected.

```cpp
CHECK(live_reference_page_evicted == false);
CHECK(live_lease_page_evicted == false);
CHECK(reused_slot.index_removed_first);
CHECK(cow_before_fence.target_visible == false);
CHECK(cancelled_copy.source_unchanged);
CHECK(shared_mutable_tails == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-cache-lifetime$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Start with bounded LRU over unreferenced/unleased pages; remove index before generation reuse. Only if branching is selected, copy valid KV into reserved private pages, fence every rank and atomically remap. Appending after a full page allocates instead of copying.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run cancel/stale-generation/copy failure injection and long churn. Disabled COW must reject partial-tail sharing explicitly.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "fix(memory): fence prefix eviction and private tail copies"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 5: Qualify one precision extension at a time

**Files:**

- Create: `src/model/precision_registry.h`, `src/model/precision_registry.cpp`, `tests/oracle/precision_test.cpp`, `docs/qualification/p3/quality-profile.json`
- Modify: `src/model/pack_writer.cpp`, `src/model/implementation_registry.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 registry and the selected target-native format, calibration fixtures and model-owner thresholds.
- Produces: `Result<PrecisionVariant> resolve_precision(const PrecisionKey&, const QualityEvidence&)`; `Status validate_quantization(const QuantDescriptor&, const TensorView&)`; descriptors define groups/scales/zero points/packing and KV scale indexing.

- [ ] **Step 1: Write `precision_requires_quality_and_checked_scale_layout` assertions in `tests/oracle/precision_test.cpp` and register CTest `p3-precision`.** Use malformed scale groups, unsupported format, evidence for another model and known predeclared quality thresholds.

```cpp
CHECK(bad_scale_layout == Status::invalid_input);
CHECK(other_model_evidence == Status::unsupported);
CHECK(unqualified_format == Status::unsupported);
CHECK(unbudgeted_full_precision_copy_bytes == 0);
CHECK(accepted_variant.evidence_digest == expected_digest);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-precision$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Add the single selected low-bit format to pack/runner; separately gate weights, activation and KV formats. Do not change precision silently on fallback or materialize an unbudgeted higher-precision copy.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run board operator/logit/task, long-context retrieval and multilingual tests plus memory/performance. Select thresholds before testing; ≤1 percentage-point accuracy loss or ≤2% relative perplexity increase are roadmap examples, not universal pass rules.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(model): gate precision variants on quality evidence"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 6: Extend sampling with committed request-scoped RNG

**Files:**

- Create: `src/model/logit_processors.h`, `src/model/logit_processors.cpp`, `src/model/constraint_mask.h`, `src/model/constraint_mask.cpp`, `tests/unit/advanced_sampler_test.cpp`
- Modify: `src/model/sampler.h`, `src/model/sampler.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P1 sample API/proposed RNG advance and engine postprocess commit.
- Produces: Extend `SamplingConfig` for Top-P/Min-P/logprobs/repetition and bounded constraints; `Result<ProcessedLogits> process_logits(const LogitInput&, const SamplingConfig&, ProcessorScratch&)`; `Result<ConstraintState> compile_constraint(std::string_view, const ConstraintLimits&)` only if guided mode selected.

- [ ] **Step 1: Write `filter_order_and_rng_are_batch_independent` assertions in `tests/unit/advanced_sampler_test.cpp` and register CTest `p3-sampling`.** Pin combined-filter order/ties, identical output-position seeds under batch reorder, empty valid sets and oversized grammar/logprob requests.

```cpp
CHECK(batch_a.token == reordered_a.token);
CHECK(cancelled_step.committed_rng_counter == prior_counter);
CHECK(filtered_distribution == expected_filtered_distribution);
CHECK(no_valid_token.status() == Status::invalid_input);
CHECK(oversized_constraint.status() == Status::resource_exhausted);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-sampling$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Document processor→constraint→temperature→filters→renormalize order and exact combined-filter semantics. Bound vocabulary work, grammar states and logprob bytes. Return only proposed RNG/state changes; guided mode remains off without its separate limits/evidence.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run deterministic filter/constraint fixtures, stop/EOS boundaries and no-allocation scratch checks.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(model): extend qualified sampling and bounded constraints"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 7: Add class and thermal-aware adaptive policy

**Files:**

- Create: `src/scheduler/adaptive_policy.h`, `src/scheduler/adaptive_policy.cpp`, `tests/unit/adaptive_policy_test.cpp`
- Modify: `src/scheduler/continuous_policy.cpp`, `src/scheduler/cost_table.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P2 choose_step/estimate_step snapshots, measured thermal states and class/tenant policy profile.
- Produces: `StepSelection choose_adaptive_step(const ScheduleSnapshot&, const AdaptiveConfig&)`; config defines service-class/tenant quotas, hysteresis, conservative margins and unknown-state fallback.

- [ ] **Step 1: Write `thermal_and_class_policy_preserve_service_and_bounds` assertions in `tests/unit/adaptive_policy_test.cpp` and register CTest `p3-policy`.** Drive throttled/recovered/unknown states, oscillating sensor readings and mixed long-prefill/interactive tenants.

```cpp
CHECK(unknown_state.uses_conservative_qualified_limits);
CHECK(limit_switches <= configured_hysteresis_bound);
CHECK(aged_prefill.service_delay <= qualified_service_limit);
CHECK(tenant_a.cannot_consume_tenant_b_reserved_quota);
CHECK(replayed_policy_digest == original_policy_digest);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-policy$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use measured conservative buckets and error fallback; reduce/recover admission with hysteresis. Preserve strict reservation and prefill protection; trace policy/config/cost generation for deterministic replay.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run mixed classes and real sustained thermal/shared-memory interference tests; publish rejected demand and no guaranteed SLO under overload.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(scheduler): adapt qualified service to thermal capacity"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 8: Gate optional elastic admission and host tiers

**Files:**

- Create: `src/memory/host_tier.h`, `src/memory/host_tier.cpp`, `src/scheduler/preemption.h`, `src/scheduler/preemption.cpp`, `tests/fault/host_tier_test.cpp`
- Modify: `src/scheduler/admission.cpp`, `src/engine/engine.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P2 leases/transactions, P1 committed token/RNG/output state and a measured useful host tier/restore-cost profile.
- Produces: `Result<RestoreTxn> begin_restore(const FrozenCheckpoint&, const TierBudget&)`; `Status finish_restore(RestoreTxn&, const CommitProof&)`; `Result<PreemptionPlan> choose_preemption(const PressureSnapshot&, const ElasticConfig&)`. Config separately enables finite-horizon elastic and host-copy features.

- [ ] **Step 1: Write `optional_tiering_keeps_targets_hidden_and_output_stable` assertions in `tests/fault/host_tier_test.cpp` and register CTest `p3-tiering`.** Test disabled mode on unified memory without capacity benefit, bounded horizon pressure, delayed DMA and restore cancellation.

```cpp
CHECK(unqualified_tier.status() == Status::unsupported);
CHECK(inflight_transfer_bytes <= configured_transfer_cap);
CHECK(unfenced_target.visible == false);
CHECK(timeout_target.quarantined);
CHECK(restored.committed_rng == checkpoint.committed_rng);
CHECK(restored.reemitted_event_offsets.empty());
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-tiering$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Default both modes off. Select victims at step boundaries with measured fairness/restore cost; reserve target capacity and source leases transactionally. A unified-memory host tier requires measured benefit; never substitute unlimited recomputation/queues for admission bounds.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run strict and elastic modes as distinct reports. Enable only selected branches with real restore/net-benefit evidence; otherwise verify explicit unsupported status.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(memory): gate bounded elastic and host restore modes"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 9: Run allocation, quality and sustained R0.3 qualification

**Files:**

- Create: `src/observability/p3_acceptance.h`, `src/observability/p3_acceptance.cpp`
- Create: `tests/perf/p3_acceptance_test.cpp`, `bench/workloads/p3-mixed.json`, `docs/qualification/p3/release-report.md`
- Modify: `tools/bench/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 1–8 reports and matched R0.2 baseline.
- Produces: `Status validate_p3_report(const PhaseEvidence&, const P3AcceptanceProfile&)`; report separates each optional mode/precision and engine/vendor allocations. Define P3AcceptanceProfile and the gate function in p3_acceptance.h/.cpp, consuming PhaseEvidence from P1 Task 10.

- [ ] **Step 1: Write `p3_gate_rejects_hidden_quality_or_allocation_regressions` assertions in `tests/perf/p3_acceptance_test.cpp` and register CTest `p3-acceptance`.** Validate synthetic warm/cold cache counts, allocation growth, unqualified quality and a precision-changed performance comparison.

```cpp
CHECK(steady_state_hot_path_allocations == 0);
CHECK(full_prefix_lookup_rate == 1.0);
CHECK(effective_reuse_tokens <= lookup_hit_tokens);
CHECK(changed_precision_labeled_separately);
CHECK(unqualified_quality_blocks_acceptance);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p3-acceptance$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Aggregate allocation counters, kernel/cache/policy evidence and raw trace links without hiding excluded cold paths or vendor allocations. Expose maximum declared context/load/temperature and p99.9 tails.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run 24-hour board soak with no monotonic LUMEN growth, cached/uncached parity and predeclared quality tests. Compare identical precision/workload R0.2/R0.3; candidate goals ≥15% goodput or ≥20% targeted device-step reduction require energy/thermal/quality context.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(kernels): qualify optimized prefix aware serving"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

## Phase acceptance and handoff

Require qualified registry/precision entries, safe APC/full-hit behavior, bounded optional modes, zero declared steady-state hot-path allocations and a 24-hour soak without monotonic LUMEN growth. Publish actual improvement and p50/p95/p99/p99.9/energy/thermal trade-offs; suggested performance/quality numbers remain provisional. Freeze the supported R0.3 matrix before mesh work.

## Spec coverage

| Spec sections | Tasks |
|---|---|
| §1–2 kernels/graphs | 1–2 |
| §3–4 cache identity/full hits | 3 |
| §5 eviction/COW/tiering | 4, 8 |
| §6 precision/sampling | 5–6 |
| §7 adaptive policy | 7–8 |
| §8–9 qualification | 9 and feature-specific tests |
