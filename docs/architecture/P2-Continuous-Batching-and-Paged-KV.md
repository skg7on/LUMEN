# P2 Continuous batching and paged KV

Document ID: LUMEN-P2-DESIGN | Version: 0.1 | Status: Proposed design

Date: 2026-11-25 | Release: R0.2 | Source: roadmap §5

## 1. Outcome and entry gate

Replace P1's fixed contiguous storage and simple batching with transactional logical pages, iteration-level admission/removal, decode-first scheduling, and bounded chunked prefill. Demonstrate higher SLO goodput on a matched mixed workload while preserving [shared runtime contracts](Shared-Runtime-Contracts.md).

Entry requires the accepted [P1 baseline](P1-Native-Single-NPU-MVP.md), CPU/mock correctness path, real target timing tables, and a decision on native paged attention versus a bounded gather fallback. The initial model remains full-attention dense GQA/MHA. APC policy, quantized KV, speculative steps, and distributed KV remain later work.

## 2. Data model and ownership

| Record | Proposed fields and invariant |
|---|---|
| PoolDescriptor | Model/layout identity, page tokens, rank-local bytes/strides, capacity; immutable while requests reference it |
| BlockHandle | Pool, slot, generation; checked at every dereference/commit |
| PageRecord | Lifecycle, committed valid length, sequence references, reader/writer leases, epoch; no free page has a live ref/lease |
| SequenceBlockTable | Ordered logical pages and private tail; engine-owned, snapshotted into plan slabs |
| ReservationTxn | Proposed mappings, new slots, future commitments, output/scratch credits; all-or-nothing before publication |
| StepSnapshot | Input positions, context/valid lengths, block handles, checked slot map, graph bucket, lease set |

Workers allocate physical arenas at startup. The engine manipulates logical capacity and generation handles; workers calculate checked addresses from their layout descriptors. Use P=16 for initial experiments, with page size encoded in the descriptor. Fixed slot arrays, preallocated block-table storage, and intrusive free/retired lists avoid hot-path allocation.

Size KV from actual aligned rank-local layout, including metadata and replication. For conventional full attention, the unpadded starting formula is `2 * layers * kv_heads * head_dim * dtype_bytes` per token. Do not apply it unchanged to future sliding-window/MLA layouts. Strict capacity includes allocated storage and future commitments without double-counting the same reserved bytes.

## 3. Reservation and commit transaction

1. Policy selects candidates from a read-only snapshot. The KV manager validates lengths and current generations, computes page/tail/scratch/output demand, and tentatively reserves every required resource.
2. Prepare an immutable plan and lease set. If anything fails before submission, roll back the whole transaction and shrink/defer the selection.
3. Executor acceptance transfers the completion obligation. Mark the step in flight before processing its completion event; even an immediate CPU/mock completion passes through the owner-thread event queue.
4. While WRITING, new pages and appended positions are not visible as committed context. Existing committed tail positions are read-only; appends write only the private unused range. Never append into a shared mutable page.
5. After all required device fences, bounded detokenizer/stop proposals, and identity checks, commit cursors, decoder/output state and logical mappings together, then release leases. A full completed page becomes SEALED; a partial exclusive page remains PRIVATE_TAIL.
6. On cancellation or failed/uncertain execution, publish no new positions. Quarantine touched pages until fence/reset. Keep previously committed data only when the backend contract proves it intact; otherwise fail the request.

P2 retains the SEALED state but does not yet add cross-request prefix lookup. A dropped sequence reference moves a page to RETIRED; free it only after leases retire, then advance its generation. Optional later cache index membership never substitutes for references or fences.

## 4. Scheduling algorithm

The scheduler returns candidates, not allocations. Its snapshot includes request phases/cursors/deadlines/age/tenant deficit, output credits, physical and future KV capacity, executor availability, supported graph buckets, scratch limits, and measured costs.

```text
drain bounded completions and cancellations; retire eligible resources
build immutable policy snapshot
order eligible decodes by deadline slack, tenant deficit, arrival, stable ID
protect an aged prefill service opportunity when its quota is due
select feasible decode inputs within sequence/token/time/resource budgets
add aged prefills using the largest feasible supported chunk
validate/reserve selection transactionally
dispatch one immutable iteration; otherwise shrink or defer with reason
```

Each selected decode contributes its one pending sampled token. Each prefill contributes only its next uncomputed chunk. The policy reserves service for old prefills rather than waiting indefinitely for decode residual capacity. Configure maximum prefill service delay/quotas within a qualified traffic envelope; if the SLOs cannot coexist, admission rejects/defer new demand. A single infeasible long request cannot block all following feasible work forever; queue bypass has bounded aging and an observable reason.

If mixed prefill/decode graphs are unsupported, choose separate programs and alternate at iteration boundaries under the same policy. A submitted NPU step is nonpreemptible unless target qualification proves otherwise. Chunk size is a graph/cost choice and need not equal page size. Admission checks that at least one supported chunk/decode variant can make progress within the declared memory/context envelope.

The budget is `{max_tokens, max_sequences, available_KV, future_commitments, scratch_bytes, output_credits, iteration_time_cap}`. Context-dependent KV read cost and graph padding matter; raw token count is insufficient. Strict admission remains the default. Overload is explicit, never hidden as unlimited queuing or emergency allocation.

## 5. Attention and executor paths

First implement independent dense and paged CPU attention. The paged operator consumes Q, ordered block handles, valid lengths, positions, causal mask, and a versioned layout descriptor. A prefill query may read the prefix and earlier/current positions in its chunk, never future positions. All-masked rows have a defined output; padding is never read uninitialized.

The native NPU path resolves block tables directly. A gather fallback copies only valid KV into preallocated contiguous scratch, fences the copy, then runs qualified attention. Account for its extra bytes, latency, and maximum context; reject any request beyond the fallback's bounded envelope. Do not call the fallback a native paging performance result.

Per-step metadata staging is leased and may double-buffer only when buffer reuse is fenced. One plan per worker is the baseline. The executor validates model/layout/variant compatibility and checked slot mapping before launch. Group/sequence/model epochs and step IDs are validated again on completion. Partial launch is an execution failure requiring fencing, not a reservation rollback shortcut.

## 6. Cost model, replay, and instrumentation

Store measured step distributions by phase, batch, query tokens, context histogram, precision, variant, and thermal state. Use conservative quantiles and interpolate only inside measured ranges. Unknown or high-error estimates choose a safe supported bucket or disable the adaptive selection; never manufacture an unmeasured guarantee.

`lumen-replay` consumes versioned bounded traces containing event ordering, request lengths/IDs, scheduling/capacity snapshots, cost-table version, selected plan, reservation outcome, fence/result metadata, and output timestamps. Prompt/token contents are excluded by default; content-sensitive reference runs require explicit fixture handling. Preserve an independent invariant checker so replay does not merely repeat the same allocator bugs.

Record queue/admission wait, chunk and decode selection, plan/padding sizes, dispatch/device/fence/postprocess/commit times, allocated/reserved/retired/free pages, generation errors, output pressure, fairness delay, and rejection reason. Telemetry backpressure drops telemetry with a counter; it cannot block a device completion.

## 7. Verification and exit gate

| Work package | Required evidence |
|---|---|
| P2.1 KV allocator | At least 100,000 seeded randomized allocate/append/retire transitions, plus failed reservation, stale generation, cancel, duplicate/stale completion and partial launch injection |
| P2.2 attention | Dense/paged equivalence over random noncontiguous layouts, page boundaries, partial pages, masks, context lengths and supported dtypes; CPU/NPU comparisons |
| P2.3 scheduler | Mixed short/long traffic, prompt-final emission only, bounded prefill service, overload/credits and no cross-request leakage |
| P2.4 executor | Immutable snapshots, early completion races, invalid plans, delayed fences, safe cancellation and no reuse before retirement |
| P2.5 cost/simulator | Supported-bucket calibration, held-out workload prediction, unknown-cost fallback and deterministic decisions |
| P2.6 instrumentation | Reproducible trace/replay, bounded overhead/drop behavior, complete timing/resource categories |

No double free, stale active handle, uncommitted context read, partial logical commit, or OOM inside the declared strict envelope is acceptable. Verify replay produces identical selections for identical ordered events/configuration/cost snapshots; cross-backend token equality uses separately defined numeric rules.

Compare R0.2 with R0.1 on identical model, precision, limits, requests, arrival distribution, hardware, and warmup. Report offered/admitted/completed/rejected demand, SLO goodput and p99 ITL/TTFT, not only aggregate throughput. The suggested ≥1.3× goodput target on a representative mixed workload is provisional. If no gain, attribute gather/launch/padding/policy costs before proceeding; do not pass the performance gate by changing quality or rejection policy silently.

## 8. Deliverables and open decisions

Deliver the logical page pool, transactional reservations, CPU and qualified NPU/gather attention, continuous scheduler, cost-table schema, discrete-event simulator/replay tool, property/fault corpus, benchmark comparisons, and layout/capability inspector extensions. Retain the P1 contiguous path as a reference/fallback only within its declared limits.

Accept [ADR 0003](../adr/0003-logical-kv-layout-and-page-size.md) and [ADR 0004](../adr/0004-scheduler-and-admission-policy.md) for the selected envelope. Target evidence must settle native attention support, scratch/metadata bounds, graph phase mixing, bucket choices, fairness service settings, and iteration margin. Exact container types and trace encoding are builder choices subject to boundedness and versioning.
