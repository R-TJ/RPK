# RPK
A folder archiver designed to be used in game engines

Still to be finished and I still have to finish the README

Only has Mac and Linux support because I have no way to test on Windows and no reason to learn Windows only functions

## Installation
### Requirements
- CMake
- vcpkg
- C++ 17 compatible compiler

### CMake

```
git clone github.com/Desnio/RPK.git
cd RPK
CMake -B build
```

## Usage
### Packing
```
./Packer <folder path to pack> <the root path> <compression level> <encryption>
```


The folder path is the path of the folder to pack. Works with relative paths. The output archives are put next to this folder

The root path is a path to be put before the relative path of the file relative to the parent of the archived folder. This is quite confusing and better shown in examples

The compression level is a number between 1 and 12, you can look up more on the lz4hc documentation

The encryption is an option to encrypt the Pak_dir.npk with aead aegis256 with libsodium. If encryption is on then it will export a file "key" next to the Pak_dir.npk. You can use the key in your application to use the encrypted data. If you use the unpacker then you need the "key" file to be next to the Pak_dir.npk.

### Unpacking
```
./Unpacker <enryption(t/f)>
```


The executable must be in the same folder as the Pak_dir.npk and the "key" file if encryption is used

## Library
ALWAYS CHECK THE ERROR CODE AFTER CREATING THE CLASS

### Usage





## Examples

### Packing the source code

```
./RPK/build-mac/Packer RPK root 12 100 f
```

It will pack the source code into files with the root set as "root" with max compression and a maximum size of 100MB per archive and without encryption

To acces the CMakeLists.txt you must use the path "root/RPK/CMakeLists.txt" because the actual path relative to the packed folder is RPK/CMakeLists.txt and the root path is "root"
