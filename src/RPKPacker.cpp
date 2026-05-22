#include "RPKPacker.hpp"

std::optional<std::vector<unsigned char>>
readFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);

    if(!file.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return std::nullopt;
    }

    std::vector<unsigned char> buffer(std::istream_iterator<char>(file), {});

    return buffer;
}

int
packFolder(const std::filesystem::path &folderPath,
           const std::filesystem::path &root_Path, int compression_level,
           int max_archive_size, bool encrypt, unsigned char *key)
{
    max_archive_size *= 1000 * 1000; // 1 MB

    if(sodium_init() < 0)
    {
        std::cout << "Failed to initialise libsodium";
        return LIBSODIUM_FAIL_CODE;
    }

    auto nonce
        = std::make_unique<unsigned char[]>(crypto_aead_aegis256_NPUBBYTES);

    randombytes(nonce.get(), crypto_aead_aegis256_NPUBBYTES);

    std::vector<File> files;

    uint16_t archives = 0;

    std::filesystem::path archivesPath
        = "Pak_" + std::to_string(archives) + ".rpk";

    std::ofstream of(folderPath.parent_path() / archivesPath,
                     std::ios::binary);

    if(!of.is_open())
    {
        std::cout << "Failed to create file: " << archivesPath << std::endl;
        return OFSTREAM_FAIL_CODE;
    }

    float MB = 0;
    float MS = 0;
    float MB_P_S = 0;
    float old_time = 0;

    for(auto &entry : std::filesystem::recursive_directory_iterator(
            folderPath,
            std::filesystem::directory_options::skip_permission_denied))
    {
        if(!entry.is_regular_file() || entry.path().filename() == ".DS_Store")
            continue;

        auto start = std::chrono::steady_clock::now();

        auto relative = std::filesystem::relative(entry.path(),
                                                  folderPath.parent_path());
        std::filesystem::path rel = root_Path / relative;

        auto dataUncompressed = readFile(entry.path());

        if(!dataUncompressed.has_value())
        {
            return IFSTREAM_FAIL_CODE;
        }

        if(dataUncompressed->empty())
        {
            std::cout << "Warning, skipping empty file: " << entry.path()
                      << std::endl;
            continue;
        }

        std::vector<unsigned char> data(
            LZ4_compressBound(dataUncompressed->size()));

        int result = LZ4_compress_HC(
            reinterpret_cast<char *>(dataUncompressed->data()),
            reinterpret_cast<char *>(data.data()), dataUncompressed->size(),
            data.size(), compression_level);

        if(result <= 0)
        {
            std::cerr << "Failed to compress file: " << relative << std::endl;
            continue;
        }

        data.resize(result);

        files.emplace_back();

        auto &file = files.back();

        file.compressed_size = result;
        file.original_size = dataUncompressed->size();
        file.path_size = rel.generic_string().size();
        file.path = rel.generic_string();

        if(of.tellp() < 0)
        {
            std::cerr << "Tellp failed\n";
            return TELLP_FAIL_CODE;
        }
        file.offset = of.tellp();
        file.archive_path = archivesPath.generic_string();
        file.archive_path_size = archivesPath.generic_string().size();

        of.write(reinterpret_cast<char *>(data.data()), file.compressed_size);

        if(!of)
        {
            std::cerr << "Failed to write to file: " << file.path << std::endl;
            return OFSTREAM_WRITE_FAIL;
        }

        if(of.tellp() > max_archive_size)
        {
            of.close();
            archives++;
            archivesPath = "Pak_" + std::to_string(archives) + ".rpk";

            of.open(folderPath.parent_path() / archivesPath, std::ios::binary);

            if(!of.is_open())
            {
                std::cout << "Failed to make archive: " << archivesPath;
                return OFSTREAM_FAIL_CODE;
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto dur = end - start;
        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                        .count();
        MB += file.original_size / 1000000.0f;
        MS += time;
        float diff = MS - old_time;
        if(diff > 500)
        {
            MB_P_S = MB / (MS / 1000.0f);
            std::cout << MB_P_S << "mb/s\n";
            old_time = MS;
        }
    }

    std::ofstream Pak_dir(folderPath.parent_path() / "Pak_dir.rpk",
                          std::ios::binary);

    if(!Pak_dir.is_open())
    {
        std::cerr << "Failed to create Pak_dir archive\n";
        return PAK_DIR_FAIL_CODE;
    }

    size_t size = files.size();
    Pak_dir.write(reinterpret_cast<const char *>(&size), sizeof(size));

    if(!Pak_dir)
    {
        std::cerr << "Size write failed\n";
        return OFSTREAM_WRITE_FAIL;
    }

    size_t ex_size = 0;
    for(auto &file : files)
    {
        ex_size += sizeof(file.archive_path_size);
        ex_size += sizeof(file.original_size);
        ex_size += sizeof(file.path_size);
        ex_size += sizeof(file.compressed_size);
        ex_size += sizeof(file.offset);
        ex_size += file.path_size;
        ex_size += file.archive_path_size;
    }

    auto pak_data = std::make_unique<unsigned char[]>(ex_size);

    size_t offset = 0;

    for(auto &file : files)
    {
        std::memcpy(pak_data.get() + offset, &file.offset,
                    sizeof(file.offset));
        offset += sizeof(file.offset);

        std::memcpy(pak_data.get() + offset, &file.compressed_size,
                    sizeof(file.compressed_size));
        offset += sizeof(file.compressed_size);

        std::memcpy(pak_data.get() + offset, &file.original_size,
                    sizeof(file.original_size));
        offset += sizeof(file.original_size);

        std::memcpy(pak_data.get() + offset, &file.path_size,
                    sizeof(file.path_size));
        offset += sizeof(file.path_size);

        std::memcpy(pak_data.get() + offset, &file.archive_path_size,
                    sizeof(file.archive_path_size));
        offset += sizeof(file.archive_path_size);

        std::memcpy(pak_data.get() + offset, file.path.data(), file.path_size);
        offset += file.path_size;

        std::memcpy(pak_data.get() + offset, file.archive_path.data(),
                    file.archive_path_size);
        offset += file.archive_path_size;
    }

    std::unique_ptr<unsigned char[]> out;
    unsigned long long out_size;
    if(encrypt)
    {
        out = std::make_unique<unsigned char[]>(ex_size
                                                + crypto_aead_aegis256_ABYTES);

        int ciph = crypto_aead_aegis256_encrypt(
            out.get(), &out_size, pak_data.get(), ex_size, nullptr, 0, nullptr,
            nonce.get(), key);
        if(ciph == -1)
        {
            std::cerr << "Failed to encrypt data\n";
            return ENCRYPT_FAIL_CODE;
        }
    }
    else
    {
        out = std::move(pak_data);
        out_size = ex_size;
    }

    Pak_dir.write(reinterpret_cast<char *>(&out_size), sizeof(out_size));
    if(!Pak_dir)
    {
        std::cerr << "Failed out_size write\n";
        return OFSTREAM_WRITE_FAIL;
    }

    if(encrypt)
    {
        Pak_dir.write(reinterpret_cast<char *>(nonce.get()),
                      crypto_aead_aegis256_NPUBBYTES);
        if(!Pak_dir)
        {
            std::cerr << "failed nonce write\n";
            return OFSTREAM_WRITE_FAIL;
        }
    }

    Pak_dir.write(reinterpret_cast<char *>(out.get()), out_size);
    if(!Pak_dir)
    {
        std::cerr << "Failed data write\n";
        return OFSTREAM_WRITE_FAIL;
    }

    return 0;
}

#ifdef CLI

int
main(int argc, char *argv[])
{
    if(argc < 6)
    {
        std::cout << "too few arguments. usage: ./Packer <folder path> <root "
                     "path> <compression level> <max archive size> <optional "
                     "encryption(t/f)>";
        return 1;
    }

    if(sodium_init() < 0)
    {
        std::cerr << "failed to init sodium";
        return LIBSODIUM_FAIL_CODE;
    }

    auto key
        = std::make_unique<unsigned char[]>(crypto_aead_aegis256_KEYBYTES);
    crypto_aead_aegis256_keygen(key.get());

    bool encrypt = false;
    if(*argv[5] == 't' || *argv[5] == 'T')
    {
        encrypt = true;
    }

    int x = packFolder(argv[1], argv[2], std::stoi(argv[3]),
                       std::stoi(argv[4]), encrypt, key.get());

    if(encrypt)
    {
        std::filesystem::path path(argv[1]);
        std::ofstream of(path.parent_path() / "key.bin", std::ios::binary);
        if(!of.is_open())
        {
            std::cerr << "Failed to open key save file";
            return OFSTREAM_FAIL_CODE;
        }
        of.write(reinterpret_cast<char *>(key.get()),
                 crypto_aead_aegis256_KEYBYTES);
        if(!of)
        {
            std::cerr << "Failed to write key";
            return OFSTREAM_WRITE_FAIL;
        }

        std::cout << "Your key is stored in the key file\n";
    }

    return 0;
}

#endif
