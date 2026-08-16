import math
import os

import unreal


PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_DIR = os.path.join(PROJECT_DIR, "SourceArt", "ObjectAssembly", "Meshes")
SCULPTURE_FOLDER = "/Game/Assets/Art/ObjectAssembly/Sculpture"
CERAMIC_FOLDER = "/Game/Assets/Art/ObjectAssembly/Ceramic"


def add_vertex(vertices, x, y, z):
    vertices.append((float(x), float(y), float(z)))
    return len(vertices)


def add_box(vertices, faces, x_min, x_max, y_min, y_max, z_min, z_max):
    first = len(vertices) + 1
    vertices.extend(
        [
            (x_min, y_min, z_min),
            (x_max, y_min, z_min),
            (x_max, y_max, z_min),
            (x_min, y_max, z_min),
            (x_min, y_min, z_max),
            (x_max, y_min, z_max),
            (x_max, y_max, z_max),
            (x_min, y_max, z_max),
        ]
    )
    a, b, c, d, e, f, g, h = range(first, first + 8)
    faces.extend(
        [
            (a, d, c, b),
            (e, f, g, h),
            (a, b, f, e),
            (b, c, g, f),
            (c, d, h, g),
            (d, a, e, h),
        ]
    )


def add_triangular_prism(vertices, faces, x_min, x_max, y_min, y_max, z_min, z_max):
    first = len(vertices) + 1
    vertices.extend(
        [
            (x_min, y_min, z_min),
            (x_min, y_max, z_min),
            (x_min, 0.0, z_max),
            (x_max, y_min, z_min),
            (x_max, y_max, z_min),
            (x_max, 0.0, z_max),
        ]
    )
    a, b, c, d, e, f = range(first, first + 6)
    faces.extend([(a, c, b), (d, e, f), (a, d, f, c), (b, c, f, e), (a, b, e, d)])


def add_octahedron(vertices, faces, radius, height):
    bottom = add_vertex(vertices, 0, 0, 0)
    east = add_vertex(vertices, radius, 0, height * 0.5)
    north = add_vertex(vertices, 0, radius, height * 0.5)
    west = add_vertex(vertices, -radius, 0, height * 0.5)
    south = add_vertex(vertices, 0, -radius, height * 0.5)
    top = add_vertex(vertices, 0, 0, height)
    faces.extend(
        [
            (bottom, north, east),
            (bottom, west, north),
            (bottom, south, west),
            (bottom, east, south),
            (top, east, north),
            (top, north, west),
            (top, west, south),
            (top, south, east),
        ]
    )


def add_lathe(vertices, faces, rings, segments=12, cap_bottom=True, cap_top=True):
    ring_indices = []
    for z, radius in rings:
        indices = []
        for segment in range(segments):
            angle = 2.0 * math.pi * segment / segments
            indices.append(add_vertex(vertices, radius * math.cos(angle), radius * math.sin(angle), z))
        ring_indices.append(indices)

    for ring_index in range(len(ring_indices) - 1):
        lower = ring_indices[ring_index]
        upper = ring_indices[ring_index + 1]
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((lower[segment], lower[next_segment], upper[next_segment], upper[segment]))

    if cap_bottom:
        center = add_vertex(vertices, 0, 0, rings[0][0])
        lower = ring_indices[0]
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((center, lower[next_segment], lower[segment]))

    if cap_top:
        center = add_vertex(vertices, 0, 0, rings[-1][0])
        upper = ring_indices[-1]
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((center, upper[segment], upper[next_segment]))


def sculpture_core():
    vertices, faces = [], []
    add_box(vertices, faces, -18, 18, -22, 22, 10, 68)
    add_box(vertices, faces, -14, 14, -34, 34, 62, 84)
    add_box(vertices, faces, -7, 7, -7, 7, 84, 105)
    return vertices, faces


def sculpture_head():
    vertices, faces = [], []
    add_box(vertices, faces, -10, 10, -10, 10, 0, 24)
    add_triangular_prism(vertices, faces, -10, 10, -10, 10, 24, 32)
    return vertices, faces


def sculpture_arm_left():
    vertices, faces = [], []
    # OBJ import mirrors the source Y axis. Author the left part on +Y so the
    # imported mesh extends toward -Y from its connection pivot.
    add_box(vertices, faces, -7, 7, 0, 34, -24, 6)
    add_box(vertices, faces, -8, 8, 28, 42, -36, -20)
    return vertices, faces


def sculpture_arm_right():
    vertices, faces = [], []
    add_box(vertices, faces, -7, 7, -34, 0, -24, 6)
    add_box(vertices, faces, -8, 8, -42, -28, -36, -20)
    return vertices, faces


def sculpture_crest():
    vertices, faces = [], []
    add_triangular_prism(vertices, faces, -5, 5, -12, 12, 0, 30)
    return vertices, faces


def sculpture_pedestal():
    vertices, faces = [], []
    add_box(vertices, faces, -18, 18, -18, 18, -7, 2)
    add_box(vertices, faces, -26, 26, -26, 26, -16, -7)
    return vertices, faces


def sculpture_decoy():
    vertices, faces = [], []
    add_octahedron(vertices, faces, 12, 32)
    return vertices, faces


def ceramic_core():
    vertices, faces = [], []
    add_lathe(vertices, faces, [(5, 14), (18, 27), (52, 34), (82, 27), (100, 18)], 12, True, False)
    return vertices, faces


def ceramic_lid():
    vertices, faces = [], []
    add_lathe(vertices, faces, [(0, 19), (8, 21), (13, 15), (18, 7)], 12, True, True)
    return vertices, faces


def ceramic_handle_left():
    vertices, faces = [], []
    add_box(vertices, faces, -5, 5, 0, 17, 7, 14)
    add_box(vertices, faces, -5, 5, 0, 17, -14, -7)
    add_box(vertices, faces, -5, 5, 15, 22, -12, 12)
    return vertices, faces


def ceramic_handle_right():
    vertices, faces = [], []
    add_box(vertices, faces, -5, 5, -17, 0, 7, 14)
    add_box(vertices, faces, -5, 5, -17, 0, -14, -7)
    add_box(vertices, faces, -5, 5, -22, -15, -12, 12)
    return vertices, faces


def ceramic_spout():
    vertices, faces = [], []
    add_box(vertices, faces, 0, 24, -8, 8, -5, 8)
    add_box(vertices, faces, 22, 38, -6, 6, 4, 14)
    return vertices, faces


def ceramic_foot():
    vertices, faces = [], []
    add_lathe(vertices, faces, [(-14, 22), (-6, 18), (0, 14)], 12, True, True)
    return vertices, faces


def ceramic_decoy():
    vertices, faces = [], []
    add_triangular_prism(vertices, faces, -4, 4, -14, 14, 0, 26)
    return vertices, faces


MESH_SPECS = [
    (
        SCULPTURE_FOLDER,
        "SM_SculptureGallery_Core",
        sculpture_core,
        [
            ("Head", (0, 0, 105)),
            ("Shoulder_L", (0, -34, 76)),
            ("Shoulder_R", (0, 34, 76)),
            ("Crest", (-14, 0, 88)),
            ("Pedestal", (0, 0, 10)),
        ],
    ),
    (SCULPTURE_FOLDER, "SM_SculptureGallery_Head", sculpture_head, []),
    (SCULPTURE_FOLDER, "SM_SculptureGallery_ArmLeft", sculpture_arm_left, []),
    (SCULPTURE_FOLDER, "SM_SculptureGallery_ArmRight", sculpture_arm_right, []),
    (SCULPTURE_FOLDER, "SM_SculptureGallery_Crest", sculpture_crest, []),
    (SCULPTURE_FOLDER, "SM_SculptureGallery_Pedestal", sculpture_pedestal, []),
    (SCULPTURE_FOLDER, "SM_SculptureGallery_Decoy", sculpture_decoy, []),
    (
        CERAMIC_FOLDER,
        "SM_CeramicGallery_Core",
        ceramic_core,
        [
            ("Lid", (0, 0, 100)),
            ("Handle_L", (0, -30, 60)),
            ("Handle_R", (0, 30, 60)),
            ("Spout", (28, 0, 72)),
            ("Foot", (0, 0, 5)),
        ],
    ),
    (CERAMIC_FOLDER, "SM_CeramicGallery_Lid", ceramic_lid, []),
    (CERAMIC_FOLDER, "SM_CeramicGallery_HandleLeft", ceramic_handle_left, []),
    (CERAMIC_FOLDER, "SM_CeramicGallery_HandleRight", ceramic_handle_right, []),
    (CERAMIC_FOLDER, "SM_CeramicGallery_Spout", ceramic_spout, []),
    (CERAMIC_FOLDER, "SM_CeramicGallery_Foot", ceramic_foot, []),
    (CERAMIC_FOLDER, "SM_CeramicGallery_Decoy", ceramic_decoy, []),
]

FORCE_REIMPORT_NAMES = {asset_name for _folder, asset_name, _builder, _sockets in MESH_SPECS}


def write_obj(asset_name, vertices, faces):
    os.makedirs(SOURCE_DIR, exist_ok=True)
    file_path = os.path.join(SOURCE_DIR, f"{asset_name}.obj")
    lines = [f"o {asset_name}"]
    lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices)
    # UE 5.8's Interchange OBJ translator expects a valid UV index for every
    # face vertex. These generated kit meshes do not need authored UV islands,
    # but explicit deterministic UV entries avoid an importer ensure and make
    # reimport logs clean on a fresh checkout.
    lines.extend("vt 0.000000 0.000000" for _vertex in vertices)
    lines.extend("f " + " ".join(f"{index}/{index}" for index in face) for face in faces)
    with open(file_path, "w", encoding="utf-8", newline="\n") as obj_file:
        obj_file.write("\n".join(lines) + "\n")
    return file_path


def make_import_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_mesh = True
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    import_data = options.static_mesh_import_data
    import_data.combine_meshes = True
    import_data.auto_generate_collision = False
    import_data.generate_lightmap_u_vs = True
    import_data.convert_scene = False
    import_data.convert_scene_unit = False
    return options


def import_mesh(asset_tools, folder, asset_name, source_file):
    package_path = f"{folder}/{asset_name}"
    asset_exists = unreal.EditorAssetLibrary.does_asset_exist(package_path)
    if asset_exists and asset_name not in FORCE_REIMPORT_NAMES:
        existing_mesh = unreal.load_asset(package_path)
        if not isinstance(existing_mesh, unreal.StaticMesh):
            raise RuntimeError(f"Existing asset is not a Static Mesh: {package_path}")
        return existing_mesh

    task = unreal.AssetImportTask()
    task.automated = True
    task.destination_path = folder
    task.destination_name = asset_name
    task.filename = source_file
    task.options = make_import_options()
    task.replace_existing = asset_exists
    task.save = True
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(package_path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"Static Mesh import failed: {package_path}; imported={list(task.imported_object_paths)}")
    return mesh


def add_sockets(mesh, sockets):
    mesh.modify()
    for socket_name, location in sockets:
        if mesh.find_socket(socket_name):
            continue
        socket = unreal.StaticMeshSocket(outer=mesh)
        socket.set_editor_property("socket_name", socket_name)
        socket.set_editor_property("relative_location", unreal.Vector(*location))
        socket.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 0))
        socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
        mesh.add_socket(socket)


def import_data_tables():
    tables = [
        (
            "/Game/Data/DataTable/DT_ObjectAssemblyPart",
            os.path.join(PROJECT_DIR, "DataTableImports", "DT_ObjectAssemblyPartRow.json"),
        ),
        (
            "/Game/Data/DataTable/DT_ObjectAssemblyTemplate",
            os.path.join(PROJECT_DIR, "DataTableImports", "DT_ObjectAssemblyTemplateRow.json"),
        ),
        (
            "/Game/Data/DataTable/DT_ArtifactData",
            os.path.join(PROJECT_DIR, "DataTableImports", "DT_ArtifactDataRow.json"),
        ),
    ]
    imported = []
    for asset_path, json_path in tables:
        data_table = unreal.load_asset(asset_path)
        if not isinstance(data_table, unreal.DataTable):
            raise RuntimeError(f"Missing DataTable: {asset_path}")
        if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(data_table, json_path):
            raise RuntimeError(f"DataTable JSON import failed: {asset_path} <- {json_path}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, only_if_is_dirty=False):
            raise RuntimeError(f"DataTable save failed: {asset_path}")
        imported.append(asset_path)
    return imported


def validate_assets(asset_paths):
    failures = []
    for asset_path in asset_paths:
        mesh = unreal.load_asset(asset_path)
        if not isinstance(mesh, unreal.StaticMesh):
            failures.append(f"{asset_path}:MissingStaticMesh")
            continue
        if mesh.get_num_triangles(0) <= 0:
            failures.append(f"{asset_path}:NoTriangles")
    for folder, asset_name, _builder, sockets in MESH_SPECS:
        mesh = unreal.load_asset(f"{folder}/{asset_name}")
        for socket_name, _location in sockets:
            if not mesh.find_socket(socket_name):
                failures.append(f"{asset_name}:MissingSocket:{socket_name}")
    if failures:
        raise RuntimeError("Asset validation failed: " + ", ".join(failures))


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.EditorAssetLibrary.make_directory(SCULPTURE_FOLDER)
    unreal.EditorAssetLibrary.make_directory(CERAMIC_FOLDER)

    created_mesh_paths = []
    for folder, asset_name, builder, sockets in MESH_SPECS:
        vertices, faces = builder()
        source_file = write_obj(asset_name, vertices, faces)
        mesh = import_mesh(asset_tools, folder, asset_name, source_file)
        add_sockets(mesh, sockets)
        mesh.set_material(0, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
        created_mesh_paths.append(f"{folder}/{asset_name}")

    imported_tables = import_data_tables()
    validate_assets(created_mesh_paths)
    unreal.log(
        "Object Assembly content rebuild PASS: "
        f"Meshes={len(created_mesh_paths)} Sockets=10 DataTables={len(imported_tables)}"
    )
    print(
        {
            "result": "PASS",
            "meshes": created_mesh_paths,
            "sockets": 10,
            "data_tables": imported_tables,
        }
    )


main()
