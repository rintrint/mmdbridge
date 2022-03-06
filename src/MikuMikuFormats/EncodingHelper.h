#pragma once

#include <locale.h>
#include <stdio.h>
#include <string>
#include <wchar.h>
#include <Windows.h>
#include <memory>
#include <vector>
#include <codecvt>

namespace oguna
{
	/// CP932,UTF8,UTF16を相互変換する
	class EncodingConverter
	{
		/// 指定したサイズのバッファを持ったコンバータを初期化する(初期値:MAX_PATH)
		EncodingConverter(int initial_buffer_size = MAX_PATH)
		{
			buffer.resize(initial_buffer_size, 0);
		}
		/// UTF8からCP932(std::string)へ変換する
		int Utf8ToCp932_Impl(const char* src, int size, std::string* out)
		{
			std::wstring unicode;
			Utf8ToUtf16(src, size, &unicode);
			return Utf16ToCp932(unicode.data(), unicode.length(), out);
		}

		/// CP932からUTF8(std::string)へ変換する
		int Cp932ToUtf8_Impl(const char* src, int length, std::string* out)
		{
			std::wstring unicode;
			Cp932ToUtf16(src, length, &unicode);
			return Utf16ToUtf8(unicode.c_str(), unicode.length(), out);
		}

		/// CP932からUTF16(std::wstring)へ変換する
		int Cp932ToUtf16_Impl(const char* src, int length, std::wstring* out)
		{
			int size;
			size = ::MultiByteToWideChar(932, MB_PRECOMPOSED, src, length, NULL, NULL);
			buffer.resize(size * sizeof(wchar_t) * 2, 0);
			MultiByteToWideChar(932, MB_PRECOMPOSED, src, length, (LPWSTR)buffer.data(), buffer.size() * 2);
			out->assign((wchar_t*)buffer.data(), size);
			return size;
		}

		/// UTF16からCP932(std::string)へ変換する
		int Utf16ToCp932_Impl(const wchar_t* src, int length, std::string* out)
		{
			int size;
			size = WideCharToMultiByte(932, NULL, src, length, NULL, NULL, NULL, NULL);
			buffer.resize(size, 0);
			WideCharToMultiByte(932, NULL, src, length, (LPSTR)buffer.data(), buffer.size(), NULL, NULL);
			out->assign(buffer.data(), size);
			return size;
		}

		/// UTF8からUTF16(std::wstring)へ変換する
		int Utf8ToUtf16_Impl(const char* src, int length, std::wstring* out)
		{
			int size;
			size = ::MultiByteToWideChar(CP_UTF8, 0, src, length, NULL, NULL);
			buffer.resize(size * sizeof(wchar_t), 0);
			MultiByteToWideChar(CP_UTF8, 0, src, length, (LPWSTR)buffer.data(), buffer.size());
			out->swap(std::wstring((wchar_t*)buffer.data(), size));
			return size;
		}

		/// UTF16からUTF8(std::string)へ変換する
		int Utf16ToUtf8_Impl(const wchar_t* src, int length, std::string* out)
		{
			int size;
			size = WideCharToMultiByte(CP_UTF8, NULL, src, length, NULL, NULL, NULL, NULL);
			buffer.resize(size, 0);
			WideCharToMultiByte(CP_UTF8, NULL, src, length, (LPSTR)buffer.data(), buffer.size(), NULL, NULL);
			out->assign(buffer.data(), size);
			return size;
		}
		std::vector<char> buffer;
	public:

		static EncodingConverter& Get() {
			static EncodingConverter instance;
			return instance;
		}

		/// UTF8からCP932(std::string)へ変換する
		static int Utf8ToCp932(const char* src, int size, std::string *out)
		{
			return Get().Utf8ToCp932_Impl(src, size, out);
		}

		/// CP932からUTF8(std::string)へ変換する
		static int Cp932ToUtf8(const char* src, int length, std::string *out)
		{
			return Get().Cp932ToUtf8_Impl(src, length, out);
		}

		/// CP932からUTF16(std::wstring)へ変換する
		static int Cp932ToUtf16(const char *src, int length, std::wstring *out)
		{
			return Get().Cp932ToUtf16_Impl(src, length, out);
		}

		/// UTF16からCP932(std::string)へ変換する
		static int Utf16ToCp932(const wchar_t *src, int length, std::string *out)
		{
			return Get().Utf16ToCp932_Impl(src, length, out);
		}

		/// UTF8からUTF16(std::wstring)へ変換する
		static int Utf8ToUtf16(const char *src, int length, std::wstring *out)
		{
			return Get().Utf8ToUtf16_Impl(src, length, out);
		}

		/// UTF16からUTF8(std::string)へ変換する
		static int Utf16ToUtf8(const wchar_t* src, int length, std::string* out)
		{
			return Get().Utf16ToUtf8_Impl(src, length, out);
		}

		static std::string wstringTostring(const std::wstring& src)
		{
			std::string res(src.length() * 3 + 1, 0);
			size_t len = ::WideCharToMultiByte(CP_ACP, 0, src.c_str(), src.length(), &res[0], res.length(), NULL, NULL);
			res.resize(len);
			return res;
		}

		static std::wstring stringTowstring(const std::string& src)
		{
			std::wstring res(src.length(), 0);
			size_t len = ::MultiByteToWideChar(CP_ACP, 0, src.c_str(), src.length(), &res[0], res.length());
			res.resize(len);
			return res;
		}
	};
}
