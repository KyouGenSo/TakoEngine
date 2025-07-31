import bpy

class MYADDON_OT_stretch_vertex(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_stretch_vertex"
    bl_label = "頂点を伸ばす"
    bl_description = "頂点座標を引っ張って伸ばす"
    # redo undo可能オプション
    bl_options = {'REGISTER', 'UNDO'}

    # メニュー実行した時に呼ばれるコールバック関数
    def execute(self, context):
        bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0
        print("Vertex moved")

        # オペレーターの命令終了をBlenderに伝える
        # これをしないと、Blenderがオペレーターが終わったことを認識できない
        return {'FINISHED'}