import bpy

class MYADDON_OT_disabled_option(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_disabled_option"
    bl_label = "無効フラグオプション"
    bl_description = "オブジェクトの無効フラグを設定するオペレーター"
    # redo undo可能オプション
    bl_options = {'REGISTER', 'UNDO'}

    # メニュー実行した時に呼ばれるコールバック関数
    def execute(self, context):
        # ['disabled']カスタムプロパティを追加
        context.object["disabled"] = True

        # オペレーターの命令終了をBlenderに伝える
        return {'FINISHED'}
    
class OBJECT_PT_disabled_option(bpy.types.Panel):
    bl_idname = "OBJECT_PT_disabled_option"
    bl_label = "Disabled"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        layout = self.layout

        # パネルに項目を追加
        if "disabled" in context.object:
            # 既に['disabled']カスタムプロパティが存在する場合は表示
            layout.prop(context.object, '["disabled"]', text=self.bl_label)
        else:
            # まだ['disabled']カスタムプロパティが存在しない場合は、追加ボタンを表示
            layout.operator(MYADDON_OT_disabled_option.bl_idname, text="Add Disabled Flag", icon='ADD')