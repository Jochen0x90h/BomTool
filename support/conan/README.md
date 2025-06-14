## Conan profiles

Run

```
conan profile detect
```

to create a default profile if necessary.

Windows: It should contain these lines:

```
compiler=msvc
compiler.cppstd=20
```

If not, Install Visual Studio Community Edition (in addition to VSCode!)
with C++ for desktop option and/or change cppstd to 20.

Create debug profile by copying the default profile and set

```
build_type=Debug
```

## Presets

Copy presets.txt from the subdirectory for your platform into the root of the project and modify it to contain all
desired platforms (native / microcontrollers).

Then run

```
python configure.py
```

It generates CMakeUserPresets.json which can be used by IDEs such as VSCode


## Install to Conan Cache

To install to the local conan cache, run

```
python create.py
```
