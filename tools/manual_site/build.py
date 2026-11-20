"""C++ スクリプトのマニュアル（manual/ScriptingManual.md）を 1 枚のサイトにし、GitHub Pages へ出す。

    python tools/manual_site/build.py            dist/manual_site/index.html を作る
    python tools/manual_site/build.py --publish  作ってから、origin の gh-pages ブランチへ出す

公開先は https://kyougenso.github.io/TakoEngine/。リリースのときは tools/release.ps1 が --publish を流す。
サイトに出すバージョンは、リポジトリの VERSION。md の中の #リンクの行き先が無いと、ビルドが止まる。

要る物: Python と、pip install markdown-it-py pygments。公開には、origin へ push できる認証。
"""
import html
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from urllib.parse import unquote

from markdown_it import MarkdownIt
from pygments import highlight
from pygments.formatters import HtmlFormatter
from pygments.lexers import CppLexer

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SRC = ROOT / "manual" / "ScriptingManual.md"
SITE = ROOT / "dist" / "manual_site"
URL = "https://kyougenso.github.io/TakoEngine/"
DESCRIPTION = "TakoEngine でゲームの動きを書くための手引き。スクリプトの作り方、プロパティ、Actor とシーンの操作、入力、当たり判定、コンポーネントの関数。"


def slug(text, used):
    """GitHub と同じ付け方の見出しの id（小文字にし、記号を消し、空白を - にする。重なったら -1、-2）"""
    base = re.sub(r"[^\w\- ]", "", text.lower()).replace(" ", "-")
    count = used.get(base, 0)
    used[base] = count + 1
    return base if count == 0 else f"{base}-{count}"


def render(text):
    """md を、本文の HTML・目次の HTML にする"""
    md = MarkdownIt("commonmark").enable("table")
    md.options["highlight"] = lambda code, lang, _: highlight(code, CppLexer(), HtmlFormatter(nowrap=True)) if lang == "cpp" else ""
    md.add_render_rule("table_open", lambda *_: '<div class="table-wrap"><table>\n')
    md.add_render_rule("table_close", lambda *_: "</table></div>\n")
    fence = md.renderer.rules["fence"]
    md.add_render_rule("fence", lambda self, tokens, idx, options, env: f'<div class="code">{fence(tokens, idx, options, env)}</div>\n')

    tokens = md.parse(text)
    used, ids, toc, links = {}, set(), [], []
    for i, token in enumerate(tokens):
        if token.type == "heading_open":
            title = "".join(child.content for child in tokens[i + 1].children if child.type in ("text", "code_inline"))
            token.attrSet("id", slug(title, used))
            ids.add(token.attrGet("id"))
            if token.tag in ("h2", "h3"):
                toc.append(f'        <li class="{token.tag}"><a href="#{token.attrGet("id")}">{html.escape(title)}</a></li>')
        for child in token.children or []:
            if child.type == "link_open" and child.attrGet("href").startswith("#"):
                links.append(unquote(child.attrGet("href")[1:]))
    missing = sorted(set(links) - ids)
    assert not missing, f"行き先の無いリンク: {missing}"

    # 本文の「## 目次」の節は、左の目次と同じ中身なので出さない
    start = next(i for i, token in enumerate(tokens) if token.type == "heading_open" and tokens[i + 1].content == "目次")
    end = next(i for i in range(start + 1, len(tokens)) if tokens[i].type == "heading_open" and tokens[i].tag == "h2")
    del tokens[start:end]
    toc = [line for line in toc if 'href="#目次"' not in line]

    return md.renderer.render(tokens, md.options, {}), "\n".join(toc)


def build():
    text = SRC.read_text(encoding="utf-8")
    content, toc = render(text)
    values = {
        "TITLE": html.escape(re.search(r"^# (.+)$", text, re.M).group(1)),
        "DESCRIPTION": DESCRIPTION,
        "URL": URL,
        "VERSION": (ROOT / "VERSION").read_text(encoding="utf-8").split()[0],
        "TOC": toc,
        "CONTENT": content,
    }
    page = (HERE / "template.html").read_text(encoding="utf-8")
    for key, value in values.items():
        page = page.replace(f"<!--{key}-->", value)
    SITE.mkdir(parents=True, exist_ok=True)
    (SITE / "index.html").write_text(page, encoding="utf-8", newline="\n")
    # GitHub Pages に、Jekyll を通さずそのまま出させる
    (SITE / ".nojekyll").write_bytes(b"")
    print(f"{SITE / 'index.html'} ({(SITE / 'index.html').stat().st_size} bytes, TakoEngine {values['VERSION']})")
    return values["VERSION"]


def publish(version):
    remote = subprocess.run(["git", "remote", "get-url", "origin"], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
    # ponytail: 毎回 1 コミットで gh-pages を上書きする（サイト専用のブランチ）。履歴が要るなら、clone して積む形にする
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
        shutil.copytree(SITE, tmp, dirs_exist_ok=True)
        for args in (
            ["init", "-q", "-b", "gh-pages"],
            ["add", "-A"],
            ["commit", "-q", "-m", f"update: マニュアルのサイトを更新した\n\n- TakoEngine {version}"],
            ["push", "--force", remote, "gh-pages"],
        ):
            subprocess.run(["git", *args], cwd=tmp, check=True)
    print(f"公開した: {URL}")


if __name__ == "__main__":
    built = build()
    if "--publish" in sys.argv:
        publish(built)
