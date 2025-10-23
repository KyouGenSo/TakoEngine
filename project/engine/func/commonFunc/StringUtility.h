#pragma once
#include<string>

/// <summary>
/// 文字列変換ユーティリティ名前空間
/// マルチバイト文字列(std::string)とワイド文字列(std::wstring)の相互変換機能を提供
/// Windows APIとの連携やDirectX APIで必要なワイド文字列への変換に使用
/// </summary>
namespace StringUtility {
	/// <summary>
	/// マルチバイト文字列をワイド文字列に変換
	/// </summary>
	/// <param name="str">変換元のマルチバイト文字列</param>
	/// <returns>変換されたワイド文字列</returns>
	std::wstring ConvertString(const std::string& str);

	/// <summary>
	/// ワイド文字列をマルチバイト文字列に変換
	/// </summary>
	/// <param name="str">変換元のワイド文字列</param>
	/// <returns>変換されたマルチバイト文字列</returns>
	std::string ConvertString(const std::wstring& str);
}