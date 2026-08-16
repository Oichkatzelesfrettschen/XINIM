#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from typing import Any


PATCHED_METADATA_NAME = ".xinim-patched-source.json"
SOURCE_METADATA_NAME = ".xinim-source.json"


def sha256_file(file_path: Path) -> str:
    digest = hashlib.sha256()
    with file_path.open("rb") as input_file:
        for block in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_manifest(manifest_path: Path) -> dict[str, Any]:
    manifest = json.loads(manifest_path.read_text(encoding="ascii"))
    if manifest.get("schema") != 1:
        raise RuntimeError("unsupported mksh patch manifest schema")
    if not isinstance(manifest.get("files"), dict) or not manifest["files"]:
        raise RuntimeError("mksh patch manifest has no file hashes")
    return manifest


def verify_file_hashes(
    source_directory: Path, manifest: dict[str, Any], hash_key: str
) -> None:
    for relative_name, file_record in sorted(manifest["files"].items()):
        source_path = source_directory / relative_name
        if not source_path.is_file():
            raise RuntimeError(f"mksh {hash_key} file is missing: {relative_name}")
        actual_hash = sha256_file(source_path)
        expected_hash = file_record[hash_key]
        if actual_hash != expected_hash:
            raise RuntimeError(
                f"mksh {hash_key} mismatch for {relative_name}: "
                f"expected {expected_hash}, got {actual_hash}"
            )


def verify_pristine_metadata(source_directory: Path, manifest: dict[str, Any]) -> None:
    metadata_path = source_directory / SOURCE_METADATA_NAME
    if not metadata_path.is_file():
        raise RuntimeError(f"pristine mksh metadata is missing: {metadata_path}")
    metadata = json.loads(metadata_path.read_text(encoding="ascii"))
    archive_record = manifest["archive"]
    for key in ("sha256", "url", "version"):
        if metadata.get(key) != archive_record[key]:
            raise RuntimeError(
                f"pristine mksh metadata mismatch for {key}: "
                f"expected {archive_record[key]}, got {metadata.get(key)}"
            )


def verify_patch(patch_path: Path, manifest: dict[str, Any]) -> None:
    expected_name = manifest["patch"]["name"]
    if patch_path.name != expected_name:
        raise RuntimeError(
            f"mksh patch name mismatch: expected {expected_name}, got {patch_path.name}"
        )
    actual_hash = sha256_file(patch_path)
    expected_hash = manifest["patch"]["sha256"]
    if actual_hash != expected_hash:
        raise RuntimeError(
            f"mksh patch SHA256 mismatch: expected {expected_hash}, got {actual_hash}"
        )


def expected_patched_metadata(manifest: dict[str, Any]) -> dict[str, Any]:
    return {
        "archive": manifest["archive"],
        "files": manifest["files"],
        "mechanism": manifest["mechanism"],
        "patch": manifest["patch"],
        "schema": manifest["schema"],
        "standards_authority": manifest["standards_authority"],
    }


def verify_existing_target(target_directory: Path, manifest: dict[str, Any]) -> bool:
    if not target_directory.exists():
        return False
    if not target_directory.is_dir():
        raise RuntimeError(
            f"mksh patched target is not a directory: {target_directory}"
        )
    if not any(target_directory.iterdir()):
        target_directory.rmdir()
        return False
    metadata_path = target_directory / PATCHED_METADATA_NAME
    if not metadata_path.is_file():
        raise RuntimeError(
            f"refusing to overwrite unverified mksh patched target: {target_directory}"
        )
    actual_metadata = json.loads(metadata_path.read_text(encoding="ascii"))
    if actual_metadata != expected_patched_metadata(manifest):
        raise RuntimeError(
            f"refusing to overwrite stale mksh patched target: {target_directory}"
        )
    verify_file_hashes(target_directory, manifest, "postimage_sha256")
    return True


def apply_patch(
    source_directory: Path,
    target_directory: Path,
    patch_path: Path,
    manifest_path: Path,
) -> None:
    manifest = load_manifest(manifest_path)
    verify_pristine_metadata(source_directory, manifest)
    verify_file_hashes(source_directory, manifest, "preimage_sha256")
    verify_patch(patch_path, manifest)

    if source_directory == target_directory:
        raise RuntimeError("pristine and patched mksh directories must differ")
    if verify_existing_target(target_directory, manifest):
        return

    target_directory.parent.mkdir(parents=True, exist_ok=True)
    temporary_root = Path(
        tempfile.mkdtemp(
            prefix=f".{target_directory.name}.", dir=target_directory.parent
        )
    )
    candidate_directory = temporary_root / "tree"
    try:
        shutil.copytree(source_directory, candidate_directory, symlinks=True)
        result = subprocess.run(
            [
                "patch",
                "--batch",
                "--forward",
                "--fuzz=0",
                "--strip=1",
                f"--input={patch_path}",
            ],
            cwd=candidate_directory,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if result.returncode != 0:
            raise RuntimeError(f"mksh patch application failed:\n{result.stdout}")
        verify_file_hashes(candidate_directory, manifest, "postimage_sha256")
        metadata_path = candidate_directory / PATCHED_METADATA_NAME
        metadata_path.write_text(
            json.dumps(expected_patched_metadata(manifest), indent=2, sort_keys=True)
            + "\n",
            encoding="ascii",
        )
        os.replace(candidate_directory, target_directory)
    finally:
        shutil.rmtree(temporary_root, ignore_errors=True)


def require_failure(action: Any, expected_text: str) -> None:
    try:
        action()
    except RuntimeError as error:
        if expected_text not in str(error):
            raise RuntimeError(
                f"mutation failed for the wrong reason: expected {expected_text!r}, "
                f"got {str(error)!r}"
            ) from error
    else:
        raise RuntimeError(f"mutation unexpectedly passed: {expected_text}")


def self_test(source_directory: Path, patch_path: Path, manifest_path: Path) -> None:
    manifest = load_manifest(manifest_path)
    with tempfile.TemporaryDirectory(prefix="xinim-mksh-patch-test-") as temporary:
        temporary_directory = Path(temporary)

        altered_source = temporary_directory / "altered-source"
        shutil.copytree(source_directory, altered_source)
        altered_file = altered_source / sorted(manifest["files"])[0]
        altered_file.write_bytes(altered_file.read_bytes() + b"\n")
        require_failure(
            lambda: apply_patch(
                altered_source,
                temporary_directory / "altered-source-output",
                patch_path,
                manifest_path,
            ),
            "preimage_sha256 mismatch",
        )

        altered_patch = temporary_directory / patch_path.name
        altered_patch.write_bytes(patch_path.read_bytes() + b"\n")
        require_failure(
            lambda: apply_patch(
                source_directory,
                temporary_directory / "altered-patch-output",
                altered_patch,
                manifest_path,
            ),
            "patch SHA256 mismatch",
        )

        patch_bytes = patch_path.read_bytes()
        last_file_marker = b"--- a/syn.c\n"
        marker_offset = patch_bytes.find(last_file_marker)
        if marker_offset < 0:
            raise RuntimeError("could not locate final mksh patch hunk")
        missing_hunk_patch = temporary_directory / "missing-hunk.patch"
        missing_hunk_patch.write_bytes(patch_bytes[:marker_offset])
        missing_hunk_manifest = dict(manifest)
        missing_hunk_manifest["patch"] = {
            "name": missing_hunk_patch.name,
            "sha256": sha256_file(missing_hunk_patch),
        }
        missing_hunk_manifest_path = temporary_directory / "missing-hunk.json"
        missing_hunk_manifest_path.write_text(
            json.dumps(missing_hunk_manifest, indent=2, sort_keys=True) + "\n",
            encoding="ascii",
        )
        require_failure(
            lambda: apply_patch(
                source_directory,
                temporary_directory / "missing-hunk-output",
                missing_hunk_patch,
                missing_hunk_manifest_path,
            ),
            "postimage_sha256 mismatch",
        )

        valid_output = temporary_directory / "valid-output"
        apply_patch(source_directory, valid_output, patch_path, manifest_path)
        apply_patch(source_directory, valid_output, patch_path, manifest_path)
        verify_file_hashes(source_directory, manifest, "preimage_sha256")
        verify_file_hashes(valid_output, manifest, "postimage_sha256")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--target-dir")
    parser.add_argument("--patch", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    source_directory = Path(arguments.source_dir).resolve()
    patch_path = Path(arguments.patch).resolve()
    manifest_path = Path(arguments.manifest).resolve()
    if arguments.self_test:
        self_test(source_directory, patch_path, manifest_path)
        return 0
    if arguments.target_dir is None:
        parser.error("--target-dir is required unless --self-test is used")
    target_directory = Path(arguments.target_dir).resolve()
    apply_patch(source_directory, target_directory, patch_path, manifest_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
