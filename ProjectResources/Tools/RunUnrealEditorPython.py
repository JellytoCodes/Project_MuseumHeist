"""Run a project script inside the explicitly delegated live Unreal Editor."""
import importlib.util
import json
import sys
import time
from pathlib import Path

plugin = Path('D:/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python/remote_execution.py')
spec = importlib.util.spec_from_file_location('unreal_remote_execution', plugin)
remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(remote)
session = remote.RemoteExecution()
session.start()
try:
    deadline = time.monotonic() + 12
    while not session.remote_nodes and time.monotonic() < deadline:
        time.sleep(.2)
    nodes = [node for node in session.remote_nodes if node.get('project_name') == 'Project_MuseumHeist']
    if len(nodes) != 1:
        raise RuntimeError('Expected one MuseumHeist Editor: ' + repr(session.remote_nodes))
    session.open_command_connection(nodes[0]['node_id'])
    result = session.run_command(str(Path(sys.argv[1]).resolve()), exec_mode=remote.MODE_EXEC_FILE)
    print(json.dumps(result, ensure_ascii=False))
    if not result.get('success'):
        sys.exit(1)
finally:
    session.stop()
