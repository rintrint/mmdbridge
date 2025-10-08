# MMDBridge

MMDBridge can bake and export files from MMD to multiple formats including VMD and ABC.

## For Users

### Installation

1. Go to the [Releases page](https://github.com/rintrint/mmdbridge/releases) to download the latest version.
2. Extract the downloaded file to use. MikuMikuDance program is included, no separate download required.

### System Requirements

Before running, please ensure you have installed the following system components:

* [Microsoft Visual C++ Redistributable Runtimes](https://github.com/abbodi1406/vcredist)
* [DirectX End-User Runtimes (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=8109)

### User Guide

For detailed instructions, please refer to the [User Guide](docs/how_to_use.md).

---

## For Developers

If you want to compile the source code yourself or participate in development, please refer to the following instructions.

### Build Requirements

* Visual Studio 2022
* [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812)
* MikuMikuDance Ver.9.32

### Build Instructions

For detailed build steps, please refer to the [Build Instructions](docs/how_to_build.md).

## Dependencies

* vcpkg (for building)
* DirectX SDK (June 2010)
* MikuMikuFormats
* alembic
* pybind11
* python3
* MinHook

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contributors

[MMDBridge is open source and anyone can participate in development.](https://github.com/rintrint/mmdbridge/graphs/contributors)

## Acknowledgements

Special thanks to:

* **樋口優** for creating the original MikuMikuDance application
* [**吃爆米花的小熊**](https://github.com/walogia) for the Simplified Chinese localization of MikuMikuDance
* [**XPRAMT**](https://github.com/XPRAMT) for the Traditional Chinese localization of MikuMikuDance
