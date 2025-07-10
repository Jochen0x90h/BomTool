# BOM Tool
Tool for generating BOM and CPL files from KiCad PCB (.kicad_pcb) files

## Features
* Support for KiCad 8
* Zips gerber files
* Generate BOM and CPL files for JLCPCB

## Usage

$ bomtool [-n \<name>] \<option> \<path to .kicad_pcb file> \<output directory>

Option | Description
-------|-------------
-n     | Output name, optional parameter
-g     | Zip gerber files (path to gerber is read from the .kicad_pcb file)
-b     | Generate BOM and CPL files for JLCPCB in addition to gerber

Multiple .kicad_pcb files can be processed at once. This example zips the gerber for both onlyPcb.kicad_pcb and pcbAndBom.kicad_pcb and generats BOM files for pcbAndBom.kicad_pcb:

```console
$ bomtool -g onlyPcb.kicad_pcb -b pcbAndBom.kicad_pcb /path/to/output/directory
```

## Build
Use [conan](support/conan/README.md) or [vcpkg](support/vcpkg/README.md).
