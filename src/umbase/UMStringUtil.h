/**
 * @file UMStringUtil.h
 *
 * @author tori31001 at gmail.com
 *
 * Copyright (C) 2013 Kazuma Hatta
 * Licensed under the MIT license.
 *
 */
#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <locale>
#include "UMMacro.h"
#include <wchar.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace umbase
{

/**
 * string utility
 */
class UMStringUtil
{
	DISALLOW_COPY_AND_ASSIGN(UMStringUtil);

public:

	template <typename T>
	static std::wstring number_to_wstring(T value)
	{
		std::wstringstream converter;
		std::wstring wstr;
		converter << value;
		converter >> wstr;
		return wstr;
	}

	template <typename T>
	static std::string number_to_string(T value)
	{
		std::stringstream converter;
		std::string str;
		converter << value;
		converter >> str;
		return str;
	}

	template <typename T>
	static std::string number_to_sequence_string(T value, int n)
	{
		std::stringstream converter;
		std::string str;
		converter << std::setw(n) << std::setfill('0') << value;
		converter >> str;
		return str;
	}

	/**
	 * convert wstring to utf16
	 */
	static umstring wstring_to_utf16(const std::wstring& str)
	{
#if defined _WIN32 && !defined (WITH_EMSCRIPTEN)
		const char16_t* p = reinterpret_cast<const char16_t*>(str.c_str());
		umstring u16str(p);
#else
		umstring u16str;
		u16str.resize(str.size());
		const wchar_t* orig = str.c_str();
		wcsnrtombs(&u16str[0], &orig, str.size(), str.size(), NULL);
#endif
		return u16str;
	}

	/**
	 * convert utf16 to wstring
	 */
	static std::wstring utf16_to_wstring(const umstring& utf16str)
	{
#if defined _WIN32 && !defined (WITH_EMSCRIPTEN)
		const wchar_t* p = reinterpret_cast<const wchar_t*>(utf16str.c_str());
		std::wstring wstr(p);
#else
		// not implemented
		std::wstring wstr;
#endif
		return wstr;
	}

	/**
	 * convert wstring to utf8
	 */
	static std::string wstring_to_utf8(const std::wstring& str)
	{
#if defined _WIN32 && !defined (WITH_EMSCRIPTEN)
		if (str.empty()) return std::string();

		int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
										nullptr, 0, nullptr, nullptr);
		if (size <= 0) return std::string();

		std::string utf8str(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
							&utf8str[0], size, nullptr, nullptr);
		return utf8str;
#else
		// not implemented
		std::string utf8str;
		return utf8str;
#endif
	}

	/**
	 * convert utf8 string to utf16
	 */
	static umstring utf8_to_utf16(const std::string& utf8str)
	{
#if defined _WIN32 && !defined (WITH_EMSCRIPTEN)
		if (utf8str.empty()) return umstring();

		int size = MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), static_cast<int>(utf8str.length()),
										nullptr, 0);
		if (size <= 0) return umstring();

		std::wstring wstr(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), static_cast<int>(utf8str.length()),
							&wstr[0], size);

		const char16_t* p = reinterpret_cast<const char16_t*>(wstr.c_str());
		umstring utf16str(p);
		return utf16str;
#else
		// not implemented
		umstring utf16str;
		return utf16str;
#endif
	}

	/**
	 * convert utf16 to utf8 string
	 */
	static std::string utf16_to_utf8(const umstring& str)
	{
#if defined _WIN32 && !defined (WITH_EMSCRIPTEN)
		if (str.empty()) return std::string();

		const wchar_t* wstr_ptr = reinterpret_cast<const wchar_t*>(str.c_str());
		size_t len = str.length();

		int size = WideCharToMultiByte(CP_UTF8, 0, wstr_ptr, static_cast<int>(len),
										nullptr, 0, nullptr, nullptr);
		if (size <= 0) return std::string();

		std::string stdstr(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr_ptr, static_cast<int>(len),
							&stdstr[0], size, nullptr, nullptr);
		return stdstr;
#else
		// not implemented
		std::string stdstr = str;
		return stdstr;
#endif
	}

#if !defined (WITH_EMSCRIPTEN)
	/**
	 * convert utf8 to utf32 string
	 */
	static std::u32string utf8_to_utf32(const std::string& str)
	{
#if defined _WIN32
		if (str.empty()) return std::u32string();

		int utf16_size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
											nullptr, 0);
		if (utf16_size <= 0) return std::u32string();

		std::wstring utf16_str(utf16_size, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
							&utf16_str[0], utf16_size);

		std::u32string u32str;
		u32str.reserve(utf16_str.length());

		for (size_t i = 0; i < utf16_str.length(); ++i) {
			wchar_t wch = utf16_str[i];

			if (wch >= 0xD800 && wch <= 0xDBFF && i + 1 < utf16_str.length()) {
				wchar_t low = utf16_str[i + 1];
				if (low >= 0xDC00 && low <= 0xDFFF) {
					char32_t codepoint = 0x10000 + ((wch - 0xD800) << 10) + (low - 0xDC00);
					u32str.push_back(codepoint);
					++i;
					continue;
				}
			}

			u32str.push_back(static_cast<char32_t>(wch));
		}
		return u32str;
#else
		// not implemented
		std::u32string u32str;
		return u32str;
#endif
	}
#endif // !defined (WITH_EMSCRIPTEN)

};

} // umbase
