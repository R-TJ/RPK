#include "RPKLoader.hpp"

#if defined(_WIN32)

void
NPK::mapFile(const std::string &archivepath)
{
    archives.emplace_back();
    archives.back().file
        = CreateFileA(archivepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    LARGE_INTEGER fileSize;
    GetFileSizeEx(archives.back().file, &fileSize);
    archives.back().size = static_cast<size_t>(fileSize.QuadPart);

    archives.back().mapping = CreateFileMappingA(archives.back().file, NULL,
                                                 PAGE_READONLY, 0, 0, NULL);

    void *datatemp
        = MapViewOfFile(archives.back().mapping, FILE_MAP_READ, 0, 0, 0);

    archives.back().data = static_cast<const char *>(datatemp);

    archives.back().path = archivepath;
}

void
NPK::unMap()
{
    for(auto &archive : archives)
    {
        UnmapViewOfFile(archive.data);
        CloseHandle(archive.mapping);
        CloseHandle(archive.file);
    }
}

void
NPK::un_map_file(std::string &file_path)
{
    for(auto &archive : archives)
    {
        if(archive.path == file_path)
        {
            UnmapViewOfFile(archive.data);
            CloseHandle(archive.mapping);
            CloseHandle(archive.file);
        }
    }
}

#elif defined(__APPLE__) || defined(__linux__)

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int
NPK::mapFile(const std::string &archivepath)
{
    std::cout << archivepath + "\n";

    archive a;

    a.fd = open(archivepath.c_str(), O_RDONLY);
    if(a.fd < 0)
    {
        perror(("open failed: " + archivepath).c_str());
        return OPEN_FAIL_CODE;
    }

    struct stat st;
    if(fstat(a.fd, &st) < 0)
    {
        perror("fstat failed");
        close(a.fd);
        return FSTAT_FAIL_CODE;
    }

    a.size = st.st_size;

    std::cout << std::to_string(a.size) + "\n";

    void *ptr = mmap(nullptr, a.size, PROT_READ, MAP_PRIVATE, a.fd, 0);
    if(ptr == MAP_FAILED)
    {
        perror("mmap failed");
        close(a.fd);
        return MMAP_FAIL_CODE;
    }

    a.data = static_cast<const char *>(ptr);
    a.path = archivepath;

    archives.push_back(a);

    return 0;
}

void
NPK::unMap()
{
    for(auto &archive : archives)
    {
        void *data = const_cast<char *>(archive.data);
        munmap(data, archive.size);
        close(archive.fd);
    }
}

int
NPK::un_map_file(std::string &file_path)
{
    bool unarchived = false;
    for(auto &archive : archives)
    {
        if(archive.path == file_path)
        {
            void *data = const_cast<char *>(archive.data);
            munmap(data, archive.size);
            close(archive.fd);

            unarchived = true;
        }
    }

    if(unarchived == false)
    {
        return UN_MAP_FAIL_CODE;
    }

    return 0;
}

#endif

NPK::NPK(std::string pak_dir, bool encrypt, unsigned char *key)
{
    if(sodium_init() < 0)
    {
        std::cerr << "sodium failed init\n";
        initialised = false;
        error_code = -1;
        return;
    }

    int code = mapFile(pak_dir);
    if(code < 0)
    {
        initialised = false;
        error_code = code;
        return;
    }

    const char *data = archives.back().data;

    uint32_t filecount;
    std::memcpy(&filecount, data, sizeof(filecount));
    std::cout << "Filecount: " + std::to_string(filecount) + "\n";

    uint64_t offset = sizeof(filecount);

    unsigned long long size;
    std::memcpy(&size, data + offset, sizeof(size));
    offset += sizeof(size);

    std::cout << "size: " + std::to_string(size) + "\n";

    if(encrypt)
    {
        auto nonce = std::make_unique<unsigned char[]>(
            crypto_aead_aegis256_NPUBBYTES);
        std::memcpy(nonce.get(), data + offset,
                    crypto_aead_aegis256_NPUBBYTES);
        offset += crypto_aead_aegis256_NPUBBYTES;

        unsigned long long mlen = size - crypto_aead_aegis256_ABYTES;
        unencrypted = std::make_unique<unsigned char[]>(mlen);
        auto encrypted = std::make_unique<unsigned char[]>(size);
        std::memcpy(encrypted.get(), data + offset, size);

        unsigned long long decrypt_size;
        int ciph = crypto_aead_aegis256_decrypt(
            unencrypted.get(), &decrypt_size, nullptr, encrypted.get(), size,
            nullptr, 0, nonce.get(), key);

        if(ciph < 0)
        {
            std::cerr << "failed to decrypt\n";
            initialised = false;
            error_code = DECRYPT_FAIL_CODE;
            return;
        }

        data = reinterpret_cast<char *>(unencrypted.get());
        offset = 0;
    }

    files.resize(filecount);
    if(files.size() != filecount)
    {
        std::cerr << "Failed to resize vector: files\n";
        initialised = false;
        error_code = VECTOR_RESIZE_FAIL_CODE;
        return;
    }

    for(auto &file : files)
    {
        std::memcpy(&file.offset, data + offset, sizeof(file.offset));
        offset += sizeof(file.offset);

        std::memcpy(&file.size, data + offset, sizeof(file.size));
        offset += sizeof(file.size);

        std::memcpy(&file.originalsize, data + offset,
                    sizeof(file.originalsize));
        offset += sizeof(file.originalsize);

        std::memcpy(&file.pathsize, data + offset, sizeof(file.pathsize));
        offset += sizeof(file.pathsize);

        std::memcpy(&file.archivepathsize, data + offset,
                    sizeof(file.archivepathsize));
        offset += sizeof(file.archivepathsize);

        std::string temp;
        temp.resize(file.pathsize);
        std::memcpy(temp.data(), data + offset, file.pathsize);
        offset += file.pathsize;
        std::filesystem::path tmp(temp);
        file.path = tmp;

        temp.resize(file.archivepathsize);
        std::memcpy(temp.data(), data + offset, file.archivepathsize);
        offset += file.archivepathsize;
        std::filesystem::path tmp2(temp);
        file.archivepath = tmp2;

        bool archiveExists = false;
    }
}

NPK::~NPK() { unMap(); }

std::vector<unsigned char> *
NPK::LoadFile(std::string filePath)
{
    for(auto &file : files)
    {
        if(file.path.generic_string() == filePath)
        {
            bool archived = false;
            const char *archive_ptr;
            if(archives.size() > 16)
            {
                unMap();
            }
            for(auto &archive : archives)
            {
                if(archive.path == file.path.generic_string())
                {
                    archived = true;
                    archive_ptr = archive.data;
                    break;
                }
            }

            if(!archived)
            {
                mapFile(file.archivepath.generic_string());
                archive_ptr = archives.back().data;
            }

            if(file.loaded == true)
            {
                return &file.data;
            }

            file.data.resize(file.size);
            std::memcpy(file.data.data(), archive_ptr + file.offset,
                        file.data.size());

            std::vector<unsigned char> decompData(file.originalsize);

            int result = LZ4_decompress_safe(
                reinterpret_cast<char *>(file.data.data()),
                reinterpret_cast<char *>(decompData.data()), file.data.size(),
                file.originalsize);

            if(result <= 0)
            {
                std::cerr << "failed to decompress\n";
                return {};
            }

            file.data = std::move(decompData);

            file.loaded = true;

            return &file.data;
        }
    }

    return {};
}

void
NPK::unload_File(std::string path)
{
    for(auto &file : files)
    {
        if(file.path == path)
        {
            file.data.resize(0);
            file.loaded = false;
        }
    }
}

#ifdef CLI

int
main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr
            << "too few arguments: usage: ./Unpacker <use encryption(t/f)> \n";
        return 1;
    }

    bool encrypt = false;
    if(*argv[1] == 't' || *argv[1] == 'T')
    {
        encrypt = true;
    }
    auto key
        = std::make_unique<unsigned char[]>(crypto_aead_aegis256_KEYBYTES);

    std::ifstream file("key");
    file.read(reinterpret_cast<char *>(key.get()),
              crypto_aead_aegis256_KEYBYTES);
    file.close();

    NPK npk("./Pak_dir.npk", encrypt, key.get());

    auto files = npk.get_Files();

    for(auto &file : *files)
    {
        auto data = npk.LoadFile(file.path.generic_string());

        if(file.path.has_parent_path())
        {
            std::filesystem::create_directories(file.path.parent_path());
        }

        std::ofstream of("." / file.path);
        of.write(reinterpret_cast<char *>(file.data.data()), file.data.size());
        of.close();
    }

    return 0;
}

#endif
