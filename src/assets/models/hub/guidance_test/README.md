# Hub guidance test meshes

These OBJ files are deliberately external diagnostic assets. C++ code references
them by object type; docking gates, clearances and other semantic points are not
encoded in mesh vertices. They live in `assets/data/navigation/hub_semantic_anchors.json`.

Both meshes use the project logical authoring basis: `+X` right, `+Y` up, `-Z`
forward. The through corridor is along Z so a hub attachment whose visual -Z
axis is orbital prograde exposes its docking axis along the direction of travel.

Both current test bodies use a narrow rectangular docking slot on each end face
and a through corridor along local Z.  They rotate slowly around that same Z
axis; the mesh files remain replaceable presentation assets and do not own the
semantic docking-port definitions.
