# How to Build
This is a procedure to easily build by delegating the build of dependent packages to vcpkg.
The procedure is as follows.

# Setup vcpkg

This allows you to set up alembic and pybind11, which mmdbridge's build depends on.

* Clone [vcpkg](https://github.com/Microsoft/vcpkg).
* Execute bootstrap-vcpkg.bat to build vcpkg.

# Install dependent libraries with vcpkg

Note that `chcp 65001` is required.

```cmd
chcp 65001
vcpkg install alembic[hdf5]:x64-windows pybind11:x64-windows
```

Build artifacts are stored under `VCPKG_DIR/installed/x64-windows`, which will be used in subsequent steps.
Please set the environment variable `VCPKG_DIR` to the top-level directory of vcpkg.

Example:

```
VCPKG_DIR="C:\vcpkg"
```

## Preparing DirectX SDK
Since old D3D9 is required, you need to install the SDK.

* https://www.microsoft.com/en-us/download/details.aspx?id=6812

Please set the installation path to the environment variable `DXSDK_DIR`.

Example:

```
DXSDK_DIR=C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)
```

## Pre-build modification work

Before building mmdbridge, you need to modify the following files.

### Modifying d3dx9core.h

Please modify according to https://gist.github.com/t-mat/1540248#d3dx9corehを参考に修正してください.

Change
```cpp
#ifdef UNICODE
    HRESULT GetDesc(D3DXFONT_DESCW *pDesc) { return GetDescW(pDesc); }
    HRESULT PreloadText(LPCWSTR pString, INT Count) { return PreloadTextW(pString, Count); }
#else
    HRESULT GetDesc(D3DXFONT_DESCA *pDesc) { return GetDescA(pDesc); }
    HRESULT PreloadText(LPCSTR pString, INT Count) { return PreloadTextA(pString, Count); }
#endif
```
to
```cpp
#ifdef UNICODE
    HRESULT GetDesc(D3DXFONT_DESCW *pDesc) { return GetDescW(reinterpret_cast<ID3DXFont*>(this), pDesc); }
    HRESULT PreloadText(LPCWSTR pString, INT Count) { return PreloadTextW(reinterpret_cast<ID3DXFont*>(this), pString, Count); }
#else
    HRESULT GetDesc(D3DXFONT_DESCA *pDesc) { return GetDescA(reinterpret_cast<ID3DXFont*>(this), pDesc); }
    HRESULT PreloadText(LPCSTR pString, INT Count) { return PreloadTextA(reinterpret_cast<ID3DXFont*>(this), pString, Count); }
#endif
```

# Preparing the MikuMikuDance_v932x64 folder
Extract the 64-bit version of MMD into the MikuMikuDance_v932x64 folder.

```
mmdbridge
    MikuMikuDance_v932x64
        MikuMikuDance.exe
```

# Building mmdbridge
Execute `cmake_vs2022_64.bat` and build the generated `build_vs2022_64/mmdbridge.sln`. Building `INSTALL` will copy the necessary dll and py files for execution to MikuMikuDance_v932x64.

# Debug execution of mmdbridge
You can attach the debugger by specifying `MikuMikuDance_v932x64/MikuMikuDance.exe` in INSTALL project properties - Debug - Command - Browse and running with `F5`. In debug builds, pdb is embedded with the `/Z7` compile option.
