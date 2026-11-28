# P0 NPU Qualification (LUMEN-002) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Implementation requires a separate execution request; this document authorizes no hardware operation.

**Goal:** Qualify one actual NPU/board/Linux/driver/SDK/toolchain combination and produce reproducible correctness, execution-lifetime, memory, timing, and capability evidence for issue #6.

**Architecture:** Add an opt-in native qualification executable with a vendor-independent probe runner and a narrowly scoped, versioned C qualification interface. Reuse the existing backend ABI for ordinary buffer/copy/queue/event operations; keep experimental compute launch and forced recovery in a qualification-only extension until device evidence informs the production ABI. Keep host fake-adapter checks and real-board acceptance separate, and derive downstream eligibility from recorded evidence rather than SDK feature names.

**Tech Stack:** C++20, C11-compatible backend records, CMake 3.25+, Ninja 1.11+, CTest, the existing checked arithmetic and always-active test harness, one explicitly selected vendor SDK. Python 3.11+ standard-library scripts are development-only evidence validators, never a runtime or packer dependency.

**Spec:** [Issue #6](https://github.com/skg7on/LUMEN/issues/6), [roadmap §3 and §14](../../init-draft/LUMEN-Implementation-Roadmap.md), [architecture §6](../../init-draft/LUMEN-Architecture-Design.md), [shared runtime contracts §§3–7](../../architecture/Shared-Runtime-Contracts.md), and the [P0 integration contract](README.md#p0-readiness-contract).

**Status:** Proposed implementation plan | Date: 2026-11-27 | Phase: P0 | Source checkout: `fa4aaeda36821acb452305ec54bfb275b1e01e94`.

**User decision:** Target selection and hardware access are the first required gate. No NPU vendor, SKU, board, SDK, model, or precision has been selected by this plan.

**Execution status (2026-11-28):** Tasks 2–3 are implemented and verified in the SDK-free harness on a host with no NPU board or vendor SDK. Task 1 and the device-gated steps of Tasks 4–12 remain unexecuted. The harness milestone does not close issue #6; no real-device evidence has been produced.

## Global Constraints

- “Hardware absence is a blocker for qualification, not a passing mock substitute.” — issue #6.
- “Numerical targets remain provisional until measured.” — issue #6.
- “Submission success is not completion.” — current `include/lumen/backend.h`.
- “A timeout does not imply cancellation.” — shared runtime contracts §6.
- “CPU and mock adapters build without vendor headers, libraries, or devices.” — shared runtime contracts §6.
- “No C++ exceptions or STL types cross this boundary.” — shared runtime contracts §6.
- Preserve C++20/C11, CMake/Ninja presets, explicit dependencies, checked sizes/ranges, opaque device resources, and bounded queues/workspace/staging.
- Write only in a worktree under `.claude/worktrees/`, on a `docs/`, `feat/`, or other policy-compliant branch; use conventional commit subjects. The main checkout remains read-only.
- Unsupported or unmeasured capabilities stay disabled. Record unknown, unsupported, failed, and unavailable distinctly; none means supported.
- Do not build an engine, scheduler, model loader, production attention optimizer, or distributed runtime in this issue. A small standalone numeric oracle is sufficient; LUMEN-003/004 remain separate work.

## Review Focus

- A launch times out after commands have already touched resources: retain leases and quarantine until a real completion or verified quiescence proof — Tasks 3, 5, and 10.
- A closed SDK appears asynchronous because a host thread runs a blocking call: require native submission/completion evidence; classify a blocking-only target as no-go for this issue — Tasks 4 and 5.
- A graph compiles but silently pads, converts dtype, or falls back to CPU: validate actual execution placement, masks, precision, and every enabled shape — Tasks 4, 8, and 9.
- An SDK reports free bytes on shared DRAM while workspace, fragmentation, or thermal conditions reduce the usable envelope: measure the combined bounded workload and preserve provenance — Tasks 6 and 10.
- A truncated or stale evidence bundle reports support for the wrong SDK/build: reject incomplete bundles, identity mismatches, missing raw samples, and synthetic evidence — Tasks 2, 7, and 12.

---

## 1. Verified starting point and design decisions

The source checkout contains a static `lumen` target, core status/checked-math/TensorView primitives, and CPU/mock adapters. `tests/CMakeLists.txt` registers eight host tests, including a C11 header test. Host CI covers Linux/macOS, sanitizers, formatting with clang-format 20.1.8, and an ARM64 cross-build. `target-npu` and `target-npu-x86_64` build the CPU/mock foundation for Linux; they do not link a vendor SDK or qualify an NPU.

The public ABI is exactly version 1.0 and validates exact table size and version. Its commands are `NOP`, `COPY`, and `FILL`; it has no graph/kernel registration or compute launch operation. `context_reset` succeeds only with no live child handles and resets simulated state. Existing mock controls provide delayed logical ticks and pending-command counts; they do not implement the roadmap's future partial-launch/reset-fault executor.

Code was located with the knowledge graph and the `submit` and `reset_context` implementations were inspected. The graph did not resolve inbound indirect function-table calls; that empty trace is not evidence of no callers. Direct header reads were needed for the complete ABI comments and record definitions.

### Approach choice

| Approach | Benefit | Cost / disposition |
|---|---|---|
| Vendor example programs alone | Fast first board smoke test | Useful in Task 1, but cannot establish a common evidence/lifetime contract; insufficient as the deliverable |
| Extend the production backend ABI first | One apparent interface for qualification and future serving | Freezes unverified launch/reset semantics and changes exact-size ABI validation before evidence; defer |
| Native runner plus a private qualification extension | Exercises current primitives, isolates vendor behavior, and preserves a path to the production adapter | Requires explicit extension contracts and a fake adapter; selected for this plan |

Use the third approach. A vendor sample is a setup check, followed by the same native harness and independent oracle used for acceptance. Static linking is enough for P0; do not add runtime plugin discovery. Keep the extension private and label it experimental. A later production ABI change requires an ADR and reconciliation with the P1/P2 plans; this issue must not pretend the private launch interface is already that production contract.

### Completion boundaries

1. **Harness ready:** host tests and tooling work without the SDK. This milestone can be reached without hardware and does not close #6.
2. **Target path ready:** one selected SDK builds and an actual asynchronous compute case passes its independent oracle.
3. **Issue accepted:** the full required matrix, safety evidence, bounded fallback, and reviewed P1/P2/P3 decisions are recorded. Missing hardware or unresolved lifetime uncertainty blocks this milestone.

## 2. Target-selection gate and dependency order

Task 1 selects exactly one target. Record alternatives only to explain that choice; do not implement several adapters. Selection must establish:

- Physical board access, device identity, operator, reproducible Linux image, firmware/driver/SDK/compiler compatibility, and rights to use/distribute required test artifacts.
- Native C/C++ submission and completion facilities, a usable compute operator/graph, and a way to prove device placement. A Python-only serving wrapper is insufficient.
- Memory kinds, documented alignment/coherency, finite graph/kernel/workspace limits, and a safely exercisable failure/teardown path.
- Availability of a finite contiguous attention path or a plausible bounded gather path. Lack of custom kernels is acceptable only with a stated fallback and downstream limits.
- A board-approved memory/power/temperature budget and maintenance window for recovery probes. No arbitrary stress-to-OOM or system-wide reset.

No vendor-specific calls or SDK package names are invented here. At Task 1 completion, pin them in `target-selection.md` and `target-profile.json`, cite the exact vendor documentation revision, and name the one translation unit that implements each adapter operation. If the selected target cannot meet asynchronous execution or basic safety requirements, record no-go and reselect before implementing more probes.

Order: **1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12**. Tasks 2–3 and the host portions of later tasks may proceed while board access is pending, using an explicitly synthetic target profile. Task 4 and all actual acceptance remain gated by Task 1. Task 8's representative shapes use the selected candidate model's published dimensions; they do not depend on implementing its model loader or Transformer.

## 3. Planned files and ownership

All paths marked new below are proposed, not present in the source checkout.

| Files | Responsibility |
|---|---|
| `tools/qualify/main.cpp`, `cli.{h,cpp}`, `runner.{h,cpp}` | Native CLI, finite suite selection, orchestration, exit codes |
| `tools/qualify/types.h`, `profile.{h,cpp}`, `profile_config.h.in` | Probe/evidence records and compiled immutable profile |
| `tools/qualify/lifecycle.{h,cpp}` | Receipts, leases, epochs, quarantine, and proof-driven retirement |
| `tools/qualify/{execution,memory,gemm,attention,overlap,thermal}_probe.{h,cpp}` | Individually bounded probe families |
| `tools/qualify/oracle.{h,cpp}` | Independent small numeric/reference calculations |
| `tools/qualify/{statistics,report,decision}.{h,cpp}` | Raw samples, JSON/CSV output, evidence-derived decisions |
| `src/backend/qualification/qualification_api.h` | Private versioned C extension for experimental compute/recovery |
| `src/backend/target_npu/target_npu_backend.{h,cpp}`, `qualification_adapter.cpp` | Exactly one selected SDK's base backend and qualification implementation |
| `tests/support/qualification_fake.{h,cpp}` | Scriptable fake receipts/fences/failures; never hardware evidence |
| `tests/qualification/*_test.cpp`, `tests/abi/qualification_header_c.c` | Host behavior, lifecycle, numeric, and C-boundary tests |
| `tests/device/p0_qualification_test.cpp` | Actual-board suite entry; no skip-to-pass behavior |
| `cmake/LumenQualification.cmake`, `cmake/FindLumenTargetSDK.cmake` | Opt-in targets, profile generation, SDK discovery |
| `scripts/qualification/{validate_evidence.py,test_validate_evidence.py}` | Development-only bundle validation and negative fixtures |
| `docs/qualification/p0/{README.md,target-selection.md,target-profile.json,qualification.schema.json}` | Setup, frozen inputs, evidence format, reproduction |
| `docs/qualification/p0/{qualification.json,summary.md}` | Real accepted result and concise limitations; create only from an actual run |
| `docs/adr/0007-npu-qualification-and-execution-envelope.md` | Selected hardware, private extension, safe fallback, recovery and phase decisions |

Modify `CMakeLists.txt`, `tests/CMakeLists.txt`, `cmake/dependencies.lock.json`, `.github/workflows/host-ci.yml`, `docs/build.md`, and this plan index as needed. Preserve existing preset meanings; a separate explicit build directory is used for the SDK harness. Put bulky raw samples and generated graphs under ignored `build/qualification/`, with stable archived links and SHA-256 digests in the committed manifest. Do not commit proprietary SDK binaries, model payloads, board credentials, or a fake canonical qualification report.

## 4. Common records and evidence semantics

These are proposed exact internal names in namespace `lumen::qualification`; Task 2 owns their definitions. They are internal C++ records, not public ABI types.

| Record | Required fields / meaning |
|---|---|
| `EvidenceState` | `supported`, `unsupported`, `unavailable`, `failed`, `not_tested`; only the first permits a capability, and only with real-device evidence |
| `ProbeId` | Stable string such as `execution/async-gemm/fp16/16x16x16`; unique in a run |
| `TargetIdentity` | vendor, SKU, board/revision, stable non-secret device identifier, architecture, Linux distribution/kernel, firmware, driver, SDK/runtime/compiler versions and digests where readable |
| `RunIdentity` | schema version `1`, run ID, UTC timestamps, git commit/dirty flag/diff digest, compiler/linker/CMake/build flags, adapter/profile/graph hashes, source kind `real_device` or `synthetic` |
| `RunLimits` | host/staging/device/workspace byte budgets, max in-flight commands, graph count, sample count, warmup count, per-case timeout, soak duration, temperature/power stop rules |
| `ProbeResult` | ID, state, reason code/detail, required/optional flag, dependency IDs, input shape/dtype/layout/seed, observed semantics, correctness metrics, timing/memory records, raw artifact references, cleanup outcome |
| `TimingSample` | run/probe/sample IDs, phase `cold`/`warmup`/`measured`, host submit start/return/observed-complete times in monotonic ns, optional device duration, operation bytes/FLOPs, final status |
| `NumericTolerance` | comparison mode `exact` or `abs_rel`, absolute/relative thresholds, accumulation/output dtypes, and profile identity; frozen before device measurement |
| `CapabilityDecision` | capability, state, allowed shapes/dtypes/limits, supporting probe IDs, fallback mode and disabled reason |
| `QualificationDecision` | overall `qualified`/`blocked`/`failed`, reason codes, P1 eligibility, P2 native/gather/no-go, P3 eligible probe families and limitations |
| `EvidenceBundle` | target/run/limits, probe results, capability decisions, final decision, artifact manifest, measurement-completion marker, and integrity state `pending`/`verified` |

`RunLimits` values must be positive, finite, and checked for overflow; the target profile freezes board-specific memory and thermal limits. Warmup/sample counts below are initial experimental defaults, not product performance guarantees. Unsupported numeric cases return evidence with a reason, not an empty passing tensor. Missing metrics use JSON `null` plus a reason; never serialize NaN/Infinity or fabricate zero latency/power.

The profile is JSON consumed at configure time by CMake `string(JSON ...)`, type-checked, and used to generate `profile_config.h`. The executable consumes that compiled profile plus a small set of validated CLI overrides that may only reduce resource limits. Do not implement a second general-purpose JSON parser in C++. Reconfiguration creates a new profile hash; runs never append to a report from a different profile.

### Evidence bundle layout

```text
build/qualification/runs/<run-id>/
  qualification.json       # written last, after every required artifact is durable
  samples.csv              # every measured timing, including errors
  telemetry.csv            # timestamped temperature/power/frequency/memory
  events.csv               # receipt/fence/epoch/retirement sequence
  correctness.json         # per-case oracle method, tolerances, errors, output digests
  build-identity.txt
  graphs-manifest.json     # input/compiler/options/artifact hashes, placement evidence
```

Use temporary files plus atomic rename on one filesystem; an interrupted run must never leave measurements marked complete. The native writer initially sets integrity to `pending`; its successful probe decision remains provisional. The development validator's explicit `--seal` mode computes artifact SHA-256 digests, validates the whole completed bundle, and atomically sets integrity to `verified`. Ordinary validation never mutates evidence and rejects a pending/unsealed bundle when qualification is requested. It resolves artifact paths under the bundle root, rejects path escape, verifies hashes, and cross-checks IDs/counts/identities. Overall acceptance requires both the native probe decision and verified integrity. The committed `docs/qualification/p0/qualification.json` is the validated small manifest with stable raw-artifact locations and digests, not an edited set of favorable results.

---

## 5. Implementation tasks

### Task 1: Select the board and freeze a reproducible qualification profile

**Files:** Create `docs/qualification/p0/README.md`, `target-selection.md`, and `target-profile.json`; create ADR 0007 as Proposed and add it to `docs/adr/README.md`.

**Interfaces:** Produces the authoritative `TargetIdentity`, board-specific `RunLimits`, candidate dense GQA/MHA dimensions, supported precision candidates, vendor documentation references, exact SDK/toolchain setup, and recovery procedure consumed by every later task.

- [ ] **Step 1: Collect candidate facts.** Record board availability, SKU/revision, Linux/driver/firmware/SDK/compiler compatibility, native asynchronous API, graph/custom-kernel support, accessible memory, licensing, telemetry, and reset scope. Distinguish documentation claims from observations.
- [ ] **Step 2: Select one target.** Explain why it can run the P1 contiguous baseline and which P2/P3 risks remain. Record exact install/build/environment commands, paths with secrets omitted, and expected successful device enumeration. Select one candidate model's published dimensions without downloading/executing arbitrary repository code.
- [ ] **Step 3: Freeze safe experimental inputs.** Set all resource limits and timeouts before running stress probes; select shape/precision candidates and tolerance policy. Record operator-approved recovery actions and whether they affect other workloads. A missing budget, SDK license, board, or asynchronous API is a blocking reason.
- [ ] **Step 4: Check reproducibility on the board.** Record OS/driver/runtime/toolchain identity and run a vendor-supported native compute sample with output checking. Preserve its command/log and mark it setup evidence; it does not replace the harness tests. If hardware is unavailable, leave this step unchecked and publish the blocker.
- [ ] **Step 5: Review and commit the selection/profile.** Verify documentation links and no provisional claim is presented as measured. Suggested subject: `docs(backend): define the p0 npu qualification target`.

**Done when:** Another operator can identify the exact target and reproduce native SDK setup, or the work is explicitly blocked with no qualification claim. Do not choose a vendor by guesswork to make this step appear complete.

### Task 2: Define evidence records, profile validation, and acceptance logic

**Files:** Create `tools/qualify/{types.h,profile.h,profile.cpp,profile_config.h.in,decision.h,decision.cpp}`, `cmake/LumenQualification.cmake`, `tests/qualification/{profile_test,decision_test}.cpp`, `docs/qualification/p0/qualification.schema.json`, and development validator scripts. Modify CMake/test registration and dependency metadata.

**Interfaces:** `Result<RunLimits> validate_limits(const RunLimits&)`; `Status validate_profile(const TargetIdentity&, const RunLimits&)`; `QualificationDecision evaluate(const EvidenceBundle&)`. The evaluator checks probe/identity/safety conditions; integrity remains an additional publication gate enforced by the validator. CMake emits the fixed profile records; no runtime vendor SDK is needed to exercise these functions. Validator CLI: `python3 scripts/qualification/validate_evidence.py <manifest> --require-real-device`; explicit sealing adds `--seal`.

- [x] **Step 1: Write failing behavior tests.** Register `p0-qual-profile` and `p0-qual-decision`. Check zero/overflowing budgets and counts, missing SDK/board identity, unknown enums, absent required probes, mismatched profile hashes, synthetic source, and unresolved quarantine. Assert each prevents `qualified`; optional unsupported collectives must not by themselves block the single-NPU result.
- [x] **Step 2: Verify red.** Configure `host-debug`, build, then run `ctest --preset host-debug -R '^p0-qual-(profile|decision)$' --output-on-failure --no-tests=error`. A missing implementation or behavioral failure is valid red; SDK/toolchain setup failure is not.
- [x] **Step 3: Implement the records and fail-closed evaluator.** Qualification requires real identity, actual native asynchronous compute plus independent correctness, required memory/copy/fence/lifetime/recovery evidence, completed sustained run, and a bounded attention path. P2 native paging requires measured native/custom attention evidence; gather eligibility requires the bounded gather experiment. P3 allows only tested shape/precision/operator families. `unavailable` and `not_tested` never enable a path.
- [x] **Step 4: Implement development validation with negative fixtures.** Use the Python standard library and `unittest`; reject duplicate JSON keys, missing fields, wrong types, unknown schema versions, non-finite numbers, truncated bundles, path escape, missing/hash-mismatched artifacts, and fake evidence passed with `--require-real-device`. Record this as test tooling, preserving no runtime dependencies. The schema documents the format; the script enforces the explicitly listed structure and cross-file invariants, without claiming to implement general JSON Schema.
- [x] **Step 5: Rebuild, verify both CTest entries and `python3 -m unittest discover -s scripts/qualification -p 'test_*.py'`, then commit.** Expected: all named cases pass and synthetic fixtures fail real-device validation. Suggested subject: `feat(qualification): define evidence and fail closed gates`.

### Task 3: Specify the private compute/recovery C interface and lifecycle machine

**Files:** Create `src/backend/qualification/qualification_api.h`, `tools/qualify/lifecycle.{h,cpp}`, `tests/support/qualification_fake.{h,cpp}`, `tests/qualification/lifecycle_test.cpp`, and `tests/abi/qualification_header_c.c`. Modify test registration.

**Interfaces:** Private `lumen_qualification_api_v1` has its own size/version fields and points to a validated `const lumen_backend_api_v1 *base_api`. It adds opaque program registration/unregistration, compute launch, memory/telemetry query, and recovery. It is statically linked and never added to `include/lumen/backend.h` in this issue.

The header must define fixed-width C descriptors for program bytes/digest, bounded buffer bindings/ranges, shape/dtype/layout, workspace, and flags. Use these exact operation names:

```c
lumen_status_t program_register(lumen_device_t, const lumen_qual_program_desc_v1 *, lumen_qual_program_t *);
lumen_status_t program_unregister(lumen_qual_program_t);
lumen_qual_submit_receipt_v1 submit_command(lumen_queue_t, const lumen_command_v1 *);
lumen_qual_submit_receipt_v1 launch(lumen_queue_t, const lumen_qual_launch_desc_v1 *);
lumen_status_t query_memory(lumen_device_t, lumen_qual_memory_v1 *);
lumen_status_t read_telemetry(lumen_device_t, lumen_qual_telemetry_v1 *);
lumen_qual_recovery_proof_v1 recover(lumen_context_t, uint64_t expected_epoch);
```

These signatures are function-table members. Descriptors carry `struct_size` and extension major/minor. Both submission functions return a receipt by value so an error can still carry a completion obligation without violating base ABI failed-output rules. `submit_command` covers copies/fills/no-ops as well as their possible uncertainty; a copy failure must not bypass receipt handling. Receipt fields: size/version, `call_status`, disposition `rejected`/`accepted`/`uncertain`, context epoch, optional ordinary `lumen_event_t` completion fence, and touched-resource-set ID. Rejected means no command references resources; accepted requires an owned fence; uncertain retains the full pre-reserved resource set even if no fence can be obtained. The caller never reduces that conservative lease set based only on an adapter-supplied touched-set ID.

Recovery proof fields: size/version, call status, old/new epochs, `quiescent`, `old_handles_invalidated`, recovery mechanism code, and evidence ID. Only a successful real device-wide/context-scoped proof for every affected queue permits retirement after uncertainty. Unsupported recovery returns no proof. Successful forced recovery explicitly invalidates old child handles; this extension behavior does not change base `context_reset` preconditions.

Internal functions: `Status observe_receipt(const Receipt&, LeaseSet&)`; `Status observe_fence(const FenceObservation&, LeaseSet&)`; `Status apply_recovery(const RecoveryProof&, LeaseSet&)`. `Receipt` and `RecoveryProof` are internal aliases of the corresponding C records. `FenceObservation` holds resource-set ID, epoch, event ID, query status, final event/command status, and a qualified-all-work-stopped flag. `LeaseSet` owns bounded resource IDs and epoch; its states are `reserved`, `inflight`, `quarantined`, `retired`.

- [x] **Step 1: Write failing tests.** Register `p0-qual-lifecycle` and `abi-qualification-c`. Script immediate completion; delayed completion; rejection with no side effects; accepted work; uncertainty with/without fence; timeout followed by late success; failed reset; stale/duplicate proof; epoch wrap rejection; early program/buffer/event destruction. Assert no uncertain resource retires on timeout or merely on a failed/pending fence query.
- [x] **Step 2: Build and run the two named tests to verify red.** The C11 translation unit must compile the exact extension header and check required table pointers and version rejection.
- [x] **Step 3: Implement the state machine and fake.** Reserve receipt/event/lease storage before dispatch. Establish ownership before observing an immediate completion. Event FAILED is a retirement fence only if the adapter guarantees all relevant DMA/compute stopped; an API error or failed poll call is not such a fence. Duplicates cannot release twice; stale observations cannot retire a newer epoch.
- [x] **Step 4: Verify Debug, Release, and sanitizer behavior.** Reconfigure/rebuild each preset before running `-R '^(p0-qual-lifecycle|abi-qualification-c)$'`. Linux sanitizer runs use leak detection; macOS omits unsupported LeakSanitizer as documented in `docs/build.md`.
- [x] **Step 5: Commit the passing interface/lifecycle unit.** Suggested subject: `feat(backend): define qualification launch and recovery receipts`.

### Task 4: Build the native runner and exactly one SDK adapter

**Files:** Create `tools/qualify/{main,cli,runner}.{h,cpp}` where applicable (`main.cpp` only), `src/backend/target_npu/{target_npu_backend.h,target_npu_backend.cpp,qualification_adapter.cpp}`, `cmake/FindLumenTargetSDK.cmake`, and `tests/qualification/{cli,adapter_contract}_test.cpp`. Modify CMake, dependency metadata, and setup docs.

**Interfaces:** `Result<Options> parse_options(std::span<const std::string_view>)`; `int run(const Options&, const lumen_qualification_api_v1&)`; `const lumen_backend_api_v1 *lumen_target_npu_backend_api(void)`; `const lumen_qualification_api_v1 *lumen_target_npu_qualification_api(void)`. The latter exports live in the private target header. Vendor code implements its own opaque-handle ownership; never feed its handles into the CPU/mock registry.

Proposed options: `LUMEN_BUILD_QUALIFICATION=OFF`, `LUMEN_ENABLE_TARGET_NPU=OFF`, and `LUMEN_QUALIFICATION_PROFILE=<absolute profile path>`. `LUMEN_BUILD_TESTS=ON` continues to build host tests and an SDK-free `lumen_qualification_support` static library. The new tests link that library plus `lumen_qualification_test_support`; enabling qualification additionally builds the `lumen-npu-qualify` executable. Enable SDK discovery only when the target option is ON; explicitly fail a requested SDK build if missing or incompatible. The standalone executable requires the target option; the host fake runner is test-only and must label every result `synthetic`.

- [ ] **Step 1: Write failing tests.** Register `p0-qual-cli` and `p0-qual-adapter`. Reject unknown/duplicate flags, malformed/overflowing integers, absent output directory, increasing compiled memory limits, incompatible tables, cross-context handles, and missing required adapter operations. Test that an explicit NPU request never falls back to CPU/mock.
- [ ] **Step 2: Verify red using the host fake.** Run the two tests after configure/build. Keep a separate configure check that requests an SDK at a nonexistent path and requires an actionable configure failure.
- [ ] **Step 3: Implement the CLI, build split, and selected adapter.** CLI suites are `execution`, `memory`, `gemm`, `attention`, `overlap`, `thermal`, `recovery`, and `all`; `--output`, `--warmup`, `--samples`, `--soak-seconds`, and `--recover-only` have validated meanings. Exit codes: `0` complete successful full measurement run (acceptance still requires sealing/validation), `2` invalid input, `3` blocked/unavailable, `4` correctness/safety/probe failure, `5` unresolved quarantine. A successful subset run or override below the frozen required sample/soak counts reports its results but returns `3` for overall qualification. Freeze a different accepted count by regenerating the profile before measurement, not by reducing it after failure.
- [ ] **Step 4: Implement SDK ownership/error mapping.** Catch exceptions inside the boundary; validate every size/range/alignment/context. Pin asynchronous inputs/programs/workspace until a proven fence. Translate SDK outcomes to existing status codes plus extension disposition/reasons. If an operation cannot satisfy the existing base `queue_submit` failure contract, reject it there as unsupported before submission and use the private receipt path for qualification. Identify device placement and disable CPU fallback explicitly. A blocking SDK call on a helper thread cannot be advertised as native async completion.
- [ ] **Step 5: Verify host tests and target compilation.** Normal host configure/build/CTest must work with no SDK environment variables. Build the explicit target command in §7 on the selected toolchain; preserve build identity and SDK library versions. Hardware smoke may wait for Task 5, but cannot be reported passing before run.
- [ ] **Step 6: Commit runner/adapter integration.** Suggested subject: `feat(backend): add the selected npu qualification adapter`.

### Task 5: Prove asynchronous compute and completion/ownership semantics

**Files:** Create `tools/qualify/execution_probe.{h,cpp}`, the initial `oracle.{h,cpp}`, `tests/qualification/execution_test.cpp`, and `tests/device/p0_qualification_test.cpp`; modify runner and CTest registration.

**Interfaces:** `ProbeResult probe_execution(ProbeContext&)`; `Status reference_gemm(const GemmCase&, std::span<const double> a, std::span<const double> b, std::span<double> out)`; `Comparison compare_numeric(std::span<const double> actual, std::span<const double> expected, const NumericTolerance&)`. Task 2 defines `Comparison` with count, max absolute/relative error, non-finite count, mismatch count, and pass flag. `ProbeContext` contains the fixed profile, validated API, owned session/lease registry, bounded scratch, and report sink.

- [ ] **Step 1: Write failing host tests and register `p0-qual-execution`.** Check submit-return-before-fence handling, pending poll, wait timeout, immediate completion, ordered commands on one queue, independent queues without assumed ordering, failed event query, corrupted output, and zero/invalid tensor ranges. A deliberately synchronous fake must fail the native-async evidence criterion.
- [ ] **Step 2: Build/run `p0-qual-execution` to verify red.** Use deterministic fake clocks and scripted fences; do not add timing-sensitive sleep assertions.
- [ ] **Step 3: Implement a small actual compute case.** Prefer `16×16×16` GEMM in a native supported precision; if unsupported, freeze an equivalent small graph/operator in the profile with an independent oracle. Fill deterministic nontrivial inputs from seed `6`, poison output, perform H2D → compute → D2H with explicit dependencies, and compare after the final copy fence. A memcpy, no-op, or host computation is not compute acceptance.
- [ ] **Step 4: Collect real async evidence.** Record host submit start/return, completion query/wait behavior, event/queue identities, and placement/timeline evidence. Use enough bounded work to observe pending completion or a native trace proving compute continues after submission return; a tiny graph completing before the first poll is inconclusive by itself. Run host progress between submission and wait without accessing in-flight output. Record blocking staging/upload costs separately.
- [ ] **Step 5: Test real ownership.** Exercise permitted early destroy/release attempts and require rejection without losing ownership; retain descriptors/bindings/staging/programs until the final fence. Check in-order behavior with dependent writes/copies and explicit cross-queue dependencies or host-fenced serialization. Never read or overwrite memory to manufacture an unsafe race.
- [ ] **Step 6: Verify host and `device-p0-execution`, save raw evidence, and commit.** Missing device returns nonzero, not a skip code. A passing host fake cannot complete this task's hardware step. Suggested subject: `test(backend): qualify real async compute and fence ownership`.

### Task 6: Measure memory kinds, copies, allocation bounds, and coherency

**Files:** Create `tools/qualify/memory_probe.{h,cpp}`, `tests/qualification/memory_test.cpp`; extend target adapter, device suite, and report records.

**Interfaces:** `ProbeResult probe_memory(ProbeContext&)`; `Result<uint64_t> gather_workspace_bytes(const AttentionCase&)` is reserved for Task 9; memory queries return total/free/reserved/unknown categories with validity flags, never a single misleading capacity number.

- [ ] **Step 1: Write failing `p0-qual-memory` cases.** Test size/offset multiplication and addition overflow, alignment zero/non-power-of-two/below required minimum, unsupported memory kind, partial allocation rollback, over-budget request, map/unmap misuse, and cleanup after a rejected copy. Assert unsupported kinds stay unsupported and mapped HOST memory is not treated as coherent DEVICE/UNIFIED memory.
- [ ] **Step 2: Build/run the named test to verify red.** Also rerun `unit-checked-math` and `unit-tensor-view` after any necessary primitive change; avoid changing them merely to fit the harness.
- [ ] **Step 3: Implement bounded allocation characterization.** Snapshot memory before/after context open, program registration/prewarm, workspace allocation, measured cases, and cleanup. Probe safe allocations within the frozen budget; report the largest successfully allocated/tested amount as a lower bound, not total free capacity. Record rounding/alignment, fragmentation and repeated allocation behavior without intentionally exhausting system memory.
- [ ] **Step 4: Implement byte-checked transfers.** Probe H2D, D2H, and D2D at candidate sizes `1`, `64`, `4096`, `1048576`, and `16777216` bytes, reducing only via profile budget. Include unaligned offsets and explicit rejection where required. Use deterministic distinct source/destination patterns, guard regions, and round-trip comparisons after fences. Report unsupported directions and zero-copy aliasing/coherency separately; a host memcpy on shared DRAM is not mislabeled DMA.
- [ ] **Step 5: Measure usable combined capacity.** Run allocations with the selected graph, staging, metadata, scratch, and workspace simultaneously resident. Record host RSS/PSS when available and vendor/device high water separately. Account for driver reservations and shared-DRAM double counting; use `null` plus reason for inaccessible metrics.
- [ ] **Step 6: Verify host and `device-p0-memory`, preserve samples, then commit.** Suggested subject: `feat(qualification): measure bounded npu memory and transfers`.

### Task 7: Produce trustworthy timing distributions and atomic JSON/CSV reports

**Files:** Create `tools/qualify/{statistics,report}.{h,cpp}`, `tests/qualification/{statistics,report}_test.cpp`; extend validator fixtures and runner integration.

**Interfaces:** `Result<Distribution> summarize(std::span<const uint64_t> samples)`; `Status write_bundle(const EvidenceBundle&, const std::filesystem::path&)`. `Distribution` holds count, min/max, p50/p95/p99 and arithmetic mean; raw samples remain authoritative.

- [ ] **Step 1: Write failing `p0-qual-statistics` and `p0-qual-report`.** For sorted `1..100`, require p50 `50`, p95 `95`, p99 `99` using nearest-rank `ceil(p*n)`; empty input is invalid. Test checked accumulation, one sample, repeated values, JSON quotes/control characters, locale independence, non-finite metrics, insufficient samples, duplicate probe IDs, interrupted writes, and raw-count/hash mismatch.
- [ ] **Step 2: Build/run both tests to verify red.** Validate emitted fixture JSON independently with the development script rather than accepting a serializer's own interpretation of its output.
- [ ] **Step 3: Implement measurement discipline.** Default to `100` warmup and `1000` measured samples per short case; record counts exactly and freeze any lower counts for expensive cases before the run. Separate first-load/compile/prewarm, cold execution, warmup, and steady measurements. Use monotonic host ns, label host-observed completion bounds versus optional device timestamps, and never subtract incomparable clocks.
- [ ] **Step 4: Record distinct costs.** Keep submission latency, poll call cost, wait/completion latency, end-to-end copy/compute latency, bytes/s, allocation time, and host CPU use distinct. Keep failed samples/statuses in CSV and out of successful-latency percentiles with explicit counts. Report timer resolution, sample count, instrumentation level, and low statistical confidence for small tail samples.
- [ ] **Step 5: Implement durable reporting.** Serialize finite bounded records, check stream/rename failures, write raw artifacts first and the measurement-complete, integrity-pending manifest last, and fail qualification on output loss. The development validator's `--seal` mode computes SHA-256, checks the completed artifact set, and writes the verified manifest atomically; the native executable needs no crypto or Python runtime dependency. Seal mode refuses incomplete, failed, synthetic-as-real, or already inconsistent evidence. Final qualification publication waits for sealing and a second read-only validation of its digests.
- [ ] **Step 6: Verify both CTests, Python validator tests, and one synthetic end-to-end bundle, then commit.** Confirm `--require-real-device` rejects that bundle. Suggested subject: `feat(qualification): emit reproducible timing and capability evidence`.

### Task 8: Sweep representative GEMM, precision, shapes, and finite graph variants

**Files:** Create `tools/qualify/gemm_probe.{h,cpp}`, extend `oracle.*`, create `tests/qualification/gemm_test.cpp`; extend target graph build instructions and graph manifest.

**Interfaces:** `ProbeResult probe_gemm(ProbeContext&, const GemmCase&)`; `GemmCase` contains M/N/K, input/output/accumulation dtype, layout/transpose, graph bucket, seed, and `NumericTolerance`. `reference_gemm` from Task 5 computes the independent expected result with deterministic loops and double accumulation on the already rounded/dequantized device inputs.

- [ ] **Step 1: Write failing `p0-qual-gemm` assertions.** Cover rectangular matrices, tails outside tile alignment, padding/masks, unsupported dtype/bucket, non-finite output, wrong transpose/layout, corrupted output, and tolerance frozen before execution. Comparing device output to a second call to the same SDK must fail the independence requirement.
- [ ] **Step 2: Build/run to verify red.** Include tiny hand-computed exact integer cases to validate the oracle separately from the device comparison.
- [ ] **Step 3: Freeze the representative matrix.** Use candidate decode M `{1,4,16}` and prefill M `{32,128,512}`; derive K/N from selected model QKV/output/MLP dimensions and record exact values. Bound by profile memory/graph count. Include a deliberately unsupported shape. Static SDKs use a finite precompiled bucket set with actual-length/mask tests; dynamic SDKs test min/max/interior and out-of-range inputs. Record compile/prewarm time and variant workspace.
- [ ] **Step 4: Freeze precision comparisons before measuring.** Candidate normalized FP32 abs/rel thresholds are `1e-5/1e-4`, FP16 `1e-2/1e-2`, and BF16 `5e-2/5e-2`; these are proposed experiment values, not universal model tolerances. Adopt or replace them in the target profile using independent error analysis before the device run. Compare `abs(a-e) <= atol + rtol*abs(e)` elementwise and fail NaNs/Infs for finite-input cases. Quantized cases require explicit scales/zero-points/grouping and a separately frozen error budget; otherwise mark them not tested.
- [ ] **Step 5: Measure each enabled case.** Check independent correctness, actual device placement, effective precision and accumulation mode, compilation/cache behavior, latency distributions, workspace, and host overhead. Do not report TFLOP/s for a failed or CPU-fallback case. Exact model-scale CPU reference cost may be offline; use deterministic independent full outputs, not unchecked samples alone, for every enabled shape.
- [ ] **Step 6: Verify host and `device-p0-gemm`, validate raw evidence, and commit.** Suggested subject: `feat(qualification): characterize npu gemm and graph envelopes`.

### Task 9: Qualify contiguous attention, native paging feasibility, or bounded gather

**Files:** Create `tools/qualify/attention_probe.{h,cpp}`, extend `oracle.*`, create `tests/qualification/attention_test.cpp`; extend report decisions and ADR 0007.

**Interfaces:** `ProbeResult probe_attention(ProbeContext&, const AttentionCase&)`; `Status reference_attention(const AttentionCase&, std::span<const double> q, std::span<const double> k, std::span<const double> v, std::span<double> out)`; `Result<uint64_t> gather_workspace_bytes(const AttentionCase&)`. `AttentionCase` contains phase, batch/query/context lengths, query/KV heads, head dimension, dtype, page tokens, block table, valid lengths, mask traits, and frozen tolerance.

- [ ] **Step 1: Write failing `p0-qual-attention`.** Cover contiguous and shuffled pages, experimental page size `16`, contexts `1,15,16,17,31,32,33`, partial last page, mixed valid lengths, causal prefill, fully masked queries defined as zero output, GQA head mapping, invalid block indices, poisoned padding, scratch overflow and unsupported context. Assert output equals the independent dense oracle, invalid/future/padded reads are zero, and a failed gather never launches compute.
- [ ] **Step 2: Build/run to verify red.** Validate the independent dense softmax oracle with hand-checkable examples; it must not reuse vendor page/gather indexing.
- [ ] **Step 3: Probe mechanisms separately.** Record custom-kernel build/launch, native paged-attention availability and actual block-table behavior, device gather/copy feasibility, supported contiguous attention graph, mask inputs, mixed-phase constraints, and static/dynamic shape limits. SDK documentation alone establishes only a claim, not support.
- [ ] **Step 4: Implement the bounded fallback experiment.** Gather valid K/V into preallocated contiguous scratch, fence metadata and gather before attention, then retain scratch through the attention fence. For separate K and V scratch, baseline bytes are `2 * batch * kv_heads * context_bucket * head_dim * element_bytes`, checked and augmented by alignment, metadata, query/output, and SDK workspace. Record graph buckets, maximum tested context/batch, staging/transfer bytes and total latency; reject inputs outside the tested envelope before launch. Shared memory still needs qualified coherency/fences.
- [ ] **Step 5: Make explicit phase decisions.** P1 may use the qualified contiguous path. P2 native mode requires actual paged/custom attention correctness and bounded storage; gather mode requires the fallback experiment and explicit copy/scratch limits. If neither can implement the logical page contract safely, P2 is no-go. P3 native-paged optimization remains disabled on closed backends; qualified contiguous/GEMM optimizations may remain eligible. Feasibility is not a claim of P2/P3 throughput benefit.
- [ ] **Step 6: Verify host and `device-p0-attention`, update the ADR with evidence links, and commit.** Suggested subject: `feat(qualification): qualify bounded attention fallback paths`.

### Task 10: Exercise partial submission, timeout, recovery, and repeated teardown

**Files:** Extend `lifecycle.*`, create `tests/qualification/recovery_test.cpp`, and extend device suite, adapter, runner watchdog, and persistent quarantine handling.

**Interfaces:** `ProbeResult probe_recovery(ProbeContext&)`; `Status record_quarantine(const QuarantineRecord&, const std::filesystem::path&)`. `QuarantineRecord` carries target identity, context epoch, touched-resource IDs, outstanding obligation IDs, cause, and recovery evidence. Runner refuses a new normal session while an unresolved record exists for that device.

- [ ] **Step 1: Write failing `p0-qual-recovery`.** Inject failure before any submission; after H2D acceptance; after compute acceptance; while D2H is outstanding; on polling; during reset; and during report output. Assert only the first permits immediate rollback. Timeout, watchdog termination, child-process exit, or a stale success callback never counts as device quiescence. An accepted command with successful later fence can retire normally.
- [ ] **Step 2: Build/run to verify red.** Use the scriptable fake to cover every disposition, duplicate completion, failed proof, recovery epoch mismatch, and no further dispatch while quarantined.
- [ ] **Step 3: Implement bounded failure isolation.** Run potentially hanging vendor calls in a supervised native child on Linux with a profile timeout; communicate fixed bounded result records. Parent timeout records uncertainty and stops normal device work. Preserve a durable device-identity quarantine marker across process restart and take an exclusive per-device lock before any normal open. Do not forcibly terminate a child or release pinned host resources until the selected driver's teardown/quiescence behavior is qualified; timeout alone is not that proof. Process termination does not authorize buffer reuse; `--recover-only` clears the marker only after the qualified recovery procedure proves old work cannot write and old handles are invalidated.
- [ ] **Step 4: Exercise safe real failures.** Use vendor-supported error injection or harness-controlled rejection between real accepted commands. Distinguish synthetic SDK fault simulation from real-board partial-chain execution. Never send malformed physical pointers or force a driver crash. Verify actual late completion fencing, teardown ordering, and whichever reset/teardown method the vendor supports. If no safe forced recovery is available, demonstrate the fail-closed quarantine/manual maintenance procedure; keep automatic recovery unsupported.
- [ ] **Step 5: Repeat setup/teardown.** Proposed profile: `100` complete open/register/run/fence/unregister/close cycles. Record per-cycle host/device memory and status; define acceptable bounded allocator/cache retention before the run. Monotonic unexplained growth, residual writes, leaked live commands, or unresolved quarantine fails the result. Do not claim base `context_reset` can reset in-flight work.
- [ ] **Step 6: Verify host sanitizer tests and `device-p0-recovery`, preserve event timelines, then commit.** Suggested subject: `test(backend): fence npu partial submission and recovery`.

### Task 11: Measure overlap, thermal/power behavior, and optional collectives

**Files:** Create `tools/qualify/{overlap,thermal}_probe.{h,cpp}`, `tests/qualification/{overlap,thermal}_test.cpp`; extend target telemetry and device suite.

**Interfaces:** `ProbeResult probe_overlap(ProbeContext&)`; `ProbeResult probe_thermal(ProbeContext&)`. Collectives are optional adapter-specific probe cases producing the same `ProbeResult`, not a new production collective interface.

- [ ] **Step 1: Write failing `p0-qual-overlap` and `p0-qual-thermal`.** Separate serial, independent concurrent, and dependency-constrained traces. Assert faster wall time without credible overlapping device intervals cannot enable overlap. Test missing sensor data, throttling, budget/temperature stop, timer discontinuity, shortened soak, and unrelated concurrent load. Unsupported sensors produce unavailable metrics, not zero power.
- [ ] **Step 2: Build/run both tests to verify red.** Inject telemetry and device intervals through the fake; tests must be deterministic.
- [ ] **Step 3: Measure overlap honestly.** Compare matched serial copy+compute versus independent copy/compute on allowed queues, with disjoint buffers and identical work. Record host CPU cost, device/profiler intervals when available, bytes, graph shapes, queue settings, and synchronization. Enable overlap only with trustworthy interval evidence and measured benefit; otherwise default to serialized cost and mark overlap unsupported/not proven.
- [ ] **Step 4: Run sustained load.** Proposed P0 minimum is `1800` seconds after warmup, with telemetry at `1` second intervals and latency/memory windows at `60` seconds. These durations are qualification experiment defaults, not the P6 72-hour soak. Freeze final values in Task 1. Record ambient/cooling/power mode, frequencies, throttle signals, RSS/PSS/vendor memory, initial/steady/end throughput and latency. Stop on frozen thermal/power/safety limits, retain partial evidence, and fail an incomplete required soak.
- [ ] **Step 5: Record collective access.** Query exact SDK/topology support. With no second supported participant or transport, record unavailable and leave TP/PP/remote-KV disabled. If accessible, use tiny independently checked reductions/copies and record RTT/bandwidth, host cost, and completion semantics. Multi-node benchmarks are optional and cannot delay or substitute for required single-device safety evidence; they authorize no distributed engine work.
- [ ] **Step 6: Verify host and `device-p0-overlap` / `device-p0-thermal`, validate sustained telemetry, and commit.** Suggested subject: `feat(qualification): measure overlap and sustained npu limits`.

### Task 12: Wire CI, run the full board matrix, and publish reviewed evidence

**Files:** Modify `.github/workflows/host-ci.yml`, `tests/CMakeLists.txt`, `docs/build.md`, plan/ADR indexes; finalize `docs/qualification/p0/{README.md,qualification.json,summary.md}` and ADR 0007. Add a manually triggered device workflow only if a secured qualified runner actually exists.

**Interfaces:** `lumen-npu-qualify --suite all --output <bundle>` produces the report contract; the development validator checks the sealed bundle; `evaluate()` produces the final recorded phase decisions. Device CTests use the same runner/probes, not a second implementation.

- [ ] **Step 1: Finish CI separation.** SDK-free host builds run all host qualification tests plus existing eight tests, Release checks, sanitizer checks, C11 header checks, validator tests, and format checks. Extend formatting to `tools/qualify` and new target/test sources. Device tests are labeled `device`; host test presets explicitly include label `host` so enabling the adapter locally cannot accidentally run board tests through `host-debug`.
- [ ] **Step 2: Test unavailable hardware behavior.** Register exact device names `device-p0-execution`, `device-p0-memory`, `device-p0-gemm`, `device-p0-attention`, `device-p0-recovery`, `device-p0-overlap`, and `device-p0-thermal`. A missing device/driver/profile returns nonzero and a blocked report; do not set CTest `SKIP_RETURN_CODE`. Device jobs are explicit, do not expose SDKs/boards to untrusted pull-request code, and are absent rather than fictitious when no runner exists.
- [ ] **Step 3: Run the full verification sequence in §7.** Require at least one selected test for every regex, capture commands/exit codes, and validate that old binaries/reports were not reused. A development bundle is never published as the canonical target result.
- [ ] **Step 4: Run the full board suite and seal the artifacts.** Record profile/build/board identities, every required probe, raw timings, errors, memory, thermal data, placement, correctness, and cleanup. If required evidence is missing, publish a blocked/failed summary and keep #6 open. Do not silently omit cases or raise tolerances after observing failures.
- [ ] **Step 5: Review acceptance against §8.** Explain supported shapes/dtypes, tested capacity, unsupported/unknown features, recovery limitations, and contiguous/gather/native decisions. Update ADR 0007 to Accepted only if its required real evidence is accepted; reconcile ADRs 0002/0003 or later phase plans only when their actual interface assumptions change. P0 qualification alone does not accept all of P1/P2/P3.
- [ ] **Step 6: Commit the reviewed evidence and documentation.** Suggested subject: `docs(backend): record the qualified npu execution envelope`. Open a review PR only when execution scope authorizes publishing; link #6 without automatic closure until every criterion is satisfied.

---

## 6. Test registration and red/green conventions

Each new C++ test uses `tests/support/check.h`, returns `lumen::test::summary(...)`, and keeps assertions active in Release. Share the fake adapter as a test-support library rather than copying it. Refactor `lumen_add_host_test` only as needed to link an SDK-free qualification support library; never link target SDK libraries transitively into `lumen::lumen` or the host suite.

For every task with behavior changes:

1. Add its named cases and register its CTest entry.
2. Configure after adding targets, build, then run the anchored regex with `--no-tests=error`; capture a meaningful red failure.
3. Implement only that task's behavior, rebuild, and rerun the same regex to green.
4. Run related existing tests where ownership/ABI/primitives changed; broaden once to the complete host suite before the commit/review boundary.

Task 1 is a documentation/hardware-selection gate rather than a manufactured unit-test cycle. Task 12 is integration and evidence review rather than a duplicate test of every implementation detail. Native device assertions require the actual board even if the fake already exercises the same logical condition.

## 7. Proposed verification commands

These commands describe the future implementation. They were **not run as NPU qualification while writing this plan**. Execute from the implementation worktree, use the toolchain selected in Task 1, and record actual results.

### SDK-free host regression

```bash
cmake --preset host-debug -DLUMEN_WARNINGS_AS_ERRORS=ON
cmake --build --preset host-debug
ctest --preset host-debug --output-on-failure --no-tests=error
python3 -m unittest discover -s scripts/qualification -p 'test_*.py'

cmake --preset host-release -DLUMEN_WARNINGS_AS_ERRORS=ON
cmake --build --preset host-release
ctest --preset host-release --output-on-failure --no-tests=error

cmake --preset host-sanitize -DLUMEN_WARNINGS_AS_ERRORS=ON
cmake --build --preset host-sanitize
ctest --preset host-sanitize --output-on-failure --no-tests=error
```

On Linux, run the sanitizer CTest command with `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1`. On macOS, follow the existing compiler/runtime guidance in `docs/build.md` and omit LeakSanitizer. Apply clang-format 20.1.8 to changed C/C++ files and check `git diff --check`.

### Explicit SDK/board build

```bash
cmake -S . -B build/qualification -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLUMEN_BUILD_TESTS=ON \
  -DLUMEN_BUILD_QUALIFICATION=ON \
  -DLUMEN_ENABLE_TARGET_NPU=ON \
  -DLUMEN_QUALIFICATION_PROFILE:FILEPATH="$PWD/docs/qualification/p0/target-profile.json"
cmake --build build/qualification
ctest --test-dir build/qualification -L device --output-on-failure --no-tests=error
build/qualification/lumen-npu-qualify --suite all \
  --output build/qualification/runs/acceptance
python3 scripts/qualification/validate_evidence.py \
  build/qualification/runs/acceptance/qualification.json --require-real-device --seal
python3 scripts/qualification/validate_evidence.py \
  build/qualification/runs/acceptance/qualification.json --require-real-device
```

Task 1 adds the selected SDK's exact configure variables/environment and a `CMAKE_TOOLCHAIN_FILE` when cross-compiling. Cross-compilation is followed by execution on the matching Linux target; do not run target binaries on the development Mac. The output directory must be new/empty; refuse accidental overwrites. Device CTests write separate per-suite subdirectories, while the final full run produces the one complete acceptance bundle. Device-suite timeout settings must accommodate the recorded soak; a CTest default timeout must not silently truncate it.

No device-facing configure preset or command is considered available until Task 4 implements it. Existing `target-npu` presets retain their original CPU/mock cross-build semantics.

## 8. Acceptance matrix and closure procedure

| Issue requirement | Owning tasks | Required evidence | Blocking conditions |
|---|---|---|---|
| Select one NPU/board/OS/driver/SDK/toolchain | 1, 4 | Immutable target profile, native setup log, full build/runtime identity | Missing hardware/access/version identity or incompatible SDK |
| Actual asynchronous kernel/graph with independently checked output | 4, 5, 8 | Native submit/completion timeline, device placement, oracle/tolerance/output errors | Host-only execution, synchronous wrapper, inconclusive async evidence, wrong output |
| Memory kinds/alignment/usable bytes and H2D/D2H/D2D | 6, 7 | Byte-checked copy results, unsupported reasons, combined high water and allocation lower bound | Unsafe coherency, unbounded allocations, missing required transfer path |
| Ownership, fences, partial submission, reset/teardown and quarantine | 3, 5, 10 | Host fault matrix plus real accepted-prefix/late-fence and supported recovery or fail-closed maintenance procedure | Any reuse before proof, unresolved outstanding work, false process-exit/reset proof |
| Launch/poll/copy distributions and representative GEMM | 7, 8 | Raw samples, quantile method/counts, exact shapes/dtypes, separate cold/warm timing | Missing raw data, changed tolerance, CPU fallback, incomparable clocks |
| Sustained thermal/power and real overlap where available | 10, 11 | Complete frozen soak, timestamped sensors/memory, matched overlap trials or explicit disabled state | Incomplete required soak, unsafe throttle/limit event, invented sensor values |
| Custom/paged attention or bounded gather, shape/precision constraints | 8, 9 | Independent dense comparison, finite buckets/workspace, page/mask/padding tests, explicit fallback | No safe contiguous path, unsupported advertised shapes, unbounded scratch |
| Optional collective access | 11 | Exact topology/API result or explicit unavailable/unsupported status | Optional absence does not block single-NPU qualification; distributed features stay disabled |
| Canonical `docs/qualification/p0/qualification.json` | 2, 7, 12 | Sealed real bundle, schema/semantic validation, durable raw-artifact hashes | Synthetic, stale, truncated, missing identity/artifacts, unresolved quarantine |
| Closed backend P2/P3 go/no-go | 9, 12 | ADR and machine-readable native/gather/no-go plus tested limits | Vague “supports attention” claim or implied native-paging performance |

The report must say whether no reset exists, reset was not exercised, or a specific reset/teardown mechanism was exercised and proved quiescence; those are different outcomes. A target without automatic reset can qualify only with a demonstrated safe fence/teardown or fail-closed maintenance procedure and no unresolved work at acceptance. It cannot advertise automatic recovery. A fundamental inability to contain or eventually retire outstanding DMA/compute is a no-go for the target.

P1 eligibility means the primitive execution envelope is usable for subsequent model integration; it does not certify complete model numerical quality. P2/P3 decisions are per capability and bounded shape/precision, not blanket phase acceptance. Collectives, TP/PP, remote KV, APC, and native paged-attention performance remain disabled unless their own later requirements are met.

Close #6 only after all four issue acceptance checkboxes have reviewable actual-device evidence, all mandatory probes complete, and limitations are explicit. If only the host harness is ready, report that milestone and leave the issue open. Record commands actually executed, their results, and missing checks; never turn this plan's expected outputs into reported measurements.

## 9. Plan self-review and handoff

- Scope maps to roadmap P0.1/P0.5/P0.6 and LUMEN-002; it does not consume or duplicate the remaining CPU Transformer/engine work items.
- Every issue acceptance criterion has an owning task and artifact in §8.
- Base ABI 1.0 and its no-live-children reset rule remain distinct from the experimental compute/recovery extension.
- The five Review Focus failure classes each have concrete tests in their owning tasks.
- Host checks, synthetic fault injection, real board observations, and provisional experiment defaults are explicitly distinguished.
- Hardware selection remains the first gate by user choice. No source, measurement, hardware access, or vendor behavior has been invented to fill it.

This deliverable is the implementation plan only. Review the target-selection gate, private-extension boundary, experimental defaults, and acceptance matrix before requesting execution. The related interfaces favor native execution in one working context; either execution method must preserve the hardware and evidence gates.
