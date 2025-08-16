# how to build
vcpkgに依存パッケージのビルドを任せて簡単にビルドする手順です。
以下、手順。

# vcpkgをsetupする

mmdbridgeのビルドが依存しているalembic, pybind11のセットアップができる。

* [vcpkg](https://github.com/Microsoft/vcpkg)をcloneする。
* bootstrap-vcpkg.batを実行してvcpkgをビルドする。

# vcpkgで依存ライブラリをインストールする

``chcp 65001``が必要なのに注意。

```cmd
chcp 65001
vcpkg install alembic[hdf5]:x64-windows pybind11:x64-windows
```

``VCPKG_DIR/installed/x64-windows``以下にビルド成果物が格納されるので以降の手順でこれを利用します。
環境変数``VCPKG_DIR``にvcpkgのトップレベルのディレクトリを設定してください。

例。

```
VCPKG_DIR="C:\vcpkg"
```

## DirectX SDKの準備
昔のD3D9が必要なのでSDKをインストールする必要があります。

* https://www.microsoft.com/en-us/download/details.aspx?id=6812

インストールパスを環境変数``DXSDK_DIR``に設定してください。

例。

```
DXSDK_DIR=C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)
```

## ビルド前の修正作業

mmdbridgeをビルドする前に、以下のファイルを修正する必要があります。

### d3dx9core.hの修正

https://gist.github.com/t-mat/1540248#d3dx9corehを参考に修正してください。

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

### MMDExport.hの修正

``MikuMikuDance_x64\Data\MMDExport.h``のコードページをUTF-8に変更して保存してください。

# MikuMikuDance_x64フォルダの準備
64bit版のMMDをMikuMikuDance_x64フォルダに展開します。

```
mmdbridge
    MikuMikuDance_x64
        MikuMikuDance.exe
        Data
            MMDExport.h
            MMDExport.lib
```

# mmdbridgeのビルド
``cmake_vs2022_64.bat``を実行して生成された``build_vs2022_64/mmdbridge.sln``をビルドしてください。``INSTALL``をビルドすると実行に必要なdllとpyをMikuMikuDance_x64にコピーします。

# mmdbridgeのデバッグ実行
INSTALLプロジェクトのプロパティ - デバッグ - コマンド - 参照で``MikuMikuDance_x64/MikuMikuDance.exe``を指定して``F5``実行するとデバッガをアタッチできます。デバッグビルドには、``/Z7``コンパイルオプションでpdbを埋め込んであります。
