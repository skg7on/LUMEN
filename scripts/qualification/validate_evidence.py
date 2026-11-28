#!/usr/bin/env python3
# Copyright 2026 Gavin Sun
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Development-only qualification evidence validator.

This script is not shipped and is not a runtime or packer dependency. It
validates the explicitly documented bundle structure and cross-file invariants;
it does not implement general JSON Schema.

Ordinary validation is read-only and requires a sealed (`verified`) bundle.
`--seal` validates a completed bundle, computes artifact SHA-256 digests, and
atomically writes the verified manifest last. Neither mode is device evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
EVIDENCE_STATES = {
    "supported",
    "unsupported",
    "unavailable",
    "failed",
    "not_tested",
}
OVERALL_DECISIONS = {"qualified", "blocked", "failed"}
P2_MODES = {"native_paged", "gather", "no_go"}
SOURCE_KINDS = {"real_device", "synthetic"}
INTEGRITY_STATES = {"pending", "verified"}

TARGET_FIELDS = (
    "vendor",
    "sku",
    "board",
    "board_revision",
    "device_identifier",
    "architecture",
    "os",
    "kernel_version",
    "firmware",
    "driver_version",
    "sdk_version",
    "runtime_version",
    "compiler_version",
)

RUN_FIELDS = (
    "run_id",
    "started_utc",
    "finished_utc",
    "git_commit",
    "diff_digest",
    "compiler",
    "linker",
    "cmake_version",
    "build_flags",
    "adapter_hash",
    "profile_hash",
    "graph_hash",
)

LIMIT_INT_FIELDS = (
    "host_budget_bytes",
    "staging_budget_bytes",
    "device_budget_bytes",
    "workspace_budget_bytes",
    "max_inflight_commands",
    "max_graphs",
    "sample_count",
    "warmup_count",
    "per_case_timeout_ns",
    "soak_duration_ns",
)

LIMIT_FLOAT_FIELDS = ("temperature_stop_celsius", "power_stop_watts")


class ValidationError(Exception):
    """Raised for any structural or integrity violation."""


def _object_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValidationError(f"duplicate JSON key: {key!r}")
        result[key] = value
    return result


def _reject_constant(value: str) -> Any:
    raise ValidationError(f"non-finite JSON number: {value}")


def _check_finite(value: Any, context: str = "$") -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise ValidationError(f"non-finite number at {context}")
    if isinstance(value, dict):
        for key, item in value.items():
            _check_finite(item, f"{context}.{key}")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            _check_finite(item, f"{context}[{index}]")


def load_manifest(path: Path) -> dict[str, Any]:
    """Load a manifest while rejecting duplicate keys and non-finite numbers."""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ValidationError(f"cannot read manifest {path}: {exc}") from exc
    try:
        data = json.loads(
            text,
            object_pairs_hook=_object_pairs,
            parse_constant=_reject_constant,
        )
    except json.JSONDecodeError as exc:
        raise ValidationError(f"invalid JSON: {exc}") from exc
    _check_finite(data)
    return data


def _require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValidationError(f"{context} must be an object")
    return value


def _require_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise ValidationError(f"{context} must be an array")
    return value


def _require_str(value: Any, context: str, *, non_empty: bool = True) -> str:
    if not isinstance(value, str):
        raise ValidationError(f"{context} must be a string")
    if non_empty and not value:
        raise ValidationError(f"{context} must not be empty")
    return value


def _require_int(value: Any, context: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValidationError(f"{context} must be an integer")
    if value < minimum:
        raise ValidationError(f"{context} must be >= {minimum}")
    return value


def _require_number(value: Any, context: str, *, positive: bool) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValidationError(f"{context} must be a number")
    number = float(value)
    if not math.isfinite(number):
        raise ValidationError(f"{context} must be finite")
    if positive and number <= 0.0:
        raise ValidationError(f"{context} must be positive")
    return number


def _require_bool(value: Any, context: str) -> bool:
    if not isinstance(value, bool):
        raise ValidationError(f"{context} must be a boolean")
    return value


def _require_key(mapping: dict[str, Any], key: str, context: str) -> Any:
    if key not in mapping:
        raise ValidationError(f"{context}.{key} is required")
    return mapping[key]


def _validate_target(target: Any) -> dict[str, Any]:
    obj = _require_object(target, "target")
    for field in TARGET_FIELDS:
        _require_str(_require_key(obj, field, "target"), f"target.{field}")
    return obj


def _validate_run(run: Any) -> dict[str, Any]:
    obj = _require_object(run, "run")
    schema = _require_int(_require_key(obj, "schema_version", "run"),
                          "run.schema_version", minimum=1)
    if schema != SCHEMA_VERSION:
        raise ValidationError(f"unsupported run schema version: {schema}")
    for field in RUN_FIELDS:
        _require_str(_require_key(obj, field, "run"), f"run.{field}",
                     non_empty=field != "diff_digest")
    _require_bool(_require_key(obj, "git_dirty", "run"), "run.git_dirty")
    source = _require_str(_require_key(obj, "source_kind", "run"),
                          "run.source_kind")
    if source not in SOURCE_KINDS:
        raise ValidationError(f"unknown run.source_kind: {source!r}")
    return obj


def _validate_limits(limits: Any) -> dict[str, Any]:
    obj = _require_object(limits, "limits")
    for field in LIMIT_INT_FIELDS:
        _require_int(_require_key(obj, field, "limits"), f"limits.{field}",
                     minimum=1)
    for field in LIMIT_FLOAT_FIELDS:
        _require_number(_require_key(obj, field, "limits"), f"limits.{field}",
                        positive=True)
    _require_str(_require_key(obj, "profile_hash", "limits"),
                 "limits.profile_hash")
    return obj


def _validate_timing(timing: Any, run_id: str, probe_id: str,
                     context: str) -> None:
    obj = _require_object(timing, context)
    if _require_str(_require_key(obj, "run_id", context),
                    f"{context}.run_id") != run_id:
        raise ValidationError(f"{context}.run_id does not match run.run_id")
    if _require_str(_require_key(obj, "probe_id", context),
                    f"{context}.probe_id") != probe_id:
        raise ValidationError(f"{context}.probe_id does not match probe id")
    _require_int(_require_key(obj, "sample_id", context), f"{context}.sample_id")
    phase = _require_str(_require_key(obj, "phase", context), f"{context}.phase")
    if phase not in {"cold", "warmup", "measured"}:
        raise ValidationError(f"unknown timing phase: {phase!r}")
    for field in (
        "submit_start_ns",
        "submit_return_ns",
        "observed_complete_ns",
        "operation_bytes",
        "operation_flops",
    ):
        _require_int(_require_key(obj, field, context), f"{context}.{field}")
    _require_str(_require_key(obj, "final_status", context),
                 f"{context}.final_status")


def _validate_probes(probes: Any, run_id: str) -> list[dict[str, Any]]:
    items = _require_list(probes, "probes")
    if not items:
        raise ValidationError("probes must not be empty")
    seen: set[str] = set()
    result: list[dict[str, Any]] = []
    for index, value in enumerate(items):
        context = f"probes[{index}]"
        probe = _require_object(value, context)
        probe_id = _require_str(_require_key(probe, "id", context),
                                f"{context}.id")
        if probe_id in seen:
            raise ValidationError(f"duplicate probe id: {probe_id}")
        seen.add(probe_id)
        state = _require_str(_require_key(probe, "state", context),
                             f"{context}.state")
        if state not in EVIDENCE_STATES:
            raise ValidationError(f"unknown evidence state: {state!r}")
        _require_bool(_require_key(probe, "required", context),
                      f"{context}.required")
        _require_bool(_require_key(probe, "cleanup_ok", context),
                      f"{context}.cleanup_ok")
        _require_bool(_require_key(probe, "quarantine_resolved", context),
                      f"{context}.quarantine_resolved")
        _require_int(_require_key(probe, "correctness_mismatches", context),
                     f"{context}.correctness_mismatches")
        timings = _require_list(probe.get("timings", []), f"{context}.timings")
        for timing_index, timing in enumerate(timings):
            _validate_timing(timing, run_id, probe_id,
                             f"{context}.timings[{timing_index}]")
        result.append(probe)
    return result


def _validate_capabilities(capabilities: Any) -> list[Any]:
    items = _require_list(capabilities, "capabilities")
    for index, value in enumerate(items):
        context = f"capabilities[{index}]"
        capability = _require_object(value, context)
        _require_str(_require_key(capability, "capability", context),
                     f"{context}.capability")
        state = _require_str(_require_key(capability, "state", context),
                             f"{context}.state")
        if state not in EVIDENCE_STATES:
            raise ValidationError(f"unknown capability state: {state!r}")
        _require_list(capability.get("supporting_probes", []),
                      f"{context}.supporting_probes")
    return items


def _validate_decision(decision: Any, probes: list[dict[str, Any]]) -> dict[str, Any]:
    obj = _require_object(decision, "decision")
    overall = _require_str(_require_key(obj, "overall", "decision"),
                           "decision.overall")
    if overall not in OVERALL_DECISIONS:
        raise ValidationError(f"unknown overall decision: {overall!r}")
    reasons = _require_list(_require_key(obj, "reason_codes", "decision"),
                            "decision.reason_codes")
    for index, reason in enumerate(reasons):
        _require_str(reason, f"decision.reason_codes[{index}]")
    _require_bool(_require_key(obj, "p1_eligible", "decision"),
                  "decision.p1_eligible")
    p2_mode = _require_str(_require_key(obj, "p2_mode", "decision"),
                           "decision.p2_mode")
    if p2_mode not in P2_MODES:
        raise ValidationError(f"unknown p2_mode: {p2_mode!r}")
    for index, family in enumerate(
            _require_list(_require_key(obj, "p3_eligible_probe_families",
                                       "decision"),
                          "decision.p3_eligible_probe_families")):
        _require_str(family, f"decision.p3_eligible_probe_families[{index}]")
    for index, limitation in enumerate(
            _require_list(obj.get("limitations", []), "decision.limitations")):
        _require_str(limitation, f"decision.limitations[{index}]")

    if overall == "qualified":
        for probe in probes:
            if not probe["required"]:
                continue
            if probe["state"] != "supported":
                raise ValidationError(
                    "qualified decision cites a required probe that is not "
                    f"supported: {probe['id']}")
            if not probe["cleanup_ok"] or not probe["quarantine_resolved"]:
                raise ValidationError(
                    "qualified decision cites unresolved required evidence: "
                    f"{probe['id']}")
    return obj


def _validate_artifacts_shape(artifacts: Any) -> list[dict[str, Any]]:
    items = _require_list(artifacts, "artifacts")
    seen: set[str] = set()
    result: list[dict[str, Any]] = []
    for index, value in enumerate(items):
        context = f"artifacts[{index}]"
        entry = _require_object(value, context)
        path = _require_str(_require_key(entry, "path", context),
                            f"{context}.path")
        if path in seen:
            raise ValidationError(f"duplicate artifact path: {path}")
        seen.add(path)
        result.append(entry)
    return result


def validate_manifest(data: Any, *, require_real_device: bool = False,
                      require_sealed: bool = True) -> dict[str, Any]:
    """Validate structure and cross-file invariants without mutating `data`."""
    manifest = _require_object(data, "manifest")
    schema = _require_int(_require_key(manifest, "schema_version", "manifest"),
                          "manifest.schema_version", minimum=1)
    if schema != SCHEMA_VERSION:
        raise ValidationError(f"unsupported manifest schema version: {schema}")

    target = _validate_target(_require_key(manifest, "target", "manifest"))
    run = _validate_run(_require_key(manifest, "run", "manifest"))
    limits = _validate_limits(_require_key(manifest, "limits", "manifest"))
    if run["profile_hash"] != limits["profile_hash"]:
        raise ValidationError("run.profile_hash does not match limits.profile_hash")
    if require_real_device and run["source_kind"] != "real_device":
        raise ValidationError(
            "real-device qualification requested but source_kind is "
            f"{run['source_kind']!r}")

    probes = _validate_probes(_require_key(manifest, "probes", "manifest"),
                              run["run_id"])
    _validate_capabilities(_require_key(manifest, "capabilities", "manifest"))
    decision = _validate_decision(
        _require_key(manifest, "decision", "manifest"), probes)
    if decision["overall"] == "qualified" and run["source_kind"] != "real_device":
        raise ValidationError(
            "qualified decision requires real_device source_kind")

    _validate_artifacts_shape(_require_key(manifest, "artifacts", "manifest"))
    if not _require_bool(_require_key(manifest, "measurement_complete",
                                      "manifest"),
                         "manifest.measurement_complete"):
        raise ValidationError("measurement_complete must be true")

    integrity = _require_str(_require_key(manifest, "integrity", "manifest"),
                             "manifest.integrity")
    if integrity not in INTEGRITY_STATES:
        raise ValidationError(f"unknown integrity state: {integrity!r}")
    if require_sealed and integrity != "verified":
        raise ValidationError(
            "evidence bundle is not sealed; run the unmodified validator with "
            "--seal on the completed bundle")
    return manifest


def _artifact_root(manifest_path: Path) -> Path:
    return manifest_path.parent.resolve()


def verify_artifacts(manifest: dict[str, Any], manifest_path: Path,
                     *, seal: bool) -> None:
    """Resolve, bound, hash, and (with `seal`) record every artifact."""
    root = _artifact_root(manifest_path)
    for index, entry in enumerate(manifest["artifacts"]):
        context = f"artifacts[{index}]"
        relative = entry["path"]
        candidate = Path(relative)
        if candidate.is_absolute():
            raise ValidationError(f"{context}.path must be relative: {relative}")
        resolved = (root / candidate).resolve()
        if not resolved.is_relative_to(root):
            raise ValidationError(f"{context}.path escapes the bundle: {relative}")
        if not resolved.is_file():
            raise ValidationError(f"{context}.path is missing: {relative}")

        size = resolved.stat().st_size
        digest = hashlib.sha256(resolved.read_bytes()).hexdigest()
        if seal:
            expected_digest = entry.get("sha256")
            if expected_digest is not None:
                if _require_str(expected_digest, f"{context}.sha256") != digest:
                    raise ValidationError(
                        f"{context}.sha256 does not match {relative}")
            entry["sha256"] = digest
            entry["size_bytes"] = size
        else:
            expected_digest = _require_str(
                _require_key(entry, "sha256", context), f"{context}.sha256")
            if expected_digest != digest:
                raise ValidationError(
                    f"{context}.sha256 does not match {relative}")
            _require_int(_require_key(entry, "size_bytes", context),
                         f"{context}.size_bytes")
            if entry["size_bytes"] != size:
                raise ValidationError(
                    f"{context}.size_bytes does not match {relative}")


def _atomic_write_json(path: Path, data: dict[str, Any]) -> None:
    text = json.dumps(data, indent=2, allow_nan=False) + "\n"
    descriptor, temporary = tempfile.mkstemp(
        dir=path.parent, prefix=f".{path.name}.", suffix=".tmp")
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def seal_manifest(manifest_path: Path, *,
                  require_real_device: bool = False) -> dict[str, Any]:
    """Validate a completed bundle and atomically mark it verified."""
    data = load_manifest(manifest_path)
    manifest = validate_manifest(data, require_real_device=require_real_device,
                                 require_sealed=False)
    if manifest["decision"]["overall"] != "qualified":
        raise ValidationError("seal refuses a decision that is not qualified")
    verify_artifacts(manifest, manifest_path, seal=True)
    manifest["integrity"] = "verified"
    _atomic_write_json(manifest_path, manifest)
    return manifest


def validate_file(manifest_path: Path, *,
                  require_real_device: bool = False) -> dict[str, Any]:
    data = load_manifest(manifest_path)
    manifest = validate_manifest(data, require_real_device=require_real_device,
                                 require_sealed=True)
    verify_artifacts(manifest, manifest_path, seal=False)
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate a LUMEN qualification evidence bundle.")
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--require-real-device", action="store_true",
                        help="reject synthetic source_kind")
    parser.add_argument("--seal", action="store_true",
                        help="hash artifacts and atomically mark verified")
    args = parser.parse_args(argv)

    try:
        if args.seal:
            manifest = seal_manifest(
                args.manifest, require_real_device=args.require_real_device)
        else:
            manifest = validate_file(
                args.manifest, require_real_device=args.require_real_device)
    except (ValidationError, OSError) as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1

    print(f"PASS {args.manifest} "
          f"(overall={manifest['decision']['overall']}, "
          f"integrity={manifest['integrity']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
