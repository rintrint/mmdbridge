/**
 * @file UMPath.cpp
 *
 * @author tori31001 at gmail.com
 *
 * Copyright (C) 2013 Kazuma Hatta
 * Licensed  under the MIT license.
 *
 */
#if !defined(WITH_EMSCRIPTEN)
#include <windows.h>
#include <Mmsystem.h>
#include <tchar.h>
#include <shlwapi.h>
#endif

#include <string>

#include "UMPath.h"
#include "UMStringUtil.h"

namespace umbase
{

bool UMPath::exists(const wchar_t* absolute_path)
{
#ifdef WITH_EMSCRIPTEN
	return true;
#else
	if (::PathFileExistsW(absolute_path))
	{
		return true;
	}
	return false;
#endif // WITH_EMSCRIPTEN
}

bool UMPath::is_folder(const wchar_t* absolute_path)
{
	return !!PathIsDirectory(absolute_path);
}

bool UMPath::get_child_path_list(
	std::vector<umstring>& dst_folder_list,
	std::vector<umstring>& dst_file_list,
	const umstring& src_absolute_folder_path)
{
	WIN32_FIND_DATA find_dir_data;
	std::wstring inpath = UMStringUtil::utf16_to_wstring(src_absolute_folder_path);
	std::wstring serch_path = inpath + _T("\\*.*");
	HANDLE hFind = FindFirstFile(serch_path.c_str() , &find_dir_data);
	bool result = (hFind != INVALID_HANDLE_VALUE);
	do
	{
		if (hFind != INVALID_HANDLE_VALUE)
		{
			if( wcscmp(find_dir_data.cFileName, _T(".") ) && wcscmp(find_dir_data.cFileName, _T("..") ))
			{
				if (find_dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					std::wstring wstr = find_dir_data.cFileName;
					std::wstring wpath = inpath + _T("\\") + wstr;
					dst_folder_list.push_back(UMStringUtil::wstring_to_utf16(wpath));
				}
				else
				{
					std::wstring wstr = find_dir_data.cFileName;
					std::wstring wpath = inpath + _T("\\") + wstr;
					dst_file_list.push_back(UMStringUtil::wstring_to_utf16(wpath));
				}
			}
		}
	}
	while(FindNextFile(hFind,&find_dir_data));

	return result;
}

umstring UMPath::get_base_folder(const umstring& absolute_path)
{
	umstring path = absolute_path;
	umstring target = UMStringUtil::utf8_to_utf16("\\");
	umstring::size_type pos = absolute_path.find_last_of(target);
	if (pos != umstring::npos)
	{
		return path.substr(0, pos);
	}
	return umstring();
}

umstring UMPath::get_temp_absolute_path()
{
	DWORD size = ::GetTempPath(0, NULL);
	std::vector<wchar_t> buffer;
	buffer.resize(size);
	GetTempPath(size, &buffer[0]);
	std::wstring temp_path;
	for (size_t i = 0; i < buffer.size(); ++i)
	{
		temp_path += buffer[i];
	}
	wchar_t temp_file_path[MAX_PATH];
	if (::GetTempFileName(temp_path.c_str(), NULL, 0, temp_file_path) != 0)
	{
		return umbase::UMStringUtil::wstring_to_utf16(temp_file_path);
	}
	return umstring();
}

bool UMPath::remove_file(const umstring& file_path)
{
	std::wstring path = UMStringUtil::utf16_to_wstring(file_path);
	if (UMPath::exists(path.c_str()) && !UMPath::is_folder(path.c_str()))
	{
		_wremove(path.c_str());
		return true;
	}
	return false;
}

umstring UMPath::module_absolute_path()
{
#ifdef WITH_EMSCRIPTEN
	umstring none;
	return none;
#else
	TCHAR path[1024];
	GetModuleFileName(NULL, path, sizeof(path) / sizeof(TCHAR));
	return UMStringUtil::wstring_to_utf16(path);
#endif // WITH_EMSCRIPTEN
}

umstring UMPath::resource_absolute_path(const umstring& file_name)
{
#ifdef WITH_EMSCRIPTEN
	return umstring("resource/") + file_name;
#else
	TCHAR path[1024];
	GetModuleFileName(NULL, path, sizeof(path) / sizeof(TCHAR));
	PathRemoveFileSpec(path);
	SetCurrentDirectory(path);
	SetCurrentDirectory(_T("../../../resource/"));
	GetCurrentDirectory(1024, path);
	std::wstring inpath = path + std::wstring(_T("\\")) + UMStringUtil::utf16_to_wstring(file_name);
	// honban you kozaiku
	if (! ::PathFileExistsW(inpath.c_str()))
	{
		SetCurrentDirectory(path);
		SetCurrentDirectory(_T("./resource/"));
		GetCurrentDirectory(1024, path);
		inpath = path + std::wstring(_T("\\")) + UMStringUtil::utf16_to_wstring(file_name);
	}
	return UMStringUtil::wstring_to_utf16(inpath);
#endif // WITH_EMSCRIPTEN
}

umstring UMPath::resource_absolute_path(const std::string& file_name)
{
	umstring utf16name = UMStringUtil::utf8_to_utf16(file_name);
	return resource_absolute_path(utf16name);
}

umstring UMPath::get_file_name(const umstring& file_path)
{
#ifdef WITH_EMSCRIPTEN
	return file_path;
#else
	std::wstring path = UMStringUtil::utf16_to_wstring(file_path);
	std::wstring filename(::PathFindFileName(path.c_str()));
	return UMStringUtil::wstring_to_utf16(filename);
#endif // WITH_EMSCRIPTEN
}

umstring UMPath::get_absolute_path(const umstring& base_path, umstring& file_name)
{
#ifdef WITH_EMSCRIPTEN
	return base_path + "/" + file_name;
#else
	umstring base_path_copy = base_path;
	umstring base_file_name = get_file_name(base_path_copy);
	if (!base_file_name.empty())
	{
		umstring::size_type pos = base_path_copy.find(base_file_name);
		if (pos != umstring::npos)
		{
			base_path_copy.erase(
				base_path_copy.begin() + pos ,
				base_path_copy.begin() + pos + base_file_name.length());
		}
	}
	return base_path_copy + file_name;
#endif // WITH_EMSCRIPTEN
}

umstring UMPath::get_env(const umstring& env)
{
#ifdef WITH_EMSCRIPTEN
	return env;
#else
	wchar_t* buffer = nullptr;
	size_t bufferSize = 0;

	errno_t err = _wdupenv_s(&buffer, &bufferSize, UMStringUtil::utf16_to_wstring(env).c_str());

	if (err == 0 && buffer != nullptr)
	{
		umstring result = UMStringUtil::wstring_to_utf16(buffer);
		free(buffer);
		return result;
	}

	if (buffer != nullptr)
	{
		free(buffer);
	}

	return umstring();
#endif
}

} // umbase
