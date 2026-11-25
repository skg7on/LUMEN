# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository status

This checkout is **design-only at present**: there is no `src/`, `CMakeLists.txt`, build system, test suite, CI configuration, or config parser. It contains the architecture design and roadmap in `docs/init-draft/`, proposed phase specifications in `docs/architecture/`, proposed decisions in `docs/adr/`, project guidance, `README.md`, and the MIT `LICENSE`.

Treat the design documents as proposals, not descriptions of implemented behavior. Verify the tree before referring to a module, command, test, backend, or capability as available.

## Commands

No build, lint, or test commands exist yet. Do not assume `cmake --preset …`, `ctest`, or any `lumen-*` binary works in this checkout. Creating the build/test skeleton is itself the first deliverable (P0.3, below).

When the native build is introduced, the roadmap prescribes:

- CMake + Ninja with toolchain files under `cmake/toolchains/`; CPU and mock backends must build and run without a vendor NPU SDK.
- Artifacts: `liblumen` (optional), `lumend`, `lumen-pack`, `lumen-bench`, `lumen-replay`, `lumen-inspect`.
- Development-only scripts may be Python; no Python in the shipped runtime or mandatory packer.
- Use CMake presets and a dependency lock/manifest.

## Architecture

The design separates **serving semantics** (requests, scheduling, logical KV ownership, SLOs) from **device execution** (physical layout, queues, kernels, collectives). The data path is:

```text
Frontend (HTTP/SSE, auth, quotas, tokenizer)
  -> Engine Core (single writer: sequence state, iteration loop, commit)
     <-> Scheduler (pure policy over a snapshot: admission, batching, deadlines)
     <-> KV Manager (logical block tables, reservations, prefix cache)
  -> Executor (rank group, dispatch, completion fence)
  -> Workers (model runner, device arena)
  -> Backend (NPU / CPU / mock: graphs, kernels, DMA, events)
```

Mesh routing and observability are cross-cutting; the offline model packer is outside the serving path.

Contracts that require reading multiple documents to grasp:

- **Single-writer engine, asynchronous workers.** Only the engine commits request state, token accounting, and logical KV mappings. Workers receive immutable `StepPlan` objects and return epoch-tagged `StepResult`s. Engine accepts a result only for matching sequence/step/generation/model-epoch; submission success is not completion.
- **One uncommitted step per sequence** in the baseline. Commit a rank-group iteration only after every required fence completes.
- **Token accounting.** Computed KV and sampled output are distinct; the last sampled token is normally the next decode input and has no KV yet. Do not emit output until the final prompt chunk yields valid logits; a full prefix-cache hit still needs last-token recomputation or validated cached logits.
- **KV paging.** Logical pages map to physical slots; page size starts at 16 tokens but belongs to the layout descriptor. Page lifecycle `FREE -> RESERVED -> WRITING -> SEALED` (or `PRIVATE_TAIL`) `-> RETIRED -> CACHED_EVICTABLE -> FREE`. Publish only full, committed, immutable pages; a page with live references or leases is never reused.
- **Backend ABI.** C++20 host code; versioned C-compatible ABI with opaque handles. No STL types or C++ exceptions across the ABI; status codes, explicit ownership, and destruction rules. Device addresses, vendor contexts, and physical layouts stay inside workers/backends.
- **Scheduling operates on capabilities, not assumptions.** Do not assume CUDA/NCCL semantics, dynamic shapes, custom paged attention, or DMA/compute overlap; negotiate per device/SDK and keep unsupported combinations disabled.
- **Distribution.** Prefer replica routing; TP/PP/remote KV only with explicit hardware support, correctness evidence, and measured benefit.

## Documentation map

- `docs/init-draft/LUMEN-Architecture-Design.md` — module contracts (§3), runtime loop and scheduling (§4), KV/memory invariants (§5), backend ABI and kernels (§6), model packing (§7), mesh (§8), verification strategy and open decisions (§12).
- `docs/init-draft/LUMEN-Implementation-Roadmap.md` — phases P0–P6 with exit gates, benchmark methodology, risk register, and the first-four-weeks backlog (LUMEN-001…010). P0 order: build skeleton + C ABI, NPU qualification harness, fixtures + native tensor loader, CPU reference Transformer, `StepPlan` contract, mock async executor.
- `docs/architecture/README.md` — phase-specification index for P1–P6, starting with the MVP; `Shared-Runtime-Contracts.md` defines common ownership, execution, resource, and ABI contracts. P0 remains an entry dependency and P5 remains optional for R1.0.
- `docs/adr/README.md` — proposed decisions about state ownership, backend ABI, KV layout/page size, scheduler policy, model format/precision, and distributed authority. Accept or supersede these with qualification evidence before freezing interfaces or enabling features. Update the design docs when implementation intentionally departs from them.

## Operative rules

`.claude/rules/` contains the enforced project rules and is auto-loaded: `project-scope.md`, `code-discovery.md`, `verification.md`, `runtime-and-kv.md`, `native-backends.md`, `worktree-isolation.md`, `pr-naming.md`. The ones that shape every change:

- All writes happen in a git worktree under `.claude/worktrees/` (gitignored); main checkout is read-only. Branches follow `category/short-description` (`feat/`, `fix/`, `docs/`, `refactor/`, `exp/`). Note that `EnterWorktree` creates a `worktree-<category>+<name>` branch and directory; rename the branch to the policy form immediately.
- Conventional commit/PR subjects: `<type>(<optional-scope>): <imperative subject>` with scopes such as `engine`, `scheduler`, `memory`, `executor`, `backend`, `kernels`, `model`, `tokenizer`, `frontend`, `collective`, `mesh`, `modelpack`.
- Numerical examples, dates, and performance targets in the docs are provisional until measured on a qualified workload; report commands and results actually run.
- Do not build ahead of the roadmap: no distributed KV before the single-NPU demo milestone, and no Python in shipped components.
