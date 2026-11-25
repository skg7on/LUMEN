# ADR 0002 Versioned backend ABI

Status: Proposed | Date: 2026-11-25

## Context

Vendor contexts, physical layouts, toolchains, and asynchronous error behavior differ. A C++ object ABI would couple adapters to STL and compiler details.

## Proposed decision

Use a versioned C function table with size/version fields, fixed-width records, status codes, opaque handles, explicit destruction, and capability negotiation. Keep C++20 RAII wrappers internal. Define failed-call side effects, queue ordering, event retirement, and reset semantics before using an adapter. Build CPU/mock adapters independently of vendor SDKs.

## Alternatives and consequences

An internal-only C++ virtual interface is simpler initially but unsuitable as the public adapter ABI. A stable C boundary costs explicit validation and conversion. Static or versioned shared adapters are both possible; packaging is an implementation choice until the target SDK is known.

## Acceptance evidence

C header compilation, ABI version/size rejection, lifetime/error tests, delayed mock completion, and actual target queue/event/reset probes. See [shared contracts](../architecture/Shared-Runtime-Contracts.md).
