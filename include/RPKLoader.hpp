#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include "lz4.h"


#include "sodium.h"
#include "sodium/crypto_aead_aegis256.h"


#ifdef _WIN32

#include <windows.h>

#endif

struct archive
{
    off_t size;
    const char* data;

    std::filesystem::path path;

#ifdef _WIN32
    HANDLE mapping;
    HANDLE file;
#elif defined(__linux__) || defined(__APPLE__)
    int fd;
#endif
};
struct file
{
    size_t offset;
    size_t size;
    size_t originalsize;
    size_t pathsize;
    size_t archivepathsize;
    std::filesystem::path path;
    std::filesystem::path archivepath;
    std::vector<unsigned char> data;
    bool loaded = false;
};

class RPK
{
public:
	RPK(std::filesystem::path pak_dir, bool encrypt, unsigned char* key);
	~RPK();

	std::vector<unsigned char>* LoadFile(std::string path);
    
    std::vector<archive>* get_Archives() {return &archives;};
    std::vector<file>* get_Files() {return &files;};
    
    void unload_File(std::string path);

    bool initialised = false;
    int error_code = 0;

private:
	int mapFile(const std::string& archivepath);
	void unMap();
    int un_map_file(std::string& file_path);
	std::vector<archive> archives;
	std::vector<file> files;

    std::unique_ptr<unsigned char[]> unencrypted;

    float MB_P_S = 0;
    float MS = 0;
    float MB = 0;
    float old_time = 0;
};
