#define CINTERFACE

#include "d3d9.h"
#include "d3dx9.h"

#include <windows.h>
#include <shlwapi.h>
#include <intrin.h>
#include <commctrl.h>
#include <richedit.h>
#include <process.h>
#include <direct.h>
#include <excpt.h>

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <regex>
#include <algorithm>

#include <MinHook.h>

#include <pybind11/eval.h>
#include <pybind11/stl_bind.h>
namespace py = pybind11;

#include "bridge_parameter.h"
#include "alembic.h"
#include "vmd.h"
#include "pmx.h"
#include "resource.h"
#include "UMStringUtil.h"
#include "UMPath.h"
#include "EncodingHelper.h"

// ===== Python Version Check =====
#if PY_VERSION_HEX < 0x030B0000
#error "This code requires Python 3.11 or higher"
#endif

// ===== Platform-Specific Definitions =====
#ifdef _WIN64
#define _LONG_PTR LONG_PTR
#else
#define _LONG_PTR LONG
#endif

// ===== Global Variables =====
static HMODULE g_d3d9_system_module = NULL;
static bool g_is_window_hooked = false; // Tracks if the window procedure has been hooked

HWND g_hWnd = NULL;	  // Window handle
HMENU g_hMenu = NULL; // Menu
HWND g_hFrame = NULL; // Frame number

LONG_PTR originalWndProc = NULL;
HINSTANCE hInstance = NULL;

// ===== Forward Declarations =====
static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);

// ===== Helper Functions =====
// Get the code page for a specific ANSI API function from INI settings
static UINT GetAnsiFunctionCodePage(const wchar_t* function_name)
{
	const auto& encoding_map = BridgeParameter::instance().encoding_map;
	auto it = encoding_map.find(function_name);
	if (it != encoding_map.end())
	{
		return it->second;
	}
	return CP_ACP; // Fall back to system default ANSI code page (0)
}

// +++++ MINHOOK HOOKING LOGIC START +++++
// =======================================================================
// MMD API 修正：攔截 ExpGetPmdFilename
// =======================================================================

// 宣告一個 thread_local 旗標，用於在我們的 Utf8 函式和 Hook 之間通訊
thread_local bool g_is_explicit_utf8_call = false;

// 建立一個 RAII 輔助類別，以確保旗標被安全地設定和重設
struct Utf8CallGuard
{
	Utf8CallGuard() { g_is_explicit_utf8_call = true; }
	~Utf8CallGuard() { g_is_explicit_utf8_call = false; }
};

// 輔助函數：根據逆向的結果，從 modelIndex 獲取真實的 UTF16 路徑
const wchar_t* GetInternalPathFromModelIndex(int modelIndex)
{
	const uintptr_t MODEL_MANAGER_POINTER_OFFSET = 0x1445F8;
	const int MODEL_ARRAY_OFFSET = 0xBE8;
	const int PATH_POINTER_OFFSET = 0x2548;

	__try
	{
		uintptr_t mmd_base = (uintptr_t)GetModuleHandle(NULL);
		if (!mmd_base)
			return nullptr;

		// 步驟 A: 獲取模型管理物件的指標
		uintptr_t model_manager_ptr = *(uintptr_t*)(mmd_base + MODEL_MANAGER_POINTER_OFFSET);
		if (!model_manager_ptr)
			return nullptr;

		// 步驟 B: 獲取模型陣列的基底位址。
		uintptr_t* model_array = (uintptr_t*)(model_manager_ptr + MODEL_ARRAY_OFFSET);

		// 步驟 C: 根據索引直接獲取模型物件指標
		uintptr_t model_object = model_array[modelIndex];
		if (!model_object)
			return nullptr;

		// 步驟 D: 從模型物件中加上偏移量，獲取最終的路徑指標
		const wchar_t* internalPathPtr = (const wchar_t*)(model_object + PATH_POINTER_OFFSET);

		if (internalPathPtr[0] == L'\0')
			return internalPathPtr;

		return internalPathPtr;
	}
	__except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return nullptr;
	}
}

// Original function pointer
char* (*fpExpGetPmdFilename_Original)(int) = nullptr;
// Detour
char* WINAPI Detour_ExpGetPmdFilename(int modelIndex)
{
	// 步驟 1: 呼叫我們的逆向邏輯，獲取內部真實的 UTF16 路徑
	const wchar_t* internalPathPtr = GetInternalPathFromModelIndex(modelIndex);
	if (!internalPathPtr || internalPathPtr[0] == L'\0')
	{
		static char empty_string[] = "";
		return empty_string;
	}

	// 步驟 2: 根據我們的 thread_local 旗標，決定目標編碼
	UINT targetCodePage;
	if (g_is_explicit_utf8_call)
	{
		targetCodePage = CP_UTF8;
	}
	else
	{
		// 旗標為 false 返回 CP932 以維持相容性
		targetCodePage = 932;
	}

	// 步驟 3: 轉換字串並透過 thread_local 緩衝區返回
	// 緩衝區大小設置為 MAX_PATH * 4 以應對 UTF8 編碼可能需要更多空間的情況
	thread_local char resultBuffer[MAX_PATH * 4];
	WideCharToMultiByte(targetCodePage, 0, internalPathPtr, -1, resultBuffer, sizeof(resultBuffer), NULL, NULL);
	return resultBuffer;
}

// 提供給 MMDBridge 內部使用的公開包裝函數，確保能安全地獲取 UTF-8 檔名
const char* ExpGetPmdFilenameUtf8(int modelIndex)
{
	Utf8CallGuard guard;
	return ExpGetPmdFilename(modelIndex);
}

// =======================================================================
// 導出AVI時的視窗標題亂碼 修正：攔截 SetWindowTextW
// =======================================================================
BOOL(WINAPI* fpSetWindowTextW_Original)(HWND hWnd, LPCWSTR lpString) = nullptr;
BOOL WINAPI Detour_SetWindowTextW(HWND hWnd, LPCWSTR lpString)
{
	// Fix RecWindow title using temporary subclassing
	wchar_t className[256];
	if (GetClassNameW(hWnd, className, 256) > 0 && wcscmp(className, L"RecWindow") == 0)
	{
		if (!IsWindowUnicode(hWnd))
		{
			WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)DefWindowProcW);
			BOOL result = fpSetWindowTextW_Original(hWnd, lpString);
			SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)oldProc);
			return result;
		}
	}
	return fpSetWindowTextW_Original(hWnd, lpString);
}

// =======================================================================
// 切換語言時的頂端選單亂碼 修正：攔截 ModifyMenuA
// =======================================================================
BOOL(WINAPI* fpModifyMenuA_Original)(HMENU hMnu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem) = nullptr;
BOOL WINAPI Detour_ModifyMenuA(HMENU hMnu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem)
{
	if (lpNewItem && !(uFlags & (MF_BITMAP | MF_OWNERDRAW)))
	{
		__try
		{
			int code_page = GetAnsiFunctionCodePage(L"ModifyMenuA");
			int wide_len = MultiByteToWideChar(code_page, 0, lpNewItem, -1, NULL, 0);
			if (wide_len > 0 && wide_len <= 512)
			{
				wchar_t wide_buf[512];
				MultiByteToWideChar(code_page, 0, lpNewItem, -1, wide_buf, wide_len);
				return ModifyMenuW(hMnu, uPosition, uFlags, uIDNewItem, wide_buf);
			}
		}
		__except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			OutputDebugStringW(L"[MMDBridge] Access violation in Detour_ModifyMenuA\n");
		}
	}
	return fpModifyMenuA_Original(hMnu, uPosition, uFlags, uIDNewItem, lpNewItem);
}

// =======================================================================
// 切換語言時的頂端子選單亂碼 修正：攔截 SetMenuItemInfoA
// =======================================================================
BOOL(WINAPI* fpSetMenuItemInfoA_Original)(HMENU hMenu, UINT uItem, BOOL fByPosition, LPCMENUITEMINFOA lpmii) = nullptr;
BOOL WINAPI Detour_SetMenuItemInfoA(HMENU hMenu, UINT uItem, BOOL fByPosition, LPCMENUITEMINFOA lpmii)
{
	if (lpmii && (lpmii->fMask & MIIM_STRING) && lpmii->dwTypeData)
	{
		__try
		{
			MENUITEMINFOW miiW;
			memcpy(&miiW, lpmii, sizeof(MENUITEMINFOA));

			const char* original_str = lpmii->dwTypeData;
			int code_page = GetAnsiFunctionCodePage(L"SetMenuItemInfoA");
			int wide_len = MultiByteToWideChar(code_page, 0, original_str, -1, NULL, 0);
			if (wide_len > 0 && wide_len <= 512)
			{
				wchar_t wide_buf[512];
				MultiByteToWideChar(code_page, 0, original_str, -1, wide_buf, wide_len);
				miiW.dwTypeData = wide_buf;
				miiW.cch = 0;
				return SetMenuItemInfoW(hMenu, uItem, fByPosition, &miiW);
			}
		}
		__except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			OutputDebugStringW(L"[MMDBridge] Access violation in Detour_SetMenuItemInfoA\n");
		}
	}
	return fpSetMenuItemInfoA_Original(hMenu, uItem, fByPosition, lpmii);
}

// =======================================================================
// 按鈕變藍色時的亂碼 修正：攔截 GetWindowTextA
// =======================================================================
int(WINAPI* fpGetWindowTextA_Original)(HWND hWnd, LPSTR lpString, int nMaxCount) = nullptr;
int WINAPI Detour_GetWindowTextA(HWND hWnd, LPSTR lpString, int nMaxCount)
{
	wchar_t wide_buf[512];
	int wide_chars = GetWindowTextW(hWnd, wide_buf, 512);
	if (wide_chars > 0 && wide_chars <= 510)
	{
		__try
		{
			int code_page = GetAnsiFunctionCodePage(L"GetWindowTextA");
			int required_bytes = WideCharToMultiByte(code_page, 0, wide_buf, wide_chars, NULL, 0, NULL, NULL);
			if (required_bytes > 0 && required_bytes < nMaxCount)
			{
				int bytes_written = WideCharToMultiByte(code_page, 0, wide_buf, wide_chars, lpString, nMaxCount, NULL, NULL);
				if (bytes_written > 0 && bytes_written < nMaxCount)
				{
					lpString[bytes_written] = '\0';
				}
				return bytes_written;
			}
		}
		__except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			OutputDebugStringW(L"[MMDBridge] Access violation in Detour_GetWindowTextA\n");
		}
	}
	return fpGetWindowTextA_Original(hWnd, lpString, nMaxCount);
}

// =======================================================================
// 骨骼下拉式選單亂碼 亂碼 修正：攔截 SendMessageA
// =======================================================================
LRESULT(WINAPI* fpSendMessageA_Original)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) = nullptr;
LRESULT WINAPI Detour_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	// Extend our monitoring scope, adding WM_SETTEXT to handle garbled text issues in more UI elements
	if (Msg == CB_ADDSTRING || Msg == CB_INSERTSTRING || Msg == WM_SETTEXT)
	{
		const char* original_str = (const char*)lParam;
		if (original_str)
		{
			__try
			{
				// Perform a protected volatile read operation to verify if the pointer is readable.
				(void)*(volatile char*)original_str;

				// If the read succeeds, proceed with character encoding conversion
				int code_page = GetAnsiFunctionCodePage(L"SendMessageA");
				int wide_len = MultiByteToWideChar(code_page, 0, original_str, -1, NULL, 0);
				if (wide_len > 0 && wide_len <= 512)
				{
					wchar_t wide_buf[512];
					MultiByteToWideChar(code_page, 0, original_str, -1, wide_buf, wide_len);
					return SendMessageW(hWnd, Msg, wParam, (LPARAM)wide_buf);
				}
			}
			__except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				OutputDebugStringW(L"[MMDBridge] Access violation in Detour_SendMessageA\n");
			}
		}
	}
	return fpSendMessageA_Original(hWnd, Msg, wParam, lParam);
}

// =======================================================================
// 關閉MMD時的警告視窗 修正：攔截 MessageBoxA
// =======================================================================
int(WINAPI* fpMessageBoxA_Original)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) = nullptr;
int WINAPI Detour_MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	__try
	{
		int code_page = GetAnsiFunctionCodePage(L"MessageBoxA");
		wchar_t wide_caption[512] = { 0 };
		wchar_t wide_text[4096] = { 0 };
		bool need_fallback = false;

		// title
		if (lpCaption)
		{
			int caption_len = MultiByteToWideChar(code_page, 0, lpCaption, -1, NULL, 0);
			if (caption_len > 0 && caption_len <= 512)
			{
				MultiByteToWideChar(code_page, 0, lpCaption, -1, wide_caption, caption_len);
				wide_caption[511] = L'\0';
			}
			else
			{
				need_fallback = true;
			}
		}
		// content
		if (lpText)
		{
			int text_len = MultiByteToWideChar(code_page, 0, lpText, -1, NULL, 0);
			if (text_len > 0 && text_len <= 4096)
			{
				MultiByteToWideChar(code_page, 0, lpText, -1, wide_text, text_len);
				wide_text[4095] = L'\0';
			}
			else
			{
				need_fallback = true;
			}
		}

		if (!need_fallback)
		{
			return MessageBoxW(hWnd, wide_text, wide_caption, uType);
		}
	}
	__except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		OutputDebugStringW(L"[MMDBridge] Access violation in Detour_MessageBoxA\n");
	}
	return fpMessageBoxA_Original(hWnd, lpText, lpCaption, uType);
}
// +++++ MINHOOK HOOKING LOGIC END +++++

///////////////////////////////////////////////////////////////////////////
// MMD Export Function Pointers - Definitions and Implementation
///////////////////////////////////////////////////////////////////////////

// Add initialization state tracking
static bool g_mmd_exports_initialized = false;
static bool g_mmd_exports_init_attempted = false;

// Define function pointers
float (*ExpGetFrameTime)() = nullptr;
int (*ExpGetPmdNum)() = nullptr;
char* (*ExpGetPmdFilename)(int) = nullptr;
int (*ExpGetPmdOrder)(int) = nullptr;
int (*ExpGetPmdMatNum)(int) = nullptr;
D3DMATERIAL9 (*ExpGetPmdMaterial)(int, int) = nullptr;
int (*ExpGetPmdBoneNum)(int) = nullptr;
char* (*ExpGetPmdBoneName)(int, int) = nullptr;
D3DMATRIX (*ExpGetPmdBoneWorldMat)(int, int) = nullptr;
int (*ExpGetPmdMorphNum)(int) = nullptr;
char* (*ExpGetPmdMorphName)(int, int) = nullptr;
float (*ExpGetPmdMorphValue)(int, int) = nullptr;
bool (*ExpGetPmdDisp)(int) = nullptr;
int (*ExpGetPmdID)(int) = nullptr;

int (*ExpGetAcsNum)() = nullptr;
int (*ExpGetPreAcsNum)() = nullptr;
char* (*ExpGetAcsFilename)(int) = nullptr;
int (*ExpGetAcsOrder)(int) = nullptr;
D3DMATRIX (*ExpGetAcsWorldMat)(int) = nullptr;
float (*ExpGetAcsX)(int) = nullptr;
float (*ExpGetAcsY)(int) = nullptr;
float (*ExpGetAcsZ)(int) = nullptr;
float (*ExpGetAcsRx)(int) = nullptr;
float (*ExpGetAcsRy)(int) = nullptr;
float (*ExpGetAcsRz)(int) = nullptr;
float (*ExpGetAcsSi)(int) = nullptr;
float (*ExpGetAcsTr)(int) = nullptr;
bool (*ExpGetAcsDisp)(int) = nullptr;
int (*ExpGetAcsID)(int) = nullptr;
int (*ExpGetAcsMatNum)(int) = nullptr;
D3DMATERIAL9 (*ExpGetAcsMaterial)(int, int) = nullptr;

int (*ExpGetCurrentObject)() = nullptr;
int (*ExpGetCurrentMaterial)() = nullptr;
int (*ExpGetCurrentTechnic)() = nullptr;
void (*ExpSetRenderRepeatCount)(int) = nullptr;
int (*ExpGetRenderRepeatCount)() = nullptr;
bool (*ExpGetEnglishMode)() = nullptr;

// Implementation of initialization function
bool InitializeMMDExports()
{
	// Check if already attempted initialization
	if (g_mmd_exports_init_attempted)
	{
		return g_mmd_exports_initialized;
	}

	// Mark that we've attempted initialization
	g_mmd_exports_init_attempted = true;

	HMODULE hExe = GetModuleHandle(NULL);
	if (!hExe)
	{
		::MessageBoxW(NULL, L"Failed to get EXE module handle", L"MMD Export Init Error", MB_OK);
		g_mmd_exports_initialized = false;
		return false;
	}

// Load all function pointers
#define LOAD_FUNC(name)                                                            \
	name = (decltype(name))GetProcAddress(hExe, #name);                            \
	if (!name)                                                                     \
	{                                                                              \
		std::string error_msg = "Failed to load function: " #name;                 \
		std::wstring w_error_msg = umbase::UMStringUtil::utf16_to_wstring(         \
			umbase::UMStringUtil::utf8_to_utf16(error_msg));                       \
		::MessageBoxW(NULL, w_error_msg.c_str(), L"MMD Export Init Error", MB_OK); \
		g_mmd_exports_initialized = false;                                         \
		return false;                                                              \
	}

	// PMD related functions
	LOAD_FUNC(ExpGetFrameTime);
	LOAD_FUNC(ExpGetPmdNum);
	LOAD_FUNC(ExpGetPmdFilename);
	LOAD_FUNC(ExpGetPmdOrder);
	LOAD_FUNC(ExpGetPmdMatNum);
	LOAD_FUNC(ExpGetPmdMaterial);
	LOAD_FUNC(ExpGetPmdBoneNum);
	LOAD_FUNC(ExpGetPmdBoneName);
	LOAD_FUNC(ExpGetPmdBoneWorldMat);
	LOAD_FUNC(ExpGetPmdMorphNum);
	LOAD_FUNC(ExpGetPmdMorphName);
	LOAD_FUNC(ExpGetPmdMorphValue);
	LOAD_FUNC(ExpGetPmdDisp);
	LOAD_FUNC(ExpGetPmdID);

	// Accessory related functions
	LOAD_FUNC(ExpGetAcsNum);
	LOAD_FUNC(ExpGetPreAcsNum);
	LOAD_FUNC(ExpGetAcsFilename);
	LOAD_FUNC(ExpGetAcsOrder);
	LOAD_FUNC(ExpGetAcsWorldMat);
	LOAD_FUNC(ExpGetAcsX);
	LOAD_FUNC(ExpGetAcsY);
	LOAD_FUNC(ExpGetAcsZ);
	LOAD_FUNC(ExpGetAcsRx);
	LOAD_FUNC(ExpGetAcsRy);
	LOAD_FUNC(ExpGetAcsRz);
	LOAD_FUNC(ExpGetAcsSi);
	LOAD_FUNC(ExpGetAcsTr);
	LOAD_FUNC(ExpGetAcsDisp);
	LOAD_FUNC(ExpGetAcsID);
	LOAD_FUNC(ExpGetAcsMatNum);
	LOAD_FUNC(ExpGetAcsMaterial);

	// Current state functions
	LOAD_FUNC(ExpGetCurrentObject);
	LOAD_FUNC(ExpGetCurrentMaterial);
	LOAD_FUNC(ExpGetCurrentTechnic);
	LOAD_FUNC(ExpSetRenderRepeatCount);
	LOAD_FUNC(ExpGetRenderRepeatCount);
	LOAD_FUNC(ExpGetEnglishMode);

#undef LOAD_FUNC

	g_mmd_exports_initialized = true;
	return true;
}

///////////////////////////////////////////////////////////////////////////
// End of MMD Export Function Pointers
///////////////////////////////////////////////////////////////////////////

template <class T>
std::string to_string(T value)
{
	return umbase::UMStringUtil::number_to_string(value);
}

template <class T>
std::wstring to_wstring(T value)
{
	return umbase::UMStringUtil::number_to_wstring(value);
}

static void messagebox(const std::wstring& message, const std::wstring& title = L"MMDBridge")
{
	::MessageBoxW(NULL, message.c_str(), title.c_str(), MB_OK);
}

// Hook functions for IDirect3DDevice9
static void hookDevice(void);
static void originalDevice(void);
// Hooked device
IDirect3DDevice9* p_device = NULL;

RenderData renderData;

std::vector<std::pair<IDirect3DTexture9*, bool>> finishTextureBuffers;

std::map<IDirect3DTexture9*, RenderedTexture> renderedTextures;
std::map<int, std::map<int, std::shared_ptr<RenderedMaterial>>> renderedMaterials;
//-----------------------------------------------------------------------------------------------------------------

static bool writeTextureToFile(const wchar_t* texturePath, IDirect3DTexture9* texture, D3DXIMAGE_FILEFORMAT fileFormat);

static bool writeTextureToFiles(const std::wstring& texturePath, const std::wstring& textureType, bool uncopied = false);

static bool copyTextureToFiles(const std::wstring& texturePath);

static bool writeTextureToMemory(const std::wstring& textureName, IDirect3DTexture9* texture, bool copied);

//------------------------------------------Python invocation--------------------------------------------------------
static bool is_exporting = false;
static int presentCount = 0;
static int process_frame = -1;
static int ui_frame = 0;

// Transform 3D vector with matrix
// Almost same as D3DXVec3Transform
static void d3d_vector3_dir_transform(
	D3DXVECTOR3& dst,
	const D3DXVECTOR3& src,
	const D3DXMATRIX& matrix)
{
	const float tmp[] = {
		src.x * matrix.m[0][0] + src.y * matrix.m[1][0] + src.z * matrix.m[2][0],
		src.x * matrix.m[0][1] + src.y * matrix.m[1][1] + src.z * matrix.m[2][1],
		src.x * matrix.m[0][2] + src.y * matrix.m[1][2] + src.z * matrix.m[2][2]
	};
	dst.x = tmp[0];
	dst.y = tmp[1];
	dst.z = tmp[2];
}

static void d3d_vector3_transform(
	D3DXVECTOR3& dst,
	const D3DXVECTOR3& src,
	const D3DXMATRIX& matrix)
{
	const float tmp[] = {
		src.x * matrix.m[0][0] + src.y * matrix.m[1][0] + src.z * matrix.m[2][0] + 1.0f * matrix.m[3][0],
		src.x * matrix.m[0][1] + src.y * matrix.m[1][1] + src.z * matrix.m[2][1] + 1.0f * matrix.m[3][1],
		src.x * matrix.m[0][2] + src.y * matrix.m[1][2] + src.z * matrix.m[2][2] + 1.0f * matrix.m[3][2]
	};
	dst.x = tmp[0];
	dst.y = tmp[1];
	dst.z = tmp[2];
}

// python
namespace
{
	int script_call_setting = 1;
	std::map<int, int> exportedFrames;

	/// Reload script.
	bool relaod_python_script()
	{
		BridgeParameter::mutable_instance().mmdbridge_python_script.clear();
		std::ifstream ifs(BridgeParameter::instance().python_script_path.c_str());
		if (!ifs)
			return false;
		char buf[2048];
		while (ifs.getline(buf, sizeof(buf)))
		{
			BridgeParameter::mutable_instance().mmdbridge_python_script.append(buf);
			BridgeParameter::mutable_instance().mmdbridge_python_script.append("\r\n");
		}
		ifs.close();
		return true;
	}

	/// Reloads the python script paths.
	void reload_python_file_paths()
	{
		BridgeParameter& mutable_parameter = BridgeParameter::mutable_instance();

		mutable_parameter.python_script_name_list.clear();
		mutable_parameter.python_script_path_list.clear();

		std::wstring searchPath = mutable_parameter.base_path + L"mmdbridge_scripts/";
		std::wstring searchStr(searchPath + L"*.py");

		std::vector<std::pair<std::wstring, std::wstring>> found_scripts;

		// Find python files.
		WIN32_FIND_DATA find;
		HANDLE hFind = FindFirstFile(searchStr.c_str(), &find);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					std::wstring name(find.cFileName);
					std::wstring path(searchPath + find.cFileName);
					found_scripts.emplace_back(name, path);
				}
			}
			while (FindNextFile(hFind, &find));
			FindClose(hFind);
		}

		// Sort scripts using a natural sort order to handle numbers in filenames correctly.
		std::sort(found_scripts.begin(), found_scripts.end(), [](const auto& a, const auto& b) {
			return StrCmpLogicalW(a.first.c_str(), b.first.c_str()) < 0;
		});

		// Repopulate the main lists from the sorted temporary list.
		for (const auto& script : found_scripts)
		{
			if (mutable_parameter.python_script_name.empty())
			{
				mutable_parameter.python_script_name = script.first;
				mutable_parameter.python_script_path = script.second;
			}
			mutable_parameter.python_script_name_list.push_back(script.first);
			mutable_parameter.python_script_path_list.push_back(script.second);
		}
	}

	// Get a reference to the main module.
	PyObject* main_module = NULL;

	// Get the main module's dictionary
	// and make a copy of it.
	PyObject* main_dict = NULL;

	size_t get_vertex_buffer_size()
	{
		return BridgeParameter::instance().finish_buffer_list.size();
	}

	size_t get_vertex_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).vertecies.size();
	}

	std::vector<float> get_vertex(int at, int vpos)
	{
		const RenderedBuffer& buffer = BridgeParameter::instance().render_buffer(at);
		float x = buffer.vertecies[vpos].x;
		float y = buffer.vertecies[vpos].y;
		float z = buffer.vertecies[vpos].z;
		std::vector<float> result;
		result.push_back(x);
		result.push_back(y);
		result.push_back(z);
		return result;
	}

	size_t get_normal_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).normals.size();
	}

	std::vector<float> get_normal(int at, int vpos)
	{
		const RenderedBuffer& buffer = BridgeParameter::instance().render_buffer(at);
		float x = buffer.normals[vpos].x;
		float y = buffer.normals[vpos].y;
		float z = buffer.normals[vpos].z;
		std::vector<float> result;
		result.push_back(x);
		result.push_back(y);
		result.push_back(z);
		return result;
	}

	size_t get_uv_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).uvs.size();
	}

	std::vector<float> get_uv(int at, int vpos)
	{
		const RenderedBuffer& buffer = BridgeParameter::instance().render_buffer(at);
		float u = buffer.uvs[vpos].x;
		float v = buffer.uvs[vpos].y;
		std::vector<float> result;
		result.push_back(u);
		result.push_back(v);
		return result;
	}

	size_t get_material_size(int at)
	{
		return BridgeParameter::instance().render_buffer(at).materials.size();
	}

	bool is_accessory(int at)
	{
		if (BridgeParameter::instance().render_buffer(at).isAccessory)
		{
			return true;
		}
		return false;
	}

	int get_pre_accessory_count()
	{
		return ExpGetPreAcsNum();
	}

	std::vector<float> get_diffuse(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->diffuse.x);
		result.push_back(mat->diffuse.y);
		result.push_back(mat->diffuse.z);
		result.push_back(mat->diffuse.w);
		return result;
	}

	std::vector<float> get_ambient(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->ambient.x);
		result.push_back(mat->ambient.y);
		result.push_back(mat->ambient.z);
		return result;
	}

	std::vector<float> get_specular(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->specular.x);
		result.push_back(mat->specular.y);
		result.push_back(mat->specular.z);
		return result;
	}

	std::vector<float> get_emissive(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		std::vector<float> result;
		result.push_back(mat->emissive.x);
		result.push_back(mat->emissive.y);
		result.push_back(mat->emissive.z);
		return result;
	}

	float get_power(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		float power = mat->power;
		return power;
	}

	std::wstring get_texture(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		return mat->texture;
	}

	std::string get_exported_texture(int at, int mpos)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];
		return mat->memoryTexture;
	}

	size_t get_face_size(int at, int mpos)
	{
		return BridgeParameter::instance().render_buffer(at).materials[mpos]->surface.faces.size();
	}

	std::vector<int> get_face(int at, int mpos, int fpos)
	{
		RenderedSurface& surface = BridgeParameter::instance().render_buffer(at).materials[mpos]->surface;
		int v1 = surface.faces[fpos].x;
		int v2 = surface.faces[fpos].y;
		int v3 = surface.faces[fpos].z;
		std::vector<int> result;
		result.push_back(v1);
		result.push_back(v2);
		result.push_back(v3);
		return result;
	}

	size_t get_texture_buffer_size()
	{
		return finishTextureBuffers.size();
	}

	std::vector<int> get_texture_size(int at)
	{
		std::vector<int> result;
		result.push_back(renderedTextures[finishTextureBuffers[at].first].size.x);
		result.push_back(renderedTextures[finishTextureBuffers[at].first].size.y);
		return result;
	}

	std::wstring get_texture_name(int at)
	{
		return renderedTextures[finishTextureBuffers[at].first].name;
	}

	std::vector<float> get_texture_pixel(int at, int tpos)
	{
		UMVec4f& rgba = renderedTextures[finishTextureBuffers[at].first].texture[tpos];
		std::vector<float> result;
		result.push_back(rgba.x);
		result.push_back(rgba.y);
		result.push_back(rgba.z);
		result.push_back(rgba.w);
		return result;
	}

	bool export_texture(int at, int mpos, const std::wstring& dst)
	{
		const auto& mat = BridgeParameter::instance().render_buffer(at).materials[mpos];

		std::wstring_view textureType{ dst.c_str() + dst.size() - 3, 3 };
		D3DXIMAGE_FILEFORMAT fileFormat;
		if (textureType == L"bmp" || textureType == L"BMP")
		{
			fileFormat = D3DXIFF_BMP;
		}
		else if (textureType.compare(L"png") == 0 || textureType.compare(L"PNG") == 0)
		{
			fileFormat = D3DXIFF_PNG;
		}
		else if (textureType.compare(L"jpg") == 0 || textureType.compare(L"JPG") == 0)
		{
			fileFormat = D3DXIFF_JPG;
		}
		else if (textureType.compare(L"tga") == 0 || textureType.compare(L"TGA") == 0)
		{
			fileFormat = D3DXIFF_TGA;
		}
		else if (textureType.compare(L"dds") == 0 || textureType.compare(L"DDS") == 0)
		{
			fileFormat = D3DXIFF_DDS;
		}
		else if (textureType.compare(L"ppm") == 0 || textureType.compare(L"PPM") == 0)
		{
			fileFormat = D3DXIFF_PPM;
		}
		else if (textureType.compare(L"dib") == 0 || textureType.compare(L"DIB") == 0)
		{
			fileFormat = D3DXIFF_DIB;
		}
		else if (textureType.compare(L"hdr") == 0 || textureType.compare(L"HDR") == 0)
		{
			fileFormat = D3DXIFF_HDR;
		}
		else if (textureType.compare(L"pfm") == 0 || textureType.compare(L"PFM") == 0)
		{
			fileFormat = D3DXIFF_PFM;
		}
		else
		{
			return false;
		}

		if (mat->tex)
		{
			return writeTextureToFile(dst.c_str(), mat->tex, fileFormat);
		}
		return false;
	}

	bool export_textures(const std::wstring& p, const std::wstring& t)
	{
		if (umbase::UMPath::exists(p.c_str()))
		{
			return writeTextureToFiles(p, t);
		}
		return false;
	}

	bool export_uncopied_textures(const std::wstring& p, const std::wstring& t)
	{
		if (umbase::UMPath::exists(p.c_str()))
		{
			return writeTextureToFiles(p, t, true);
		}
		return false;
	}

	bool copy_textures(const std::wstring& s)
	{
		if (umbase::UMPath::exists(s.c_str()))
		{
			return copyTextureToFiles(s);
		}
		return false;
	}

	std::wstring get_base_path()
	{
		return BridgeParameter::instance().base_path;
	}

	std::vector<float> get_camera_up()
	{
		D3DXVECTOR3 v;
		D3DXVECTOR3 dst;
		UMGetCameraUp(&v);
		d3d_vector3_dir_transform(dst, v, BridgeParameter::instance().first_noaccessory_buffer().world_inv);
		std::vector<float> result;
		result.push_back(dst.x);
		result.push_back(dst.y);
		result.push_back(dst.z);
		return result;
	}

	std::vector<float> get_camera_up_org()
	{
		D3DXVECTOR3 v;
		UMGetCameraUp(&v);
		std::vector<float> result;
		result.push_back(v.x);
		result.push_back(v.y);
		result.push_back(v.z);
		return result;
	}

	std::vector<float> get_camera_at()
	{
		D3DXVECTOR3 v;
		D3DXVECTOR3 dst;
		UMGetCameraAt(&v);
		d3d_vector3_transform(dst, v, BridgeParameter::instance().first_noaccessory_buffer().world_inv);
		std::vector<float> result;
		result.push_back(dst.x);
		result.push_back(dst.y);
		result.push_back(dst.z);
		return result;
	}

	std::vector<float> get_camera_eye()
	{
		D3DXVECTOR3 v;
		D3DXVECTOR3 dst;
		UMGetCameraEye(&v);
		d3d_vector3_transform(dst, v, BridgeParameter::instance().first_noaccessory_buffer().world_inv);
		std::vector<float> result;
		result.push_back(dst.x);
		result.push_back(dst.y);
		result.push_back(dst.z);
		return result;
	}

	std::vector<float> get_camera_eye_org()
	{
		D3DXVECTOR3 v;
		UMGetCameraEye(&v);
		std::vector<float> result;
		result.push_back(v.x);
		result.push_back(v.y);
		result.push_back(v.z);
		return result;
	}

	float get_camera_fovy()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.x;
	}

	float get_camera_aspect()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.y;
	}

	float get_camera_near()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.z;
	}

	float get_camera_far()
	{
		D3DXVECTOR4 v;
		UMGetCameraFovLH(&v);
		return v.w;
	}

	int get_frame_number()
	{
		if (process_frame >= 0)
		{
			return process_frame;
		}
		else
		{
			return ui_frame;
		}
	}

	int get_start_frame()
	{
		return BridgeParameter::instance().start_frame;
	}

	int get_end_frame()
	{
		return BridgeParameter::instance().end_frame;
	}

	int get_frame_width()
	{
		return BridgeParameter::instance().frame_width;
	}

	int get_frame_height()
	{
		return BridgeParameter::instance().frame_height;
	}

	double get_export_fps()
	{
		return BridgeParameter::instance().export_fps;
	}

	std::vector<float> get_light(int at)
	{
		const UMVec3f& light = BridgeParameter::instance().render_buffer(at).light;
		std::vector<float> result;
		result.push_back(light.x);
		result.push_back(light.y);
		result.push_back(light.z);
		return result;
	}

	std::vector<float> get_light_color(int at)
	{
		const UMVec3f& light = BridgeParameter::instance().render_buffer(at).light_color;
		std::vector<float> result;
		result.push_back(light.x);
		result.push_back(light.y);
		result.push_back(light.z);
		return result;
	}

	int get_object_size()
	{
		return ExpGetPmdNum();
	}

	int get_bone_size(int at)
	{
		return ExpGetPmdBoneNum(at);
	}

	int get_accessory_size()
	{
		return ExpGetAcsNum();
	}

	std::string get_accessory_filename(int at)
	{
		const char* cp932 = ExpGetAcsFilename(at);
		if (!cp932 || *cp932 == '\0')
		{
			return "";
		}

		std::string utf8_filename;
		oguna::EncodingConverter::Cp932ToUtf8(cp932, static_cast<int>(strlen(cp932)), &utf8_filename);
		return utf8_filename;
	}

	std::string get_object_filename(int at)
	{
		const int count = get_bone_size(at);
		if (count <= 0)
			return "";

		const char* utf8_filename = ExpGetPmdFilenameUtf8(at);
		if (utf8_filename)
		{
			return std::string(utf8_filename);
		}
		return "";
	}

	std::string get_buffer_filename(int at)
	{
		auto& buffer = BridgeParameter::instance().render_buffer(at);
		if (buffer.isAccessory)
		{
			return get_accessory_filename(buffer.order);
		}
		else
		{
			return get_object_filename(buffer.order);
		}
	}

	std::string get_bone_name(int at, int bone_index)
	{
		const int count = get_bone_size(at);
		if (count <= 0)
			return "";

		const char* cp932 = ExpGetPmdBoneName(at, bone_index);
		if (!cp932 || *cp932 == '\0')
		{
			return "";
		}

		std::string utf8_name;
		oguna::EncodingConverter::Cp932ToUtf8(cp932, static_cast<int>(strlen(cp932)), &utf8_name);
		return utf8_name;
	}

	std::vector<float> get_bone_matrix(int at, int bone_index)
	{
		const int count = get_bone_size(at);
		std::vector<float> result;
		if (count <= 0)
			return result;

		D3DMATRIX mat = ExpGetPmdBoneWorldMat(at, bone_index);
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(mat.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_world(int at)
	{
		const D3DXMATRIX& world = BridgeParameter::instance().render_buffer(at).world;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(world.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_world_inv(int at)
	{
		const D3DXMATRIX& world_inv = BridgeParameter::instance().render_buffer(at).world_inv;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(world_inv.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_view(int at)
	{
		const D3DXMATRIX& view = BridgeParameter::instance().render_buffer(at).view;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(view.m[i][k]);
			}
		}
		return result;
	}

	std::vector<float> get_projection(int at)
	{
		const D3DXMATRIX& projection = BridgeParameter::instance().render_buffer(at).projection;
		std::vector<float> result;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(projection.m[i][k]);
			}
		}
		return result;
	}

	std::vector<double> invert_matrix(const std::vector<double>& tp1)
	{
		if (tp1.size() < 16)
		{
			PyErr_SetString(PyExc_IndexError, "index out of range");
			throw py::error_already_set();
		}
		std::vector<double> result;
		UMMat44d src;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				src[i][k] = static_cast<double>(tp1[i * 4 + k]);
			}
		}
		const UMMat44d dst = src.inverted();
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				result.push_back(dst[i][k]);
			}
		}
		return result;
	}

	std::vector<double> extract_xyz_degree(const std::vector<double>& tp1)
	{
		if (tp1.size() < 16)
		{
			PyErr_SetString(PyExc_IndexError, "index out of range");
			throw py::error_already_set();
		}
		std::vector<double> result;
		UMMat44d src;
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 4; ++k)
			{
				src[i][k] = static_cast<double>(tp1[i * 4 + k]);
			}
		}

		const UMVec3d euler = umbase::um_matrix_to_euler_xyz(src);
		for (int i = 0; i < 3; ++i)
		{
			result.push_back(umbase::um_to_degree(euler[i]));
		}
		return result;
	}

	bool set_texture_buffer_enabled(bool enabled)
	{
		BridgeParameter::mutable_instance().is_texture_buffer_enabled = enabled;
		return true;
	}

	bool set_int_value(int pos, int value)
	{
		BridgeParameter::mutable_instance().py_int_map[pos] = value;
		return true;
	}

	bool set_float_value(int pos, float value)
	{
		BridgeParameter::mutable_instance().py_float_map[pos] = value;
		return true;
	}

	int get_int_value(int pos)
	{
		if (BridgeParameter::instance().py_int_map.find(pos) != BridgeParameter::instance().py_int_map.end())
		{
			return BridgeParameter::mutable_instance().py_int_map[pos];
		}
		return 0;
	}

	float get_float_value(int pos)
	{
		if (BridgeParameter::instance().py_float_map.find(pos) != BridgeParameter::instance().py_float_map.end())
		{
			return BridgeParameter::mutable_instance().py_float_map[pos];
		}
		return 0;
	}

	std::vector<float> d3dx_vec3_normalize(float x, float y, float z)
	{
		D3DXVECTOR3 vec(x, y, z);
		::D3DXVec3Normalize(&vec, &vec);
		std::vector<float> result;
		result.push_back(vec.x);
		result.push_back(vec.y);
		result.push_back(vec.z);
		return result;
	}
} // namespace

PYBIND11_MAKE_OPAQUE(std::vector<float>);
PYBIND11_MAKE_OPAQUE(std::vector<int>);

PYBIND11_MODULE(mmdbridge, m)
{
	m.doc() = "MMD Bridge main module";

	m.def("get_vertex_buffer_size", get_vertex_buffer_size);
	m.def("get_vertex_size", get_vertex_size);
	m.def("get_vertex", get_vertex);
	m.def("get_normal_size", get_normal_size);
	m.def("get_normal", get_normal);
	m.def("get_uv_size", get_uv_size);
	m.def("get_uv", get_uv);
	m.def("get_material_size", get_material_size);
	m.def("is_accessory", is_accessory);
	m.def("get_pre_accessory_count", get_pre_accessory_count);
	m.def("get_ambient", get_ambient);
	m.def("get_diffuse", get_diffuse);
	m.def("get_specular", get_specular);
	m.def("get_emissive", get_emissive);
	m.def("get_power", get_power);
	m.def("get_texture", get_texture);
	m.def("get_exported_texture", get_exported_texture);
	m.def("get_face_size", get_face_size);
	m.def("get_face", get_face);
	m.def("get_texture_buffer_size", get_texture_buffer_size);
	m.def("get_texture_size", get_texture_size);
	m.def("get_texture_name", get_texture_name);
	m.def("get_texture_pixel", get_texture_pixel);
	m.def("get_camera_up", get_camera_up);
	m.def("get_camera_up_org", get_camera_up_org);
	m.def("get_camera_at", get_camera_at);
	m.def("get_camera_eye", get_camera_eye);
	m.def("get_camera_eye_org", get_camera_eye_org);
	m.def("get_camera_fovy", get_camera_fovy);
	m.def("get_camera_aspect", get_camera_aspect);
	m.def("get_camera_near", get_camera_near);
	m.def("get_camera_far", get_camera_far);
	m.def("messagebox", messagebox,
		  py::arg("message"),
		  py::arg("title") = L"MMDBridge");
	m.def("export_texture", export_texture);
	m.def("export_textures", export_textures);
	m.def("export_uncopied_textures", export_textures);
	m.def("copy_textures", copy_textures);
	m.def("get_frame_number", get_frame_number);
	m.def("get_start_frame", get_start_frame);
	m.def("get_end_frame", get_end_frame);
	m.def("get_frame_width", get_frame_width);
	m.def("get_frame_height", get_frame_height);
	m.def("get_export_fps", get_export_fps);
	m.def("get_base_path", get_base_path);
	m.def("get_light", get_light);
	m.def("get_light_color", get_light_color);
	m.def("get_buffer_filename", get_buffer_filename);
	m.def("get_accessory_size", get_accessory_size);
	m.def("get_accessory_filename", get_accessory_filename);
	m.def("get_object_size", get_object_size);
	m.def("get_object_filename", get_object_filename);
	m.def("get_bone_size", get_bone_size);
	m.def("get_bone_name", get_bone_name);
	m.def("get_bone_matrix", get_bone_matrix);
	m.def("get_world", get_world);
	m.def("get_world_inv", get_world_inv);
	m.def("get_view", get_view);
	m.def("get_projection", get_projection);
	m.def("set_texture_buffer_enabled", set_texture_buffer_enabled);
	m.def("set_int_value", set_int_value);
	m.def("set_float_value", set_float_value);
	m.def("get_int_value", get_int_value);
	m.def("get_float_value", get_float_value);
	m.def("extract_xyz_degree", extract_xyz_degree);
	m.def("invert_matrix", invert_matrix);
	m.def("d3dx_vec3_normalize", d3dx_vec3_normalize);

	py::bind_vector<std::vector<float>>(m, "VectorFloat");
	py::bind_vector<std::vector<int>>(m, "VectorInt");
}

void run_python_script()
{
	if (BridgeParameter::instance().mmdbridge_python_script.empty())
	{
		return;
	}

	if (!Py_IsInitialized())
	{
		InitAlembic();
		InitVMD();
		InitPMX();
		PyImport_AppendInittab("mmdbridge", PyInit_mmdbridge);

		// Use modern PyConfig API
		PyConfig config;
		PyConfig_InitIsolatedConfig(&config);

		// Set various configurations
		config.inspect = 0;

		// Set command line arguments
		const std::wstring wpath = BridgeParameter::instance().base_path;
		wchar_t* argv[] = { const_cast<wchar_t*>(wpath.c_str()) };
		PyConfig_SetArgv(&config, 1, argv);

		// Initialize Python
		PyStatus status = Py_InitializeFromConfig(&config);
		PyConfig_Clear(&config);

		if (PyStatus_Exception(status))
		{
			::MessageBoxW(NULL, L"Failed to initialize Python", L"Python Error", MB_OK);
			return;
		}
	}

	try
	{
		// Module initialization.
		auto global = py::dict(py::module::import("__main__").attr("__dict__"));
		auto script = BridgeParameter::instance().mmdbridge_python_script;
		// Script execution.
		auto res = py::eval<py::eval_statements>(
			script.c_str(),
			global);
	}
	catch (py::error_already_set const& ex)
	{
		std::stringstream error_report;
		error_report << "----------- MMDBridge Python/C++ Error Report -----------\n\n";
		error_report << "[C++ Exception Info]\n"
					 << ex.what() << "\n\n";
		error_report << "[Problematic Python Code Line]\n";
		std::string what_str = ex.what();
		std::regex re(R"((?:[\s\S]*)\((\d+)\):)");
		std::smatch match;
		if (std::regex_search(what_str, match, re) && match.size() > 1)
		{
			try
			{
				int reported_line_number = std::stoi(match[1].str());
				int trigger_line_number = std::max(1, reported_line_number - 1);

				const std::string& full_script = BridgeParameter::instance().mmdbridge_python_script;
				std::stringstream script_stream(full_script);
				std::string script_line;
				std::vector<std::string> lines;
				while (std::getline(script_stream, script_line, '\n'))
				{
					if (!script_line.empty() && script_line.back() == '\r')
					{
						script_line.pop_back();
					}
					lines.push_back(script_line);
				}
				error_report << "Error likely triggered by call on line " << trigger_line_number
							 << " (execution was halted before line " << reported_line_number << "):\n";
				error_report << "--------------------------------------------------\n";

				int start_line = std::max(1, trigger_line_number - 2);
				int end_line = std::min((int)lines.size(), trigger_line_number + 2);
				for (int i = start_line; i <= end_line; ++i)
				{
					if (i == trigger_line_number)
					{
						error_report << ">> " << std::setw(3) << i << ": " << lines[i - 1] << "\n";
					}
					else
					{
						error_report << "   " << std::setw(3) << i << ": " << lines[i - 1] << "\n";
					}
				}
				error_report << "--------------------------------------------------\n\n";
			}
			catch (const std::exception& e)
			{
				error_report << "Could not parse source code line: " << e.what() << "\n\n";
			}
		}
		else
		{
			error_report << "Could not parse line number from the error message.\n\n";
		}
		error_report << "[Python Traceback]\n";
		if (PyErr_Occurred())
		{
			try
			{
				py::object traceback = py::module::import("traceback");
				py::object format_exc = traceback.attr("format_exc");
				std::string full_traceback = py::str(format_exc());
				error_report << full_traceback;
			}
			catch (...)
			{
				error_report << "Failed to import the Python 'traceback' module.\n";
			}
			PyErr_Clear();
		}
		else
		{
			error_report << "No Python traceback available (PyErr_Occurred() returned false).\n";
		}
		std::string error_report_utf8 = error_report.str();

		std::wstring wide_error_message = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(error_report_utf8));
		::MessageBoxW(NULL, wide_error_message.c_str(), L"MMDBridge Detailed Error Report", MB_OK | MB_ICONERROR);
	}
}
//-----------------------------------------------------------Hook function pointers-----------------------------------------------------------

// Direct3DCreate9
IDirect3D9*(WINAPI* original_direct3d_create)(UINT)(NULL);

HRESULT(WINAPI* original_direct3d9ex_create)(UINT, IDirect3D9Ex**)(NULL);
// IDirect3D9::CreateDevice
HRESULT(WINAPI* original_create_device)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**)(NULL);

HRESULT(WINAPI* original_create_deviceex)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**)(NULL);
// IDirect3DDevice9::BeginScene
HRESULT(WINAPI* original_begin_scene)(IDirect3DDevice9*)(NULL);
// IDirect3DDevice9::EndScene
HRESULT(WINAPI* original_end_scene)(IDirect3DDevice9*)(NULL);
// IDirect3DDevice9::SetFVF
HRESULT(WINAPI* original_set_fvf)(IDirect3DDevice9*, DWORD);
// IDirect3DDevice9::Clear
HRESULT(WINAPI* original_clear)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD)(NULL);
// IDirect3DDevice9::Present
HRESULT(WINAPI* original_present)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*)(NULL);
// IDirect3DDevice9::Reset
HRESULT(WINAPI* original_reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)(NULL);

// IDirect3DDevice9::BeginStateBlock
// This function modifies lpVtbl, so we need to restore lpVtbl
HRESULT(WINAPI* original_begin_state_block)(IDirect3DDevice9*)(NULL);

// IDirect3DDevice9::EndStateBlock
// This function modifies lpVtbl, so we need to restore lpVtbl
HRESULT(WINAPI* original_end_state_block)(IDirect3DDevice9*, IDirect3DStateBlock9**)(NULL);

// IDirect3DDevice9::DrawIndexedPrimitive
HRESULT(WINAPI* original_draw_indexed_primitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT)(NULL);

// IDirect3DDevice9::SetStreamSource
HRESULT(WINAPI* original_set_stream_source)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT)(NULL);

// IDirect3DDevice9::SetIndices
HRESULT(WINAPI* original_set_indices)(IDirect3DDevice9*, IDirect3DIndexBuffer9*)(NULL);

// IDirect3DDevice9::CreateVertexBuffer
HRESULT(WINAPI* original_create_vertex_buffer)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*)(NULL);

// IDirect3DDevice9::SetTexture
HRESULT(WINAPI* original_set_texture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*)(NULL);

// IDirect3DDevice9::CreateTexture
HRESULT(WINAPI* original_create_texture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*)(NULL);
//-----------------------------------------------------------------------------------------------------------------------------

static bool writeTextureToFile(const wchar_t* texturePath, IDirect3DTexture9* texture, D3DXIMAGE_FILEFORMAT fileFormat)
{
	TextureBuffers::iterator tit = renderData.textureBuffers.find(texture);
	if (tit != renderData.textureBuffers.end())
	{
		if (texture->lpVtbl)
		{
			HRESULT res = D3DXSaveTextureToFileW(texturePath, fileFormat, (LPDIRECT3DBASETEXTURE9)texture, NULL);
			if (res == S_OK)
			{
				return true;
			}
		}
	}
	return false;
}

static bool writeTextureToFiles(const std::wstring& texturePath, const std::wstring& textureType, bool uncopied)
{
	bool res = true;

	D3DXIMAGE_FILEFORMAT fileFormat;
	if (textureType == L"bmp" || textureType == L"BMP")
	{
		fileFormat = D3DXIFF_BMP;
	}
	else if (textureType.compare(L"png") == 0 || textureType.compare(L"PNG") == 0)
	{
		fileFormat = D3DXIFF_PNG;
	}
	else if (textureType.compare(L"jpg") == 0 || textureType.compare(L"JPG") == 0)
	{
		fileFormat = D3DXIFF_JPG;
	}
	else if (textureType.compare(L"tga") == 0 || textureType.compare(L"TGA") == 0)
	{
		fileFormat = D3DXIFF_TGA;
	}
	else if (textureType.compare(L"dds") == 0 || textureType.compare(L"DDS") == 0)
	{
		fileFormat = D3DXIFF_DDS;
	}
	else if (textureType.compare(L"ppm") == 0 || textureType.compare(L"PPM") == 0)
	{
		fileFormat = D3DXIFF_PPM;
	}
	else if (textureType.compare(L"dib") == 0 || textureType.compare(L"DIB") == 0)
	{
		fileFormat = D3DXIFF_DIB;
	}
	else if (textureType.compare(L"hdr") == 0 || textureType.compare(L"HDR") == 0)
	{
		fileFormat = D3DXIFF_HDR;
	}
	else if (textureType.compare(L"pfm") == 0 || textureType.compare(L"PFM") == 0)
	{
		fileFormat = D3DXIFF_PFM;
	}
	else
	{
		return false;
	}

	wchar_t dir[MAX_PATH];
	wcscpy_s(dir, MAX_PATH, texturePath.c_str());
	PathRemoveFileSpecW(dir);

	for (size_t i = 0; i < finishTextureBuffers.size(); ++i)
	{
		IDirect3DTexture9* texture = finishTextureBuffers[i].first;
		bool copied = finishTextureBuffers[i].second;
		if (texture)
		{
			if (uncopied)
			{
				if (!copied)
				{
					wchar_t path[MAX_PATH];
					PathCombineW(path, dir, to_wstring(texture).c_str());
					std::wstring wpath(path);
					wpath += L"." + textureType;
					if (!writeTextureToFile(wpath.c_str(), texture, fileFormat))
					{
						res = false;
					}
				}
			}
			else
			{
				wchar_t path[MAX_PATH];
				PathCombineW(path, dir, to_wstring(texture).c_str());
				std::wstring wpath(path);
				wpath += L"." + textureType;
				if (!writeTextureToFile(wpath.c_str(), texture, fileFormat))
				{
					res = false;
				}
			}
		}
	}

	return res;
}

static bool copyTextureToFiles(const std::wstring& texturePath)
{
	if (texturePath.empty())
		return false;

	std::wstring path = texturePath;
	PathRemoveFileSpecW(&path[0]);
	PathAddBackslashW(&path[0]);
	if (!PathIsDirectoryW(path.c_str()))
	{
		return false;
	}

	bool res = true;
	for (size_t i = 0; i < finishTextureBuffers.size(); ++i)
	{
		IDirect3DTexture9* texture = finishTextureBuffers[i].first;
		if (texture)
		{
			if (!UMCopyTexture(path.c_str(), texture))
			{
				res = false;
			}
		}
	}
	return res;
}

static bool writeTextureToMemory(const std::wstring& textureName, IDirect3DTexture9* texture, bool copied)
{
	// Check if already in finishTextureBuffer
	bool found = false;
	for (size_t i = 0; i < finishTextureBuffers.size(); ++i)
	{
		if (finishTextureBuffers[i].first == texture)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		// Not written out yet, so add to write file list
		std::pair<IDirect3DTexture9*, bool> texturebuffer(texture, copied);
		finishTextureBuffers.push_back(texturebuffer);
	}

	if (BridgeParameter::instance().is_texture_buffer_enabled)
	{
		TextureBuffers::iterator tit = renderData.textureBuffers.find(texture);
		if (tit != renderData.textureBuffers.end())
		{
			// Write texture to memory
			D3DLOCKED_RECT lockRect;
			HRESULT isLocked = texture->lpVtbl->LockRect(texture, 0, &lockRect, NULL, D3DLOCK_READONLY);
			if (isLocked != D3D_OK)
			{
				return false;
			}

			int width = tit->second.wh.x;
			int height = tit->second.wh.y;

			RenderedTexture tex;
			tex.size.x = width;
			tex.size.y = height;
			tex.name = textureName;

			D3DFORMAT format = tit->second.format;
			for (int y = 0; y < height; y++)
			{
				unsigned char* lineHead = (unsigned char*)lockRect.pBits + lockRect.Pitch * y;

				for (int x = 0; x < width; x++)
				{
					if (format == D3DFMT_A8R8G8B8)
					{
						UMVec4f rgba;
						rgba.x = lineHead[4 * x + 0];
						rgba.y = lineHead[4 * x + 1];
						rgba.z = lineHead[4 * x + 2];
						rgba.w = lineHead[4 * x + 3];
						tex.texture.push_back(rgba);
					}
					else
					{
						std::stringstream ss;
						ss << "Not supported texture format: " << format;
						std::wstring error_message = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(ss.str()));
						::MessageBoxW(NULL, error_message.c_str(), L"Info", MB_OK);
					}
				}
			}
			renderedTextures[texture] = tex;

			texture->lpVtbl->UnlockRect(texture, 0);
			return true;
		}
	}
	return false;
}

static HRESULT WINAPI beginScene(IDirect3DDevice9* device)
{
	HRESULT res = (*original_begin_scene)(device);
	return res;
}

static HRESULT WINAPI endScene(IDirect3DDevice9* device)
{
	HRESULT res = (*original_end_scene)(device);
	return res;
}

static void GetFrame(HWND hWnd)
{
	WCHAR text[256];
	::GetWindowTextW(hWnd, text, sizeof(text) / sizeof(text[0]));
	ui_frame = _wtoi(text);
}

static BOOL CALLBACK enumChildWindowsProc(HWND hWnd, LPARAM lParam)
{
	RECT rect;
	GetClientRect(hWnd, &rect);

	if (!g_hFrame && rect.right == 48 && rect.bottom == 22)
	{
		g_hFrame = hWnd;
		GetFrame(hWnd);
	}
	if (g_hFrame)
	{
		return FALSE;
	}
	return TRUE; // continue
}

// Search for target window to hijack
static BOOL CALLBACK enumWindowsProc(HWND hWnd, LPARAM lParam)
{
	if (g_hWnd && g_hFrame)
	{
		GetFrame(g_hFrame);
		return FALSE;
	}
	HANDLE hModule = (HANDLE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
	if (GetModuleHandle(NULL) == hModule)
	{
		// Found window created by our process
		WCHAR szClassName[256];
		GetClassNameW(hWnd, szClassName, sizeof(szClassName) / sizeof(szClassName[0]));
		std::wstring name = szClassName;
		if (name == L"Polygon Movie Maker")
		{
			g_hWnd = hWnd;
			EnumChildWindows(hWnd, enumChildWindowsProc, 0);
			return FALSE; // break
		}
	}
	return TRUE; // continue
}

// Get the full path to the settings file corresponding to the current EXE
std::wstring GetSettingsFilePath()
{
	wchar_t module_path[MAX_PATH];
	GetModuleFileNameW(hInstance, module_path, MAX_PATH);
	PathRemoveFileSpecW(module_path);

	wchar_t settings_dir[MAX_PATH];
	PathCombineW(settings_dir, module_path, L"mmdbridge_settings");

	if (!PathFileExistsW(settings_dir))
	{
		CreateDirectoryW(settings_dir, NULL);
	}

	// Get host executable filename (e.g., MikuMikuDance.exe)
	wchar_t exe_path[MAX_PATH];
	GetModuleFileNameW(NULL, exe_path, MAX_PATH);
	const wchar_t* exe_filename = PathFindFileNameW(exe_path);

	// Build settings file path: settings_dir\{exe_filename}.ini
	wchar_t final_ini_path[MAX_PATH];
	PathCombineW(final_ini_path, settings_dir, (std::wstring(exe_filename) + L".ini").c_str());

	return final_ini_path;
}

// Load settings from INI file
void LoadSettings()
{
	std::wstring ini_path = GetSettingsFilePath();
	BridgeParameter& mutable_parameter = BridgeParameter::mutable_instance();

	wchar_t buffer[MAX_PATH];
	int code_page;

	// Settings
	GetPrivateProfileStringW(L"Settings", L"ScriptName", L"", buffer, MAX_PATH, ini_path.c_str());
	mutable_parameter.python_script_name = buffer;
	script_call_setting = GetPrivateProfileIntW(L"Settings", L"ScriptCallSetting", 1, ini_path.c_str());
	mutable_parameter.start_frame = GetPrivateProfileIntW(L"Settings", L"StartFrame", 0, ini_path.c_str());
	mutable_parameter.end_frame = GetPrivateProfileIntW(L"Settings", L"EndFrame", 0, ini_path.c_str());
	GetPrivateProfileStringW(L"Settings", L"ExportFPS", L"30.0", buffer, MAX_PATH, ini_path.c_str());
	mutable_parameter.export_fps = _wtof(buffer);

	// Encoding
	mutable_parameter.encoding_map.clear();
	GetPrivateProfileStringW(L"Encoding", L"ModifyMenuA", L"0", buffer, 16, ini_path.c_str());
	code_page = _wtoi(buffer);
	if (IsValidCodePage(code_page))
	{
		mutable_parameter.encoding_map[L"ModifyMenuA"] = code_page;
	}
	GetPrivateProfileStringW(L"Encoding", L"SetMenuItemInfoA", L"0", buffer, 16, ini_path.c_str());
	code_page = _wtoi(buffer);
	if (IsValidCodePage(code_page))
	{
		mutable_parameter.encoding_map[L"SetMenuItemInfoA"] = code_page;
	}
	GetPrivateProfileStringW(L"Encoding", L"SendMessageA", L"0", buffer, 16, ini_path.c_str());
	code_page = _wtoi(buffer);
	if (IsValidCodePage(code_page))
	{
		mutable_parameter.encoding_map[L"SendMessageA"] = code_page;
	}
	GetPrivateProfileStringW(L"Encoding", L"GetWindowTextA", L"0", buffer, 16, ini_path.c_str());
	code_page = _wtoi(buffer);
	if (IsValidCodePage(code_page))
	{
		mutable_parameter.encoding_map[L"GetWindowTextA"] = code_page;
	}
	GetPrivateProfileStringW(L"Encoding", L"MessageBoxA", L"0", buffer, 16, ini_path.c_str());
	code_page = _wtoi(buffer);
	if (IsValidCodePage(code_page))
	{
		mutable_parameter.encoding_map[L"MessageBoxA"] = code_page;
	}
}

// Save current settings to INI file
void SaveSettings()
{
	std::wstring ini_path = GetSettingsFilePath();
	const BridgeParameter& parameter = BridgeParameter::instance();

	WritePrivateProfileStringW(L"Settings", L"ScriptName", parameter.python_script_name.c_str(), ini_path.c_str());
	WritePrivateProfileStringW(L"Settings", L"ScriptCallSetting", std::to_wstring(script_call_setting).c_str(), ini_path.c_str());
	WritePrivateProfileStringW(L"Settings", L"StartFrame", std::to_wstring(parameter.start_frame).c_str(), ini_path.c_str());
	WritePrivateProfileStringW(L"Settings", L"EndFrame", std::to_wstring(parameter.end_frame).c_str(), ini_path.c_str());
	std::wstringstream wss;
	wss << parameter.export_fps;
	WritePrivateProfileStringW(L"Settings", L"ExportFPS", wss.str().c_str(), ini_path.c_str());
}

static void setMyMenu()
{
	if (g_hMenu)
		return;
	if (g_hWnd)
	{
		HMENU hmenu = GetMenu(g_hWnd);
		HMENU hsubs = CreatePopupMenu();
		int count = GetMenuItemCount(hmenu);

		MENUITEMINFO minfo;
		minfo.cbSize = sizeof(MENUITEMINFO);
		minfo.fMask = MIIM_ID | MIIM_TYPE | MIIM_SUBMENU;
		minfo.fType = MFT_STRING;
		wchar_t bridgeMenuText[] = L"MMDBridge";
		minfo.dwTypeData = bridgeMenuText;
		minfo.hSubMenu = hsubs;

		InsertMenuItem(hmenu, count + 1, TRUE, &minfo);
		minfo.fMask = MIIM_ID | MIIM_TYPE;
		minfo.dwTypeData = L"Plugin Settings";
		minfo.wID = 1020;
		InsertMenuItem(hsubs, 1, TRUE, &minfo);

		SetMenu(g_hWnd, hmenu);
		g_hMenu = hmenu;
	}
}

static void setMySize()
{
	if (!g_hWnd)
		return;
	RECT rc;
	if (!::GetWindowRect(g_hWnd, &rc))
		return;
	if (rc.bottom - rc.top <= 40)
		SetWindowPos(g_hWnd, HWND_TOP, 0, 0, 1920, 1080, NULL);
}

static void OpenSettingsDialog(HWND hWnd)
{
	std::wstring ini_path = GetSettingsFilePath();
	wchar_t lang_code[16];

	// Check if MMD is in English mode first.
	if (ExpGetEnglishMode())
	{
		wcscpy_s(lang_code, L"en-US");
	}
	else
	{
		// Read the language setting from the INI file. Defaults to "ja-JP" if not found.
		GetPrivateProfileStringW(L"Localization", L"Language", L"ja-JP", lang_code, 16, ini_path.c_str());
	}
	lang_code[15] = L'\0';

	LCID locale_id = LocaleNameToLCID(lang_code, 0);
	LANGID target_lang_id = MAKELANGID(PRIMARYLANGID(locale_id), SUBLANGID(locale_id));
	LANGID original_lang_id = GetThreadUILanguage();

	SetThreadUILanguage(target_lang_id);
	::DialogBoxW(hInstance, L"IDD_DIALOG1", hWnd, DialogProc);
	SetThreadUILanguage(original_lang_id);
}

static LRESULT CALLBACK overrideWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
		case WM_INITMENUPOPUP:
		{
			// Update menu text based on language settings
			HMENU hPopupMenu = (HMENU)wp;
			MENUITEMINFOW mii_check = { sizeof(mii_check) };
			mii_check.fMask = MIIM_ID;

			if (GetMenuItemInfoW(hPopupMenu, 1020, FALSE, &mii_check))
			{
				std::wstring ini_path = GetSettingsFilePath();
				wchar_t lang_code[16];

				// Check if MMD is in English mode first.
				if (ExpGetEnglishMode())
				{
					wcscpy_s(lang_code, L"en-US");
				}
				else
				{
					// Read the language setting from the INI file. Defaults to "ja-JP" if not found.
					GetPrivateProfileStringW(L"Localization", L"Language", L"ja-JP", lang_code, 16, ini_path.c_str());
				}
				lang_code[15] = L'\0';

				LCID locale_id = LocaleNameToLCID(lang_code, 0);
				LANGID target_lang_id = MAKELANGID(PRIMARYLANGID(locale_id), SUBLANGID(locale_id));
				LANGID original_lang_id = GetThreadUILanguage();

				SetThreadUILanguage(target_lang_id);
				wchar_t settingsMenuText[256];
				if (LoadStringW(hInstance, IDS_MENU_PLUGIN_SETTINGS, settingsMenuText, 256) == 0)
				{
					wcscpy_s(settingsMenuText, L"Plugin Settings");
				}
				SetThreadUILanguage(original_lang_id);

				MENUITEMINFOW mii_update = { sizeof(mii_update) };
				mii_update.fMask = MIIM_STRING;
				mii_update.dwTypeData = settingsMenuText;

				SetMenuItemInfoW(hPopupMenu, 1020, FALSE, &mii_update);
			}
			break;
		}

		case WM_COMMAND:
		{
			switch (LOWORD(wp))
			{
				case 1020: // プラグイン設定
					OpenSettingsDialog(hWnd);
					break;
			}
			break;
		}
		case WM_DESTROY:
			break;
	}

	// Messages not handled by subclass are processed by the original window procedure
	return CallWindowProc((WNDPROC)originalWndProc, hWnd, msg, wp, lp);
}

static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	const BridgeParameter& parameter = BridgeParameter::instance();
	BridgeParameter& mutable_parameter = BridgeParameter::mutable_instance();
	HWND hCombo1 = GetDlgItem(hWnd, IDC_COMBO1);
	HWND hEdit1 = GetDlgItem(hWnd, IDC_EDIT1);
	HWND hEdit2 = GetDlgItem(hWnd, IDC_EDIT2);
	HWND hEdit5 = GetDlgItem(hWnd, IDC_EDIT5);
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			reload_python_file_paths();
			for (size_t i = 0; i < parameter.python_script_name_list.size(); i++)
			{
				SendMessageW(hCombo1, CB_ADDSTRING, 0, (LPARAM)parameter.python_script_name_list[i].c_str());
			}
			// Try to restore the previous selection, otherwise default to the first script.
			LRESULT index1 = SendMessageW(hCombo1, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)parameter.python_script_name.c_str());
			if (index1 == CB_ERR && !parameter.python_script_name_list.empty())
			{
				index1 = 0;
			}
			SendMessageW(hCombo1, CB_SETCURSEL, index1, 0);

			// 実行する AUTOCHECKBOX
			if (script_call_setting == 1)
			{
				CheckDlgButton(hWnd, IDC_COMBO2, BST_CHECKED);
			}
			else
			{
				CheckDlgButton(hWnd, IDC_COMBO2, BST_UNCHECKED);
			}

			::SetWindowTextW(hEdit1, to_wstring(parameter.start_frame).c_str());
			::SetWindowTextW(hEdit2, to_wstring(parameter.end_frame).c_str());
			::SetWindowTextW(hEdit5, to_wstring(parameter.export_fps).c_str());
		}
			return TRUE;
		case WM_CLOSE:
			EndDialog(hWnd, IDCANCEL);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case 1020: // プラグイン設定
				{
					OpenSettingsDialog(hWnd);
					break;
				}
				case IDOK: // Button was pressed
				{
					UINT num1 = (UINT)SendMessageW(hCombo1, CB_GETCURSEL, 0, 0);
					if (num1 < parameter.python_script_name_list.size())
					{
						const std::wstring& selected_name = parameter.python_script_name_list[num1];
						mutable_parameter.python_script_name = selected_name;
						mutable_parameter.python_script_path = parameter.python_script_path_list[num1];
						relaod_python_script();
					}

					if (IsDlgButtonChecked(hWnd, IDC_COMBO2) == BST_CHECKED)
					{
						script_call_setting = 1; // 実行する
					}
					else
					{
						script_call_setting = 2; // 実行しない
					}

					wchar_t text1[32];
					wchar_t text2[32];
					wchar_t text5[32];
					::GetWindowTextW(hEdit1, text1, 32);
					::GetWindowTextW(hEdit2, text2, 32);
					::GetWindowTextW(hEdit5, text5, 32);
					mutable_parameter.start_frame = _wtoi(text1);
					mutable_parameter.end_frame = _wtoi(text2);
					mutable_parameter.export_fps = _wtof(text5);

					if (parameter.start_frame > parameter.end_frame)
					{
						mutable_parameter.end_frame = parameter.start_frame + 1;
						::SetWindowTextW(hEdit2, to_wstring(parameter.end_frame).c_str());
					}

					SaveSettings();
					EndDialog(hWnd, IDOK);
				}
				break;
				case IDCANCEL:
					EndDialog(hWnd, IDCANCEL);
					break;
				case IDC_BUTTON1: // 再検索
				{
					wchar_t current_selection_text[MAX_PATH] = { 0 };
					LRESULT current_selection_index = SendMessageW(hCombo1, CB_GETCURSEL, 0, 0);
					if (current_selection_index != CB_ERR)
					{
						SendMessageW(hCombo1, CB_GETLBTEXT, current_selection_index, (LPARAM)current_selection_text);
					}
					reload_python_file_paths();
					SendMessageW(hCombo1, CB_RESETCONTENT, 0, 0);
					for (const auto& name : parameter.python_script_name_list)
					{
						SendMessageW(hCombo1, CB_ADDSTRING, 0, (LPARAM)name.c_str());
					}
					LRESULT index_to_select = SendMessageW(hCombo1, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)current_selection_text);
					if (index_to_select == CB_ERR && parameter.python_script_name_list.size() > 0)
					{
						index_to_select = 0;
					}
					if (index_to_select != CB_ERR)
					{
						SendMessageW(hCombo1, CB_SETCURSEL, index_to_select, 0);
					}
				}
				break;
			}
			return TRUE;
	}
	return FALSE;
}

// Window hijacking
static void overrideGLWindow()
{
	setMyMenu();
	setMySize();
	// Subclassing
	if (g_hWnd && !originalWndProc)
	{
		originalWndProc = GetWindowLongPtrW(g_hWnd, GWLP_WNDPROC);
		SetWindowLongPtrW(g_hWnd, GWLP_WNDPROC, (_LONG_PTR)overrideWndProc);
	}
}

static bool IsValidCallSetting()
{
	return script_call_setting == 1;
}

// Helper struct for the EnumWindows callback, passing a PID and returning a HWND.
struct FindWindowData
{
	DWORD process_id;
	HWND found_hwnd;
};

// EnumWindows callback that finds "RecWindow" belonging to a specific process.
BOOL CALLBACK FindRecWindowProc(HWND hWnd, LPARAM lParam)
{
	FindWindowData* pData = (FindWindowData*)lParam;

	// Check if the window's class name is "RecWindow".
	WCHAR className[256];
	if (GetClassNameW(hWnd, className, sizeof(className) / sizeof(className[0])) > 0)
	{
		if (wcscmp(className, L"RecWindow") == 0)
		{
			DWORD windowProcessId;
			GetWindowThreadProcessId(hWnd, &windowProcessId);

			if (windowProcessId == pData->process_id)
			{
				pData->found_hwnd = hWnd;
				return FALSE; // Returning FALSE stops the EnumWindows loop.
			}
		}
	}

	// Continue enumerating windows.
	return TRUE;
}

static bool IsValidFrame()
{
	FindWindowData data;
	data.process_id = GetCurrentProcessId();
	data.found_hwnd = NULL;

	EnumWindows(FindRecWindowProc, (LPARAM)&data);

	return (data.found_hwnd != NULL);
}

static bool IsValidTechniq()
{
	const int technic = ExpGetCurrentTechnic();
	return (technic == 0 || technic == 1 || technic == 2);
}

static HRESULT WINAPI present(
	IDirect3DDevice9* device,
	const RECT* pSourceRect,
	const RECT* pDestRect,
	HWND hDestWindowOverride,
	const RGNDATA* pDirtyRegion)
{
	const float time = ExpGetFrameTime();

	if (pDestRect)
	{
		BridgeParameter::mutable_instance().frame_width = pDestRect->right - pDestRect->left;
		BridgeParameter::mutable_instance().frame_height = pDestRect->bottom - pDestRect->top;
	}
	BridgeParameter::mutable_instance().is_exporting_without_mesh = false;
	if (!g_is_window_hooked)
	{
		EnumWindows(enumWindowsProc, 0);
		if (g_hWnd)
		{
			overrideGLWindow();
			g_is_window_hooked = true;
		}
	}
	const bool validFrame = IsValidFrame();
	const bool validCallSetting = IsValidCallSetting();
	const bool validTechniq = IsValidTechniq();
	if (validFrame && validCallSetting && validTechniq)
	{
		// Exporting
		const BridgeParameter& parameter = BridgeParameter::instance();
		int frame = static_cast<int>(time * BridgeParameter::instance().export_fps + 0.5f);
		if (frame >= parameter.start_frame && frame <= parameter.end_frame)
		{
			process_frame = frame;
			if (!is_exporting && process_frame == parameter.start_frame)
			{
				relaod_python_script();
				exportedFrames.clear();
				is_exporting = true;
			}
			if (exportedFrames.find(process_frame) == exportedFrames.end())
			{
				run_python_script();
				exportedFrames[process_frame] = 1;
				if (process_frame == parameter.end_frame)
				{
					is_exporting = false;
				}
			}
		}
		BridgeParameter::mutable_instance().finish_buffer_list.clear();
		presentCount++;
	}
	else
	{
		// Not exporting
		is_exporting = false;
	}
	HRESULT res = (*original_present)(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	return res;
}

static HRESULT WINAPI reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT res = (*original_reset)(device, pPresentationParameters);
	::MessageBoxW(NULL, L"MMDBridge does not support 3D Vision", L"HOGE", MB_OK);
	return res;
}

static HRESULT WINAPI setFVF(IDirect3DDevice9* device, DWORD fvf)
{
	HRESULT res = (*original_set_fvf)(device, fvf);

	if (script_call_setting == 1)
	{
		renderData.fvf = fvf;
		DWORD pos = (fvf & D3DFVF_POSITION_MASK);
		renderData.pos = (pos > 0);
		renderData.pos_xyz = ((pos & D3DFVF_XYZ) > 0);
		renderData.pos_rhw = ((pos & D3DFVF_XYZRHW) > 0);
		renderData.pos_xyzb[0] = ((fvf & D3DFVF_XYZB1) == D3DFVF_XYZB1);
		renderData.pos_xyzb[1] = ((fvf & D3DFVF_XYZB2) == D3DFVF_XYZB2);
		renderData.pos_xyzb[2] = ((fvf & D3DFVF_XYZB3) == D3DFVF_XYZB3);
		renderData.pos_xyzb[3] = ((fvf & D3DFVF_XYZB4) == D3DFVF_XYZB4);
		renderData.pos_xyzb[4] = ((fvf & D3DFVF_XYZB5) == D3DFVF_XYZB5);
		renderData.pos_last_beta_ubyte4 = ((fvf & D3DFVF_LASTBETA_UBYTE4) > 0);
		renderData.normal = ((fvf & D3DFVF_NORMAL) > 0);
		renderData.psize = ((fvf & D3DFVF_PSIZE) > 0);
		renderData.diffuse = ((fvf & D3DFVF_DIFFUSE) > 0);
		renderData.specular = ((fvf & D3DFVF_SPECULAR) > 0);
		renderData.texcount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	}

	return res;
}

static void setFVF(DWORD fvf)
{
	renderData.fvf = fvf;

	DWORD pos = (fvf & D3DFVF_POSITION_MASK);
	renderData.pos = (pos > 0);
	renderData.pos_xyz = ((pos & D3DFVF_XYZ) > 0);
	renderData.pos_rhw = ((pos & D3DFVF_XYZRHW) > 0);
	renderData.pos_xyzb[0] = ((fvf & D3DFVF_XYZB1) == D3DFVF_XYZB1);
	renderData.pos_xyzb[1] = ((fvf & D3DFVF_XYZB2) == D3DFVF_XYZB2);
	renderData.pos_xyzb[2] = ((fvf & D3DFVF_XYZB3) == D3DFVF_XYZB3);
	renderData.pos_xyzb[3] = ((fvf & D3DFVF_XYZB4) == D3DFVF_XYZB4);
	renderData.pos_xyzb[4] = ((fvf & D3DFVF_XYZB5) == D3DFVF_XYZB5);
	renderData.pos_last_beta_ubyte4 = ((fvf & D3DFVF_LASTBETA_UBYTE4) > 0);
	renderData.normal = ((fvf & D3DFVF_NORMAL) > 0);
	renderData.psize = ((fvf & D3DFVF_PSIZE) > 0);
	renderData.diffuse = ((fvf & D3DFVF_DIFFUSE) > 0);
	renderData.specular = ((fvf & D3DFVF_SPECULAR) > 0);
	renderData.texcount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
}

HRESULT WINAPI clear(
	IDirect3DDevice9* device,
	DWORD count,
	const D3DRECT* pRects,
	DWORD flags,
	D3DCOLOR color,
	float z,
	DWORD stencil)
{
	HRESULT res = (*original_clear)(device, count, pRects, flags, color, z, stencil);
	return res;
}

static void getTextureParameter(TextureParameter& param)
{
	TextureSamplers::iterator tit0 = renderData.textureSamplers.find(0);
	TextureSamplers::iterator tit1 = renderData.textureSamplers.find(1);
	TextureSamplers::iterator tit2 = renderData.textureSamplers.find(2);

	param.hasTextureSampler0 = (tit0 != renderData.textureSamplers.end());
	param.hasTextureSampler1 = (tit1 != renderData.textureSamplers.end());
	param.hasTextureSampler2 = (tit2 != renderData.textureSamplers.end());

	if (param.hasTextureSampler1)
	{
		param.texture = tit1->second;
		param.textureMemoryName = to_string(param.texture);

		DWORD requiredSize = UMGetTextureName(param.texture, nullptr, 0);

		if (requiredSize > 0)
		{
			std::vector<wchar_t> nameBuffer(requiredSize);

			if (UMGetTextureName(param.texture, nameBuffer.data(), requiredSize) > 0)
			{
				param.textureName = nameBuffer.data();
			}
		}
		else
		{
			param.textureName.clear();
		}

		if (UMIsAlphaTexture(param.texture))
		{
			param.hasAlphaTexture = true;
		}
	}
}

// Write vertex/normal buffer/texture to memory
static bool writeBuffersToMemory(IDirect3DDevice9* device)
{
	// const int currentTechnic = ExpGetCurrentTechnic();
	// const int currentMaterial = ExpGetCurrentMaterial();
	const int currentObject = ExpGetCurrentObject();

	BYTE* pVertexBuf;
	IDirect3DVertexBuffer9* pStreamData = renderData.pStreamData;

	VertexBufferList& finishBuffers = BridgeParameter::mutable_instance().finish_buffer_list;
	if (std::find(finishBuffers.begin(), finishBuffers.end(), pStreamData) == finishBuffers.end())
	{
		VertexBuffers::iterator vit = renderData.vertexBuffers.find(pStreamData);
		if (vit != renderData.vertexBuffers.end())
		{
			RenderBufferMap& renderedBuffers = BridgeParameter::mutable_instance().render_buffer_map;
			pStreamData->lpVtbl->Lock(pStreamData, 0, 0, (void**)&pVertexBuf, D3DLOCK_READONLY);

			// Get FVF
			DWORD fvf;
			device->lpVtbl->GetFVF(device, &fvf);
			if (renderData.fvf != fvf)
			{
				setFVF(fvf);
			}

			RenderedBuffer renderedBuffer;

			::D3DXMatrixIdentity(&renderedBuffer.world);
			::D3DXMatrixIdentity(&renderedBuffer.view);
			::D3DXMatrixIdentity(&renderedBuffer.projection);
			::D3DXMatrixIdentity(&renderedBuffer.world_inv);
			device->lpVtbl->GetTransform(device, D3DTS_WORLD, &renderedBuffer.world);
			device->lpVtbl->GetTransform(device, D3DTS_VIEW, &renderedBuffer.view);
			device->lpVtbl->GetTransform(device, D3DTS_PROJECTION, &renderedBuffer.projection);

			::D3DXMatrixInverse(&renderedBuffer.world_inv, NULL, &renderedBuffer.world);

			int bytePos = 0;

			if (renderedBuffer.world.m[0][0] == 0 && renderedBuffer.world.m[1][1] == 0 && renderedBuffer.world.m[2][2] == 0)
			{
				return false;
			}

			renderedBuffer.isAccessory = false;
			D3DXMATRIX accesosoryMat;
			for (int i = 0; i < ExpGetAcsNum(); ++i)
			{
				int order = ExpGetAcsOrder(i);
				if (order == currentObject)
				{
					renderedBuffer.isAccessory = true;
					renderedBuffer.accessory = i;
					renderedBuffer.order = i;
					accesosoryMat = ExpGetAcsWorldMat(i);
				}
			}

			if (!renderedBuffer.isAccessory)
			{
				for (int i = 0; i < ExpGetPmdNum(); ++i)
				{
					int order = ExpGetPmdOrder(i);
					if (order == currentObject)
					{
						renderedBuffer.order = i;
						break;
					}
				}
			}

			// Vertices
			if (renderData.pos_xyz)
			{
				// size_t initialVertexSize = renderedBuffer.vertecies.size();
				const int size = (vit->second - bytePos) / renderData.stride;
				renderedBuffer.vertecies.resize(size);
				for (size_t i = bytePos, n = 0; i < vit->second; i += renderData.stride, ++n)
				{
					D3DXVECTOR3 v;
					memcpy(&v, &pVertexBuf[i], sizeof(D3DXVECTOR3));
					if (renderedBuffer.isAccessory)
					{
						D3DXVECTOR4 dst;
						::D3DXVec3Transform(&dst, &v, &accesosoryMat);
						v.x = dst.x;
						v.y = dst.y;
						v.z = dst.z;
					}

					renderedBuffer.vertecies[n] = v;
				}
				bytePos += (sizeof(DWORD) * 3);
			}

			// Weight (skip)

			// Normals
			if (renderData.normal)
			{
				for (size_t i = bytePos; i < vit->second; i += renderData.stride)
				{
					D3DXVECTOR3 n;
					memcpy(&n, &pVertexBuf[i], sizeof(D3DXVECTOR3));
					renderedBuffer.normals.push_back(n);
				}
				bytePos += (sizeof(DWORD) * 3);
			}

			// Vertex color
			if (renderData.diffuse)
			{
				for (size_t i = 0; i < vit->second; i += renderData.stride)
				{
					DWORD diffuse;
					memcpy(&diffuse, &pVertexBuf[i], sizeof(DWORD));
					renderedBuffer.diffuses.push_back(diffuse);
				}
				bytePos += (sizeof(DWORD));
			}

			// UV
			if (renderData.texcount > 0)
			{
				for (int n = 0; n < renderData.texcount; ++n)
				{
					for (size_t i = bytePos; i < vit->second; i += renderData.stride)
					{
						UMVec2f uv;
						memcpy(&uv, &pVertexBuf[i], sizeof(UMVec2f));
						renderedBuffer.uvs.push_back(uv);
					}
					bytePos += (sizeof(DWORD) * 2);
				}
			}

			pStreamData->lpVtbl->Unlock(pStreamData);

			// Save to memory
			finishBuffers.push_back(pStreamData);
			renderedBuffers[pStreamData] = renderedBuffer;
		}
	}
	return true;
}

static bool writeMaterialsToMemory(TextureParameter& textureParameter)
{
	const int currentTechnic = ExpGetCurrentTechnic();
	const int currentMaterial = ExpGetCurrentMaterial();
	const int currentObject = ExpGetCurrentObject();

	IDirect3DVertexBuffer9* pStreamData = renderData.pStreamData;
	RenderBufferMap& renderedBuffers = BridgeParameter::mutable_instance().render_buffer_map;
	if (renderedBuffers.find(pStreamData) == renderedBuffers.end())
	{
		return false;
	}

	bool notFoundObjectMaterial = (renderedMaterials.find(currentObject) == renderedMaterials.end());
	if (notFoundObjectMaterial || renderedMaterials[currentObject].find(currentMaterial) == renderedMaterials[currentObject].end())
	{
		// Get D3DMATERIAL9
		D3DMATERIAL9 material = ExpGetPmdMaterial(currentObject, currentMaterial);
		// p_device->lpVtbl->GetMaterial(p_device, &material);

		auto mat = std::make_shared<RenderedMaterial>();
		mat->diffuse.x = material.Diffuse.r;
		mat->diffuse.y = material.Diffuse.g;
		mat->diffuse.z = material.Diffuse.b;
		mat->diffuse.w = material.Diffuse.a;
		mat->specular.x = material.Specular.r;
		mat->specular.y = material.Specular.g;
		mat->specular.z = material.Specular.b;
		mat->ambient.x = material.Ambient.r;
		mat->ambient.y = material.Ambient.g;
		mat->ambient.z = material.Ambient.b;
		mat->emissive.x = material.Emissive.r;
		mat->emissive.y = material.Emissive.g;
		mat->emissive.z = material.Emissive.b;
		mat->power = material.Power;

		// Shader time
		if (currentTechnic == 2)
		{
			LPD3DXEFFECT* effect = UMGetEffect();

			if (effect)
			{
				D3DXHANDLE current = (*effect)->lpVtbl->GetCurrentTechnique(*effect);
				D3DXHANDLE texHandle1 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "DiffuseBSSphiaTexTec");
				D3DXHANDLE texHandle2 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "DiffuseBSTextureTec");
				D3DXHANDLE texHandle3 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "BShadowSphiaTextureTec");
				D3DXHANDLE texHandle4 = (*effect)->lpVtbl->GetTechniqueByName(*effect, "BShadowTextureTec");

				textureParameter.hasTextureSampler2 = false;
				if (current == texHandle1)
				{
					textureParameter.hasTextureSampler2 = true;
				}
				if (current == texHandle2)
				{
					textureParameter.hasTextureSampler2 = true;
				}
				if (current == texHandle3)
				{
					textureParameter.hasTextureSampler2 = true;
				}
				if (current == texHandle4)
				{
					textureParameter.hasTextureSampler2 = true;
				}

				D3DXHANDLE hEdge = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "EgColor");
				D3DXHANDLE hDiffuse = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "MatDifColor");
				D3DXHANDLE hToon = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "ToonColor");
				D3DXHANDLE hSpecular = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "SpcColor");
				D3DXHANDLE hTransp = (*effect)->lpVtbl->GetParameterByName(*effect, NULL, "transp");

				float edge[4];
				float diffuse[4];
				float specular[4];
				float toon[4];
				BOOL transp;
				(*effect)->lpVtbl->GetFloatArray(*effect, hEdge, edge, 4);
				(*effect)->lpVtbl->GetFloatArray(*effect, hToon, toon, 4);
				(*effect)->lpVtbl->GetFloatArray(*effect, hDiffuse, diffuse, 4);
				(*effect)->lpVtbl->GetFloatArray(*effect, hSpecular, specular, 4);
				(*effect)->lpVtbl->GetBool(*effect, hTransp, &transp);
				mat->diffuse.x = diffuse[0];
				mat->diffuse.y = diffuse[1];
				mat->diffuse.z = diffuse[2];
				mat->diffuse.w = diffuse[3];
				mat->power = specular[3];

				if (specular[0] != 0 || specular[1] != 0 || specular[2] != 0)
				{
					mat->specular.x = specular[0];
					mat->specular.y = specular[1];
					mat->specular.z = specular[2];
				}
			}
		}

		if (renderData.texcount > 0)
		{
			DWORD colorRop0;
			DWORD colorRop1;

			p_device->lpVtbl->GetTextureStageState(p_device, 0, D3DTSS_COLOROP, &colorRop0);
			p_device->lpVtbl->GetTextureStageState(p_device, 1, D3DTSS_COLOROP, &colorRop1);

			if (textureParameter.hasTextureSampler2)
			{

				mat->tex = textureParameter.texture;
				mat->texture = textureParameter.textureName;
				mat->memoryTexture = textureParameter.textureMemoryName;

				if (!textureParameter.hasAlphaTexture)
				{
					mat->diffuse.w = 1.0f;
				}
			}
			else if (textureParameter.hasTextureSampler0 || textureParameter.hasTextureSampler1)
			{
				if (colorRop0 != D3DTOP_DISABLE && colorRop1 != D3DTOP_DISABLE)
				{
					mat->tex = textureParameter.texture;
					mat->texture = textureParameter.textureName;
					mat->memoryTexture = textureParameter.textureMemoryName;
					if (!textureParameter.hasAlphaTexture)
					{
						mat->diffuse.w = 1.0f;
					}
				}
			}
		}

		RenderedBuffer& renderedBuffer = renderedBuffers[pStreamData];
		if (renderedBuffer.isAccessory)
		{
			D3DMATERIAL9 accessoryMat = ExpGetAcsMaterial(renderedBuffer.accessory, currentMaterial);
			mat->diffuse.x = accessoryMat.Diffuse.r * 10.0f;
			mat->diffuse.y = accessoryMat.Diffuse.g * 10.0f;
			mat->diffuse.z = accessoryMat.Diffuse.b * 10.0f;
			mat->specular.x = accessoryMat.Specular.r * 10.0f;
			mat->specular.y = accessoryMat.Specular.g * 10.0f;
			mat->specular.z = accessoryMat.Specular.b * 10.0f;
			mat->ambient.x = accessoryMat.Ambient.r;
			mat->ambient.y = accessoryMat.Ambient.g;
			mat->ambient.z = accessoryMat.Ambient.b;
			mat->diffuse.w = accessoryMat.Diffuse.a;
			mat->diffuse.w *= ::ExpGetAcsTr(renderedBuffer.accessory);
		}

		renderedBuffer.materials.push_back(mat);
		renderedBuffer.material_map[currentMaterial] = mat;
		renderedMaterials[currentObject][currentMaterial] = mat;
	}
	else
	{
		auto& materialMap = renderedMaterials[currentObject];
		renderedBuffers[pStreamData].materials.push_back(materialMap[currentMaterial]);
		renderedBuffers[pStreamData].material_map[currentMaterial] = materialMap[currentMaterial];
		renderedMaterials[currentObject][currentMaterial] = materialMap[currentMaterial];
	}

	if (renderedBuffers[pStreamData].materials.size() > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void writeMatrixToMemory(IDirect3DDevice9* device, RenderedBuffer& dst)
{
	::D3DXMatrixIdentity(&dst.world);
	::D3DXMatrixIdentity(&dst.view);
	::D3DXMatrixIdentity(&dst.projection);
	device->lpVtbl->GetTransform(device, D3DTS_WORLD, &dst.world);
	device->lpVtbl->GetTransform(device, D3DTS_VIEW, &dst.view);
	device->lpVtbl->GetTransform(device, D3DTS_PROJECTION, &dst.projection);
}

static void writeLightToMemory(IDirect3DDevice9* device, RenderedBuffer& renderedBuffer)
{
	BOOL isLight;
	int lightNumber = 0;
	p_device->lpVtbl->GetLightEnable(p_device, lightNumber, &isLight);
	if (isLight)
	{
		D3DLIGHT9 light;
		p_device->lpVtbl->GetLight(p_device, lightNumber, &light);
		UMVec3f& umlight = renderedBuffer.light;
		D3DXVECTOR3 v(light.Direction.x, light.Direction.y, light.Direction.z);
		D3DXVECTOR4 dst;
		// D3DXVec3Transform(&dst, &v, &renderedBuffer.world);
		//  NOTE: There might be a function that only rotates without crushing the parallel translation component.
		D3DXMATRIX m = renderedBuffer.world_inv;
		// ugly hack.
		m._41 = m._42 = m._43 = 0;
		m._14 = m._24 = m._34 = m._44 = 0;
		D3DXVec3Transform(&dst, &v, &m);

		umlight.x = dst.x;
		umlight.y = dst.y;
		umlight.z = dst.z;

		// Specular is closest to the value set in MMD UI.
		// But you need to col * 256.0 / 255.0 to be in the range 0~1.
		// see: http://ch.nicovideo.jp/sovoro_mmd/blomaga/ar319862
		FLOAT s = 256.0f / 255.0f;
		renderedBuffer.light_color.x = light.Specular.r * s;
		renderedBuffer.light_color.y = light.Specular.g * s;
		renderedBuffer.light_color.z = light.Specular.b * s;

		// renderedBuffer.light_diffuse.x = light.Diffuse.r;
		// renderedBuffer.light_diffuse.y = light.Diffuse.g;
		// renderedBuffer.light_diffuse.z = light.Diffuse.b;
		// renderedBuffer.light_diffuse.w = light.Diffuse.a;
		// renderedBuffer.light_specular.x = light.Specular.r;
		// renderedBuffer.light_specular.y = light.Specular.g;
		// renderedBuffer.light_specular.z = light.Specular.b;
		// renderedBuffer.light_specular.w = light.Specular.a;
		// renderedBuffer.light_position.x = light.Position.x;
		// renderedBuffer.light_position.y = light.Position.y;
		// renderedBuffer.light_position.z = light.Position.z;
	}
}

static HRESULT WINAPI drawIndexedPrimitive(
	IDirect3DDevice9* device,
	D3DPRIMITIVETYPE type,
	INT baseVertexIndex,
	UINT minIndex,
	UINT numVertices,
	UINT startIndex,
	UINT primitiveCount)
{
	const int currentMaterial = ExpGetCurrentMaterial();
	// const int currentObject = ExpGetCurrentObject();

	const bool validCallSetting = IsValidCallSetting();
	const bool validFrame = IsValidFrame();
	const bool validTechniq = IsValidTechniq();
	const bool validBuffer = (!BridgeParameter::instance().is_exporting_without_mesh);

	if (validBuffer && validCallSetting && validFrame && validTechniq && type == D3DPT_TRIANGLELIST)
	{
		// Start rendering
		if (renderData.pIndexData && renderData.pStreamData && renderData.pos_xyz)
		{
			// Get texture information
			TextureParameter textureParameter;
			getTextureParameter(textureParameter);

			// Save texture to memory
			if (textureParameter.texture)
			{
				if (!textureParameter.textureName.empty())
				{
					writeTextureToMemory(textureParameter.textureName, textureParameter.texture, true);
				}
				else
				{
					writeTextureToMemory(textureParameter.textureName, textureParameter.texture, false);
				}
			}

			// Write vertex buffer/normal buffer/texture buffer to memory
			if (!writeBuffersToMemory(device))
			{
				return (*original_draw_indexed_primitive)(device, type, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
			}

			// Write material to memory
			if (!writeMaterialsToMemory(textureParameter))
			{
				return (*original_draw_indexed_primitive)(device, type, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
			}

			// Write index buffer to memory
			// Calculate normal if no normal
			IDirect3DVertexBuffer9* pStreamData = renderData.pStreamData;
			IDirect3DIndexBuffer9* pIndexData = renderData.pIndexData;

			D3DINDEXBUFFER_DESC indexDesc;
			if (pIndexData->lpVtbl->GetDesc(pIndexData, &indexDesc) == D3D_OK)
			{
				void* pIndexBuf;
				if (pIndexData->lpVtbl->Lock(pIndexData, 0, 0, (void**)&pIndexBuf, D3DLOCK_READONLY) == D3D_OK)
				{
					RenderBufferMap& renderedBuffers = BridgeParameter::mutable_instance().render_buffer_map;
					RenderedBuffer& renderedBuffer = renderedBuffers[pStreamData];
					RenderedSurface& renderedSurface = renderedBuffer.material_map[currentMaterial]->surface;
					renderedSurface.faces.clear();

					// Write transformation matrix to memory
					writeMatrixToMemory(device, renderedBuffer);

					// Write light to memory
					writeLightToMemory(device, renderedBuffer);

					// Write index buffer to memory
					// Fix normals
					for (size_t i = 0, size = primitiveCount * 3; i < size; i += 3)
					{
						UMVec3i face;
						if (indexDesc.Format == D3DFMT_INDEX16)
						{
							WORD* p = (WORD*)pIndexBuf;
							face.x = static_cast<int>((p[startIndex + i + 0]) + 1);
							face.y = static_cast<int>((p[startIndex + i + 1]) + 1);
							face.z = static_cast<int>((p[startIndex + i + 2]) + 1);
						}
						else
						{
							DWORD* p = (DWORD*)pIndexBuf;
							face.x = static_cast<int>((p[startIndex + i + 0]) + 1);
							face.y = static_cast<int>((p[startIndex + i + 1]) + 1);
							face.z = static_cast<int>((p[startIndex + i + 2]) + 1);
						}
						const size_t vsize = renderedBuffer.vertecies.size();
						if (face.x > vsize || face.y > vsize || face.z > vsize)
						{
							continue;
						}
						if (face.x <= 0 || face.y <= 0 || face.z <= 0)
						{
							continue;
						}
						renderedSurface.faces.push_back(face);
						if (!renderData.normal)
						{
							if (renderedBuffer.normals.size() != vsize)
							{
								renderedBuffer.normals.resize(vsize);
							}
							D3DXVECTOR3 n;
							D3DXVECTOR3 v0 = renderedBuffer.vertecies[face.x - 1];
							D3DXVECTOR3 v1 = renderedBuffer.vertecies[face.y - 1];
							D3DXVECTOR3 v2 = renderedBuffer.vertecies[face.z - 1];
							D3DXVECTOR3 v10 = v1 - v0;
							D3DXVECTOR3 v20 = v2 - v0;
							::D3DXVec3Cross(&n, &v10, &v20);
							renderedBuffer.normals[face.x - 1] += n;
							renderedBuffer.normals[face.y - 1] += n;
							renderedBuffer.normals[face.z - 1] += n;
						}
						if (!renderData.normal)
						{
							for (size_t j = 0, normalSize = renderedBuffer.normals.size(); j < normalSize; ++j)
							{
								D3DXVec3Normalize(
									&renderedBuffer.normals[j],
									&renderedBuffer.normals[j]);
							}
						}
					}
				}
			}
			pIndexData->lpVtbl->Unlock(pIndexData);
		}
	}

	HRESULT res = (*original_draw_indexed_primitive)(device, type, baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);

	UMSync();
	return res;
}

static HRESULT WINAPI createTexture(
	IDirect3DDevice9* device,
	UINT width,
	UINT height,
	UINT levels,
	DWORD usage,
	D3DFORMAT format,
	D3DPOOL pool,
	IDirect3DTexture9** ppTexture,
	HANDLE* pSharedHandle)
{
	HRESULT res = (*original_create_texture)(device, width, height, levels, usage, format, pool, ppTexture, pSharedHandle);

	TextureInfo info;
	info.wh.x = width;
	info.wh.y = height;
	info.format = format;

	renderData.textureBuffers[*ppTexture] = info;

	return res;
}

static HRESULT WINAPI createVertexBuffer(
	IDirect3DDevice9* device,
	UINT length,
	DWORD usage,
	DWORD fvf,
	D3DPOOL pool,
	IDirect3DVertexBuffer9** ppVertexBuffer,
	HANDLE* pHandle)
{
	HRESULT res = (*original_create_vertex_buffer)(device, length, usage, fvf, pool, ppVertexBuffer, pHandle);

	renderData.vertexBuffers[*ppVertexBuffer] = length;

	return res;
}

static HRESULT WINAPI setTexture(
	IDirect3DDevice9* device,
	DWORD sampler,
	IDirect3DBaseTexture9* pTexture)
{
	if (presentCount == 0)
	{
		IDirect3DTexture9* texture = reinterpret_cast<IDirect3DTexture9*>(pTexture);
		renderData.textureSamplers[sampler] = texture;
	}

	HRESULT res = (*original_set_texture)(device, sampler, pTexture);

	return res;
}

static HRESULT WINAPI setStreamSource(
	IDirect3DDevice9* device,
	UINT streamNumber,
	IDirect3DVertexBuffer9* pStreamData,
	UINT offsetInBytes,
	UINT stride)
{
	HRESULT res = (*original_set_stream_source)(device, streamNumber, pStreamData, offsetInBytes, stride);

	int currentTechnic = ExpGetCurrentTechnic();

	const bool validCallSetting = IsValidCallSetting();
	const bool validFrame = IsValidFrame();
	const bool validTechniq = IsValidTechniq() || currentTechnic == 5;

	if (validCallSetting && validFrame && validTechniq)
	{
		if (pStreamData)
		{
			renderData.streamNumber = streamNumber;
			renderData.pStreamData = pStreamData;
			renderData.offsetInBytes = offsetInBytes;
			renderData.stride = stride;
		}
	}

	return res;
}

// IDirect3DDevice9::SetIndices
static HRESULT WINAPI setIndices(IDirect3DDevice9* device, IDirect3DIndexBuffer9* pIndexData)
{
	HRESULT res = (*original_set_indices)(device, pIndexData);

	int currentTechnic = ExpGetCurrentTechnic();

	const bool validCallSetting = IsValidCallSetting();
	const bool validFrame = IsValidFrame();
	const bool validTechniq = IsValidTechniq() || currentTechnic == 5;
	if (validCallSetting && validFrame && validTechniq)
	{
		renderData.pIndexData = pIndexData;
	}

	return res;
}

// IDirect3DDevice9::BeginStateBlock
// This function modifies lpVtbl, so we need to restore lpVtbl
static HRESULT WINAPI beginStateBlock(IDirect3DDevice9* device)
{
	originalDevice();
	HRESULT res = (*original_begin_state_block)(device);

	p_device = device;
	hookDevice();

	return res;
}

// IDirect3DDevice9::EndStateBlock
// This function modifies lpVtbl, so we need to restore lpVtbl
static HRESULT WINAPI endStateBlock(IDirect3DDevice9* device, IDirect3DStateBlock9** ppSB)
{
	originalDevice();
	HRESULT res = (*original_end_state_block)(device, ppSB);

	p_device = device;
	hookDevice();

	return res;
}

static void hookDevice()
{
	if (p_device)
	{
		// Grant write attribute
		DWORD old_protect;
		VirtualProtect(reinterpret_cast<void*>(p_device->lpVtbl), sizeof(p_device->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);

		p_device->lpVtbl->BeginScene = beginScene;
		p_device->lpVtbl->EndScene = endScene;
		// p_device->lpVtbl->Clear = clear;
		p_device->lpVtbl->Present = present;
		// p_device->lpVtbl->Reset = reset;
		p_device->lpVtbl->BeginStateBlock = beginStateBlock;
		p_device->lpVtbl->EndStateBlock = endStateBlock;
		p_device->lpVtbl->SetFVF = setFVF;
		p_device->lpVtbl->DrawIndexedPrimitive = drawIndexedPrimitive;
		p_device->lpVtbl->SetStreamSource = setStreamSource;
		p_device->lpVtbl->SetIndices = setIndices;
		p_device->lpVtbl->CreateVertexBuffer = createVertexBuffer;
		p_device->lpVtbl->SetTexture = setTexture;
		p_device->lpVtbl->CreateTexture = createTexture;
		// p_device->lpVtbl->SetTextureStageState = setTextureStageState;

		// Restore original write attribute
		VirtualProtect(reinterpret_cast<void*>(p_device->lpVtbl), sizeof(p_device->lpVtbl), old_protect, &old_protect);
	}
}

static void originalDevice()
{
	if (p_device)
	{
		// Grant write attribute
		DWORD old_protect;
		VirtualProtect(reinterpret_cast<void*>(p_device->lpVtbl), sizeof(p_device->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);

		p_device->lpVtbl->BeginScene = original_begin_scene;
		p_device->lpVtbl->EndScene = original_end_scene;
		// p_device->lpVtbl->Clear = clear;
		p_device->lpVtbl->Present = original_present;
		// p_device->lpVtbl->Reset = reset;
		p_device->lpVtbl->BeginStateBlock = original_begin_state_block;
		p_device->lpVtbl->EndStateBlock = original_end_state_block;
		p_device->lpVtbl->SetFVF = original_set_fvf;
		p_device->lpVtbl->DrawIndexedPrimitive = original_draw_indexed_primitive;
		p_device->lpVtbl->SetStreamSource = original_set_stream_source;
		p_device->lpVtbl->SetIndices = original_set_indices;
		p_device->lpVtbl->CreateVertexBuffer = original_create_vertex_buffer;
		p_device->lpVtbl->SetTexture = original_set_texture;
		p_device->lpVtbl->CreateTexture = original_create_texture;
		// p_device->lpVtbl->SetTextureStageState = setTextureStageState;

		// Restore original write attribute
		VirtualProtect(reinterpret_cast<void*>(p_device->lpVtbl), sizeof(p_device->lpVtbl), old_protect, &old_protect);
	}
}

static HRESULT WINAPI createDevice(
	IDirect3D9* direct3d,
	UINT adapter,
	D3DDEVTYPE type,
	HWND window,
	DWORD flag,
	D3DPRESENT_PARAMETERS* param,
	IDirect3DDevice9** device)
{
	if (g_hWnd == NULL)
	{
		g_hWnd = window;
	}

	HRESULT res = (*original_create_device)(direct3d, adapter, type, window, flag, param, device);
	p_device = (*device);

	if (p_device)
	{
		original_begin_scene = p_device->lpVtbl->BeginScene;
		original_end_scene = p_device->lpVtbl->EndScene;
		// original_clear = p_device->lpVtbl->Clear;
		original_present = p_device->lpVtbl->Present;
		// original_reset = p_device->lpVtbl->Reset;
		original_begin_state_block = p_device->lpVtbl->BeginStateBlock;
		original_end_state_block = p_device->lpVtbl->EndStateBlock;
		original_set_fvf = p_device->lpVtbl->SetFVF;
		original_draw_indexed_primitive = p_device->lpVtbl->DrawIndexedPrimitive;
		original_set_stream_source = p_device->lpVtbl->SetStreamSource;
		original_set_indices = p_device->lpVtbl->SetIndices;
		original_create_vertex_buffer = p_device->lpVtbl->CreateVertexBuffer;
		original_set_texture = p_device->lpVtbl->SetTexture;
		original_create_texture = p_device->lpVtbl->CreateTexture;
		// original_set_texture_stage_state = p_device->lpVtbl->SetTextureStageState;

		hookDevice();
	}
	return res;
}

static HRESULT WINAPI createDeviceEx(
	IDirect3D9Ex* direct3dex,
	UINT adapter,
	D3DDEVTYPE type,
	HWND window,
	DWORD flag,
	D3DPRESENT_PARAMETERS* param,
	D3DDISPLAYMODEEX* displayMode,
	IDirect3DDevice9Ex** device)
{
	if (g_hWnd == NULL)
	{
		g_hWnd = window;
	}

	HRESULT res = (*original_create_deviceex)(direct3dex, adapter, type, window, flag, param, displayMode, device);
	p_device = reinterpret_cast<IDirect3DDevice9*>(*device);

	if (p_device)
	{
		original_begin_scene = p_device->lpVtbl->BeginScene;
		// original_clear = p_device->lpVtbl->Clear;
		original_present = p_device->lpVtbl->Present;
		// original_reset = p_device->lpVtbl->Reset;
		original_begin_state_block = p_device->lpVtbl->BeginStateBlock;
		original_end_state_block = p_device->lpVtbl->EndStateBlock;
		original_set_fvf = p_device->lpVtbl->SetFVF;
		original_draw_indexed_primitive = p_device->lpVtbl->DrawIndexedPrimitive;
		original_set_stream_source = p_device->lpVtbl->SetStreamSource;
		original_set_indices = p_device->lpVtbl->SetIndices;
		original_create_vertex_buffer = p_device->lpVtbl->CreateVertexBuffer;
		original_set_texture = p_device->lpVtbl->SetTexture;
		original_create_texture = p_device->lpVtbl->CreateTexture;
		// original_set_texture_stage_state = p_device->lpVtbl->SetTextureStageState;

		hookDevice();
	}
	return res;
}

extern "C"
{
	// Fake Direct3DCreate9
	IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
	{
		IDirect3D9* direct3d((*original_direct3d_create)(SDKVersion));
		original_create_device = direct3d->lpVtbl->CreateDevice;

		// Grant write attribute
		DWORD old_protect;
		VirtualProtect(reinterpret_cast<void*>(direct3d->lpVtbl), sizeof(direct3d->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);

		direct3d->lpVtbl->CreateDevice = createDevice;

		// Restore original write attribute
		VirtualProtect(reinterpret_cast<void*>(direct3d->lpVtbl), sizeof(direct3d->lpVtbl), old_protect, &old_protect);

		return direct3d;
	}

	HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ppD3D)
	{
		IDirect3D9Ex* direct3d9ex = NULL;
		(*original_direct3d9ex_create)(SDKVersion, &direct3d9ex);

		if (direct3d9ex)
		{
			original_create_deviceex = direct3d9ex->lpVtbl->CreateDeviceEx;
			if (original_create_deviceex)
			{
				// Grant write attribute
				DWORD old_protect;
				VirtualProtect(reinterpret_cast<void*>(direct3d9ex->lpVtbl), sizeof(direct3d9ex->lpVtbl), PAGE_EXECUTE_READWRITE, &old_protect);

				direct3d9ex->lpVtbl->CreateDeviceEx = createDeviceEx;

				// Restore original write attribute
				VirtualProtect(reinterpret_cast<void*>(direct3d9ex->lpVtbl), sizeof(direct3d9ex->lpVtbl), old_protect, &old_protect);

				*ppD3D = direct3d9ex;
				return S_OK;
			}
		}
		return E_ABORT;
	}

} // extern "C"

bool d3d9_initialize()
{
	// Get MMD full path.
	{
		WCHAR app_full_path[MAX_PATH] = { 0 };
		GetModuleFileNameW(NULL, app_full_path, MAX_PATH);
		PathRemoveFileSpecW(app_full_path);
		PathAddBackslashW(app_full_path);

		BridgeParameter::mutable_instance().base_path = app_full_path;
		replace(BridgeParameter::mutable_instance().base_path.begin(), BridgeParameter::mutable_instance().base_path.end(), L'\\', L'/');
	}

	reload_python_file_paths();
	relaod_python_script();

	// +++++ MINHOOK HOOKING LOGIC START +++++
	if (MH_Initialize() != MH_OK)
	{
		::MessageBoxW(NULL, L"MH_Initialize failed!", L"MinHook Error", MB_OK);
		return false;
	}

	HMODULE hMMD = GetModuleHandle(NULL);

	// ExpGetPmdFilename Hook
	if (hMMD)
	{
		void* pTarget = (void*)GetProcAddress(hMMD, "ExpGetPmdFilename");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_ExpGetPmdFilename, (LPVOID*)&fpExpGetPmdFilename_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for ExpGetPmdFilename failed!", L"MinHook Error", MB_OK);
				return false;
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for ExpGetPmdFilename failed!", L"MinHook Error", MB_OK);
				return false;
			}
		}
	}

	HMODULE hUser32 = GetModuleHandleW(L"user32.dll");

	// SetWindowTextW Hook
	OutputDebugStringW(L"[MMDBridge] Initializing SetWindowTextW Hook...\n");
	if (hUser32)
	{
		void* pTarget = (void*)GetProcAddress(hUser32, "SetWindowTextW");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_SetWindowTextW, (LPVOID*)&fpSetWindowTextW_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for SetWindowTextW failed!", L"MinHook Error", MB_OK);
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for SetWindowTextW failed!", L"MinHook Error", MB_OK);
			}
			OutputDebugStringW(L"[MMDBridge] SetWindowTextW Hook is active.\n");
		}
	}

	// ModifyMenuA Hook
	OutputDebugStringW(L"[MMDBridge] Initializing ModifyMenuA Hook...\n");
	if (hUser32)
	{
		void* pTarget = (void*)GetProcAddress(hUser32, "ModifyMenuA");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_ModifyMenuA, (LPVOID*)&fpModifyMenuA_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for ModifyMenuA failed!", L"MinHook Error", MB_OK);
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for ModifyMenuA failed!", L"MinHook Error", MB_OK);
			}
			OutputDebugStringW(L"[MMDBridge] ModifyMenuA Hook is active.\n");
		}
	}

	// SetMenuItemInfoA Hook
	OutputDebugStringW(L"[MMDBridge] Initializing SetMenuItemInfoA Hook...\n");
	if (hUser32)
	{
		void* pTarget = (void*)GetProcAddress(hUser32, "SetMenuItemInfoA");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_SetMenuItemInfoA, (LPVOID*)&fpSetMenuItemInfoA_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for SetMenuItemInfoA failed!", L"MinHook Error", MB_OK);
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for SetMenuItemInfoA failed!", L"MinHook Error", MB_OK);
			}
			OutputDebugStringW(L"[MMDBridge] SetMenuItemInfoA Hook is active.\n");
		}
	}

	// GetWindowTextA Hook
	OutputDebugStringW(L"[MMDBridge] Initializing GetWindowTextA Hook...\n");
	if (hUser32)
	{
		void* pTarget = (void*)GetProcAddress(hUser32, "GetWindowTextA");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_GetWindowTextA, (LPVOID*)&fpGetWindowTextA_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for GetWindowTextA failed!", L"MinHook Error", MB_OK);
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for GetWindowTextA failed!", L"MinHook Error", MB_OK);
			}
			OutputDebugStringW(L"[MMDBridge] GetWindowTextA Hook is active.\n");
		}
	}

	// SendMessageA Hook
	OutputDebugStringW(L"[MMDBridge] Initializing SendMessageA Hook...\n");
	if (hUser32)
	{
		void* pTarget = (void*)GetProcAddress(hUser32, "SendMessageA");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_SendMessageA, (LPVOID*)&fpSendMessageA_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for SendMessageA failed!", L"MinHook Error", MB_OK);
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for SendMessageA failed!", L"MinHook Error", MB_OK);
			}
			OutputDebugStringW(L"[MMDBridge] SendMessageA Hook is active.\n");
		}
	}

	// MessageBoxA Hook
	OutputDebugStringW(L"[MMDBridge] Initializing MessageBoxA Hook...\n");
	if (hUser32)
	{
		void* pTarget = (void*)GetProcAddress(hUser32, "MessageBoxA");
		if (pTarget)
		{
			if (MH_CreateHook(pTarget, &Detour_MessageBoxA, (LPVOID*)&fpMessageBoxA_Original) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_CreateHook for MessageBoxA failed!", L"MinHook Error", MB_OK);
			}
			if (MH_EnableHook(pTarget) != MH_OK)
			{
				::MessageBoxW(NULL, L"MH_EnableHook for MessageBoxA failed!", L"MinHook Error", MB_OK);
			}
			OutputDebugStringW(L"[MMDBridge] MessageBoxA Hook is active.\n");
		}
	}
	// +++++ MINHOOK HOOKING LOGIC END +++++

	// System path storage
	WCHAR system_path_buffer[MAX_PATH] = { 0 };
	GetSystemDirectoryW(system_path_buffer, MAX_PATH);
	std::wstring d3d9_path(system_path_buffer);
	replace(d3d9_path.begin(), d3d9_path.end(), L'\\', L'/');
	d3d9_path.append(L"/d3d9.dll");

	// Original d3d9.dll module
	g_d3d9_system_module = LoadLibraryW(d3d9_path.c_str());

	if (!g_d3d9_system_module)
	{
		return FALSE;
	}

	// Get original Direct3DCreate9 function pointer
	original_direct3d_create = reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(g_d3d9_system_module, "Direct3DCreate9"));
	if (!original_direct3d_create)
	{
		return FALSE;
	}
	original_direct3d9ex_create = reinterpret_cast<HRESULT(WINAPI*)(UINT, IDirect3D9Ex**)>(GetProcAddress(g_d3d9_system_module, "Direct3DCreate9Ex"));
	if (!original_direct3d9ex_create)
	{
		return FALSE;
	}

	return TRUE;
}

void d3d9_dispose()
{
	if (g_d3d9_system_module)
	{
		FreeLibrary(g_d3d9_system_module);
		g_d3d9_system_module = NULL;
	}

	// +++++ MINHOOK HOOKING LOGIC START +++++
	// Disable all hooks in the reverse order they were created.
	// --- Disable hooks in user32.dll ---
	HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
	if (hUser32)
	{
		void* pTarget;

		pTarget = (void*)GetProcAddress(hUser32, "MessageBoxA");
		if (pTarget)
			MH_DisableHook(pTarget);

		pTarget = (void*)GetProcAddress(hUser32, "SendMessageA");
		if (pTarget)
			MH_DisableHook(pTarget);

		pTarget = (void*)GetProcAddress(hUser32, "GetWindowTextA");
		if (pTarget)
			MH_DisableHook(pTarget);

		pTarget = (void*)GetProcAddress(hUser32, "SetMenuItemInfoA");
		if (pTarget)
			MH_DisableHook(pTarget);

		pTarget = (void*)GetProcAddress(hUser32, "ModifyMenuA");
		if (pTarget)
			MH_DisableHook(pTarget);

		pTarget = (void*)GetProcAddress(hUser32, "SetWindowTextW");
		if (pTarget)
			MH_DisableHook(pTarget);
	}
	// --- Disable hooks in MMD's main module ---
	HMODULE hMMD = GetModuleHandle(NULL);
	if (hMMD)
	{
		void* pTarget;

		pTarget = (void*)GetProcAddress(hMMD, "ExpGetPmdFilename");
		if (pTarget)
			MH_DisableHook(pTarget);
	}
	// // CRITICAL: Do NOT call Py_FinalizeEx() here as it is unsafe in DllMain
	// MH_Uninitialize();
	// +++++ MINHOOK HOOKING LOGIC END +++++

	// // CRITICAL: Do NOT call Py_FinalizeEx() here as it is unsafe in DllMain
	// if (Py_IsInitialized())
	// {
	// 	Py_FinalizeEx();
	// }

	renderData.dispose();
	DisposePMX();
	DisposeVMD();
	DisposeAlembic();
}

// DLL entry point
BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
			hInstance = hinst;

			// Initialize MMD export function pointers
			if (!InitializeMMDExports())
			{
				::MessageBoxW(NULL, L"Failed to initialize MMD export functions", L"Initialization Error", MB_OK);
				return FALSE;
			}

			LoadSettings();

			d3d9_initialize();
			break;
		case DLL_PROCESS_DETACH:
			d3d9_dispose();
			break;
	}
	return TRUE;
}
