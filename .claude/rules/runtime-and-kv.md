# Runtime ownership and KV safety

## Engine and execution

- The engine is the single writer of request state, token accounting, scheduling
  commits, and logical KV mappings. Workers execute immutable `StepPlan` objects
  and return completion results.
- Keep at most one uncommitted step per sequence in the baseline. Accept results
  only for matching sequence/step identifiers, handle generations, and model/group
  epochs. Commit a rank-group iteration only after every required fence completes.
- Count computed KV separately from sampled output: the last sampled token normally
  becomes input to the next decode step and does not yet have KV.
- Emit output only after the final prompt chunk produces valid logits. A full
  prefix hit still needs final-token recomputation or separately validated cached
  logits; KV alone cannot produce the next token.
- Cancellation prevents future scheduling immediately. Retire in-flight leases
  before reclaiming storage. After partial launch failure, quarantine touched
  pages until work is fenced or the device context is reset.

## Allocation and cache invariants

- Bound admission, requests, token slabs, queues, output credits, scratch, KV
  reservations, and transfers. Defer or reject overload explicitly.
- Use generation-tagged handles and transactional reserve/commit/rollback. Never
  expose a partially reserved rank group or reuse a page with live references or
  read/write leases.
- Publish only full, committed, immutable prefix pages in the baseline. Keep
  mutable tails private; copy-on-write must fence its copy before remapping.
- Cache identity includes model and layout versions, attention/position settings,
  tokens, and tenant scope. Pin compatible hits atomically and remove index entries
  before slot reuse. Keep cross-tenant sharing opt-in.
- Bound slow-client buffering. Pause scheduling or cancel when output credits
  cannot recover within the configured timeout.

## Distributed ownership

- Keep scheduling local to independent replicas. A model-parallel group has one
  logical engine and stable, epoch-fenced membership for a request.
- Remote KV handoff requires compatible layouts, validated bytes, device fences,
  and an authoritative ownership grant. A timeout does not prove the former owner
  stopped; fail closed if ownership cannot be fenced.
- Resume only from a committed token/RNG/output-offset checkpoint. Never silently
  regenerate or re-emit tokens already delivered to the client.

Source: [architecture sections 3–5 and 8](../../docs/init-draft/LUMEN-Architecture-Design.md).
