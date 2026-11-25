# Native code and backend boundaries

- Use C++20 for host code and a versioned C-compatible backend ABI with opaque
  device, buffer, queue, and event handles.
- Keep STL types and C++ exceptions out of the public C/backend ABI. Return
  explicit status codes and define ownership and destruction rules for each handle.
  Use RAII wrappers inside C++ modules.
- Keep device addresses, vendor contexts, physical tensor layouts, and kernel
  launch details inside workers and backends. Scheduling operates on logical
  descriptors and capability snapshots.
- Treat every asynchronous descriptor, metadata slab, workspace, and device buffer
  as live until its completion fence retires. Submission success is not completion.
- Check arithmetic, offsets, shapes, lengths, alignment, and allocation sizes
  before accessing tensors, parsing model files, or accepting execution plans.
  Shared-memory messages use validated offsets and counts, not process pointers.
- Negotiate capabilities for the exact device and SDK. Do not assume CUDA/NCCL
  semantics, dynamic shapes, custom paged attention, or DMA/compute overlap.
- Reject unsupported combinations or use an explicitly qualified, bounded fallback.
  Keep graph variants, shape buckets, workspace, and staging memory finite and
  prewarmed; avoid LUMEN-owned heap allocation in steady-state hot paths.
- Keep CPU and mock backends usable without a vendor SDK. Use CMake and explicit
  toolchain/dependency configuration as the native build is introduced; do not
  make ordinary host correctness checks depend on target hardware.
- Preserve tokenizer/chat-template parity and explicit model traits. Validate
  model artifacts before allocation; never execute arbitrary model repository code.

Source: [architecture sections 3, 6, and 7](../../docs/init-draft/LUMEN-Architecture-Design.md).
