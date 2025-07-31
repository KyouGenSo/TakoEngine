import bpy
import bpy_extras
import math
import json

class MYADDON_OT_export_scene(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    """シーンをエクスポートするオペレーター"""
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーンをエクスポート"
    bl_description = "シーンをエクスポートする"
    # 出力するファイルの拡張子
    filename_ext = ".json"

    # メニュー実行した時に呼ばれるコールバック関数
    def execute(self, context):
        print("Exporting scene...")

        #ファイルに出力
        self.export_json()

        print("Export complete")
        self.report({'INFO'}, "シーンのエクスポートが完了しました")

        # オペレーターの命令終了をBlenderに伝える
        return {'FINISHED'}
    
    def export(self):
        """ファイルに出力"""

        print("Exporting to file: %r" % self.filepath)
        with open(self.filepath, 'wt') as file:      
            self.write_and_print(file, "SCENE")

            for object in bpy.context.scene.objects:

                if(object.parent):
                    # オブジェクトが親を持つ場合はスキップ
                    continue

                # シーンを再帰的に解析してファイルに書き込む
                self.parse_scene_recursive(file, object, 0)

    def export_json(self):
        """シーンをJSON形式でエクスポート"""

        # 保存する情報をまとめるdict
        json_object_root = dict()

        # ノード名
        json_object_root["name"] = "scene"
        # オブジェクトリストを作成
        json_object_root["objects"] = list()

        # シーン内のオブジェクト走査してパック
        for object in bpy.context.scene.objects:
            if object.parent:
                continue
            self.parse_scene_recursive_json(json_object_root["objects"], object, 0)

        # オブジェクトをJSON文字列にエンコード
        json_text = json.dumps(json_object_root, ensure_ascii=False, cls=json.JSONEncoder, indent=4)
        # コンソールに表示
        print(json_text)

        # ファイルをテキスト形式で書き出す用にオープン
        with open(self.filepath, "wt", encoding="utf-8") as file:
            # ファイルに文字列を書き込む
            file.write(json_text)
    
    def write_and_print(self, file, str):
        """ファイルに書き込み、コンソールに出力"""
        file.write(str + "\n")
        print(str)

    def parse_scene_recursive(self, file, object, level):
        """シーンを再帰的に解析してファイルに書き込む"""

        indent = ''
        for i in range(level):
            indent += "\t"

        # オブジェクトタイプ書き込む
        self.write_and_print(file, indent + object.type)
        # ローカルトランスフォーム行列から平行移動、回転、スケーリングを抽出
        trans, rot, scale = object.matrix_local.decompose()
        # ラジアンから度に変換
        rot = rot.to_euler()

        # トランスフォーム情報を書き込む
        self.write_and_print(file, indent + "T %f %f %f" % (trans.x, trans.y, trans.z))
        self.write_and_print(file, indent + "R %f %f %f" % (rot.x, rot.y, rot.z))
        self.write_and_print(file, indent + "S %f %f %f" % (scale.x, scale.y, scale.z))

        # カスタムプロパティ'file_name'
        if "file_name" in object:
            self.write_and_print(file, indent + "N %s" % object["file_name"])

        # カスタムプロパティ'collider'
        if "collider" in object:
            self.write_and_print(file, indent + "C %s" % object["collider"])
            temp_str = indent + "CC %f %f %f"
            temp_str %= (object["collider_center"][0], object["collider_center"][1], object["collider_center"][2])
            self.write_and_print(file, temp_str)
            temp_str = indent + "CS %f %f %f"
            temp_str %= (object["collider_size"][0], object["collider_size"][1], object["collider_size"][2])
            self.write_and_print(file, temp_str)

        self.write_and_print(file, indent + 'END')
        self.write_and_print(file, '')

        # 子ノードを再帰的に解析
        for child in object.children:
            self.parse_scene_recursive(file, child, level + 1)

    def parse_scene_recursive_json(self, data_parent, object, level):

        json_object = dict()
        json_object["type"] = object.type
        json_object["name"] = object.name

        # ローカルトランスフォーム行列から平行移動、回転、スケーリングを抽出
        trans, rot, scale = object.matrix_local.decompose()
        # ラジアンから度に変換
        rot = rot.to_euler()
        # ラジアンから度に変換
        rot.x = math.degrees(rot.x)
        rot.y = math.degrees(rot.y)
        rot.z = math.degrees(rot.z)
        # トランスフォーム情報をdictに格納
        transform = dict()
        transform["translation"] = (trans.x, trans.y, trans.z)
        transform["rotation"] = (rot.x, rot.y, rot.z)
        transform["scale"] = (scale.x, scale.y, scale.z)
        # まとめて一個分のjsonオブジェクトに登録
        json_object["transform"] = transform

        # カスタムプロパティ'disabled'
        if "disabled" in object:
            # 'disabled'カスタムプロパティが存在する場合は登録
            json_object["disabled"] = object["disabled"]

        # カスタムプロパティ'file_name'
        if "file_name" in object:
            # 'file_name'カスタムプロパティが存在する場合は登録
            json_object["file_name"] = object["file_name"]

        # カスタムプロパティ'collider'
        if "collider" in object:
            collider = dict()
            collider["type"] = object["collider"]
            collider["center"] = (object["collider_center"].to_list())
            collider["size"] = (object["collider_size"].to_list())
            json_object["collider"] = collider

        # 1個分のjsonオブジェクトを親オブジェクトに登録
        data_parent.append(json_object)

        # 直接子供リストを走査
        if len(object.children) > 0:
            # 子ノードリストを作成
            json_object["children"] = list()

            for child in object.children:
                self.parse_scene_recursive_json(json_object["children"], child, level + 1)