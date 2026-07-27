#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Build and validate a deterministic catalog of MaterialX NodeDefs."""

__all__ = (
    "build_catalog",
    "catalog_as_json",
    "main",
)

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Iterable

EXPECTED_NODEDEF_COUNT = 802

SOURCE_DIR = Path(__file__).resolve().parents[2]
DEFAULT_LIBRARY_ROOT_CANDIDATES = (
    SOURCE_DIR / "lib" / "windows_x64" / "MaterialX" / "python" / "Release" / "MaterialX" / "libraries",
    SOURCE_DIR / "lib" / "linux_x64" / "MaterialX" / "python" / "Release" / "MaterialX" / "libraries",
    SOURCE_DIR / "lib" / "darwin_arm64" / "MaterialX" / "python" / "Release" / "MaterialX" / "libraries",
    SOURCE_DIR / "lib" / "darwin_x64" / "MaterialX" / "python" / "Release" / "MaterialX" / "libraries",
    SOURCE_DIR / "build_files" / "build_environment" / "build" / "materialx" / "src" /
    "external_materialx" / "libraries",
)


def _normalized_library_root(library_root: Path) -> Path:
    library_root = library_root.resolve()
    if (library_root / "libraries").is_dir():
        return library_root / "libraries"
    return library_root


def _default_library_root() -> Path:
    for candidate in DEFAULT_LIBRARY_ROOT_CANDIDATES:
        if candidate.is_dir():
            return candidate
    raise FileNotFoundError(
        "Unable to find bundled MaterialX libraries. Pass --library-root pointing at "
        "the MaterialX source root or its libraries directory."
    )


def _xml_children_by_local_name(element: ET.Element, name: str) -> Iterable[ET.Element]:
    for child in element:
        if child.tag.rpartition("}")[2] == name:
            yield child


def _relative_source(path: Path, library_root: Path) -> str:
    return Path("libraries", path.relative_to(library_root)).as_posix()


def _source_preference(source: str) -> tuple[int, str]:
    parts = Path(source).parts
    generated_targets = {"genglsl", "genmdl", "genmsl", "genosl"}
    return (sum(part in generated_targets for part in parts), source)


def _iter_nodedef_files(library_root: Path) -> Iterable[Path]:
    yield from sorted(library_root.rglob("*.mtlx"))


def _entry_from_nodedef(nodedef: ET.Element, source: str) -> dict[str, object] | None:
    nodedef_id = nodedef.get("name")
    if not nodedef_id:
        return None

    types = set()
    nodedef_type = nodedef.get("type")
    if nodedef_type:
        types.add(nodedef_type)
    for child in nodedef:
        if child.tag.rpartition("}")[2] in {"input", "output", "token"}:
            child_type = child.get("type")
            if child_type:
                types.add(child_type)

    return {
        "id": nodedef_id,
        "category": nodedef.get("nodegroup") or nodedef.get("node") or "",
        "types": sorted(types),
        "source": source,
    }


def build_catalog(library_root: os.PathLike[str] | str | None = None) -> list[dict[str, object]]:
    """Return unique MaterialX NodeDef catalog entries sorted by NodeDef id."""
    root = _normalized_library_root(Path(library_root)) if library_root is not None else _default_library_root()
    if not root.is_dir():
        raise FileNotFoundError(f"MaterialX library root not found: {root}")

    entries_by_id: dict[str, dict[str, object]] = {}
    for path in _iter_nodedef_files(root):
        try:
            document = ET.parse(path)
        except ET.ParseError:
            continue

        source = _relative_source(path, root)
        for nodedef in _xml_children_by_local_name(document.getroot(), "nodedef"):
            entry = _entry_from_nodedef(nodedef, source)
            if entry is None:
                continue
            nodedef_id = str(entry["id"])
            existing = entries_by_id.get(nodedef_id)
            if existing is None or _source_preference(source) < _source_preference(str(existing["source"])):
                entries_by_id[nodedef_id] = entry

    return [entries_by_id[nodedef_id] for nodedef_id in sorted(entries_by_id)]


def catalog_as_json(catalog: list[dict[str, object]]) -> str:
    return json.dumps(catalog, indent=2, sort_keys=True) + "\n"


def _validate_catalog(catalog: list[dict[str, object]], expected_count: int) -> None:
    ids = [str(entry["id"]) for entry in catalog]
    if len(catalog) != expected_count:
        raise ValueError(f"Expected {expected_count} unique NodeDefs, found {len(catalog)}")
    if len(ids) != len(set(ids)):
        raise ValueError("Catalog contains duplicate NodeDef ids")
    if ids != sorted(ids):
        raise ValueError("Catalog entries are not sorted by id")
    for entry in catalog:
        if set(entry.keys()) != {"id", "category", "types", "source"}:
            raise ValueError(f"Unexpected keys in entry {entry.get('id')!r}: {sorted(entry.keys())}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library-root", type=Path, default=None)
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument("--validate", action="store_true")
    parser.add_argument("--expected-count", type=int, default=EXPECTED_NODEDEF_COUNT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    try:
        catalog = build_catalog(args.library_root)
        if args.validate:
            _validate_catalog(catalog, args.expected_count)
    except (FileNotFoundError, ValueError) as ex:
        print(f"materialx_catalog.py: error: {ex}", file=sys.stderr)
        return 1

    output = catalog_as_json(catalog)
    if args.output is None:
        sys.stdout.write(output)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")

    if args.validate:
        print(f"Validated {len(catalog)} unique MaterialX NodeDefs", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
