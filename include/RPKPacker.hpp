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
    size_t offset;
    size_t compressed_size;
    size_t original_size;
    size_t path_size;
    size_t archive_path_size;
    std::string path;
    std::string archive_path;
};

std::optional<std::vector<unsigned char>> readFile(const std::filesystem::path &path);

int
packFolder(const std::filesystem::path &folderPath,
           const std::filesystem::path &root_Path, int compression_level,
           int max_archive_size, bool encrypt, unsigned char* key);


