import bpy
import os
from bpy.props import StringProperty
from bpy_extras.io_utils import ImportHelper

bl_info = {
    "name": "MMDBridge Alembic and Material Import",
    "author": "Kazuma Hatta",
    "version": (1, 0, 1),
    "blender": (3, 0, 0),
    "location": "File > Import > MMDBridge Alembic and Material (.abc, .mtl)",
    "description": "Import Alembic files (.abc) and MMDBridge Materials (.mtl)",
    "category": "Import-Export",
}


class Mtl:
    def __init__(self):
        self.name = ""
        self.textureMap = ""
        self.alphaMap = ""
        self.diffuse = [0.7, 0.7, 0.7, 1.0]
        self.specular = [0.0, 0.0, 0.0]
        self.ambient = [0.0, 0.0, 0.0]
        self.trans = 1.0
        self.power = 0.0
        self.lum = 1
        self.faceSize = 0
        self.isAccessory = False


def import_mtl(path, result, relation):
    current = None
    export_mode = 0

    with open(path, "r", encoding="utf-8") as mtl:
        for line in mtl.readlines():
            words = line.split()
            if len(words) < 2:
                continue
            if "newmtl" in words[0]:
                if current is not None and current.name != "":
                    result[current.name] = current
                current = Mtl()
                current.name = str(words[1])

                nameSplits = current.name.split("_")
                if len(nameSplits) >= 3:
                    try:
                        objectNumber = int(nameSplits[1])
                        materialNumber = int(nameSplits[2])

                        if objectNumber not in relation.keys():
                            relation[objectNumber] = []

                        relation[objectNumber].append(materialNumber)
                    except ValueError:
                        print(f"Warning: Unable to parse object and material numbers from {current.name}")
                        continue

            if "Ka" == words[0]:
                current.ambient = [float(words[1]), float(words[2]), float(words[3])]
            elif "Kd" == words[0]:
                current.diffuse = [float(words[1]), float(words[2]), float(words[3]), current.diffuse[3]]
            elif "Ks" == words[0]:
                current.specular = [float(words[1]), float(words[2]), float(words[3])]
            elif "Ns" == words[0]:
                current.power = float(words[1])
            elif "d" == words[0]:
                current.trans = float(words[1])
                current.diffuse[3] = current.trans
            elif "map_Kd" == words[0]:
                current.textureMap = line[line.find(words[1]) : line.find(".png") + 4]
            elif "map_d" == words[0]:
                current.alphaMap = line[line.find(words[1]) : line.find(".png") + 4]
            elif "#" == words[0]:
                if words[1] == "face_size":
                    current.faceSize = int(words[2])
                elif words[1] == "is_accessory":
                    current.isAccessory = True
                elif words[1] == "mode":
                    export_mode = int(words[2])

    if current is not None and current.name != "":
        result[current.name] = current

    for rel in relation.values():
        rel.sort()

    return export_mode


def match_mesh_and_material(mesh_name, mtl_name):
    def _strip_name(name):
        stripped = name.replace("xform", "").replace("material", "").replace("mesh", "")
        # Keep one underscore between words, remove leading/trailing underscores
        while "__" in stripped:
            stripped = stripped.replace("__", "_")
        stripped = stripped.strip("_")
        return stripped

    mesh_stripped = _strip_name(mesh_name)
    mtl_stripped = _strip_name(mtl_name)
    return mesh_stripped == mtl_stripped


def assign_material(base_path, obj, mesh, mtlmat, image_dict):
    if mtlmat.name in bpy.data.materials:
        mat = bpy.data.materials[mtlmat.name]
    else:
        mat = bpy.data.materials.new(name=mtlmat.name)

    mat.use_nodes = True

    if mat.name not in mesh.materials:
        mesh.materials.append(mat)

    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    nodes.clear()

    bsdf = nodes.new(type="ShaderNodeBsdfPrincipled")
    material_output = nodes.new(type="ShaderNodeOutputMaterial")

    bsdf.location = (0, 0)
    material_output.location = (400, 0)

    links.new(bsdf.outputs["BSDF"], material_output.inputs["Surface"])

    bsdf.inputs["Base Color"].default_value = mtlmat.diffuse

    # Check Blender version and adjust specular input accordingly
    if bpy.app.version >= (4, 0, 0):
        # For Blender 4.0 and above
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = sum(mtlmat.specular) / 3
    else:
        # For Blender 3.6 and below
        if "Specular" in bsdf.inputs:
            bsdf.inputs["Specular"].default_value = sum(mtlmat.specular) / 3

    bsdf.inputs["Roughness"].default_value = 1.0 - (mtlmat.power / 100)

    if mtlmat.textureMap:
        tex_image_node = nodes.new("ShaderNodeTexImage")
        tex_image_node.location = (-300, 0)

        texture_file_path = os.path.normpath(bpy.path.abspath(os.path.join(base_path, mtlmat.textureMap)))

        if texture_file_path in image_dict:
            tex_image_node.image = image_dict[texture_file_path]
        elif os.path.exists(texture_file_path):
            tex_image_node.image = bpy.data.images.load(texture_file_path)
            image_dict[texture_file_path] = tex_image_node.image

        if tex_image_node.image:
            links.new(tex_image_node.outputs["Color"], bsdf.inputs["Base Color"])

    if mtlmat.trans < 1.0:
        bsdf.inputs["Alpha"].default_value = mtlmat.trans
        mat.blend_method = "BLEND"


def import_mmdbridge_material(filepath, context):
    image_dict = {}
    mtlDict = {}
    relationDict = {}
    import_mtl(filepath, mtlDict, relationDict)

    base_path, file_name = os.path.split(filepath)

    for key, mtlmat in mtlDict.items():
        for obj in bpy.data.objects:
            if obj.type == "MESH" and obj.data is not None:
                if match_mesh_and_material(obj.name, key):
                    assign_material(base_path, obj, obj.data, mtlmat, image_dict)
                    print(f"Assigned material {mtlmat.name} to object {obj.name}")


def import_alembic_and_mtl(filepath, context):
    # Create a new collection called "abc"
    abc_collection = bpy.data.collections.new("abc")
    bpy.context.scene.collection.children.link(abc_collection)

    # Import Alembic file (.abc)
    bpy.ops.wm.alembic_import(filepath=filepath)

    # Move all newly imported objects to the "abc" collection
    for obj in bpy.context.selected_objects:
        for collection in obj.users_collection:
            collection.objects.unlink(obj)
        abc_collection.objects.link(obj)

    # Record original materials
    original_materials = {}
    for obj in abc_collection.objects:
        if obj.type == "MESH":
            original_materials[obj.name] = [slot.material.name if slot.material else None for slot in obj.material_slots]

    # Clear all materials
    for obj in abc_collection.objects:
        if obj.type == "MESH":
            obj.data.materials.clear()

    # Find .mtl file in the same directory
    base_path, file_name = os.path.split(filepath)
    mtl_file = os.path.join(base_path, file_name.replace(".abc", ".mtl"))

    if os.path.exists(mtl_file):
        # If corresponding .mtl file is found, import materials
        import_mmdbridge_material(mtl_file, context)
    else:
        print("Warning: .mtl file not found for Alembic file.")

    # Update MTL file
    update_mtl_file(mtl_file, original_materials, abc_collection.objects)


def update_mtl_file(mtl_file, original_materials, objects):
    with open(mtl_file, "a", encoding="utf-8") as f:
        f.write("\n# Original material information\n")
        for obj_name, materials in original_materials.items():
            f.write(f"# Object: {obj_name}\n")
            for i, mat_name in enumerate(materials):
                if mat_name:
                    f.write(f"# Material {i}: {mat_name}\n")

        f.write("\n# New material assignments\n")
        for obj in objects:
            if obj.type == "MESH":
                f.write(f"# Object: {obj.name}\n")
                for i, slot in enumerate(obj.material_slots):
                    if slot.material:
                        f.write(f"# Material {i}: {slot.material.name}\n")


class MMDBridgeAlembicImportOperator(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.mmdbridge_alembic_material"
    bl_label = "MMDBridge Alembic and Material Importer (.abc, .mtl)"

    filename_ext = ".abc"
    filter_glob: StringProperty(default="*.abc", options={"HIDDEN"})

    def execute(self, context):
        import_alembic_and_mtl(self.filepath, context)
        return {"FINISHED"}


def menu_func_import(self, context):
    self.layout.operator(MMDBridgeAlembicImportOperator.bl_idname, text="MMDBridge Alembic and Material (.abc, .mtl)")


def register():
    bpy.utils.register_class(MMDBridgeAlembicImportOperator)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(MMDBridgeAlembicImportOperator)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
