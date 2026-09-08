import json
import os
from pathlib import Path
import re
import sys
import types


repo = Path(__file__).resolve().parents[4]
source = (repo / "ProjectResources/Tools/Forgery/ImportSurfaceForgeryPack.ps1").read_text(encoding="utf-8-sig")
script = re.search(r"\$pythonSource = @'\r?\n(.*?)\r?\n'@", source, re.S).group(1)
events = []


class Library:
    @staticmethod
    def does_directory_exist(path):
        return True

    @staticmethod
    def delete_directory(path):
        events.append({"action": "delete_directory", "path": path})
        raise AssertionError("Import must not delete existing texture directories")


class Task:
    def __init__(self):
        self.properties = {}

    def set_editor_property(self, name, value):
        self.properties[name] = value


def import_tasks(tasks):
    assert len(tasks) == 240
    assert all(task.properties["replace_existing"] for task in tasks)
    events.append({"action": "import_asset_tasks", "count": len(tasks)})
    raise RuntimeError("Injected import failure")


sys.modules["unreal"] = types.SimpleNamespace(
    AssetToolsHelpers=types.SimpleNamespace(get_asset_tools=lambda: types.SimpleNamespace(import_asset_tasks=import_tasks)),
    EditorAssetLibrary=Library,
    AssetImportTask=Task,
)
os.environ["HEIST_SURFACE_FORGERY_ROOT"] = str(repo)
try:
    exec(compile(script, "ImportSurfaceForgeryPack.embedded.py", "exec"), {})
except RuntimeError as error:
    assert str(error) == "Injected import failure"
else:
    raise AssertionError("Import failure was swallowed")
assert events == [{"action": "import_asset_tasks", "count": 240}]
print(json.dumps({"name": "import_failure_preserves_existing_directories", "pass": True, "events": events}))
