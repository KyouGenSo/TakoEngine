import bpy

class MYADDON_OT_add_filename(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_filename"
    bl_label = "FileNameを追加"
    bl_description = "['file_name']カスタムプロパティを追加するオペレーター"
    # redo undo可能オプション
    bl_options = {'REGISTER', 'UNDO'}

    # メニュー実行した時に呼ばれるコールバック関数
    def execute(self, context):
        print("Adding file name...")

        # ['file_name']カスタムプロパティを追加
        context.object["file_name"] = ""

        print("File name added")
        self.report({'INFO'}, "ファイル名が追加されました")

        # オペレーターの命令終了をBlenderに伝える
        return {'FINISHED'}
    

class OBJECT_PT_file_name(bpy.types.Panel):
    bl_idname = "OBJECT_PT_file_name"
    bl_label = "FileName"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        layout = self.layout

        # パネルに項目を追加
        if "file_name" in context.object:
            # 既に['file_name']カスタムプロパティが存在する場合は表示
            layout.prop(context.object, '["file_name"]', text=self.bl_label)
        else:
            # まだ['file_name']カスタムプロパティが存在しない場合は、追加ボタンを表示
            layout.operator(MYADDON_OT_add_filename.bl_idname, text="Add FileName", icon='ADD')