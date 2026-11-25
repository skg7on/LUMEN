# LUMEN phase implementation plans

Status: Proposed implementation plans | Date: 2026-11-25

These plans implement the [P1–P6 design specifications](../../architecture/README.md). They describe future changes; this checkout still has no native build system, tests, or executable tools. Creating the plans does not execute them or qualify their hardware assumptions.

## Plan map

| Plan | Dependency | Deliverable |
|---|---|---|
| [P1 native single-NPU MVP](2026-11-25-p1-native-single-npu-mvp.md) | P0 readiness contract below | R0.1 native loading, generation and bounded streaming |
| [P2 continuous batching and paged KV](2026-11-25-p2-continuous-batching-and-paged-kv.md) | Accepted P1 | R0.2 safe pages, policy, replay and mixed-load evidence |
| [P3 NPU optimization and prefix reuse](2026-11-25-p3-npu-optimization-and-prefix-reuse.md) | Accepted P2 | R0.3 qualified kernels, APC and adaptive policy |
| [P4 replica mesh and local TP](2026-11-25-p4-replica-mesh-and-local-tp.md) | Accepted P3; separate TP topology gate | R0.5 routing, lifecycle and conditional group execution |
| [P5 advanced distribution](2026-11-25-p5-advanced-distribution.md) | Accepted P4 and per-feature entry evidence | Optional R0.7 PP, remote ownership and phase separation |
| [P6 industrial qualification](2026-11-25-p6-industrial-qualification.md) | Accepted P3; P4/P5 only if shipping | R1.0 supported matrix and complete release evidence |

Execute tasks in order within each plan. P4's TP branch and every P5 extension remain disabled unless their entry evidence justifies them. P6 can qualify a P3/P4-based release without P5. Security, CI, observability and failure tests start with P0/P1, not at P6.

## P0 readiness contract

Before P1, finish roadmap P0/LUMEN-001…006. P1 Task 1 verifies and records those outputs; it does not replace P0. Missing prerequisites block P1 execution and require a separate P0 implementation effort.

The following canonical paths/names are proposed integration contracts for that prerequisite, not discovered code. If P0 uses different paths or names, reconcile the affected plans in one reviewed edit before executing tasks; do not create a competing implementation of the same primitive.

| Required P0 output | Canonical integration surface |
|---|---|
| Native build/test skeleton | `CMakeLists.txt`, `CMakePresets.json`, `tests/CMakeLists.txt`, `cmake/toolchains/arm64.cmake`, `cmake/toolchains/x86_64.cmake`; `host-debug` configure/build/test presets and `target-npu` configure/build presets |
| Core types | `src/core/status.h`: `enum class Status { ok, invalid_input, unsupported, resource_exhausted, stale, timeout, device_error, corrupt }`; `Result<T>::ok()`, `status()`, `value()` |
| Checked tensor/size primitives | `src/core/checked_math.h`, `src/model/tensor_view.h`; checked offsets, shapes, alignment, dtype and layout |
| Versioned backend | `include/lumen/backend.h`, `src/backend/cpu/`, `src/backend/mock/`; opaque buffer/queue/event handles and explicit retirement |
| Independent model fixture | `fixtures/reference/manifest.json`, native tensor loader and CPU Transformer; digests/license and predeclared per-dtype tolerances |
| Engine protocol | `src/engine/step_plan.h`: immutable `StepPlan`, `StepResult`, `GenerationKey`; `src/engine/sequence.h` with committed KV/token/RNG/output cursors |
| Async mock | `src/backend/mock/mock_control.h`: controllable delayed completion, partial launch, stale/duplicate results and reset outcome |
| Native test harness | `tests/support/check.h`: always-active `CHECK`/`REQUIRE` assertions; named CTest registration, deterministic seeds and nonzero failure exit |
| Device evidence | `docs/qualification/p0/qualification.json`: board/OS/driver/SDK/toolchain, actual kernel result, queue/event/reset semantics, shapes/precisions and usable memory |

Use `TokenId=int32_t`, IDs/epochs/generations/output event offsets as `uint64_t`, and monotonic nanoseconds as `uint64_t` throughout the plan examples. `GenerationKey` carries sequence ID/generation, step ID, model epoch and group epoch. `Result<T>::value()` is accessed only after success. These are internal C++ conventions; the public C ABI uses fixed-width records and status codes with no STL or exceptions. Task-specific record types listed in Interfaces are defined in that task's listed header, including the fields used by its assertions.

Later module headers define `NodeId`, `GroupId`, `SessionId` and `TransferId` consistently as `uint64_t`; request handles stay opaque and retain explicit ownership. Output offsets index delivery events, not sampled tokens or bytes. Device and postprocess results always retain the same GenerationKey through owner-thread commit.

## Cross-plan interface ownership

| Producer | Contract | Consumers |
|---|---|---|
| P1 Task 2 | `ModelPack`, `load_pack`, `write_pack` in `src/model/` | All runner, precision, shard and trust tasks |
| P1 Tasks 3–4 | `DecoderSnapshot`/`DecoderProposal`, `postprocess`, `sample` | Engine commit, sampler extensions and remote checkpoints |
| P1 Task 5 | `AdmissionTicket`, `Capacity`, logical KV leases | P2 transactions and P3 atomic cache admission pins |
| P1 Task 7 | Engine submit/cancel/poll, OutputEvent and immutable commit | Native/HTTP, replicas and checkpoint integration |
| P1 Task 10 | `RunReport`, `PhaseEvidence`, `to_phase_evidence` in `src/observability/phase_evidence.h` | P2–P5 acceptance headers and P6 bundle assembly |
| P2 Task 2 | `StepSelection` in `src/scheduler/step_selection.h`, `KvTxn`/`CommitProof` in `src/memory/kv_transaction.h` | Policy, APC, group reservations and private speculative KV |
| P2 Tasks 5–7 | `ScheduleSnapshot`, `choose_step`, `estimate_step`, trace/replay contracts | P3 adaptive policy and later workload evidence |
| P4 Tasks 2, 5–6 | Submission/delivery bindings and rank-group fences | P5 handoff/PP and P6 recovery |
| P5 Tasks 2–5 | FrozenCheckpoint, prepared/fence/drain proofs and durable OwnerGrant | Phase separation and selected P6 distribution qualification |
| P6 Tasks 1, 9 | ReleaseProfile and evidence-backed ReleaseDecision | Shipping support matrix and release report |

Phase-specific acceptance functions and profiles live in `src/observability/p1_acceptance.h` through `p5_acceptance.h`; they consume the same evidence record rather than inventing incompatible report schemas. P6 release evaluation lives in `src/qualification/release_gate.h`. Common record/header ownership must be preserved when reconciling actual P0 names.

Each plan's File structure maps responsibilities before tasks lock in files. Paths marked Create are future files, not assertions that a module exists. A task's `tests/…` file owns its local fake/fixture setup; assertion snippets show the required semantic checks rather than a complete test translation unit. Tests must not compile away under `NDEBUG`.

## Verification commands and evidence

P0 must provide this host command contract:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --output-on-failure --no-tests=error
```

Tasks register unique CTest names such as `p1-pack`; anchored `-R` commands must select at least one test. First configure after adding targets; rebuild before every red/green run so an old binary cannot hide missing code. Expected red means a new behavioral assertion or missing implementation fails, not a broken dependency download. Hardware-only tests must be explicitly labeled `device`, excluded from host-only presets, and fail/record unavailable rather than count a skipped board check as qualification.

Each test step adds the named failure cases to the task that owns the behavior. Use independent expected tensors for numeric checks, controllable mock time/fences for lifecycle checks, and real target measurements for performance. Red/green steps apply to behavior and safety, not prose or static configuration that merely mirrors itself. Deployment and qualification tasks use synthetic evidence/invalid inputs to test their gate logic, then run the real acceptance procedure separately.

Phase evidence goes under `docs/qualification/p1/` through `p6/`; commit small metadata/reports and stable digests/links to raw traces, not large model payloads, secrets or prompts. Every report identifies commit, model/precision/layout, target/SDK, workload/seed/warmup/duration, offered/admitted/completed/rejected demand, quality, latency, memory and failures. Numerical targets remain provisional until the phase's acceptance profile is frozen with appropriate product/model-owner agreement.

Plans propose exact interfaces and CLI flags for implementation. They do not claim those commands currently run. Library/vendor selections remain bounded by P0 evidence, footprint/license review and the specs; adapter-specific work cannot be completed by substituting mock results.

## Execution boundary

This task creates plans only. Before implementing, review the chosen plan and select an execution method. Native execution keeps the related interfaces in one working context; subagent-driven execution adds separate implementer/reviewer contexts for each task. Neither method waives prerequisite or hardware gates. The [ADR register](../../adr/README.md) remains Proposed until its required evidence is accepted.

The [roadmap issue batch](../issues/2026-11-25-roadmap-batch.md) maps these tasks and P0 prerequisites to GitHub work items, dependencies, phase design/acceptance trackers, and release milestones. Use GitHub for live status.
