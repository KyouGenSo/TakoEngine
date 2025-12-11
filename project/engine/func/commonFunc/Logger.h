#pragma once
#include <string>

namespace Tako {

/// <summary>
/// ログ出力ユーティリティ名前空間
/// デバッグコンソールへのメッセージ出力機能を提供
/// OutputDebugString経由でVisual Studioの出力ウィンドウに表示
/// </summary>
namespace Logger
{
	/// <summary>
	/// 文字列をログ出力
	/// </summary>
	/// <param name="messege">出力するメッセージ</param>
	void Log(const std::string& messege);

	/// <summary>
	/// 可変長引数でフォーマット済み文字列をログ出力
	/// printf形式の書式指定をサポート
	/// </summary>
	/// <param name="format">フォーマット文字列</param>
	/// <param name="...">可変長引数</param>
	void Log(const char* format, ...);
}

} // namespace Tako
