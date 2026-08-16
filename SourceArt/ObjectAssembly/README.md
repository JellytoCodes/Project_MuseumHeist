# Object Assembly Source Art

This directory is the tracked source-of-truth for the generated Sculpture and Ceramic modular kits.

- `Meshes/`: deterministic OBJ sources imported by the Static Mesh assets under `/Game/Assets/Art/ObjectAssembly`.
- `rebuild_object_assembly_content.py`: Unreal Editor Python script that regenerates the OBJ files, imports the meshes, restores required sockets, and reimports the three Object Assembly DataTables.

Run the script only through Unreal Editor Python after reviewing the DataTable JSON changes. It intentionally uses the shared `BP_ObjectDisplayCase`; the removed prototype `BP_SculptureDisplayCase` and `BP_CeramicDisplayCase` shells are not recreated.

The geometry is project-generated procedural source art created for Project Museum Heist. No external mesh source is embedded in these OBJ files.
