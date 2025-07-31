import bpy
import mathutils
import gpu
import gpu_extras.batch
import copy

bl_info = {
     "name": "LevelEditor",
     "author": "Chaio YenChu",
     "version": (1, 0),
     "blender": (4, 4, 0),
     "location": "",
     "description": "LevelEditor",
     "warning": "",
     "wiki_url": "",
     "trcker_url": "",
     "category": "Object"
}

# モジュールをインポートする
from .stretch_vertex import MYADDON_OT_stretch_vertex
from .create_ico_sphere import MYADDON_OT_create_ico_sphere
from .add_collider import MYADDON_OT_add_collider, OBJECT_PT_collider
from .add_filename import MYADDON_OT_add_filename, OBJECT_PT_file_name
from .export_scene import MYADDON_OT_export_scene
from .disabled_option import MYADDON_OT_disabled_option, OBJECT_PT_disabled_option

class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "TOPBAR_MT_my_menu"
    bl_label = "My Menu"
    bl_description = "Custom Menu by" + bl_info["author"]

    def draw(self, context):
        layout = self.layout
        # シーンをエクスポート
        layout.operator(MYADDON_OT_export_scene.bl_idname, text= MYADDON_OT_export_scene.bl_label, icon='EXPORT')
        # 頂点を伸ばす
        layout.operator(MYADDON_OT_stretch_vertex.bl_idname, text= MYADDON_OT_stretch_vertex.bl_label, icon='MESH_CUBE')
        # ICO球生成
        layout.operator(MYADDON_OT_create_ico_sphere.bl_idname, text= MYADDON_OT_create_ico_sphere.bl_label, icon='MESH_ICOSPHERE')
        # blenderのマニュアル
        layout.operator("wm.url_open", text="Blender Manual", icon='HELP').url = "https://docs.blender.org/manual/en/latest/index.html"

    def submenu(self, context):
        layout = self.layout
        layout.menu(TOPBAR_MT_my_menu.bl_idname)

class DrawCollider:

    # 描画ハンドル
    handle = None

    # 3Dビューに登録する描画関数
    def draw_collider():

        # 頂点データ
        vertices = {"pos": []}
        # インデクスデータ
        indices = []

        # 各頂点のオブジェクト中心からのオフセット
        offsets = [
            [-0.5, -0.5, -0.5],
            [+0.5, -0.5, -0.5],
            [-0.5, +0.5, -0.5],
            [+0.5, +0.5, -0.5],
            [-0.5, -0.5, +0.5],  
            [+0.5, -0.5, +0.5],  
            [-0.5, +0.5, +0.5],  
            [+0.5, +0.5, +0.5]   
        ]

        # サイズ
        size = [2.0, 2.0, 2.0]

        # 現在のシーンのオブジェクトを走査
        for object in bpy.context.scene.objects:

            if not "collider" in object:
                # ['collider']カスタムプロパティが存在しない場合はスキップ
                continue

            center = mathutils.Vector((0.0, 0.0, 0.0))
            size = mathutils.Vector((2.0, 2.0, 2.0))

            # ['collider_center']カスタムプロパティから値を取得
            center[0] = object["collider_center"][0]
            center[1] = object["collider_center"][1]
            center[2] = object["collider_center"][2]
            # ['collider_size']カスタムプロパティから値を取得
            size[0] = object["collider_size"][0]
            size[1] = object["collider_size"][1]
            size[2] = object["collider_size"][2]

            # 追加前の頂点数
            start = len(vertices["pos"])

            # Boxの8頂点分回す
            for offset in offsets:
                # オブジェクトの中心座標をコピー
                pos = copy.copy(center)

                # オフセットを適用
                pos.x += offset[0] * size[0]
                pos.y += offset[1] * size[1]
                pos.z += offset[2] * size[2]
                # ローカル座標からワールド座標に変換
                pos = object.matrix_world @ pos

                # 頂点データに追加
                vertices["pos"].append(pos)

                # 前面を構成するインデックスを追加
                indices.append([start + 0, start + 1])
                indices.append([start + 2, start + 3])
                indices.append([start + 0, start + 2])
                indices.append([start + 1, start + 3])
                # 背面を構成するインデックスを追加
                indices.append([start + 4, start + 5])
                indices.append([start + 6, start + 7])
                indices.append([start + 4, start + 6])
                indices.append([start + 5, start + 7])
                # 側面を構成するインデックスを追加
                indices.append([start + 0, start + 4])
                indices.append([start + 1, start + 5]) 
                indices.append([start + 2, start + 6])
                indices.append([start + 3, start + 7])

        # ビルドインシェーダーを取得
        shader = gpu.shader.from_builtin('UNIFORM_COLOR')

        # バッチを作成
        batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', vertices, indices = indices)

        # シェーダーのパラメーター設定
        color = (0.5, 1.0, 1.0, 1.0)
        shader.bind()
        shader.uniform_float("color", color)
        # バッチを描画
        batch.draw(shader)

classes = (
    TOPBAR_MT_my_menu,
    MYADDON_OT_add_collider,
    MYADDON_OT_export_scene,
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_add_filename,
    OBJECT_PT_file_name,
    OBJECT_PT_collider,
    MYADDON_OT_disabled_option,
    OBJECT_PT_disabled_option,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)

    # 描画ハンドルを登録
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW')

    print("Level Editor is Active")
    
def unregister():
    bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)

    for cls in classes:
        bpy.utils.unregister_class(cls)

    # 描画ハンドルを削除
    bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
        
    print("Level Editor is Invalid")

if __name__ == "__main__":
    register()