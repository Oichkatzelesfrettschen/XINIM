#!/usr/bin/env python3

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import tarfile
import tempfile
import urllib.request


MKSH_VERSION = "R59c"
MKSH_ARCHIVE_NAME = f"mksh-{MKSH_VERSION}.tgz"
MKSH_SOURCE_URL = f"http://www.mirbsd.org/MirOS/dist/mir/mksh/{MKSH_ARCHIVE_NAME}"
MKSH_SHA256 = "77ae1665a337f1c48c61d6b961db3e52119b38e58884d1c89684af31f87bc506"
MKSH_ARCHIVE_ROOT = "mksh"
SOURCE_METADATA_NAME = ".xinim-source.json"


def sha256_file(archive_path: Path) -> str:
    digest = hashlib.sha256()
    with archive_path.open("rb") as archive_file:
        for block in iter(lambda: archive_file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_archive(archive_path: Path) -> None:
    actual_digest = sha256_file(archive_path)
    if actual_digest != MKSH_SHA256:
        raise RuntimeError(
            f"mksh archive SHA256 mismatch: expected {MKSH_SHA256}, got {actual_digest}"
        )


def validated_member_path(member: tarfile.TarInfo) -> Path:
    archive_path = PurePosixPath(member.name)
    if archive_path.is_absolute() or ".." in archive_path.parts:
        raise RuntimeError(f"unsafe mksh archive path: {member.name}")
    if not archive_path.parts or archive_path.parts[0] != MKSH_ARCHIVE_ROOT:
        raise RuntimeError(f"unexpected mksh archive root: {member.name}")
    if member.issym() or member.islnk() or member.isdev() or member.isfifo():
        raise RuntimeError(f"unsupported mksh archive member type: {member.name}")
    if not member.isdir() and not member.isfile():
        raise RuntimeError(f"unknown mksh archive member type: {member.name}")
    relative_parts = archive_path.parts[1:]
    return Path(*relative_parts) if relative_parts else Path()


def extract_archive(archive_path: Path, source_directory: Path) -> None:
    if source_directory.exists() and any(source_directory.iterdir()):
        metadata_path = source_directory / SOURCE_METADATA_NAME
        if metadata_path.is_file():
            metadata = json.loads(metadata_path.read_text(encoding="ascii"))
            if metadata.get("sha256") == MKSH_SHA256:
                return
        raise RuntimeError(
            f"refusing to overwrite nonempty source directory: {source_directory}"
        )

    source_directory.parent.mkdir(parents=True, exist_ok=True)
    temporary_directory = Path(
        tempfile.mkdtemp(
            prefix=f".{source_directory.name}.", dir=source_directory.parent
        )
    )
    try:
        with tarfile.open(archive_path, mode="r:gz") as archive:
            members = archive.getmembers()
            validated_paths = [validated_member_path(member) for member in members]
            for member, relative_path in zip(members, validated_paths, strict=True):
                if not relative_path.parts:
                    continue
                destination_path = temporary_directory / relative_path
                if member.isdir():
                    destination_path.mkdir(parents=True, exist_ok=True)
                    continue
                destination_path.parent.mkdir(parents=True, exist_ok=True)
                source_file = archive.extractfile(member)
                if source_file is None:
                    raise RuntimeError(
                        f"could not read mksh archive member: {member.name}"
                    )
                with source_file, destination_path.open("wb") as destination_file:
                    shutil.copyfileobj(source_file, destination_file)
                os.chmod(destination_path, member.mode & 0o777)

        metadata = {
            "archive": MKSH_ARCHIVE_NAME,
            "sha256": MKSH_SHA256,
            "url": MKSH_SOURCE_URL,
            "version": MKSH_VERSION,
        }
        (temporary_directory / SOURCE_METADATA_NAME).write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="ascii"
        )
        if source_directory.exists():
            source_directory.rmdir()
        os.replace(temporary_directory, source_directory)
    except BaseException:
        shutil.rmtree(temporary_directory, ignore_errors=True)
        raise


def acquire_archive(archive_path: Path) -> None:
    if archive_path.is_file():
        verify_archive(archive_path)
        return
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = archive_path.with_name(archive_path.name + ".part")
    request = urllib.request.Request(
        MKSH_SOURCE_URL,
        headers={"User-Agent": "XINIM reproducible source acquisition"},
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            with temporary_path.open("wb") as archive_file:
                shutil.copyfileobj(response, archive_file)
        verify_archive(temporary_path)
        os.replace(temporary_path, archive_path)
    finally:
        temporary_path.unlink(missing_ok=True)


def add_tar_file(archive: tarfile.TarFile, name: str, contents: bytes) -> None:
    member = tarfile.TarInfo(name)
    member.size = len(contents)
    member.mode = 0o644
    archive.addfile(member, io.BytesIO(contents))


def self_test() -> None:
    with tempfile.TemporaryDirectory(prefix="xinim-mksh-acquire-test-") as temporary:
        temporary_directory = Path(temporary)
        valid_archive = temporary_directory / "valid.tgz"
        with tarfile.open(valid_archive, "w:gz") as archive:
            add_tar_file(archive, "mksh/Build.sh", b"#!/bin/sh\n")
        with tarfile.open(valid_archive, "r:gz") as archive:
            for member in archive.getmembers():
                validated_member_path(member)

        unsafe_archive = temporary_directory / "unsafe.tgz"
        with tarfile.open(unsafe_archive, "w:gz") as archive:
            add_tar_file(archive, "mksh/../escape", b"unsafe\n")
        with tarfile.open(unsafe_archive, "r:gz") as archive:
            try:
                validated_member_path(archive.getmembers()[0])
            except RuntimeError:
                pass
            else:
                raise RuntimeError("archive traversal self-test was not rejected")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive")
    parser.add_argument("--source-dir")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()

    if arguments.self_test:
        self_test()
        return 0
    if arguments.source_dir is None:
        parser.error("--source-dir is required unless --self-test is used")

    source_directory = Path(arguments.source_dir).resolve()
    if arguments.archive is None:
        archive_path = source_directory.parent / "downloads" / MKSH_ARCHIVE_NAME
    else:
        archive_path = Path(arguments.archive).resolve()
    acquire_archive(archive_path)
    extract_archive(archive_path, source_directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
