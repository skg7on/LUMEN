# LUMEN shared runtime contracts

Document ID: LUMEN-CONTRACTS-001 | Version: 0.1 | Status: Proposed design

Date: 2026-11-25 | Parent: [phase specification index](README.md)

This document defines the invariants inherited by P1–P6. It refines architecture §§3–7 and roadmap §§1.2, 3, 10–12. Phase features may extend these contracts through versioned descriptors; they must not weaken their safety properties.

## 1. Authority and module boundaries

| Component | Owns | Cannot do |
|---|---|---|
| Frontend/tokenizer pool | Connections, parsing, tokenization and immutable detokenization/stop jobs, bounded output delivery | Commit sequence/decoder state or access device buffers |
| Engine owner thread | Sequence lifecycle, logical KV mappings, admission commits, RNG/output cursors | Treat submission as completion |
| Scheduling policy | A proposed selection over an immutable capacity/cost snapshot | Allocate pages or modify live sequences |
| KV manager | Engine-owned logical reservations, references, lease bookkeeping | Reuse storage before all fences retire |
| Executor | Plan validation, dispatch, rank-group completion aggregation | Choose policy or commit request state |
| Worker/backend | Device contexts, physical buffers/layouts, commands, events | Publish logical pages or independently emit tokens |
| Router/control plane | Replica selection; optional authoritative session ownership grants | Schedule steps across independent replicas |

One owner serializes engine events. Bounded ingress, completion, cancellation, and output queues connect other threads. A full ingress queue rejects work; a full downstream queue propagates backpressure. A completion slot is reserved before dispatch so worker progress cannot depend on allocating or dropping a result. Cancellation capacity must remain available under ingress overload, for example through per-request flags plus a bounded wakeup queue.

## 2. Sequence and token cursors

A sequence has `SeqId`, `model_epoch`, request state, terminal reason, prompt length, token slab, committed KV cursor, generated-token count, committed RNG counter, output offset, and at most one uncommitted step. IDs and generations must not wrap into valid reuse; exhaustion causes controlled drain/restart or rejection.

| State | Scheduling rule | Exit |
|---|---|---|
| QUEUED | Admission not committed | PREFILL or DRAINING |
| PREFILL | Consume next prompt chunk | PREFILL, DECODE, WAIT_KV, DRAINING |
| DECODE | Consume the pending sampled token | DECODE, WAIT_KV, DRAINING |
| WAIT_KV | No compute until restore/reservation succeeds | Saved PREFILL/DECODE cursor or DRAINING |
| DRAINING | No new steps; retire in-flight resources | FINISHED after leases and delivery bookkeeping retire |
| FINISHED | Release request slot when external handles retire | Terminal |

For a nonempty prompt of N tokens, after final prefill commits: `kv_committed_tokens=N`, `generated_tokens=1`, and the first sampled token has no KV. After k decode steps commit: `kv_committed_tokens=N+k`, `generated_tokens=1+k`. This example assumes no early EOS, output limit, or stop rule. Intermediate prefill chunks produce no output. Enforce `prompt_tokens + max_new_tokens <= max_model_len` with checked arithmetic; define zero output and empty-prompt behavior at the frontend before allocating execution resources.

A complete prefix hit requires final-token recomputation or separately validated cached logits. If recomputation touches an immutable cached page, use a private writable destination or a qualified read-only logits path; never overwrite a shared sealed page. Output emission, stop handling, and KV cursors are separate: withheld UTF-8/stop suffix bytes are not newly sampled tokens.

`output_offset` is the monotonically increasing index of a committed delivery event, distinct from sampled-token count and visible byte count. Engine enqueue/commit and router forwarding watermarks are tracked separately. One decoded text event may cover several sampled tokens because UTF-8/stop suffixes were withheld. Remote handoff must reconcile queued delivery events explicitly; a token cursor alone cannot prove output delivery.

## 3. Immutable execution protocol

The following records are semantic schemas, not complete C++ headers or a wire ABI.

| Record | Required information |
|---|---|
| StepPlan header | Schema version, step ID, model/group epochs, rank set, shape variant, bounded slab references |
| StepItem | Sequence ID and generation, expected prior cursor, phase, input range, positions, context length, logical block snapshot, valid lengths, sampling/RNG counters, emit permission |
| Resource lease | Pool identity, slot generation, read/write mode, reserved workspace/metadata/output capacity |
| Rank result | Step/model/group identity, rank, item identities, completed token counts, sampled IDs/logprobs where permitted, classified status, fence identity |
| Aggregated StepResult | Required rank results, final fence status, per-item outputs, error classification |
| PostprocessResult | Sequence/generation/step identity, expected prior decoder/output cursor, bounded candidate bytes, next detokenizer/stop-state proposal, stop reason |

Plans and their referenced metadata stay immutable and leased until retirement. IPC uses versioned offsets/counts into validated shared-memory slabs; no STL objects, process-local pointers, or external raw device addresses cross the boundary. A worker resolves logical descriptors to rank-local physical storage and validates all ranges before launch.

Dispatch has three outcomes:

1. **Rejected before submission:** no command references plan resources; roll back reservations and output credits.
2. **Accepted:** executor owns a completion obligation and the engine marks the step in flight. Every accepted step eventually reports success, fenced failure, or an unresolved-device failure requiring quarantine.
3. **Partial or uncertain submission:** some commands may have launched. Retain leases, stop affected dispatch, and fence or reset the context before reclaiming resources.

An accepted step reserves completion and result storage. Queue acceptance is not a device fence. The engine validates IDs, epochs, generations, prior cursors, result bounds, and all required rank/collective fences before committing. Invalid/stale results cannot update sequence state; their actual executor resources still require safe retirement. Duplicate completions are idempotently ignored after retirement bookkeeping.

A rank-group iteration commits all selected items atomically at the engine boundary in the baseline. Any rank failure fails the iteration; never publish a subset of rank-local KV updates. Recompute only from a verified committed state. Terminal sequences retire successful late work without emitting new output. A worker accepts one outer plan at a time; a qualified PP plan may contain multiple distinct-sequence microbatches, but their stage work remains inside that plan's aggregate fence and commit.

## 4. Commit and cancellation

After device completion, sampled tokens enter bounded detokenization/stop jobs in the fixed CPU pool. Each job consumes an immutable snapshot of engine-owned prior decoder/stop state and returns candidate bytes plus a next-state proposal. The pool never mutates live sequence state or emits output. Reserve job/completion/state/output storage before dispatch. Keep the sequence's `inflight_step` set until every required postprocess result matches its sequence/generation/step and prior-state cursor; do not schedule another step before stop evaluation. Prefill chunks without sampling need no such job.

For a successful live step, the engine applies one prepared commit after all required device and postprocess completions: validate cursors; commit written KV positions; apply the sampled token and RNG advance if emission is allowed; install decoder/stop state and terminal reason; release step leases; publish eligible sealed pages; enqueue bounded output events. Retain leases and proposal storage through this transaction; no hot-path allocation is required. All selected items in a group follow the same aggregate commit rule. A postprocess failure fails the affected iteration without publishing its new state; subsequent recovery follows the same fenced failure policy.

Cancellation sets a terminal reason and prevents future scheduling immediately. Already submitted work may finish; its resources remain leased. The engine suppresses newly arriving sampled output for a cancelled request, drains pending delivery according to the documented protocol, and transitions through DRAINING. A frontend disconnect cancels its request rather than allowing an unreachable output queue to grow.

After a device timeout, failure is not proof that DMA stopped. Fence the group epoch, quarantine potentially touched buffers, then use a qualified device reset or process/device teardown. If safe retirement cannot be proved, keep the affected pool unavailable and fail health/readiness. Reusing uncertain storage to restore capacity is forbidden.

## 5. Resource and KV accounting

Admission is a transaction over request slots, token storage, output credits, logical KV commitments, metadata, scratch, and executor capacity. Strict admission guarantees worst-case length capacity including required COW/recovery allowance. Elastic admission, introduced later, has an explicitly bounded reservation horizon and preemption policy.

Count physical allocation, future commitments, active references, in-flight leases, cached evictable pages, staging bytes, and runtime/vendor reservations separately. A cache pin or transfer target must be acquired atomically with the corresponding admission/reservation transaction. Report total RSS/PSS as well as LUMEN-owned categories.

Logical pages carry a pool/layout ID, slot, generation, valid length, reference count, and read/write lease counts. Page tokens belong to the layout descriptor; P=16 is a starting experiment. The lifecycle is FREE → RESERVED → WRITING → SEALED or PRIVATE_TAIL → RETIRED → CACHED_EVICTABLE/FREE. Only full, committed, immutable pages can enter the prefix index. Index membership is not a live reference. Remove index entries and bump generation before reuse; no page with a live reference or lease may be reused.

## 6. Backend ABI and build boundary

Use C++20 internally and a versioned C-compatible ABI with opaque context/device/buffer/queue/event handles. ABI records include version and size, fixed-width fields, explicit buffer lengths, and defined ownership. No C++ exceptions or STL types cross this boundary. ABI evolution rejects incompatible major versions and negotiates supported optional fields/capabilities; exact packing is frozen only after ABI review.

| Resource | Creation and ownership | Retirement rule |
|---|---|---|
| Context/device | Backend opens; owning worker manages | Destroy after queues/events/buffers retire, or qualified teardown |
| Buffer | Explicit memory kind, size, alignment | Free only after all referencing commands fence |
| Queue | Worker owns; ordering semantics declared | Drain/fence before destruction |
| Event | Submission returns owned completion handle | Poll/wait to final state, then release exactly once |
| Graph/kernel binding | Registered against compatible device/model/layout | Unregister after all launches retire |

Define status categories for invalid input, unsupported capability, resource pressure, retryable transport, device failure, timeout, and internal corruption. A timeout does not imply cancellation. Explicitly define whether any failed call can have side effects.

Capability reports cover graph buckets, shapes, precision, memory types/alignment, custom attention, mixed phases, sampling, queue ordering, overlap, reset, and collectives. Unsupported paths are disabled or use a qualified bounded fallback. CPU and mock adapters build without vendor headers, libraries, or devices.

Use CMake/Ninja, presets, cross-toolchain files, and a dependency lock/manifest. Proposed targets are `liblumen`, `lumend`, `lumen-pack`, `lumen-bench`, `lumen-replay`, and `lumen-inspect`; each phase only delivers its stated subset. Python may generate development fixtures but is absent from the shipped runtime and mandatory packer.

## 7. Compatibility and evidence

Every loaded model generation pins model/tensor/tokenizer/template digests, attention/RoPE traits, precision, KV layout, graph variants, and partition descriptors. Requests cannot mix generations. A feature registry enables only qualified combinations of these properties with backend/SDK/topology versions. Configuration is validated before readiness; live updates cannot mutate a request's model or cache semantics.

Use an independent reference for numerical correctness, mock delayed/faulting workers for lifetime correctness, and real device timing for performance. Replay fixes event order, snapshots, seeds, and tie-breaking; numerical reproducibility is qualified separately per backend. Log metadata and reason codes without prompt/token contents by default. Telemetry queues are bounded and count dropped events.

Open decisions are recorded in the [ADR register](../adr/README.md). No spec approves a vendor, model, precision, SLO, or deployment envelope without the associated evidence.
