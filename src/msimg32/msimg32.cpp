#include "msimg32.h"
#include <string>
#include <Windows.h>

// Declare function pointers for the original functions
typedef BOOL(WINAPI* PFN_AlphaBlend)(
	_In_ HDC hdcDest,
	_In_ int xoriginDest,
	_In_ int yoriginDest,
	_In_ int wDest,
	_In_ int hDest,
	_In_ HDC hdcSrc,
	_In_ int xoriginSrc,
	_In_ int yoriginSrc,
	_In_ int wSrc,
	_In_ int hSrc,
	_In_ BLENDFUNCTION ftn);

typedef BOOL(WINAPI* PFN_GradientFill)(
	_In_ HDC hdc,
	_In_reads_(nVertex) PTRIVERTEX pVertex,
	_In_ ULONG nVertex,
	_In_ PVOID pMesh,
	_In_ ULONG nMesh,
	_In_ ULONG ulMode);

typedef BOOL(WINAPI* PFN_TransparentBlt)(
	_In_ HDC hdcDest,
	_In_ int xoriginDest,
	_In_ int yoriginDest,
	_In_ int wDest,
	_In_ int hDest,
	_In_ HDC hdcSrc,
	_In_ int xoriginSrc,
	_In_ int yoriginSrc,
	_In_ int wSrc,
	_In_ int hSrc,
	_In_ UINT crTransparent);

// These are undocumented functions, so we use FARPROC (a generic function pointer).
typedef FARPROC PFN_vSetDdrawflag;
typedef FARPROC PFN_DllInitialize;

// Declare static variables to store the addresses of the original functions.
static PFN_AlphaBlend original_AlphaBlend = nullptr;
static PFN_GradientFill original_GradientFill = nullptr;
static PFN_TransparentBlt original_TransparentBlt = nullptr;
static PFN_vSetDdrawflag original_vSetDdrawflag = nullptr;
static PFN_DllInitialize original_DllInitialize = nullptr;

// Implement our own exported functions
extern "C"
{
	BOOL WINAPI GradientFill(
		_In_ HDC hdc,
		_In_reads_(nVertex) PTRIVERTEX pVertex,
		_In_ ULONG nVertex,
		_In_ PVOID pMesh,
		_In_ ULONG nMesh,
		_In_ ULONG ulMode)
	{
		if (original_GradientFill)
			return original_GradientFill(hdc, pVertex, nVertex, pMesh, nMesh, ulMode);
		SetLastError(ERROR_INVALID_FUNCTION);
		return FALSE;
	}

	BOOL WINAPI AlphaBlend(
		_In_ HDC hdcDest,
		_In_ int xoriginDest,
		_In_ int yoriginDest,
		_In_ int wDest,
		_In_ int hDest,
		_In_ HDC hdcSrc,
		_In_ int xoriginSrc,
		_In_ int yoriginSrc,
		_In_ int wSrc,
		_In_ int hSrc,
		_In_ BLENDFUNCTION ftn)
	{
		if (original_AlphaBlend)
			return original_AlphaBlend(hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, ftn);
		SetLastError(ERROR_INVALID_FUNCTION);
		return FALSE;
	}

	BOOL WINAPI TransparentBlt(
		_In_ HDC hdcDest,
		_In_ int xoriginDest,
		_In_ int yoriginDest,
		_In_ int wDest,
		_In_ int hDest,
		_In_ HDC hdcSrc,
		_In_ int xoriginSrc,
		_In_ int yoriginSrc,
		_In_ int wSrc,
		_In_ int hSrc,
		_In_ UINT crTransparent)
	{
		if (original_TransparentBlt)
			return original_TransparentBlt(hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, crTransparent);
		SetLastError(ERROR_INVALID_FUNCTION);
		return FALSE;
	}

	// For undocumented functions, we cannot determine their parameters, but we can call them directly.
	void WINAPI vSetDdrawflag()
	{
		if (original_vSetDdrawflag)
			original_vSetDdrawflag();
	}

	void WINAPI DllInitialize()
	{
		if (original_DllInitialize)
			original_DllInitialize();
	}
}

namespace msimg32
{
	void initialize()
	{
		// Get the path to the original msimg32.dll in the system directory.
		wchar_t systemPath[MAX_PATH];
		GetSystemDirectoryW(systemPath, MAX_PATH);
		std::wstring originalDllPath = std::wstring(systemPath) + L"\\msimg32.dll";

		// Load the original DLL.
		HMODULE hOriginalDll = LoadLibraryW(originalDllPath.c_str());
		if (!hOriginalDll)
		{
			// Error handling: If the original DLL cannot be found, we cannot proceed.
			MessageBoxW(NULL, L"Could not load the original msimg32.dll", L"Proxy DLL Error", MB_ICONERROR);
			return;
		}

		// Get the addresses of the original functions
		original_AlphaBlend = (PFN_AlphaBlend)GetProcAddress(hOriginalDll, "AlphaBlend");
		original_GradientFill = (PFN_GradientFill)GetProcAddress(hOriginalDll, "GradientFill");
		original_TransparentBlt = (PFN_TransparentBlt)GetProcAddress(hOriginalDll, "TransparentBlt");
		original_vSetDdrawflag = (PFN_vSetDdrawflag)GetProcAddress(hOriginalDll, "vSetDdrawflag");
		original_DllInitialize = (PFN_DllInitialize)GetProcAddress(hOriginalDll, "DllInitialize");
	}
} // namespace msimg32

// DLL entry point
BOOL WINAPI DllMain(HMODULE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		msimg32::initialize();
	}
	return TRUE;
}
