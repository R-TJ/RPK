#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <optional>
#include "iterator"

#include "lz4.h"
#include "lz4hc.h"

#include "sodium.h"
#include "sodium/crypto_aead_aegis256.h"

struct File {
    uint64_t offset;
    uint64_t compressed_size;
    uint64_t original_size;
    uint64_t path_size;
    uint64_t archive_path_size;
    std::filesystem::path path;
    std::filesystem::path archive_path;
};

std::optional<std::vector<unsigned char>>
readFile(const std::filesystem::path &path);

int packFolder(const std::filesystem::path &folderPath,
               const std::filesystem::path &root_Path, int compression_level,
               int max_archive_size, bool encrypt, unsigned char *key);
