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

"""Negative fixtures for the development-only evidence validator."""

from __future__ import annotations

import copy
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import validate_evidence as ve  # noqa: E402

CANONICAL_PROBES = (
    "execution/async-compute",
    "memory/kinds-and-copies",
    "lifecycle/fence-and-ownership",
    "recovery/partial-submission",
    "thermal/sustained",
    "attention/bounded-path",
)


def _base_manifest() -> dict:
    target = {field: f"fixture-{field}" for field in ve.TARGET_FIELDS}
    return {
        "schema_version": ve.SCHEMA_VERSION,
        "target": target,
        "run": {
            "schema_version": ve.SCHEMA_VERSION,
            "run_id": "run-1",
            "started_utc": "2026-11-28T00:00:00Z",
            "finished_utc": "2026-11-28T00:05:00Z",
            "git_commit": "0123456789abcdef",
            "git_dirty": False,
            "diff_digest": "",
            "compiler": "clang-20",
            "linker": "lld",
            "cmake_version": "4.2.3",
            "build_flags": "Release",
            "adapter_hash": "sha256:adapter",
            "profile_hash": "sha256:fixture",
            "graph_hash": "sha256:graph",
            "source_kind": "real_device",
        },
        "limits": {
            "host_budget_bytes": 1 << 30,
            "staging_budget_bytes": 1 << 28,
            "device_budget_bytes": 1 << 31,
            "workspace_budget_bytes": 1 << 27,
            "max_inflight_commands": 8,
            "max_graphs": 16,
            "sample_count": 1000,
            "warmup_count": 100,
            "per_case_timeout_ns": 30_000_000_000,
            "soak_duration_ns": 1_800_000_000_000,
            "temperature_stop_celsius": 85.0,
            "power_stop_watts": 20.0,
            "profile_hash": "sha256:fixture",
        },
        "probes": [
            {
                "id": probe_id,
                "state": "supported",
                "required": True,
                "cleanup_ok": True,
                "quarantine_resolved": True,
                "correctness_mismatches": 0,
                "timings": [],
            }
            for probe_id in CANONICAL_PROBES
        ],
        "capabilities": [
            {
                "capability": "execution.async_compute",
                "state": "supported",
                "supporting_probes": ["execution/async-compute"],
                "fallback_mode": "",
                "disabled_reason": "",
            }
        ],
        "decision": {
            "overall": "qualified",
            "reason_codes": [],
            "p1_eligible": True,
            "p2_mode": "gather",
            "p3_eligible_probe_families": ["gemm"],
            "limitations": [],
        },
        "artifacts": [{"path": "samples.csv"}, {"path": "events.csv"}],
        "measurement_complete": True,
        "integrity": "pending",
    }


def _write_bundle(root: Path, manifest: dict, *, artifact_names: tuple[str, ...] = (
        "samples.csv", "events.csv"), hash_artifacts: bool = False) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    for name in artifact_names:
        (root / name).write_text(f"fixture:{name}\n", encoding="utf-8")
    if hash_artifacts:
        for entry in manifest["artifacts"]:
            data = (root / entry["path"]).read_bytes()
            entry["sha256"] = hashlib.sha256(data).hexdigest()
            entry["size_bytes"] = len(data)
    path = root / "qualification.json"
    path.write_text(json.dumps(manifest), encoding="utf-8")
    return path


class ValidateEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self._temporary = tempfile.TemporaryDirectory()
        self.root = Path(self._temporary.name)

    def tearDown(self) -> None:
        self._temporary.cleanup()

    def _sealed(self) -> Path:
        manifest = _base_manifest()
        path = _write_bundle(self.root, manifest)
        ve.seal_manifest(path, require_real_device=True)
        return path

    def test_seal_then_validate_passes(self) -> None:
        manifest = _base_manifest()
        path = _write_bundle(self.root, manifest)
        sealed = ve.seal_manifest(path, require_real_device=True)
        self.assertEqual(sealed["integrity"], "verified")
        for entry in sealed["artifacts"]:
            self.assertEqual(len(entry["sha256"]), 64)
            self.assertGreater(entry["size_bytes"], 0)
        again = ve.validate_file(path, require_real_device=True)
        self.assertEqual(again["decision"]["overall"], "qualified")

    def test_ordinary_validation_rejects_pending(self) -> None:
        path = _write_bundle(self.root, _base_manifest())
        with self.assertRaises(ve.ValidationError):
            ve.validate_file(path)

    def test_duplicate_json_keys_rejected(self) -> None:
        path = self.root / "qualification.json"
        path.write_text('{"schema_version": 1, "schema_version": 1}',
                        encoding="utf-8")
        with self.assertRaisesRegex(ve.ValidationError, "duplicate JSON key"):
            ve.load_manifest(path)

    def test_missing_field_rejected(self) -> None:
        manifest = _base_manifest()
        del manifest["target"]["kernel_version"]
        with self.assertRaises(ve.ValidationError):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_wrong_type_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["limits"]["sample_count"] = "1000"
        with self.assertRaises(ve.ValidationError):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_unknown_schema_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["schema_version"] = 2
        with self.assertRaises(ve.ValidationError):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_non_finite_rejected(self) -> None:
        path = self.root / "qualification.json"
        path.write_text('{"schema_version": NaN}', encoding="utf-8")
        with self.assertRaises(ve.ValidationError):
            ve.load_manifest(path)

        manifest = _base_manifest()
        manifest["limits"]["power_stop_watts"] = float("1e999")
        path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaises(ve.ValidationError):
            ve.load_manifest(path)

    def test_truncated_bundle_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["measurement_complete"] = False
        with self.assertRaises(ve.ValidationError):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_path_escape_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["artifacts"] = [{"path": "../outside.txt"}]
        path = _write_bundle(self.root, manifest, artifact_names=())
        (self.root.parent / "outside.txt").write_text("x", encoding="utf-8")
        with self.assertRaisesRegex(ve.ValidationError, "escapes"):
            ve.seal_manifest(path)

    def test_missing_artifact_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["artifacts"] = [{"path": "missing.csv"}]
        path = _write_bundle(self.root, manifest, artifact_names=())
        with self.assertRaisesRegex(ve.ValidationError, "missing"):
            ve.seal_manifest(path)

    def test_hash_mismatch_rejected(self) -> None:
        manifest = _base_manifest()
        path = _write_bundle(self.root, manifest, hash_artifacts=True)
        manifest["integrity"] = "verified"
        manifest["artifacts"][0]["sha256"] = "0" * 64
        path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(ve.ValidationError, "does not match"):
            ve.validate_file(path)

    def test_size_mismatch_rejected(self) -> None:
        manifest = _base_manifest()
        path = _write_bundle(self.root, manifest, hash_artifacts=True)
        manifest["integrity"] = "verified"
        manifest["artifacts"][0]["size_bytes"] += 1
        path.write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(ve.ValidationError, "size_bytes"):
            ve.validate_file(path)

    def test_require_real_device_rejects_synthetic(self) -> None:
        manifest = _base_manifest()
        manifest["run"]["source_kind"] = "synthetic"
        manifest["decision"]["overall"] = "blocked"
        manifest["decision"]["p1_eligible"] = False
        path = _write_bundle(self.root, manifest)
        with self.assertRaisesRegex(ve.ValidationError,
                                    "source_kind is 'synthetic'"):
            ve.seal_manifest(path, require_real_device=True)

    def test_seal_refuses_incomplete(self) -> None:
        manifest = _base_manifest()
        manifest["measurement_complete"] = False
        path = _write_bundle(self.root, manifest)
        with self.assertRaises(ve.ValidationError):
            ve.seal_manifest(path)

    def test_seal_refuses_failed_decision(self) -> None:
        manifest = _base_manifest()
        manifest["decision"]["overall"] = "failed"
        manifest["decision"]["p1_eligible"] = False
        path = _write_bundle(self.root, manifest)
        with self.assertRaisesRegex(ve.ValidationError, "not qualified"):
            ve.seal_manifest(path)

    def test_cross_check_run_id_mismatch(self) -> None:
        manifest = _base_manifest()
        manifest["probes"][0]["timings"] = [{
            "run_id": "another-run",
            "probe_id": "execution/async-compute",
            "sample_id": 0,
            "phase": "measured",
            "submit_start_ns": 1,
            "submit_return_ns": 2,
            "observed_complete_ns": 3,
            "operation_bytes": 4,
            "operation_flops": 5,
            "final_status": "ok",
        }]
        with self.assertRaisesRegex(ve.ValidationError, "run_id"):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_duplicate_probe_id_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["probes"].append(copy.deepcopy(manifest["probes"][0]))
        with self.assertRaisesRegex(ve.ValidationError, "duplicate probe"):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_qualified_with_unsupported_required_probe_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["probes"][0]["state"] = "unsupported"
        with self.assertRaisesRegex(ve.ValidationError, "not supported"):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_profile_hash_mismatch_rejected(self) -> None:
        manifest = _base_manifest()
        manifest["run"]["profile_hash"] = "sha256:other"
        with self.assertRaisesRegex(ve.ValidationError, "profile_hash"):
            ve.validate_manifest(manifest, require_sealed=False)

    def test_cli_exit_codes(self) -> None:
        path = self._sealed()
        self.assertEqual(ve.main([str(path), "--require-real-device"]), 0)

        manifest = _base_manifest()
        manifest["run"]["source_kind"] = "synthetic"
        manifest["decision"]["overall"] = "blocked"
        manifest["decision"]["p1_eligible"] = False
        synthetic_path = _write_bundle(self.root / "synthetic",
                                       manifest,
                                       artifact_names=("samples.csv",
                                                       "events.csv"))
        self.assertEqual(
            ve.main([str(synthetic_path), "--require-real-device"]), 1)


if __name__ == "__main__":
    unittest.main()
