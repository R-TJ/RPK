# RPK
A folder archiver designed to be used in game engines

## Installation
### Requirements
- CMake
- vcpkg
- C++ 17 compatible compiler

### CMake
'''
git clone github.com/Desnio/RPK.git
cd RPK
CMake -B build
'''

## Usage
### Packing
'''
./Packer <folder path to pack> <the root path> <compression level> <encryption>
'''
The folder path is the path of the folder to pack. Works with relative paths. The output archives are put next to this folder

The root path is a path to be put before the relative path of the file relative to the parent of the archived folder
E.g. in the build folder there is a CMakeFiles folder, and a makefile in CMakeFiles, if we pack the CMakeFiles with a root path of "root", then the path to makefile will be root/CMakeFiles/makefile

The compression level is a number between 1 and 12, you can look up more on the lz4hc documentation

The encryption is an option to encrypt the Pak_dir.npk with aead aegis256 with libsodium. If encryption is on then it will export a file call key next to the Pak_dir.npk. You can use the key in your application to use the encrypted data. If you use the unpacker then you need the key file to be next to the Pak_dir.npk.

### Unpacking
'''
./Unpacker <encryption>
'''
The executable must be in the same folder as the Pak_dir.npk

### Library
ALWAYS CHECK THE ERROR CODE AFTER CREATING THE CLASS
