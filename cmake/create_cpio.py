#!/usr/bin/env python3
"""
Generate a CPIO newc (SVR4 portable without CRC, magic 070701) archive.

This script packages user-space executables and files into a deterministic
CPIO newc archive suitable for use as an initramfs.
"""

from __future__ import annotations

import argparse
import os
import stat
import sys


def _pad4(length: int) -> int:
    """Calculate the padding bytes required to align to a 4-byte boundary."""
    return (4 - (length % 4)) % 4


def _build_header(
    namesize: int,
    filesize: int,
    mode: int,
    ino: int = 1,
    nlink: int = 1,
    mtime: int = 0,
) -> bytes:
    """
    Format a 110-byte CPIO newc header in ASCII hexadecimal.
    Fields (each 8 hex digits, uppercase, except magic):
      c_magic:    070701 (6 bytes)
      c_ino:      inode number
      c_mode:     file mode and type (e.g. 0100755)
      c_uid:      user ID (0)
      c_gid:      group ID (0)
      c_nlink:    number of links
      c_mtime:    modification timestamp
      c_filesize: file content size in bytes
      c_maj:      major device number (0)
      c_min:      minor device number (0)
      c_rmaj:     device major for special files (0)
      c_rmin:     device minor for special files (0)
      c_namesize: length of filename including terminating NUL
      c_check:    CRC checksum (0 for 070701)
    """
    return (
        f"070701"
        f"{ino:08X}"
        f"{mode:08X}"
        f"{0:08X}"
        f"{0:08X}"
        f"{nlink:08X}"
        f"{mtime:08X}"
        f"{filesize:08X}"
        f"{0:08X}"
        f"{0:08X}"
        f"{0:08X}"
        f"{0:08X}"
        f"{namesize:08X}"
        f"{0:08X}"
    ).encode("ascii")


def _make_entry(
    archive_name: str,
    data: bytes,
    mode: int,
    ino: int,
    mtime: int = 0,
) -> bytes:
    """Construct an entry with header, padded name, and padded data."""
    name_bytes = archive_name.encode("utf-8") + b"\0"
    header = _build_header(
        namesize=len(name_bytes),
        filesize=len(data),
        mode=mode,
        ino=ino,
        nlink=1 if not stat.S_ISDIR(mode) else 2,
        mtime=mtime,
    )
    pad_header_name = b"\0" * _pad4(len(header) + len(name_bytes))
    pad_data = b"\0" * _pad4(len(data))
    return header + name_bytes + pad_header_name + data + pad_data


def _make_trailer() -> bytes:
    """Construct the standard TRAILER!!! record marking end of archive."""
    trailer_name = b"TRAILER!!!\0"
    header = _build_header(
        namesize=len(trailer_name),
        filesize=0,
        mode=0,
        ino=0,
        nlink=1,
        mtime=0,
    )
    pad = b"\0" * _pad4(len(header) + len(trailer_name))
    return header + trailer_name + pad


def parse_entry_spec(spec: str) -> Tuple[str, str]:
    """Parse a source:archive_path specification."""
    if ":" in spec:
        source, archive_path = spec.split(":", 1)
    else:
        source = spec
        archive_path = os.path.basename(spec)
    # Strip any leading slashes or relative prefixes for safe archive paths
    archive_path = archive_path.lstrip("/").lstrip("./")
    return source, archive_path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create a CPIO newc archive from source files."
    )
    parser.add_argument(
        "-o",
        "--output",
        required=True,
        help="Path to the output .cpio archive file",
    )
    parser.add_argument(
        "-e",
        "--entry",
        action="append",
        default=[],
        help="Entry to include, in format <source_path>[:<archive_path>]",
    )
    parser.add_argument(
        "positional_entries",
        nargs="*",
        help="Additional entries in format <source_path>[:<archive_path>]",
    )
    args = parser.parse_args()

    all_specs = args.entry + args.positional_entries
    if not all_specs:
        sys.stderr.write("Error: no entries specified to include in archive.\n")
        return 1

    # Check for SOURCE_DATE_EPOCH for reproducible builds
    mtime = 0
    if "SOURCE_DATE_EPOCH" in os.environ:
        try:
            mtime = int(os.environ["SOURCE_DATE_EPOCH"])
        except ValueError:
            mtime = 0

    entries_to_pack: list[tuple[str, str]] = []
    for spec in all_specs:
        src, dst = parse_entry_spec(spec)
        if not os.path.isfile(src):
            sys.stderr.write(f"Error: source file does not exist: {src}\n")
            return 1
        entries_to_pack.append((src, dst))

    # Keep track of created directories to ensure parent directories exist
    created_dirs = set()
    archive_bytes = bytearray()
    next_ino = 1

    for src, dst in entries_to_pack:
        # Check parent directories
        parts = dst.split("/")
        if len(parts) > 1:
            for i in range(1, len(parts)):
                dir_path = "/".join(parts[:i])
                if dir_path and dir_path not in created_dirs:
                    # Directory mode: S_IFDIR | 0755
                    dir_mode = stat.S_IFDIR | 0o755
                    archive_bytes.extend(
                        _make_entry(dir_path, b"", dir_mode, ino=next_ino, mtime=mtime)
                    )
                    created_dirs.add(dir_path)
                    next_ino += 1

        with open(src, "rb") as f:
            data = f.read()

        # Regular executable file (0100755) for ELF binaries or executable files
        file_stat = os.stat(src)
        if (file_stat.st_mode & 0o111) != 0 or data.startswith(b"\x7fELF"):
            file_mode = stat.S_IFREG | 0o755
        else:
            file_mode = stat.S_IFREG | 0o644

        archive_bytes.extend(
            _make_entry(dst, data, file_mode, ino=next_ino, mtime=mtime)
        )
        next_ino += 1

    # Append trailer
    archive_bytes.extend(_make_trailer())

    # Pad to 512-byte block boundary
    block_pad = (512 - (len(archive_bytes) % 512)) % 512
    archive_bytes.extend(b"\0" * block_pad)

    output_path = args.output
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    # Write atomically
    temp_output = f"{output_path}.tmp.{os.getpid()}"
    try:
        with open(temp_output, "wb") as f:
            f.write(archive_bytes)
        os.replace(temp_output, output_path)
    except Exception as err:
        if os.path.exists(temp_output):
            os.remove(temp_output)
        sys.stderr.write(f"Error writing archive {output_path}: {err}\n")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
