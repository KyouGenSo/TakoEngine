import bpy

class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    """ICO球を生成するオペレーター"""
    bl_idname = "myaddon.myaddon_ot_create_ico_sphere"
    bl_label = "ICO球生成"
    bl_description = "Create an Ico Sphere"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.mesh.primitive_ico_sphere_add()
        print("Ico Sphere created")

        return {'FINISHED'}