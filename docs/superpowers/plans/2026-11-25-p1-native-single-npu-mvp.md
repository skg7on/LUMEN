# P1 Native Single NPU MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a native single-NPU generation service with safe cancellation, exact model semantics and bounded streaming.

**Architecture:** Build from accepted P0 primitives into a checked native pack/loader, CPU-pool postprocessing, contiguous logical KV and one owner-thread commit loop. Add the native C interface and HTTP/SSE as adapters over the same bounded engine; gate readiness on real backend warmup.

**Tech Stack:** C++20, versioned C backend ABI, CMake/Ninja/CTest, CPU/mock adapters and a qualified NPU adapter.

**Spec:** [P1-Native-Single-NPU-MVP.md](../../architecture/P1-Native-Single-NPU-MVP.md) and [shared runtime contracts](../../architecture/Shared-Runtime-Contracts.md).

**Status:** Proposed implementation plan | Date: 2026-11-25 | Release: R0.1

## Global Constraints

- “The mandatory packer and serving path contain no Python runtime dependency.”
- “The baseline has one plan executing per worker and one uncommitted step per sequence.”
- “Only the final chunk can sample.”
- “Keep `inflight_step` set through this bounded postprocess stage; reserve its job/completion/state buffers before launch.”
- Use strict admission and contiguous KV; APC, native paging, distributed execution and full API parity remain deferred.
- Follow the [execution conventions and prerequisite contract](README.md); commands and source paths below are planned interfaces, not available commands in this design-only checkout.
- Keep all writes in an isolated worktree under `.claude/worktrees/`; use conventional commit subjects and preserve CPU/mock checks without a vendor SDK.

## Review Focus

- Malformed tensor offsets and model identity must fail before allocation — Task 2.
- Stop text split across tokens and delayed decoder proposals must not leak text or schedule an extra token — Tasks 3 and 7.
- Cancellation after partial submission must retain leases and suppress late output — Task 7.
- A slow reader and a full ingress queue must leave cancellation and terminal delivery bounded — Tasks 8–9.
- Zero output, empty normalized prompt and failed warmup must have explicit behavior — Tasks 3, 6 and 9.

## Entry gate and task order

Finish all six P0 outputs in the [readiness contract](README.md#p0-readiness-contract). Task 1 records their actual names/digests and blocks on missing target/model/precision/shape evidence. Do not implement a vendor adapter against an unspecified SDK or count mock generation as NPU acceptance.

Tasks 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10. Task 6's hardware branch waits for qualified P0 inputs; host work remains independently runnable.

## File structure

| Paths | Responsibility |
|---|---|
| `src/model/{model_pack,pack_writer}.*`, `tools/pack/main.cpp` | Checked immutable artifacts |
| `src/tokenizer/{tokenizer,postprocess}.*`, `src/model/sampler.*` | Exact token semantics and proposed output state |
| `src/memory/contiguous_kv.*`, `src/scheduler/admission.*` | Logical reservations and bounded capacity |
| `src/model/model_runner.*`, `src/engine/engine.*` | Device execution and single-writer commit |
| `include/lumen/lumen.h`, `src/frontend/{native_api,http_server}.*` | C and network adapters |
| `tools/bench/`, `tools/inspect/`, `tests/`, `docs/qualification/p1/` | Verification and first release evidence |

---

### Task 1: Verify P0 inputs and freeze the MVP integration profile

**Files:**

- Create: `src/model/qualification.h`, `src/model/qualification.cpp`
- Create: `tests/integration/p0_readiness_test.cpp`, `docs/qualification/p1/input-profile.json`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt` after P0 creates them

**Interfaces:**

- Consumes: P0 `Result<T>`, `Status`, ABI/version and fixture/device evidence described in the index.
- Produces: `Status validate_p0_inputs(const QualificationInputs&)`; `QualificationInputs` contains six evidence digests, real-device launch/fence proof, model/tokenizer/template IDs, precision, graph buckets and memory bounds.

- [ ] **Step 1: Write `p0_readiness_requires_real_evidence` assertions in `tests/integration/p0_readiness_test.cpp` and register CTest `p1-readiness`.** Construct a complete deterministic profile and copies with missing fixture digest, mock-only device proof and unsupported shape.

```cpp
CHECK(validate_p0_inputs(complete) == Status::ok);
CHECK(validate_p0_inputs(missing_fixture) == Status::invalid_input);
CHECK(validate_p0_inputs(mock_only) == Status::unsupported);
CHECK(validate_p0_inputs(unqualified_shape) == Status::unsupported);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-readiness$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Validate actual evidence and reconcile P0 path/type names with all plans before continuing. Freeze the selected model/SDK/precision and candidate limits in input-profile.json; reject incomplete profiles rather than guessing a vendor or model.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run the complete host preset and the actual P0 launch/fence probe on the named board. Record missing-board results as blocked.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(model): enforce mvp qualification prerequisites"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 2: Create the native packer and immutable loader

**Files:**

- Create: `src/model/model_pack.h`, `src/model/model_pack.cpp`, `src/model/pack_writer.h`, `src/model/pack_writer.cpp`
- Create: `tools/pack/main.cpp`, `tests/unit/model_pack_test.cpp`, `docs/modelpack/schema-v1.md`
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 profile; P0 checked arithmetic, TensorView and native tensor-loader foundation.
- Produces: `Result<ModelPack> load_pack(std::string_view path, const PackLimits&)`; `Status write_pack(const PackInput&, std::string_view output)`; ModelPack pins manifest/tensor/tokenizer/template digests, traits, precision, layout and finite graph/workspace descriptors.

- [ ] **Step 1: Write `pack_rejects_invalid_regions_and_identity` assertions in `tests/unit/model_pack_test.cpp` and register CTest `p1-pack`.** Write tiny licensed fixtures in the test temp directory: valid, overlapping tensors, overflowing offsets and checksum mismatch.

```cpp
CHECK(load_pack(valid_path, limits).ok());
CHECK(load_pack(overlap_path, limits).status() == Status::invalid_input);
CHECK(load_pack(overflow_path, limits).status() == Status::invalid_input);
CHECK(load_pack(tampered_path, limits).status() == Status::corrupt);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-pack$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Validate every range before allocation/mmap access. Extend Safetensors-first import into reproducible packed tensors/index/manifest and tokenizer assets; reject unknown traits. Add `lumen-pack --input <path> --profile <json> --output <directory>` with atomic output staging, and document schema/version compatibility.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Pack the same small fixture twice and compare semantic manifest/artifact digests. Run parser fuzz seeds, license checks and CPU fixture loading; no Python is needed by lumen-pack.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(modelpack): add validated native model packs"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 3: Implement tokenizer parity and immutable decoder proposals

**Files:**

- Create: `src/tokenizer/tokenizer.h`, `src/tokenizer/tokenizer.cpp`, `src/tokenizer/postprocess.h`, `src/tokenizer/postprocess.cpp`
- Create: `tests/oracle/tokenizer_parity_test.cpp`, `tests/unit/postprocess_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 2 ModelPack tokenizer/template identity and predeclared token fixtures.
- Produces: `Result<TokenizedPrompt> tokenize_prompt(const ModelPack&, const PromptInput&)`; `Result<DecoderProposal> postprocess(const DecoderSnapshot&, TokenId, const StopConfig&)`. Snapshot/proposal include GenerationKey, prior output cursor, bounded UTF-8/stop state, proposed bytes and terminal reason; no live sequence mutation.

- [ ] **Step 1: Write `tokenizer_and_stop_boundaries_match_reference` assertions in `tests/unit/postprocess_test.cpp` and register CTest `p1-tokenizer`.** Register the oracle and boundary suites together. Fixtures include model-specific BOS/template normalization, split UTF-8, stop text across tokens and an empty normalized input.

```cpp
CHECK(tokenized.ids == independent_expected_ids);
CHECK(first_proposal.bytes.empty()); // undecidable UTF-8 or stop suffix
CHECK(final_proposal.bytes == expected_visible_prefix);
CHECK(final_proposal.stop_matched);
CHECK(snapshot.output_offset == prior_offset); // job did not mutate input
CHECK(tokenize_prompt(pack, still_empty).status() == Status::invalid_input);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-tokenizer$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use the P0-qualified tokenizer/template library with pinned assets and footprint/license review. Bound tokenization jobs, maximum decoded token bytes, stop count/length and proposal storage. Keep unresolved suffixes in immutable next-state proposals; never emit special/stop text contrary to the model policy.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run independent tokenizer/template fixtures plus every cross-token stop/UTF-8 split. Reject out-of-vocabulary IDs and oversized stops before job admission.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(tokenizer): preserve parity and bounded stop proposals"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 4: Add deterministic bounded MVP sampling

**Files:**

- Create: `src/model/sampler.h`, `src/model/sampler.cpp`, `tests/unit/sampler_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Model vocabulary/precision, StepItem sampling configuration and request-scoped RNG counters from P0.
- Produces: `Result<Sample> sample(std::span<const float> logits, const SamplingConfig&, const RngCounter&, SamplerScratch&)`; Sample carries token and proposed next counter; SamplingConfig defines greedy/temperature/top-k and stable ties.

- [ ] **Step 1: Write `sampling_is_seeded_bounded_and_batch_independent` assertions in `tests/unit/sampler_test.cpp` and register CTest `p1-sampler`.** Use finite logits with tied maxima, invalid ranges and identical seed/output positions in different batch orders.

```cpp
CHECK(greedy_tie.token == lowest_tied_token_id);
CHECK(sample_a.token == reordered_sample_a.token);
CHECK(sample_a.next_counter == reordered_sample_a.next_counter);
CHECK(invalid_temperature.status() == Status::invalid_input);
CHECK(all_masked.status() == Status::invalid_input);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-sampler$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Implement CPU greedy/temperature/top-k with explicit processor/filter order and vocabulary-bounded scratch. Return proposed RNG advancement only; the engine commits it. Define nonfinite logits/no-valid-token behavior and maximum logit staging bytes.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Compare seeded fixtures and prove scratch never exceeds profile bounds at maximum vocabulary/batch. Measure CPU sampling and logit-transfer costs on the selected path.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(model): add deterministic bounded mvp sampler"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 5: Implement contiguous logical KV and strict admission

**Files:**

- Create: `src/memory/contiguous_kv.h`, `src/memory/contiguous_kv.cpp`, `src/scheduler/admission.h`, `src/scheduler/admission.cpp`
- Create: `tests/unit/strict_admission_test.cpp`, `tests/unit/contiguous_kv_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P0 buffer handles/lease fences, Task 1 limits and Task 2 actual aligned layout sizes.
- Produces: `Result<KvReservation> ContiguousKv::reserve(SeqId, uint32_t total_tokens)`; `Status commit(const KvReservation&, uint32_t valid_tokens)`; `Status retire(const KvReservation&, FenceState)`; `Result<AdmissionTicket> admit(const RequestBudget&, Capacity&)`. Tickets own request/token/output/scratch/KV commitments transactionally.

- [ ] **Step 1: Write `strict_admission_rolls_back_and_waits_for_fence` assertions in `tests/unit/strict_admission_test.cpp` and register CTest `p1-admission`.** Use tiny fixed arenas and one reservation with a delayed read/write lease. Exercise arithmetic overflow and the last available request/output slot.

```cpp
CHECK(over_budget.status() == Status::resource_exhausted);
CHECK(capacity_after_failed_admission == capacity_before);
CHECK(retire_before_fence == Status::stale);
CHECK(free_bytes_while_leased == 0);
CHECK(prompt_plus_output_overflow == Status::invalid_input);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-admission$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Charge actual contiguous allocations, alignment, future worst-case length and vendor reserve separately. Bound request/queue/token/output pools, expose logical snapshots without device addresses, and release only fenced resources. Keep cancellation capacity independent of new-request credits.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Randomize allocate/append/cancel/retire against a simple independent capacity ledger; verify no OOM inside the chosen strict envelope and no hidden emergency allocation.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(memory): enforce contiguous kv ownership and admission"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 6: Integrate the qualified model runner and startup lifecycle

**Files:**

- Create: `src/model/model_runner.h`, `src/model/model_runner.cpp`, `src/backend/target_npu/adapter.cpp`
- Create: `tests/integration/model_runner_test.cpp`, `tests/oracle/npu_forward_test.cpp`, `tools/inspect/main.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 2 ModelPack, Task 4 sampler, Task 5 logical storage and accepted P0 backend ABI/capabilities.
- Produces: `Status ModelRunner::load(const ModelPack&, const RunnerLimits&)`; `Status warmup()`; `Result<Submission> launch(const StepPlan&)`; `ServiceState readiness() const`. Submission distinguishes rejected/accepted/partial and owns its completion obligation.

- [ ] **Step 1: Write `runner_readiness_requires_qualified_warmup` assertions in `tests/integration/model_runner_test.cpp` and register CTest `p1-runner`.** Use mock device capabilities for host tests and an explicit device-labeled oracle target for real NPU comparisons.

```cpp
CHECK(before_load == ServiceState::starting);
CHECK(unsupported_variant == Status::unsupported);
CHECK(failed_warmup_readiness == ServiceState::failed);
CHECK(successful_warmup_readiness == ServiceState::ready);
CHECK(partial_submit.keeps_all_leases);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-runner$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Bind exact model traits, graph variants and preallocated weights/KV/metadata/scratch. Implement startup config/capability/memory checks, prewarm/self-test, bounded drain and qualified teardown. Extend inspect to print manifest/capability/disabled-mode reasons. Do not assume overlap, dynamic shapes or custom attention.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Build `target-npu` and run device-labeled first-layer/full-logit and greedy non-ambiguous fixtures with agreed tolerances. Verify failed warmup and repeated startup/shutdown never report READY or leak live handles.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(backend): integrate qualified single npu runner"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 7: Implement owner-thread execution and postprocess commit

**Files:**

- Create: `src/engine/engine.h`, `src/engine/engine.cpp`, `src/engine/completion_commit.cpp`, `src/engine/postprocess_dispatch.cpp`
- Create: `tests/fault/engine_commit_test.cpp`
- Modify: `src/engine/sequence.h`, `src/engine/step_plan.h`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 3–6 plus P0 immutable plan/result and mock completion controls.
- Produces: `Result<RequestHandle> Engine::submit(const GenerationRequest&)`; `Status cancel(RequestHandle)`; `Status on_device_result(const StepResult&)`; `Status on_postprocess_result(const DecoderProposal&)`; `Result<std::optional<OutputEvent>> poll_output(RequestHandle)`; `void tick(uint64_t now_ns)`. Engine alone owns sequence/RNG/decoder/KV/output commits.

- [ ] **Step 1: Write `engine_commits_only_matching_fenced_postprocessed_steps` assertions in `tests/fault/engine_commit_test.cpp` and register CTest `p1-engine`.** The local fixture drives a 3-token prompt, two prompt chunks, delayed decoder proposal, cancellation, duplicate/stale epochs and partial launch.

```cpp
CHECK(intermediate_prefill.generated_tokens == 0);
CHECK(after_final_prefill.kv_committed_tokens == 3);
CHECK(after_final_prefill.generated_tokens == 1);
CHECK(before_postprocess.inflight_step != 0);
CHECK(before_postprocess.next_dispatches == 0);
CHECK(stale_proposal_commits == 0);
CHECK(cancelled_late_result.output_events == 0);
CHECK(partial_failure.reused_unfenced_pages == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-engine$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Reserve completion/postprocess/output storage before dispatch, retain inflight_step through decoder evaluation, and validate every identity/prior cursor/fence. Commit all selected items atomically; suppress cancelled output and quarantine uncertain buffers. Never replay already emitted events on failure.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Inject immediate CPU completion before queue draining, stale/duplicate results and postprocess failure; verify terminal drain retires each resource once and no extra decode follows a stop.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(engine): commit fenced steps and decoder state atomically"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 8: Expose the bounded native C interface

**Files:**

- Create: `include/lumen/lumen.h`, `src/frontend/native_api.cpp`, `tests/integration/native_api_test.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 7 Engine submit/cancel and bounded engine output events.
- Produces: C ABI `lumen_submit(lumen_engine_t, const lumen_request_v1*, lumen_request_t*)`, `lumen_cancel(lumen_request_t)`, `lumen_poll(lumen_request_t, lumen_event_v1*)`, `lumen_request_release(lumen_request_t)`; version/size/ownership documented in the header. Empty polling returns LUMEN_OK with event kind LUMEN_EVENT_NONE; internal Engine::poll_output returns a successful empty optional.

- [ ] **Step 1: Write `native_api_preserves_lifetime_and_output_credits` assertions in `tests/integration/native_api_test.cpp` and register CTest `p1-native-api`.** Use the CPU/mock engine, undersized ABI records and a client that stops polling while ingress is full.

```cpp
CHECK(bad_record_status == LUMEN_INVALID_INPUT);
CHECK(cancel_twice_status == LUMEN_OK);
CHECK(cancel_when_ingress_full_status == LUMEN_OK);
CHECK(output_queue_bytes <= configured_output_bytes);
CHECK(release_before_fence_does_not_free_live_storage);
CHECK(callbacks_on_engine_owner_thread == 0);
CHECK(empty_poll.status == LUMEN_OK && empty_poll.kind == LUMEN_EVENT_NONE);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-native-api$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use opaque handles and fixed-width versioned records, no STL/exceptions. Polling returns bounded text/terminal events with monotonic event offsets. Release drops only the external reference; the engine still drains leases. Map internal statuses explicitly and document string/buffer lifetimes.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Compile public headers as C and C++; run native streamed/nonstreamed generation and disconnect/cancel lifetime scenarios.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(frontend): expose bounded native generation api"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 9: Add bounded HTTP/SSE and service controls

**Files:**

- Create: `src/frontend/http_server.h`, `src/frontend/http_server.cpp`, `src/frontend/request_validation.cpp`, `src/frontend/sse_writer.cpp`
- Create: `src/lumend_main.cpp`, `configs/dev/mvp.json`, `docs/api/mvp.md`, `tests/integration/http_stream_test.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 8 native API; Task 1 limits and Task 6 readiness.
- Produces: `Status HttpServer::start(const HttpConfig&, lumen_engine_t)`; `Result<GenerationRequest> validate_http_request(const HttpRequest&, const ModelPack&, const ApiLimits&)`; documented completions/chat/models/healthz/readyz/metrics subset.

- [ ] **Step 1: Write `http_stream_handles_limits_zero_output_and_disconnect` assertions in `tests/integration/http_stream_test.cpp` and register CTest `p1-http`.** Run a local mock-backed listener with strict authentication/limits and bounded output. Include zero requested output, invalid empty prompt, client disconnect, overload and a device error after the first delta.

```cpp
CHECK(zero_output.device_launches == 0);
CHECK(zero_output.terminal_events == 1);
CHECK(empty_prompt.http_status == 400);
CHECK(overload.http_status == 429 || overload.http_status == 503);
CHECK(post_stream_error.terminal_errors == 1);
CHECK(disconnect.cancel_observed);
CHECK(slow_client.buffer_high_water <= configured_output_bytes);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-http$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use a reviewed native HTTP/TLS library or explicitly configured trusted gateway. Bound body/time/queue/tokenization/output work, authenticate before expensive work, validate fields without silent truncation, serialize SSE events once, and propagate disconnect/cancel. Readiness remains false until warmup; shutdown drains then fences.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run API/error-schema fixtures, unsupported-field rejection, special-token/EOS usage rules, signal shutdown and slow-client timeout. Confirm callbacks/socket writes do not block engine commits.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(frontend): serve bounded native http and sse"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 10: Build the first benchmark and MVP acceptance report

**Files:**

- Create: `src/observability/phase_evidence.h`, `src/observability/phase_evidence.cpp`, `src/observability/p1_acceptance.h`, `src/observability/p1_acceptance.cpp`
- Create: `tools/bench/main.cpp`, `src/observability/request_metrics.h`, `src/observability/request_metrics.cpp`
- Create: `tests/integration/mvp_acceptance_test.cpp`, `bench/workloads/p1-mixed.json`, `docs/qualification/p1/release-report.md`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Tasks 6–9 real/CPU services, request event offsets, monotonic timing and allocation categories.
- Produces: `Result<RunReport> run_workload(const Workload&, GenerationClient&)`; `Status validate_mvp_report(const RunReport&, const MvpProfile&)`; CLI `lumen-bench --workload <json> --endpoint <url> --report <json>`. Define `RunReport` and `PhaseEvidence` in phase_evidence.h with identity/profile digests, gate outcomes, metrics, failures, durations and raw-artifact references; `PhaseEvidence to_phase_evidence(const RunReport&)`. Define MvpProfile and validate_mvp_report in p1_acceptance.h/.cpp.

- [ ] **Step 1: Write `mvp_report_counts_failed_and_rejected_requests` assertions in `tests/integration/mvp_acceptance_test.cpp` and register CTest `p1-acceptance`.** Feed a synthetic run with known admitted, rejected, timed-out and successful requests into report validation before running real load.

```cpp
CHECK(report.offered == report.admitted + report.rejected);
CHECK(report.failed_admitted == known_failures);
CHECK(report.goodput_requests == known_slo_successes);
CHECK(validate_mvp_report(mock_only_report, profile) == Status::unsupported);
CHECK(validate_mvp_report(growing_memory_report, profile) == Status::corrupt);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p1-acceptance$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Add bounded metrics/trace drops and raw timestamps without prompt logging. Separate warmup, tokenize/queue/device/postprocess/commit/stream costs and vendor/LUMEN memory. Report provisional limits honestly; include first-demo script and reproduction metadata. Keep evidence serialization reusable by subsequent phases and preserve missing/unsupported/failed outcomes distinctly.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. On the qualified board, run at least 1,000 mixed requests and a 2-hour stress test; test HTTP and native generation, safe cancel, EOS/length and repeated startup. Record TTFT/ITL/TPOT, p99 host dispatch, RSS/PSS/device high water and power if available.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(engine): qualify the native single npu mvp"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

## Phase acceptance and handoff

Require the actual NPU to stream correct generation under the named profile, safe cancellation, all required parity/lifetime/API tests, ≥1,000 mixed requests and a 2-hour stress test with no invalid reference, crash or monotonic LUMEN allocation growth. The roadmap's 4 active/16 queued/2K context and prompt/output sweeps are candidate experiments, not assumed supported limits. Record the R0.1 baseline before P2; missing board evidence blocks acceptance rather than disabling its test silently.

## Spec coverage

| Spec sections | Tasks |
|---|---|
| §1–2 scope/P0 | 1 |
| §3–4 modules/pack/startup | 2, 5, 6, 8 |
| §5 execution | 3, 4, 6, 7 |
| §6 admission/delivery | 3, 5, 7–9 |
| §7–8 API/lifecycle/faults | 6–9 |
| §9–10 evidence/deliverables | 1–10 |
