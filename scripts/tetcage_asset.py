"""Small standard-library reader for the version-1 .tetcage asset format."""

from __future__ import annotations

import math
import pathlib
import struct
from typing import Any


MAGIC = b"TETCAGE\0"
MAX_ASSET_BYTES = 256 * 1024 * 1024
MAX_ELEMENTS = 100_000_000


class AssetFormatError(ValueError):
    """Raised when a .tetcage file is truncated or structurally invalid."""


class _Reader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.offset = 0

    def read(self, fmt: str) -> Any:
        size = struct.calcsize(fmt)
        if self.offset + size > len(self.data):
            raise AssetFormatError("asset is truncated")
        value = struct.unpack_from(fmt, self.data, self.offset)
        self.offset += size
        return value[0] if len(value) == 1 else value

    def count(self, minimum_bytes: int) -> int:
        value = self.read("<Q")
        if value > MAX_ELEMENTS or value * minimum_bytes > len(self.data) - self.offset:
            raise AssetFormatError("asset count is unsafe")
        return int(value)

    def vec2(self) -> tuple[float, float]:
        return (self.read("<d"), self.read("<d"))

    def vec3(self) -> tuple[float, float, float]:
        return (self.read("<d"), self.read("<d"), self.read("<d"))

    def vec4(self) -> tuple[float, float, float, float]:
        return (self.read("<d"), self.read("<d"), self.read("<d"), self.read("<d"))


def _finite(values: tuple[float, ...]) -> bool:
    return all(math.isfinite(value) for value in values)


def load_asset(path: str | pathlib.Path) -> dict[str, Any]:
    """Load and validate the stable streams needed by a Blender importer."""

    data = pathlib.Path(path).read_bytes()
    if len(data) > MAX_ASSET_BYTES:
        raise AssetFormatError("asset exceeds the maximum supported byte size")
    reader = _Reader(data)
    if reader.data[: len(MAGIC)] != MAGIC:
        raise AssetFormatError("asset magic is invalid")
    reader.offset = len(MAGIC)
    format_version = reader.read("<I")
    if format_version != 1:
        raise AssetFormatError(f"unsupported asset format version: {format_version}")
    tolerance_version = reader.read("<I")
    tolerance = {
        "version": tolerance_version,
        "expanded_barycentric_epsilon": reader.read("<d"),
        "feature_snap_epsilon": reader.read("<d"),
        "minimum_area_relative": reader.read("<d"),
    }

    source_vertices = []
    for _ in range(reader.count(64)):
        position = reader.vec3()
        normal = reader.vec3()
        uv = reader.vec2()
        if not (_finite(position) and _finite(normal) and _finite(uv)):
            raise AssetFormatError("source vertex contains a non-finite value")
        source_vertices.append({"position": position, "normal": normal, "uv": uv})

    source_triangles = []
    for _ in range(reader.count(20)):
        source_triangles.append(
            {
                "vertex_indices": (reader.read("<I"), reader.read("<I"), reader.read("<I")),
                "primitive_id": reader.read("<I"),
                "material": reader.read("<I"),
            }
        )

    cage_vertices = []
    for _ in range(reader.count(32)):
        position = reader.vec3()
        stable_id = reader.read("<Q")
        if not _finite(position):
            raise AssetFormatError("cage vertex contains a non-finite value")
        cage_vertices.append({"position": position, "stable_id": stable_id})

    cage_tetrahedra = []
    for _ in range(reader.count(16)):
        cage_tetrahedra.append(
            (reader.read("<I"), reader.read("<I"), reader.read("<I"), reader.read("<I"))
        )

    tet_metadata = []
    for _ in range(reader.count(58)):
        determinant = reader.read("<d")
        condition = reader.read("<d")
        minimum_edge = reader.read("<d")
        mirrored = bool(reader.read("<B"))
        near_singular = bool(reader.read("<B"))
        adjacent = tuple(reader.read("<i") for _ in range(4))
        face_owner = tuple(reader.read("<I") for _ in range(4))
        tet_metadata.append(
            {
                "determinant": determinant,
                "condition_estimate": condition,
                "minimum_edge": minimum_edge,
                "mirrored": mirrored,
                "near_singular": near_singular,
                "adjacent_tet": adjacent,
                "face_owner": face_owner,
            }
        )

    generated_vertices = []
    for _ in range(reader.count(74)):
        cage_barycentric = reader.vec4()
        source_barycentric = reader.vec3()
        feature = {"cage": reader.read("<B"), "source": reader.read("<B")}
        stable_id = reader.read("<Q")
        tet_id = reader.read("<I")
        source_primitive = reader.read("<I")
        if not (_finite(cage_barycentric) and _finite(source_barycentric)):
            raise AssetFormatError("generated vertex contains a non-finite value")
        generated_vertices.append(
            {
                "cage_barycentric": cage_barycentric,
                "source_barycentric": source_barycentric,
                "feature": feature,
                "stable_id": stable_id,
                "tet_id": tet_id,
                "source_primitive": source_primitive,
            }
        )

    micro_triangles = []
    for _ in range(reader.count(32)):
        micro_triangles.append(
            {
                "vertex_indices": (reader.read("<I"), reader.read("<I"), reader.read("<I")),
                "tet_id": reader.read("<I"),
                "owner_tet": reader.read("<I"),
                "source_primitive": reader.read("<I"),
                "material": reader.read("<I"),
                "deterministic_subtriangle": reader.read("<I"),
            }
        )

    statistics = {
        "source_triangles": reader.read("<Q"),
        "generated_triangles": reader.read("<Q"),
        "generated_vertices": reader.read("<Q"),
        "occupied_tetrahedra": reader.read("<Q"),
        "boundary_fragments": reader.read("<Q"),
        "canonical_bytes": reader.read("<Q"),
        "provenance_bytes": reader.read("<Q"),
        "triangle_expansion": reader.read("<d"),
        "vertex_expansion": reader.read("<d"),
        "worst_condition": reader.read("<d"),
    }
    if reader.offset != len(data):
        raise AssetFormatError("asset has trailing bytes")

    return {
        "format_version": format_version,
        "tolerance": tolerance,
        "source_vertices": source_vertices,
        "source_triangles": source_triangles,
        "cage_vertices": cage_vertices,
        "cage_tetrahedra": cage_tetrahedra,
        "tet_metadata": tet_metadata,
        "generated_vertices": generated_vertices,
        "micro_triangles": micro_triangles,
        "statistics": statistics,
    }


def barycentric_point(
    cage_vertices: list[dict[str, Any]], tet: tuple[int, int, int, int], barycentric: tuple[float, ...]
) -> tuple[float, float, float]:
    result = [0.0, 0.0, 0.0]
    for corner, vertex_index in enumerate(tet):
        position = cage_vertices[vertex_index]["position"]
        for axis in range(3):
            result[axis] += position[axis] * barycentric[corner]
    return tuple(result)


def micro_triangle_points(
    asset: dict[str, Any], triangle: dict[str, Any], cage_vertices: list[dict[str, Any]] | None = None
) -> tuple[tuple[float, float, float], ...]:
    vertices = cage_vertices if cage_vertices is not None else asset["cage_vertices"]
    tet = asset["cage_tetrahedra"][triangle["tet_id"]]
    return tuple(
        barycentric_point(vertices, tet, asset["generated_vertices"][index]["cage_barycentric"])
        for index in triangle["vertex_indices"]
    )


def micro_triangle_uvs(
    asset: dict[str, Any], triangle: dict[str, Any]
) -> tuple[tuple[float, float], ...]:
    """Interpolate source UVs onto one generated micro-triangle."""

    source_by_primitive = {
        source["primitive_id"]: source for source in asset["source_triangles"]
    }
    source_triangle = source_by_primitive.get(triangle["source_primitive"])
    if source_triangle is None:
        raise AssetFormatError("micro-triangle source primitive is missing")
    source_uvs = tuple(
        asset["source_vertices"][vertex_index]["uv"]
        for vertex_index in source_triangle["vertex_indices"]
    )
    result = []
    for generated_index in triangle["vertex_indices"]:
        barycentric = asset["generated_vertices"][generated_index]["source_barycentric"]
        result.append(
            (
                sum(barycentric[index] * source_uvs[index][0] for index in range(3)),
                sum(barycentric[index] * source_uvs[index][1] for index in range(3)),
            )
        )
    return tuple(result)


def micro_triangle_normals(
    asset: dict[str, Any], triangle: dict[str, Any]
) -> tuple[tuple[float, float, float], ...]:
    """Interpolate source vertex normals onto one generated micro-triangle."""

    source_by_primitive = {
        source["primitive_id"]: source for source in asset["source_triangles"]
    }
    source_triangle = source_by_primitive.get(triangle["source_primitive"])
    if source_triangle is None:
        raise AssetFormatError("micro-triangle source primitive is missing")
    source_normals = tuple(
        asset["source_vertices"][vertex_index]["normal"]
        for vertex_index in source_triangle["vertex_indices"]
    )
    result = []
    for generated_index in triangle["vertex_indices"]:
        barycentric = asset["generated_vertices"][generated_index]["source_barycentric"]
        result.append(
            tuple(
                sum(barycentric[index] * source_normals[index][axis] for index in range(3))
                for axis in range(3)
            )
        )
    return tuple(result)
