# LUMEN roadmap issue batch

Created: 2026-11-25

Roadmap tracker: [#3](https://github.com/skg7on/LUMEN/issues/3).

This batch translates the [phase designs](../../architecture/README.md), [implementation plans](../plans/README.md), and P0 roadmap prerequisites into 67 GitHub issues: 59 work items, seven phase trackers, and one roadmap tracker. GitHub is the live source for status; this file records the creation mapping.

P0/P1 are the first MVP milestone and carry the `mvp` label. Later phases retain their release gates. P5 and conditional TP/tiering remain optional; P6 may qualify a P3 single-node release without P4/P5. Milestones have no committed due dates.

## Phase trackers

| Phase | Release | Tracker | Work items |
|---|---|---|---|
| P0 | R0.0 | [#4 Foundations and feasibility](https://github.com/skg7on/LUMEN/issues/4) | 6 |
| P1 | R0.1 | [#11 Native single-NPU MVP](https://github.com/skg7on/LUMEN/issues/11) | 10 |
| P2 | R0.2 | [#22 Continuous batching and paged KV](https://github.com/skg7on/LUMEN/issues/22) | 8 |
| P3 | R0.3 | [#31 NPU optimization and prefix reuse](https://github.com/skg7on/LUMEN/issues/31) | 9 |
| P4 | R0.5 | [#41 Replica mesh and conditional local TP](https://github.com/skg7on/LUMEN/issues/41) | 7 |
| P5 | R0.7 | [#49 Optional advanced distribution](https://github.com/skg7on/LUMEN/issues/49) | 10 |
| P6 | R1.0 | [#60 Industrial qualification](https://github.com/skg7on/LUMEN/issues/60) | 9 |

## Work items

Each issue contains source links, dependencies, implementation scope, test/verification requirements, and acceptance criteria. Phase trackers also contain design/ADR review and release-evidence checklists.

### P0 Foundations and feasibility

- [#5](https://github.com/skg7on/LUMEN/issues/5) [P0] LUMEN-001: native build skeleton and backend C ABI
- [#6](https://github.com/skg7on/LUMEN/issues/6) [P0] LUMEN-002: qualify one NPU, SDK and execution envelope
- [#7](https://github.com/skg7on/LUMEN/issues/7) [P0] LUMEN-003: select a licensed model fixture and checked native tensor loader
- [#8](https://github.com/skg7on/LUMEN/issues/8) [P0] LUMEN-004: implement the independent CPU reference Transformer
- [#9](https://github.com/skg7on/LUMEN/issues/9) [P0] LUMEN-005: define sequence ownership and immutable StepPlan contracts
- [#10](https://github.com/skg7on/LUMEN/issues/10) [P0] LUMEN-006: implement controllable mock async execution and replay

### P1 Native single-NPU MVP

- [#12](https://github.com/skg7on/LUMEN/issues/12) [P1] Task 1: Verify P0 inputs and freeze the MVP integration profile
- [#13](https://github.com/skg7on/LUMEN/issues/13) [P1] Task 2: Create the native packer and immutable loader
- [#14](https://github.com/skg7on/LUMEN/issues/14) [P1] Task 3: Implement tokenizer parity and immutable decoder proposals
- [#15](https://github.com/skg7on/LUMEN/issues/15) [P1] Task 4: Add deterministic bounded MVP sampling
- [#16](https://github.com/skg7on/LUMEN/issues/16) [P1] Task 5: Implement contiguous logical KV and strict admission
- [#17](https://github.com/skg7on/LUMEN/issues/17) [P1] Task 6: Integrate the qualified model runner and startup lifecycle
- [#18](https://github.com/skg7on/LUMEN/issues/18) [P1] Task 7: Implement owner-thread execution and postprocess commit
- [#19](https://github.com/skg7on/LUMEN/issues/19) [P1] Task 8: Expose the bounded native C interface
- [#20](https://github.com/skg7on/LUMEN/issues/20) [P1] Task 9: Add bounded HTTP/SSE and service controls
- [#21](https://github.com/skg7on/LUMEN/issues/21) [P1] Task 10: Build the first benchmark and MVP acceptance report

### P2 Continuous batching and paged KV

- [#23](https://github.com/skg7on/LUMEN/issues/23) [P2] Task 1: Create descriptor-based logical page pools
- [#24](https://github.com/skg7on/LUMEN/issues/24) [P2] Task 2: Reserve and publish KV mappings transactionally
- [#25](https://github.com/skg7on/LUMEN/issues/25) [P2] Task 3: Implement independent CPU paged attention
- [#26](https://github.com/skg7on/LUMEN/issues/26) [P2] Task 4: Integrate native or bounded gather execution
- [#27](https://github.com/skg7on/LUMEN/issues/27) [P2] Task 5: Add continuous decode-first scheduling with prefill service
- [#28](https://github.com/skg7on/LUMEN/issues/28) [P2] Task 6: Calibrate conservative bucket costs
- [#29](https://github.com/skg7on/LUMEN/issues/29) [P2] Task 7: Add bounded traces and independent replay checks
- [#30](https://github.com/skg7on/LUMEN/issues/30) [P2] Task 8: Qualify safety and mixed-load goodput

### P3 NPU optimization and prefix reuse

- [#32](https://github.com/skg7on/LUMEN/issues/32) [P3] Task 1: Build finite qualified implementation dispatch
- [#33](https://github.com/skg7on/LUMEN/issues/33) [P3] Task 2: Optimize measured attention and fusion bottlenecks
- [#34](https://github.com/skg7on/LUMEN/issues/34) [P3] Task 3: Implement bounded prefix lookup and full-hit reuse
- [#35](https://github.com/skg7on/LUMEN/issues/35) [P3] Task 4: Fence cache eviction and optional copy on write
- [#36](https://github.com/skg7on/LUMEN/issues/36) [P3] Task 5: Qualify one precision extension at a time
- [#37](https://github.com/skg7on/LUMEN/issues/37) [P3] Task 6: Extend sampling with committed request-scoped RNG
- [#38](https://github.com/skg7on/LUMEN/issues/38) [P3] Task 7: Add class and thermal-aware adaptive policy
- [#39](https://github.com/skg7on/LUMEN/issues/39) [P3] Task 8: Gate optional elastic admission and host tiers — optional/conditional
- [#40](https://github.com/skg7on/LUMEN/issues/40) [P3] Task 9: Run allocation, quality and sustained R0.3 qualification

### P4 Replica mesh and conditional local TP

- [#42](https://github.com/skg7on/LUMEN/issues/42) [P4] Task 1: Add authenticated bounded registry and routing
- [#43](https://github.com/skg7on/LUMEN/issues/43) [P4] Task 2: Reconcile submissions and fence output delivery
- [#44](https://github.com/skg7on/LUMEN/issues/44) [P4] Task 3: Implement node draining and immutable rollout
- [#45](https://github.com/skg7on/LUMEN/issues/45) [P4] Task 4: Pack explicit TP shards and local KV mappings — optional/conditional
- [#46](https://github.com/skg7on/LUMEN/issues/46) [P4] Task 5: Add ordered collectives and atomic rank-group execution — optional/conditional
- [#47](https://github.com/skg7on/LUMEN/issues/47) [P4] Task 6: Fence TP failures and bounded group recovery — optional/conditional
- [#48](https://github.com/skg7on/LUMEN/issues/48) [P4] Task 7: Qualify independent replica and TP tracks

### P5 Optional advanced distribution

- [#50](https://github.com/skg7on/LUMEN/issues/50) [P5] Task 1: Enforce per-feature entry and benefit gates — optional/conditional
- [#51](https://github.com/skg7on/LUMEN/issues/51) [P5] Task 2: Serialize exact frozen checkpoints and transfer manifests — optional/conditional
- [#52](https://github.com/skg7on/LUMEN/issues/52) [P5] Task 3: Implement durable single-owner grant transitions — optional/conditional
- [#53](https://github.com/skg7on/LUMEN/issues/53) [P5] Task 4: Build credit-limited integrity-checked KV transfer — optional/conditional
- [#54](https://github.com/skg7on/LUMEN/issues/54) [P5] Task 5: Orchestrate drain-before-grant handoff — optional/conditional
- [#55](https://github.com/skg7on/LUMEN/issues/55) [P5] Task 6: Pipeline distinct-sequence microbatches in one outer plan — optional/conditional
- [#56](https://github.com/skg7on/LUMEN/issues/56) [P5] Task 7: Couple prefill/decode placement and admission — optional/conditional
- [#57](https://github.com/skg7on/LUMEN/issues/57) [P5] Task 8: Add advisory tenant-scoped prefix hints — optional/conditional
- [#58](https://github.com/skg7on/LUMEN/issues/58) [P5] Task 9: Integrate only an accepted speculative algorithm — optional/conditional
- [#59](https://github.com/skg7on/LUMEN/issues/59) [P5] Task 10: Accept each advanced feature on fault and benefit evidence — optional/conditional

### P6 Industrial qualification

- [#61](https://github.com/skg7on/LUMEN/issues/61) [P6] Task 1: Freeze the supported matrix and acceptance profile
- [#62](https://github.com/skg7on/LUMEN/issues/62) [P6] Task 2: Harden watchdog and bounded recovery
- [#63](https://github.com/skg7on/LUMEN/issues/63) [P6] Task 3: Make configuration generations and reload atomic
- [#64](https://github.com/skg7on/LUMEN/issues/64) [P6] Task 4: Verify signed artifacts and atomic installation
- [#65](https://github.com/skg7on/LUMEN/issues/65) [P6] Task 5: Complete parser, tenant and diagnostic isolation
- [#66](https://github.com/skg7on/LUMEN/issues/66) [P6] Task 6: Finalize bounded metrics and truthful SLO accounting
- [#67](https://github.com/skg7on/LUMEN/issues/67) [P6] Task 7: Automate reproducible builds and deployment acceptance
- [#68](https://github.com/skg7on/LUMEN/issues/68) [P6] Task 8: Run thermal, load, recovery and 72-hour qualification
- [#69](https://github.com/skg7on/LUMEN/issues/69) [P6] Task 9: Evaluate release gates and assemble supportable artifacts

## Verification at creation

- All 67 issue bodies were read back and matched against the prepared batch.
- Labels, milestone assignment, dependency references, source links and phase task mappings were checked.
- All seven phase trackers and the roadmap tracker link to the created work items.
- The source design/plan revision is `bf5afe3`, published on `docs/phase-design-specs`.
- Issue creation is not implementation, board qualification, or acceptance evidence.
