import bpy
import math
import mathutils

class MYADDON_OT_add_collider(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_collider"
    bl_label = "Add Collider"
    bl_description = "['collider']カスタムプロパティを追加するオペレーター"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        
        context.object["collider"] = "BOX"
        context.object["collider_center"] = mathutils.Vector((0.0, 0.0, 0.0))
        context.object["collider_size"] = mathutils.Vector((2.0, 2.0, 2.0))

        self.report({'INFO'}, "Collider added to selected objects")
        return {'FINISHED'}
    

class OBJECT_PT_collider(bpy.types.Panel):
    bl_idname = "OBJECT_PT_collider"
    bl_label = "Collider"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        layout = self.layout

        # パネルに項目を追加
        if "collider" in context.object:
            # 既に['collider']カスタムプロパティが存在する場合は表示
            layout.prop(context.object, '["collider"]', text="Type")
            layout.prop(context.object, '["collider_center"]', text="Center")
            layout.prop(context.object, '["collider_size"]', text="Size")
        else:
            # まだ['collider']カスタムプロパティが存在しない場合は、追加ボタンを表示
            layout.operator(MYADDON_OT_add_collider.bl_idname, text="Add Collider", icon='ADD')