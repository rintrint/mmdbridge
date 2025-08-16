# Externals

## Files

### MMDExport.h
Modified MMDExport.h to make the build work properly.
- Re-saved the file using UTF-8 encoding
- Changed the export/import definitions:

**Original:**
```cpp
#define		_EXPORT	__declspec(dllexport)			// MMD本体用	(dll側ではコメントアウトする事)
//#define	_EXPORT	__declspec(dllimport)			// dll用		(dll側ではコメントアウトを外す事)
```

**Modified to:**
```cpp
//#define	_EXPORT	__declspec(dllexport)			// MMD本体用	(dll側ではコメントアウトする事)
#define		_EXPORT	__declspec(dllimport)			// dll用		(dll側ではコメントアウトを外す事)
```

### MikuMikuDance_v932x64.zip
MikuMikuDance Ver.9.32 binary for GitHub Actions automated builds.

### MikuMikuDance.exe
Modified MikuMikuDance.exe using CFF Explorer to enable console window for debugging purposes.
