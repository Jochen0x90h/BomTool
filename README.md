# BOM Tool
Tool for generating BOM and CPL files from KiCad PCB (.kicad_pcb) files

## Features
* Support for KiCad 8
* Zips gerber files
* Generate BOM and CPL files for JLCPCB

## Usage

$ bomtool \<options> \<path to .kicad_pcb file> \<output directory>

Option | Description
-------|-------------
-n     | Name for output files (optional, derived from .kicad_pcb file name if not given)
-g     | Export and zip gerber files (path to gerber and layers are read from the .kicad_pcb file)
-b     | Generate BOM and placement file
-j     | Generate for JLCPCB (oval holes alternate, BOM with LCSC PN, CPL file)

Multiple .kicad_pcb files can be processed at once. This example zips the gerber for both onlyPcb.kicad_pcb and pcbAndBom.kicad_pcb and generats BOM files for pcbAndBom.kicad_pcb:

```console
$ bomtool -j -g onlyPcb.kicad_pcb -g -b pcbAndBom.kicad_pcb /path/to/output/directory
```


## Build with Conan 2.x

If you use conan for the first time, run

```console
$ conan profile detect
```

It creates a default profile in ~/.conan2/profiles

### Windows
Open default profile (~/.conan2/profiles/default) with text editor, check that the compiler is msvc and set cppstd to 23:

```
[settings]
...
compiler=msvc
compiler.cppstd=23
...
```

If the compiler is missing, install Visual Studio Community Edition (in addition to VSCode!) with C++ for desktop option and run conan profile detect again.

### Linux
Open default profile (~/.conan2/profiles/default) with text editor, check that the compiler is gcc and set cppstd to 23:

```
[settings]
...
compiler=gcc
compiler.cppstd=23
...
```

If you want to let conan install missing packages, add these lines at the end of the profile:

```
[conf]
tools.system.package_manager:mode=install
tools.system.package_manager:sudo=True
```

### Debug Profile

Create debug profile ~/.conan2/profiles/debug by copying the default profile and set

```
build_type=Debug
```

### Presets

Run

```console
$ python cinstall.py
```

It generates CMakeUserPresets.json which can be used by IDEs such as VSCode
