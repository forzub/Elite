#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "tools/model_asset_editor/ModelAssetEditorSession.h").read_text(encoding="utf-8", errors="replace")
CPP = (ROOT / "tools/model_asset_editor/ModelAssetEditorSession.cpp").read_text(encoding="utf-8", errors="replace")
UI = (ROOT / "src/assets/webui/model_asset_editor.html").read_text(encoding="utf-8", errors="replace")


def require(body: str, *tokens: str) -> None:
    for token in tokens:
        if token not in body:
            raise AssertionError(f"persistent instance-family contract missing {token!r}")


require(
    HEADER,
    'std::string representation = "geometry";',
    "std::string instanceOfGeometryId;",
    "std::vector<std::string> instanceRenderNodeIds;",
    "bool finalizeGeometryInstanceAlias(",
)

require(
    CPP,
    '{"representation", record.representation}',
    '{"instanceOfGeometryId", record.instanceOfGeometryId}',
    '{"instanceRenderNodeIds", record.instanceRenderNodeIds}',
    'record.representation = item.value("representation", std::string("geometry"));',
    'oldRecord.representation != "instance"',
    'record.representation = "instance";',
    'alias.instanceOfGeometryId == sourceGeometryId',
    'alias.instanceOfGeometryId = canonicalGeometryId;',
    "claimedByOlderAliases",
    "lod.geometries.erase(",
    "remapRenderGeometryAfterErase(lod, sourceGeometryIndex);",
    'existingRecord.representation == "instance"',
    'row["kind"] = "instance_source_changed";',
    "SOURCE deletion refused: this SOURCE identity is a persistent instance link",
    "familyNodeIndices",
    "target geometry family has no render nodes",
    "for (const auto familyNodeIndex : familyNodeIndices)",
    "for (const auto& family : pending)",
    "for (const auto nodeIndex : family.nodeIndices)",
    "cannot finalize persistent instance family link",
)

# Consolidation must be transactional. A failure may not leave half of a family
# pointing at the new canonical geometry.
require(
    CPP,
    "const RenderLod lodBefore = lod;",
    "m_meshSourceRecords[lodIndex] = sourceRecordsBefore;",
    "m_meshPreparationRecords[lodIndex] = preparationBefore;",
    "m_meshOrientationOverrides[lodIndex] = orientationBefore;",
    "m_geometryTopologyClasses[lodIndex] = topologyBefore;",
    "m_rawMeshSnapshots[lodIndex] = rawBefore;",
)

require(
    UI,
    "function instanceAliasRecordsForLod(",
    "function instanceAliasRecordForRenderNode(",
    "function canonicalGeometryForAlias(",
    "function effectiveGeometry(",
    "function instanceAliasProxy(",
    "function logicalGeometryRows(",
    "INSTANCE LINK",
    "INSTANCE →",
    "data-preflight-logical-id",
    "INSTANCE LINK · properties from canonical mesh",
    "data-surface-logical-id",
    "surfaceDisplayRows=logicalGeometryRows",
    "logicalGeometryRows(lod,state.activeLod).forEach",
    "SOURCE CHANGED · INSTANCE LINK KEPT",
)

# Shared property editing is geometry-authoritative. Alias UI selection resolves
# to effective/canonical geometry instead of creating an alias-local property set.
require(
    UI,
    "const g=effectiveGeometry(display,lod)",
    "selected.add(String(g.id))",
    "data-preflight-orientation=",
    "instanceAliasRecordForRenderNode(rn?.id,state.activeLod)",
)

print("[PASS] model asset editor persistent instance-family / canonical mesh authority")
