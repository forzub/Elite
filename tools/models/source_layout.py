"""The source layout of one model (docs/MESH_EDITOR_SPEC.md 18; author 2026-11-15: "в папке источника сделаем папку с
моделью … папку со скорлупой … и дальше - папки с поврежденными модулями"; "редактор читает всю папку и подхватывает то,
у чего есть паспорт"):

  <category>/<id>/model.json                     the passport (schema elite.model.v1); no passport = not a model
  <category>/<id>/intact/assembly/LOD<N>/        the whole model's assembly (generator / Blender)
  <category>/<id>/intact/assembly/hit_volumes.json, rig.json
  <category>/<id>/intact/shell/LOD<N>/           its shell (the mesh editor builds it, or a hand-made one)
  <category>/<id>/damaged/<variant>/assembly/…   a damaged variant: the same layout
  <category>/<id>/damaged/<variant>/shell/…

Used by the generators (tools/models, tools/ships) and mirrored in C++ by the mesh editor's catalog.
"""
import json
import os

SCHEMA = "elite.model.v1"


def variant_dir(model_dir, variant="intact"):
    return os.path.join(model_dir, "intact") if variant == "intact" else os.path.join(model_dir, "damaged", variant)


def assembly_dir(model_dir, variant="intact"):
    return os.path.join(variant_dir(model_dir, variant), "assembly")


def shell_dir(model_dir, variant="intact"):
    return os.path.join(variant_dir(model_dir, variant), "shell")


def lod_dir(model_dir, lod, variant="intact"):
    return os.path.join(assembly_dir(model_dir, variant), f"LOD{lod}")


def passport(model_dir, model_id, title=None, kind="module", description=None, write=True):
    """the model's passport: read it, or (write=True) create a minimal one. An existing passport is never rewritten."""
    path = os.path.join(model_dir, "model.json")
    if os.path.exists(path):
        return json.load(open(path, encoding="utf-8"))
    p = {"schema": SCHEMA, "id": model_id, "title": title or model_id, "kind": kind,
         "variants": [{"id": "intact", "title": "Целая", "inGame": "shell"}]}
    if description:
        p["description"] = description
    if write:
        os.makedirs(model_dir, exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(json.dumps(p, ensure_ascii=False, indent=1) + "\n")
    return p
