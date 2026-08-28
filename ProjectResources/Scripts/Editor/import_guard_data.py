import json
import os

import unreal


DATA_TABLE_PATH = "/Game/Data/DataTable/DT_GuardData"
SOURCE_PATH = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "ProjectResources", "DataTableImports", "DT_GuardData.json")
)
EXPECTED_VALUES = {
    "Guard_Alert_Low": {
        "SightRadius": 750.0,
        "AggroResetDistance": 3600.0,
        "SightAngle": 60.0,
        "InvestigateSightAngle": 100.0,
    },
    "Guard_Alert_Medium": {
        "SightRadius": 900.0,
        "AggroResetDistance": 4500.0,
        "SightAngle": 70.0,
        "InvestigateSightAngle": 120.0,
    },
    "Guard_Alert_High": {
        "SightRadius": 1087.5,
        "AggroResetDistance": 5400.0,
        "SightAngle": 80.0,
        "InvestigateSightAngle": 140.0,
    },
}


if not os.path.isfile(SOURCE_PATH):
    raise RuntimeError("Guard DataTable source JSON is missing: " + SOURCE_PATH)

data_table = unreal.load_asset(DATA_TABLE_PATH)
if data_table is None:
    raise RuntimeError("Guard DataTable failed to load: " + DATA_TABLE_PATH)

if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(data_table, SOURCE_PATH):
    raise RuntimeError("Guard DataTable JSON import failed: " + SOURCE_PATH)

if not unreal.EditorAssetLibrary.save_loaded_asset(data_table, False):
    raise RuntimeError("Guard DataTable save failed")

exported_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(data_table))
rows_by_name = {str(row.get("Name")): row for row in exported_rows}
failures = []
verified_values = {}
for row_name, expected_fields in EXPECTED_VALUES.items():
    row = rows_by_name.get(row_name)
    if row is None:
        failures.append("missing row " + row_name)
        continue

    verified_values[row_name] = {}
    for field_name, expected_value in expected_fields.items():
        if field_name not in row:
            failures.append("{} missing field {}".format(row_name, field_name))
            continue
        actual_value = float(row[field_name])
        verified_values[row_name][field_name] = actual_value
        if actual_value != expected_value:
            failures.append(
                "{} {} {} != {}".format(row_name, field_name, actual_value, expected_value)
            )

payload = {
    "asset": DATA_TABLE_PATH,
    "source": SOURCE_PATH.replace("\\", "/"),
    "rows": sorted(rows_by_name),
    "verified_values": verified_values,
    "failures": failures,
}
if failures:
    unreal.log_error("MH_GUARD_DATA_IMPORT=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
    raise RuntimeError("Guard DataTable verification failed")

unreal.log_warning("MH_GUARD_DATA_IMPORT=" + json.dumps(payload, ensure_ascii=True, sort_keys=True))
