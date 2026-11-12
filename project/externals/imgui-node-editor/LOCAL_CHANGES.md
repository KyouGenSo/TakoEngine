# imgui-node-editor へのローカル修正

取り込んだ版は 0.9.4（`imgui_node_editor.h` の `IMGUI_NODE_EDITOR_VERSION`）。作者は Michał Cichoń（`LICENSE`）。

ライブラリを差し替えるときは、下の「ソースの修正」を入れ直す。修正した行には `[TakoEngine]` で始まるコメントを付けてある。

## ソースの修正

### 1. キャンバスの大きさが変わると、直前に設定したビューが古い値に戻る（2026-11-12）

- **場所**：`imgui_node_editor.cpp` の `ed::EditorContext::Begin`
- **元**：`auto previousVisibleRect = m_Canvas.ViewRect();`
- **修正後**：`auto previousVisibleRect = m_NavigateAction.GetViewRect();`
- **理由**：`m_Canvas.ViewRect()` は、前の `Begin` でキャンバスに反映したビュー。その後に `NavigateTo`（duration 0）やパンで変えたビューは `m_NavigateAction` の `m_Scroll` / `m_Zoom` にしか無い。次のフレームにキャンバスの大きさが変わると、同じ関数の「前の表示範囲を保つ」処理（`Handle canvas size change` のコメントの所）が、古い方で上書きしていた。
- **出ていた症状**：Behavior Tree Editor でツリーを開いた直後の全体表示（`ed::NavigateToContent(0.0f)`）が効かず、スクロール 0・ズーム 1.0 の表示になる。
- **確認した条件**：ツリーを読み込んだ次のフレームにキャンバスの幅が変わる場合。imgui.ini に古い `Node Inspector##BTE` とのドック分割が残った状態で BT エディタを開くと、最初のフレームだけウィンドウが狭く、この条件になる。修正前は全体が映らず、修正後は映る。

## ソース以外の違い

- `external/` と `misc/` は消してある（コミット `80e5e252`）。
- ビルドは `imgui-node-editor.vcxproj`（取り込みのコミット `62ca6e4b` で追加）。include パスは project 相対（`20b9fb0c`）、構成は x64 のみ（`4066fc21`）。
