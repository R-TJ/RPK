#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <cstdint>
#include <chrono>

#include "lz4hc.h"
#include "sodium.h"
#include "sodium/crypto_aead_aegis256.h"
#include "sodium/crypto_aead_xchacha20poly1305.h"

struct Entry {
    uint64_t offset;
    uint64_t compressed_size, original_size;
    uint64_t path_size, archive_path_size;
    std::filesystem::path file_path, archive_path;
};

struct File {
    std::vector<unsigned char> data;
    int error = 0;
    bool loaded = false;
};

File load_file(std::filesystem::path path);

int pack_folder(const std::filesystem::path folder_path,
                const std::filesystem::path root_path, int compression_level,
                int max_archive_size, bool encrypt, bool aegis,
                unsigned char *key);
