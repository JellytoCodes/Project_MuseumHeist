import json
import os

import unreal


DATA_TABLE_PATH = "/Game/Data/DataTable/DT_MapPresentation"
SOURCE_PATH = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "ProjectResources", "DataTableImports", "DT_MapPresentation.json")
)
EXPECTED_BOUNDS = {
    "M01": ((-7200.0, -5200.0), (7200.0, 5200.0)),
    "M02": ((-6400.0, -5600.0), (6400.0, 5600.0)),
    "M03": ((-8000.0, -4400.0), (8000.0, 4400.0)),
}


data_table = unreal.load_asset(DATA_TABLE_PATH)
if data_table is None:
    raise RuntimeError("Map presentation DataTable failed to load: " + DATA_TABLE_PATH)

if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(data_table, SOURCE_PATH):
    raise RuntimeError("Map presentation JSON import failed: " + SOURCE_PATH)

if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, False):
    raise RuntimeError("Map presentation DataTable save failed")

exported_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(data_table))
rows_by_name = {str(row.get("Name")): row for row in exported_rows}
failures = []
for row_name, bounds in EXPECTED_BOUNDS.items():
    row = rows_by_name.get(row_name)
    if row is None:
        failures.append("missing row " + row_name)
        continue
    actual_min = (float(row["WorldMin"]["X"]), float(row["WorldMin"]["Y"]))
    actual_max = (float(row["WorldMax"]["X"]), float(row["WorldMax"]["Y"]))
    if actual_min != bounds[0] or actual_max != bounds[1]:
        failures.append("{} bounds {}..{} != {}..{}".format(row_name, actual_min, actual_max, bounds[0], bounds[1]))
    zone_ids = {str(zone["ZoneId"]) for zone in row.get("ZoneAnchors", [])}
    if str(row.get("ContractTargetGalleryZoneId")) not in zone_ids:
        failures.append("{} target gallery zone missing from anchors".format(row_name))
    if not row.get("DefaultExitAnchors"):
        failures.append("{} has no exit anchor".format(row_name))

payload = {
    "asset": DATA_TABLE_PATH,
    "source": SOURCE_PATH.replace("\\", "/"),
    "rows": sorted(rows_by_name),
    "failures": failures,
}
if failures:
    unreal.log_error("MH_MAP_PRESENTATION_IMPORT=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
    raise RuntimeError("Map presentation DataTable verification failed")

unreal.log_warning("MH_MAP_PRESENTATION_IMPORT=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
