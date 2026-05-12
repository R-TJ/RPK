#include "RPKPacker.hpp"

std::vector<unsigned char>
readFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);

    if(!file.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return {};
    }

    file.seekg(0, std::ios::end);
    auto end = file.tellg();
    if(end < 0)
    {
        std::cerr << "tellg failed: " << path << "\n";
        return {};
    }
    std::streamsize size = static_cast<std::streamsize>(end);
    file.seekg(0, std::ios::beg);

    if(size == 0)
    {
        return {};
    }

    std::vector<unsigned char> buffer(size);

    if(!file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        std::cerr << "Failed to read file: " << path << std::endl;
        return {};
    }

    return buffer;
}

int
packFolder(const std::filesystem::path &folderPath,
           const std::filesystem::path &root_Path, int compression_level,
           int max_archive_size, bool encrypt, unsigned char *key)
{
    max_archive_size *= 1000 * 1000;

    if(sodium_init() < 0)
    {
        std::cout << "Failed to initialise libsodium";
        return -1;
    }

    auto nonce
        = std::make_unique<unsigned char[]>(crypto_aead_aegis256_NPUBBYTES);

    randombytes(nonce.get(), crypto_aead_aegis256_NPUBBYTES);

    std::vector<FileEntry> files;

    uint16_t archives = 0;

    std::filesystem::path archivesPath
        = "Pak_" + std::to_string(archives) + ".npk";

    std::ofstream of(archivesPath, std::ios::binary);

    if(!of.is_open())
    {
        std::cout << "Failed to create file: " + archivesPath.generic_string()
                         + "\n";
        return -2;
    }

    for(auto &entry :
        std::filesystem::recursive_directory_iterator(folderPath))
    {
        if(!entry.is_regular_file() || entry.path().filename() == ".DS_Store")
            continue;

        auto relative = std::filesystem::relative(entry.path(),
                                                  folderPath.parent_path());
        std::filesystem::path rel = root_Path / relative;

        auto dataUncompressed = readFile(entry.path());
        if(dataUncompressed.size() < 3)
        {
            std::string error(
                reinterpret_cast<char *>(dataUncompressed.data()));
            if(error == TELLG_FAIL)
            {
                std::cerr << "Tellg fialed\n";
                return TELLG_FAIL_CODE;
            }
            else if(error == FAIL_IFREAD)
            {
                std::cerr << "Ifstream read failed\n";
                return IFSTREAM_FAIL_READ_CODE;
            }
            else if(error == IFSTREAM_FAIL)
            {
                std::cerr << "Failed to create ifstream\n";
                return IFSTREAM_FAIL_CODE;
            }
        }

        if(dataUncompressed.empty())
            continue;

        std::vector<unsigned char> data(
            LZ4_compressBound(dataUncompressed.size()));

        int result = LZ4_compress_HC(
            reinterpret_cast<char *>(dataUncompressed.data()),
            reinterpret_cast<char *>(data.data()), dataUncompressed.size(),
            data.size(), compression_level);

        if(result < 0)
        {
            std::cerr << "failed to compress file: "
                      << relative.generic_string() << std::endl;
            continue;
        }

        data.resize(result);

        files.emplace_back();

        auto &file = files.back();

        file.size = result;
        file.originalsize = dataUncompressed.size();
        file.pathsize = rel.generic_string().size();
        file.path = rel.generic_string();

        file.offset = of.tellp();
        file.archivepath = archivesPath.generic_string();
        file.archivepathsize = archivesPath.generic_string().size();

        of.write(reinterpret_cast<char *>(data.data()), result);

        if(of.tellp() > max_archive_size)
        {
            of.close();
            archives++;
            archivesPath = "Pak_" + std::to_string(archives) + ".npk";

            of.open(archivesPath, std::ios::binary);

            if(!of.is_open())
            {
                std::cout << "failed to make archive: "
                                 + archivesPath.generic_string();
                return -2;
            }
        }
    }

    std::ofstream Pak_dir("Pak_dir.npk", std::ios::binary);

    if(!Pak_dir.is_open())
    {
        std::cerr << "failed to create Pak_dir archive\n";
        return -3;
    }

    uint32_t size = files.size();
    Pak_dir.write(reinterpret_cast<const char *>(&size), sizeof(size));

    if(!Pak_dir)
    {
        std::cerr << "size write failed\n";
        return -4;
    }

    size_t ex_size = 0;
    for(auto &file : files)
    {
        ex_size += sizeof(file.archivepathsize);
        ex_size += sizeof(file.originalsize);
        ex_size += sizeof(file.pathsize);
        ex_size += sizeof(file.size);
        ex_size += sizeof(file.offset);
        ex_size += file.pathsize;
        ex_size += file.archivepathsize;
    }

    auto pak_data = std::make_unique<unsigned char[]>(ex_size);

    size_t offset = 0;

    for(auto &file : files)
    {
        std::memcpy(pak_data.get() + offset, &file.offset,
                    sizeof(file.offset));
        offset += sizeof(file.offset);

        std::memcpy(pak_data.get() + offset, &file.size, sizeof(file.size));
        offset += sizeof(file.size);

        std::memcpy(pak_data.get() + offset, &file.originalsize,
                    sizeof(file.originalsize));
        offset += sizeof(file.originalsize);

        std::memcpy(pak_data.get() + offset, &file.pathsize,
                    sizeof(file.pathsize));
        offset += sizeof(file.pathsize);

        std::memcpy(pak_data.get() + offset, &file.archivepathsize,
                    sizeof(file.archivepathsize));
        offset += sizeof(file.archivepathsize);

        std::memcpy(pak_data.get() + offset, file.path.data(), file.pathsize);
        offset += file.pathsize;

        std::memcpy(pak_data.get() + offset, file.archivepath.data(),
                    file.archivepathsize);
        offset += file.archivepathsize;
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
            std::cout << "failed to encrypt data\n";
            return -5;
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
        std::cerr << "failed out_size write\n";
        return -4;
    }

    if(encrypt)
    {
        Pak_dir.write(reinterpret_cast<char *>(nonce.get()),
                      crypto_aead_aegis256_NPUBBYTES);
        if(!Pak_dir)
        {
            std::cerr << "failed nonce write\n";
            return -4;
        }
    }

    Pak_dir.write(reinterpret_cast<char *>(out.get()), out_size);
    if(!Pak_dir)
    {
        std::cerr << "failed data write\n";
        return -4;
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
        return -1;
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
        std::ofstream of("key");
        if(!of.is_open())
        {
            std::cerr << "failed to open key save file";
            return -5;
        }
        of.write(reinterpret_cast<char *>(key.get()),
                 crypto_aead_aegis256_KEYBYTES);

        std::cout << "Your key is stored in the key file\n";
    }

    return 0;
}

#endif
