#!/usr/bin/env python3
"""Model Asset Editor localization completeness contract."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "src/assets/localization/ui/tools/model_asset_editor.json"
data = json.loads(PATH.read_text(encoding="utf-8"))

expected_locales = ["en", "ru", "zh-Hans", "es", "ja"]
locales = data.get("locale_order")
if locales != expected_locales:
    raise AssertionError(f"unexpected locale_order: {locales!r}")

strings = data.get("strings")
if not isinstance(strings, dict) or not strings:
    raise AssertionError("localization strings map is missing or empty")

missing = []
for key, row in strings.items():
    if not isinstance(row, dict):
        missing.append((key, "<row>"))
        continue
    for locale in expected_locales:
        if not str(row.get(locale, "")).strip():
            missing.append((key, locale))
if missing:
    sample = ", ".join(f"{key}:{locale}" for key, locale in missing[:20])
    raise AssertionError(f"missing translations ({len(missing)}): {sample}")

# Exact English copies are acceptable only for intentionally shared technical /
# international tokens or words whose spelling is genuinely the same.
allowed_same_as_english = {
    ("model_editor.toolbar.sockets.title", "es"),
    ("model_editor.action.socket", "es"),
    ("model_editor.status.error", "es"),
    ("model_editor.geometry_variants.original", "es"),
    ("model_editor.source.base", "es"),
    ("model_editor.preflight.class_auto", "es"),
    ("model_editor.preflight.raw_short", "ru"),
    ("model_editor.preflight.raw_short", "zh-Hans"),
    ("model_editor.preflight.raw_short", "es"),
    ("model_editor.preflight.raw_short", "ja"),
    ("model_editor.preflight.ok", "ru"),
    ("model_editor.preflight.ok", "es"),
    ("model_editor.preflight.ok", "ja"),
    ("model_editor.preflight.auto_count", "es"),
    ("model_editor.preflight.manual_count", "es"),
    ("model_editor.surfaces.material_editor", "es"),
    ("model_editor.surfaces.material", "es"),
    ("model_editor.maintenance.issue_lods", "es"),
    ("model_editor.geometry_compare.reference_short", "es"),
}
identical = []
for key, row in strings.items():
    en = str(row.get("en", "")).strip()
    if not en:
        continue
    for locale in expected_locales[1:]:
        if str(row.get(locale, "")).strip() == en and (key, locale) not in allowed_same_as_english:
            identical.append((key, locale, en))
if identical:
    sample = ", ".join(f"{key}:{locale}={value!r}" for key, locale, value in identical[:20])
    raise AssertionError(f"unexpected English fallback copies ({len(identical)}): {sample}")

# The accepted rotation dialog must use localized strings, not English-only UI text.
required_axis_keys = {
    "model_editor.axis_rotation.title",
    "model_editor.axis_rotation.frame_title",
    "model_editor.axis_rotation.frame_legend",
    "model_editor.axis_rotation.hint",
    "model_editor.axis_rotation.positive_hint",
    "model_editor.axis_rotation.reset_pending",
    "model_editor.axis_rotation.source_no_rotation",
    "model_editor.axis_rotation.cancel",
    "model_editor.axis_rotation.pending",
    "model_editor.axis_rotation.pending_none",
    "model_editor.axis_rotation.final_orientation",
    "model_editor.axis_rotation.rotation_only",
    "model_editor.axis_rotation.apply_scope",
    "model_editor.axis_rotation.apply_button",
}
missing_axis = sorted(required_axis_keys - strings.keys())
if missing_axis:
    raise AssertionError(f"axis rotation localization keys missing: {missing_axis}")

print(f"[PASS] model asset editor localization complete: {len(strings)} keys × {len(expected_locales)} locales")
