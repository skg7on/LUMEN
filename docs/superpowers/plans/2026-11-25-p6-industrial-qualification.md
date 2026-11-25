# P6 Industrial Qualification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a bounded supported release with explicit compatibility, safety, security, recovery and performance evidence.

**Architecture:** Freeze the shipping support matrix and acceptance workload before qualification, then harden lifecycle/configuration/deployment around the existing engine. Aggregate independently measured evidence into machine-readable gates; optional P5 paths are included only if shipping and already qualified.

**Tech Stack:** C++20, versioned C backend ABI, CMake/Ninja/CTest, CPU/mock adapters and a qualified NPU adapter.

**Spec:** [P6-Industrial-Qualification.md](../../architecture/P6-Industrial-Qualification.md) and [shared runtime contracts](../../architecture/Shared-Runtime-Contracts.md).

**Status:** Proposed implementation plan | Date: 2026-11-25 | Release: R1.0

## Global Constraints

- “R1.0 may consist of P3 single-node and P4 replica mesh.”
- “Unknown combinations remain disabled.”
- “The watchdog never frees a buffer merely because a deadline passed.”
- “Invalid reload preserves the last valid configuration and reports a reason.”
- “Failed admitted requests remain misses.”
- “Hard real-time certification requires a separate platform program.”
- Follow the [execution conventions and prerequisite contract](README.md); commands and source paths below are planned interfaces, not available commands in this design-only checkout.
- Keep all writes in an isolated worktree under `.claude/worktrees/`; use conventional commit subjects and preserve CPU/mock checks without a vendor SDK.

## Review Focus

- A support-matrix mismatch or missing evidence must block readiness/release — Tasks 1 and 9.
- A responsive HTTP thread must not hide a stalled engine/device; uncertain resets must not reuse buffers — Task 2.
- Interrupted install/reload and rollback must preserve an intact old generation — Tasks 3 and 7.
- Corrupt/signature-invalid artifacts, cross-tenant diagnostics and malicious parsers must fail within limits — Tasks 4–5.
- Telemetry pressure, rejected load and timed-out admitted work must not inflate reported capacity — Tasks 6, 8 and 9.

## Entry gate and task order

Require accepted shared contracts and evidence for every selected P3/P4/P5 feature. Task 1 freezes actual model/target/SDK/SLO/quality/security thresholds with release-owner agreement; roadmap numeric examples are provisional until then. Do not rerun an easier workload and silently replace a failed release population. A single-node release can qualify directly from P3; omit mesh-only integration when P4 is not shipping, and report those gates as not applicable rather than passed.

Tasks 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9. Reliability/security/CI foundations already exist from P0/P1; these tasks close gaps and qualify the shipping envelope.

## File structure

| Paths | Responsibility |
|---|---|
| `src/qualification/{support_matrix,release_gate}.*` | Supported combinations and evidence acceptance |
| `src/observability/watchdog.*`, `src/config/config_generation.*` | Progress/fault supervision and immutable reload |
| `src/security/{artifact_trust,diagnostics}.*` | Signed installs, scoped diagnostics and tenant controls |
| `deploy/`, `tests/{fuzz,fault,integration,perf}/`, `.github/workflows/` | Reproducible images/builds and qualification automation |
| `docs/qualification/p6/`, `docs/runbooks/` | Frozen profiles, reports and support procedures |

---

### Task 1: Freeze the supported matrix and acceptance profile

**Files:**

- Create: `src/qualification/support_matrix.h`, `src/qualification/support_matrix.cpp`, `tests/unit/support_matrix_test.cpp`, `docs/qualification/p6/acceptance-profile.json`, `docs/qualification/p6/support-matrix.json`
- Modify: `tools/inspect/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P3/P4 and selected P5 evidence, immutable model/build/SDK/topology identities.
- Produces: `Status validate_support(const RuntimeIdentity&, const SupportMatrix&)`; `Result<ReleaseProfile> load_release_profile(std::string_view path)`; profile defines population/window/minimum samples, SLO predicate, quality/safety/security/recovery gates and explicitly selected features.

- [ ] **Step 1: Write `support_matrix_requires_exact_qualified_combinations` assertions in `tests/unit/support_matrix_test.cpp` and register CTest `p6-support`.** Use known model/SDK/layout combinations, an unsupported cross-product, missing evidence and a P3/P4-only profile.

```cpp
CHECK(qualified_combination == Status::ok);
CHECK(unknown_cross_product == Status::unsupported);
CHECK(missing_evidence == Status::invalid_input);
CHECK(p3_p4_only.requires_p5 == false);
CHECK(profile.thresholds_frozen_before_run);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-support$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Validate exact combinations instead of multiplying individually qualified features. Record agreed thresholds and evidence digests before the run; inspect explains every disabled/unsupported path.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Cross-check all shipping binaries/config/model variants against the matrix; no speculative future feature is listed as supported.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(qualification): freeze supported release combinations"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 2: Harden watchdog and bounded recovery

**Files:**

- Create: `src/observability/watchdog.h`, `src/observability/watchdog.cpp`, `src/executor/supervisor.h`, `src/executor/supervisor.cpp`, `tests/fault/watchdog_recovery_test.cpp`
- Modify: `src/model/model_runner.cpp`, `src/executor/group_recovery.cpp` if TP enabled, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 profile and existing local/group lifecycle, epochs, quarantine and reset proofs.
- Produces: `HealthDecision evaluate_progress(const ProgressSnapshot&, const WatchdogConfig&, uint64_t now_ns)`; `Result<RecoveryAction> Supervisor::handle(const HealthDecision&)`; snapshot separately tracks engine/worker/postprocess/transport progress.

- [ ] **Step 1: Write `watchdog_detects_stalled_engine_and_preserves_quarantine` assertions in `tests/fault/watchdog_recovery_test.cpp` and register CTest `p6-watchdog`.** Keep HTTP responsive while device/engine stalls; fail reset, exceed retry budget and recover a single group beside a healthy replica.

```cpp
CHECK(responsive_http_stalled_engine.health != ServiceState::ready);
CHECK(timeout_without_fence.freed_buffers == 0);
CHECK(retry_count <= configured_retry_cap);
CHECK(unproved_reset.readiness == ServiceState::failed);
CHECK(healthy_replica.remains_routable);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-watchdog$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Classify reason/stage, stop admission/dispatch, fence epochs and use bounded backoff/reset or process restart. Restore readiness only after safe teardown, model reload and self-test. Do not turn retry exhaustion into unsafe reuse.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Inject worker crashes/driver timeouts/transport loss on supported boards. Record detection, route restoration and local readiness recovery as separate intervals. For a single-node profile, run local recovery and mark mesh-only recovery not applicable; do not require P4 modules.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "fix(executor): bound watchdog recovery behind retirement proof"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 3: Make configuration generations and reload atomic

**Files:**

- Create: `src/config/config_generation.h`, `src/config/config_generation.cpp`, `tests/integration/config_reload_test.cpp`
- Modify: `src/lumend_main.cpp`, `src/engine/engine.cpp`, `tests/CMakeLists.txt`
- Modify: `src/mesh/node_lifecycle.cpp` only if replica mesh is selected

**Interfaces:**

- Consumes: Task 1 support matrix and immutable request/model generations.
- Produces: `Result<ValidatedConfig> validate_config(const ConfigInput&, const SupportMatrix&, const DeviceCaps&)`; `Status activate_config(const ValidatedConfig&, ReloadMode)`; request pins ConfigGeneration.

- [ ] **Step 1: Write `bad_reload_keeps_last_valid_config_and_active_semantics` assertions in `tests/integration/config_reload_test.cpp` and register CTest `p6-config`.** Reload invalid schema/overflowing limits, an unqualified graph, a safe policy adjustment and a changed layout while requests run.

```cpp
CHECK(invalid_reload.active_generation == previous_generation);
CHECK(over_budget_config == Status::resource_exhausted);
CHECK(active_request.layout_id == original_layout);
CHECK(layout_reload.requires_drain_or_new_replica);
CHECK(policy_reload.records_new_config_generation);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-config$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Validate caps/memory/queues/credentials before activation. Use atomic validated generations; reject in-place precision/model/layout/topology mutation. Preserve existing request promises during safe policy changes and keep failed reload reasons observable.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Exercise startup failure, invalid reload rollback, concurrent drain and graceful shutdown with finite deadlines.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(config): activate validated immutable configuration generations"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 4: Verify signed artifacts and atomic installation

**Files:**

- Create: `src/security/artifact_trust.h`, `src/security/artifact_trust.cpp`, `deploy/install_manifest.cpp`, `tests/integration/artifact_install_test.cpp`, `docs/runbooks/artifact-trust.md`
- Modify: `src/model/model_pack.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 trust/signing policy, P1 pack digests and Task 3 generations.
- Produces: `Status verify_artifact(const ArtifactManifest&, const TrustStore&)`; `Result<InstalledGeneration> install_atomically(const VerifiedArtifact&, const InstallPaths&)`; signature trust/rotation and rollback metadata are explicit.

- [ ] **Step 1: Write `tampered_or_interrupted_install_never_replaces_active_artifact` assertions in `tests/integration/artifact_install_test.cpp` and register CTest `p6-artifacts`.** Use corrupted payloads, wrong/revoked keys, mismatched config digest and interruption before/after atomic selection.

```cpp
CHECK(tampered_payload == Status::corrupt);
CHECK(untrusted_signature == Status::invalid_input);
CHECK(interrupted_install.active_digest == old_digest);
CHECK(successful_install.active_digest == verified_new_digest);
CHECK(rollback.reinterpreted_live_kv == false);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-artifacts$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Use a reviewed cryptographic library with pinned dependencies, canonical signed manifests and trust-store policy. Verify before activation, install immutable directories and change selection atomically; rollback only via compatible drained/new generations.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Test disk/full/permission/interruption failures and key rotation on deployment images. Record artifact provenance and trust configuration without private signing keys.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(security): verify and atomically install release artifacts"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 5: Complete parser, tenant and diagnostic isolation

**Files:**

- Create: `src/security/diagnostics.h`, `src/security/diagnostics.cpp`, `tests/fuzz/model_pack_fuzz.cpp`, `tests/fuzz/plan_slab_fuzz.cpp`, `tests/fuzz/api_config_fuzz.cpp`, `tests/integration/tenant_isolation_test.cpp`
- Modify: `src/frontend/request_validation.cpp`, `src/memory/prefix_index.cpp`, `tests/CMakeLists.txt`
- Modify: `CMakePresets.json`

**Interfaces:**

- Consumes: P1 parsers/auth/limits, P2 checked plan slabs, P3 tenant identity and Task 1 security policy.
- Produces: `Result<DiagnosticBundle> collect_diagnostics(const DiagnosticRequest&, const RedactionPolicy&)`; fuzz entry points for untrusted byte spans; tenant/auth quotas and sensitive-buffer reuse rules are explicit. Add supported host-asan/host-ubsan/host-tsan/host-fuzz configure/build/test presets here, with unsupported compiler/platform combinations reported explicitly.

- [ ] **Step 1: Write `tenant_limits_and_diagnostics_exclude_sensitive_content` assertions in `tests/integration/tenant_isolation_test.cpp` and register CTest `p6-security`.** Use malicious sizes/offsets, unauthenticated costly inputs, cross-tenant cache candidates and diagnostic strings containing secret fixture markers.

```cpp
CHECK(unauthenticated_request.tokenization_jobs == 0);
CHECK(other_tenant_prefix.reused == false);
CHECK(diagnostic_bundle.contains(secret_marker) == false);
CHECK(oversized_parser_input.allocations <= configured_parse_budget);
CHECK(cross_tenant_sensitive_buffer_cleared_per_policy);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-security$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Audit least privilege/listeners/secrets and enforce request/model/config/protocol bounds before expensive work. Add host sanitizer/fuzz corpora and production diagnostic authorization/redaction; cross-tenant sharing stays opt-in only after explicit side-channel qualification.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run reproducible ASan/UBSan/TSan configurations where applicable, parser/protocol fuzz budgets and tenant tests. Produce SBOM/license/vulnerability reports with policy/date; release-blocking findings remain blockers. Configure/build/test each supported sanitizer preset and run the host-fuzz budget; use separate builds for incompatible sanitizers.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(security): qualify parsers tenants and redacted diagnostics"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 6: Finalize bounded metrics and truthful SLO accounting

**Files:**

- Create: `src/observability/release_metrics.h`, `src/observability/release_metrics.cpp`, `tests/unit/release_metrics_test.cpp`, `docs/runbooks/diagnostics.md`
- Modify: `src/observability/request_metrics.cpp`, `src/observability/step_trace.cpp`, `tools/bench/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: P1/P2 monotonic timings, bounded telemetry and Task 1 SLO population definitions.
- Produces: `ReleaseMetrics summarize(const RunReport&, const ReleaseProfile&)`; `Status MetricsSink::try_export(const MetricBatch&)`; summary keeps token ITL distribution, per-request max gap, TTFT/TPOT, offered/admitted/completed/rejected and failed admitted misses.

- [ ] **Step 1: Write `release_metrics_count_misses_and_drop_telemetry_without_blocking` assertions in `tests/unit/release_metrics_test.cpp` and register CTest `p6-metrics`.** Use fixed timings with one timeout, one rejection, one long inter-token gap and a full export ring; node clocks have unknown skew.

```cpp
CHECK(summary.failed_admitted_are_slo_misses);
CHECK(summary.offered == summary.admitted + summary.rejected);
CHECK(summary.long_gap_request_slo_pass == false);
CHECK(full_export_ring.blocks_engine == false);
CHECK(full_export_ring.dropped_batches > 0);
CHECK(unknown_clock_skew.cross_node_duration_claims == 0);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-metrics$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Separate all timing/memory categories, count telemetry drops, bound label cardinality and exclude prompt content. Report full RSS/PSS/device/vendor alongside LUMEN categories; do not equate TPOT with worst token gaps.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Check exact synthetic percentile/population arithmetic and overload/rejection curves, then measure telemetry overhead under actual load.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(observability): report bounded truthful release metrics"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 7: Automate reproducible builds and deployment acceptance

**Files:**

- Create: `.github/workflows/host.yml`, `.github/workflows/device.yml`, `deploy/systemd/lumend.service`, `deploy/container/Containerfile`, `tests/integration/deployment_acceptance_test.cpp`, `docs/runbooks/install-upgrade-rollback.md`
- Modify: `CMakePresets.json`, `cmake/toolchains/arm64.cmake`, `cmake/toolchains/x86_64.cmake`, `tests/CMakeLists.txt` after P0 creates toolchain files

**Interfaces:**

- Consumes: P0 build/dependency lock, Task 1 matrix and Tasks 2–5 lifecycle/install/security.
- Produces: `Status verify_deployment(const DeploymentRun&, const ReleaseProfile&)`; clean-build provenance, artifact/SBOM digests and install/upgrade/drain/rollback/reinstall logs per supported image.

- [ ] **Step 1: Write `deployment_rejects_broken_provenance_and_failed_readiness` assertions in `tests/integration/deployment_acceptance_test.cpp` and register CTest `p6-deployment`.** Use test installation directories and synthetic command/provenance results; fail readiness during upgrade and interrupt selection.

```cpp
CHECK(unpinned_dependency_run.accepted == false);
CHECK(failed_warmup_upgrade.routes_new_work == false);
CHECK(interrupted_upgrade.old_generation_usable);
CHECK(clean_host_build.requires_vendor_sdk == false);
CHECK(reinstall.report_contains_all_artifact_digests);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-deployment$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Run CPU/mock CI without SDK; separate device-labeled jobs and sanitizer/fuzz jobs. Pin reproducible inputs and document vendor bit-reproducibility limits. Deploy with least privileges/resource limits and test finite drain/rollback; test harness scripts may be development-only Python, not mandatory runtime.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Execute ARM64/x86-64 clean builds where supported and real board-image install/upgrade/drain/rollback/reinstall; validate C/C++ ABI/version tests and disabled-feature configs.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "ci: qualify reproducible builds and deployment lifecycle"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 8: Run thermal, load, recovery and 72-hour qualification

**Files:**

- Create: `bench/workloads/p6-acceptance.json`, `tests/perf/p6_measurement_test.cpp`, `docs/qualification/p6/measurement-report.md`
- Modify: `tools/bench/main.cpp`, `sim/replay.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Frozen Task 1 profile, Task 6 metrics and Task 7 deployment inputs.
- Produces: `Status validate_measurement_run(const MeasurementRun&, const ReleaseProfile&)`; evidence covers quality, safety, resource/latency saturation, thermal interference, recovery and actual test duration.

- [ ] **Step 1: Write `qualification_run_cannot_hide_population_or_duration_changes` assertions in `tests/perf/p6_measurement_test.cpp` and register CTest `p6-measurement`.** Provide synthetic too-short soak, changed profile, omitted failures, simulator-only timings and valid held-out prediction errors.

```cpp
CHECK(short_soak.duration_hours < 72);
CHECK(short_soak.accepted == false);
CHECK(changed_population.accepted == false);
CHECK(simulator_only.board_qualified == false);
CHECK(omitted_failed_requests.accepted == false);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-measurement$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Automate open-loop prompt/output/context/concurrency/cache/thermal/network/fault sweeps from the frozen profile. Preserve raw evidence and node-local durations; report held-out simulator errors without extrapolating device qualification.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run 72-hour target-board soak with cancels/thermal cycles and no unexpected crash/hang or unbounded LUMEN growth. Measure proposed host-overhead ≤5%, interactive p99 TTFT ≤500 ms/ITL ≤80 ms, admitted SLO success ≥99%, engine metadata/scratch ≤64 MiB with exclusions/full RSS, and replica routing recovery ≤10 s only if those candidate thresholds were adopted. Publish failures as failures.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "test(perf): record industrial workload and recovery qualification"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

### Task 9: Evaluate release gates and assemble supportable artifacts

**Files:**

- Create: `src/qualification/release_gate.h`, `src/qualification/release_gate.cpp`, `tests/unit/release_gate_test.cpp`, `docs/qualification/p6/release-report.md`, `docs/runbooks/incidents.md`, `docs/api/supported-matrix.md`
- Modify: `CMakeLists.txt`, `tools/inspect/main.cpp`, `tests/CMakeLists.txt`

**Interfaces:**

- Consumes: Task 1 ReleaseProfile, Tasks 2–8 evidence and prior phase reports for shipping combinations.
- Produces: `Result<ReleaseDecision> evaluate_release(const EvidenceBundle&, const ReleaseProfile&)`; decision contains per-gate status/evidence, unsupported combinations and blockers; serialized bundle contains release/support manifests, correctness/safety/performance/soak/security/deployment evidence.

- [ ] **Step 1: Write `release_gate_never_treats_missing_or_skipped_evidence_as_pass` assertions in `tests/unit/release_gate_test.cpp` and register CTest `p6-release`.** Omit one required artifact, include skipped device jobs, fail safety, omit unselected P5 and supply an expired security scan.

```cpp
CHECK(missing_required_evidence.release_allowed == false);
CHECK(skipped_device_gate.status != GateStatus::pass);
CHECK(critical_safety_failure.release_allowed == false);
CHECK(unselected_p5_missing.blocks_release == false);
CHECK(expired_security_evidence.release_allowed == false);
```

- [ ] **Step 2: Verify the test is red.** Run `cmake --build --preset host-debug`, then `ctest --preset host-debug -R '^p6-release$' --output-on-failure --no-tests=error`. Expect a missing implementation/link failure or the named behavioral assertion to fail; a toolchain/configuration error does not establish a valid red test.
- [ ] **Step 3: Implement the interfaces above.** Implement machine-readable gate outcomes with links/digests and explicit release-owner decision. Package signed binaries/model manifests and complete API/limits/install/diagnostic/incident docs. A scope revision requires explicit approval and requalification; do not relabel a failed gate.
- [ ] **Step 4: Verify the deliverable.** Rebuild with `cmake --build --preset host-debug`; rerun the exact CTest command from Step 2 and require PASS. Run gate-logic tests then assemble the real complete bundle. Require accepted ADRs for every frozen/enabled decision and verify supported artifacts against the matrix. Report limitations, unresolved blockers and vendor reproducibility constraints.
- [ ] **Step 5: Commit the passing change.** Stage only this task's listed files and CMake test registration with `git add`; run `git commit -m "feat(qualification): evaluate evidence backed industrial release gates"`. Record any qualified-board evidence separately; a host PASS cannot satisfy a board gate.

## Phase acceptance and handoff

A release decision must identify every enabled model/SDK/layout/topology/API combination and its accepted evidence. Require no critical correctness/ownership/sanitizer issue, bounded resource behavior, a completed 72-hour board soak, agreed SLO/security/recovery criteria and reproducible deployment procedures. Report actual candidate-threshold outcomes; no runtime or board work is performed merely by writing this plan.

## Spec coverage

| Spec sections | Tasks |
|---|---|
| §1–2 scope/matrix/config | 1, 3 |
| §3 reliability/recovery | 2–3, 7–8 |
| §4 security/supply chain | 4–5, 7 |
| §5 observability/support | 6, 9 |
| §6 verification/build/simulation | 5, 7–8 |
| §7 acceptance matrix | 1, 8–9 |
| §8–9 evidence/release | 9 |
