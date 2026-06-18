#include "include/Packer.hpp"

File
load_file(std::filesystem::path path)
{
    File file;

    std::ifstream stream(path, std::ios::binary);

    if(!stream.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        file.loaded = false;
        file.error = IFSTREAM_FAIL;
    }

    std::vector<unsigned char> temp(
        std::istream_iterator<unsigned char>(stream), {});
    if(temp.empty())
    {
        std::cerr << "Failed to read from file: " << path << std::endl;
        file.loaded = false;
        file.error = IFSTREAM_READ_FAIL;
    }
    else
    {
        file.data = std::move(temp);
    }

    return file;
}

int
pack_folder(const std::filesystem::path folder_path,
            const std::filesystem::path root_path, int compression_level,
            int max_archive_size, bool encrypt, bool aegis, unsigned char *key)
{
    // Encryption setup
    if(sodium_init() < 0)
    {
        std::cerr << "Failed to initialise sodium\n";
        return LIBSODIUM_FAIL_CODE;
    }
    std::unique_ptr<unsigned char[]> nonce;
    if(aegis)
    {
        nonce = std::make_unique<unsigned char[]>(
            crypto_aead_aegis256_NPUBBYTES);
        randombytes(nonce.get(), crypto_aead_aegis256_NPUBBYTES);
    }
    else
    {
        nonce = std::make_unique<unsigned char[]>(
            crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
        randombytes(nonce.get(), crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    }

    max_archive_size *= 1000 * 1000; // Bytes to MB

    std::vector<Entry> entries;

    uint64_t archives = 0;

    std::filesystem::path archive_path
        = folder_path.parent_path()
          / ("Pak_" + std::to_string(archives) + ".rpk");
    archives++;

    std::ofstream archive(archive_path);

    if(!archive.is_open())
    {
        std::cerr << "Failed to create initial archive\n";
        return OFSTREAM_FAIL;
    }

    int files;                    // Speed tracking
    float ms, mb, mb_s, old_time; // Speed tracking

    for(auto &entry : std::filesystem::recursive_directory_iterator(
            folder_path,
            std::filesystem::directory_options::skip_permission_denied))
    {
        if(!entry.is_regular_file() || entry.path().filename() == ".DS_Store")
        {
            continue; // Skip irregular and ignored files;
        }

        auto start = std::chrono::steady_clock::now(); // Speed tracking

        // Get the path relative to the root folder
        auto relative = std::filesystem::relative(entry.path(),
                                                  folder_path.parent_path());

        auto final_path = root_path / relative;

        auto input_data = load_file(entry.path());
        if(!input_data.loaded)
        {
            std::cerr << "Failed to load file: " << entry.path() << std::endl;
            return input_data.error;
        }
        if(input_data.data.empty())
        {
            std::cout << "WARNING, skipping empty file: " << entry.path()
                      << std::endl;
            continue;
        }

        std::vector<unsigned char> data(
            LZ4_compressBound(input_data.data.size()));

        int result = LZ4_compress_HC(
            reinterpret_cast<char *>(input_data.data.data()),
            reinterpret_cast<char *>(data.data()), input_data.data.size(),
            data.size(), compression_level);

        if(result <= 0)
        {
            std::cerr << "Failed to compress file: " << entry.path()
                      << std::endl;
            return LZ4_FAIL;
        }

        data.resize(result);

        entries.emplace_back();

        auto &file = entries.back();

        file.file_path = entry.path();
        file.archive_path = archive_path;
        file.path_size = file.file_path.generic_string().size();
        file.archive_path_size = file.archive_path.generic_string().size();
        file.original_size = input_data.data.size();
        file.compressed_size = result;
        file.offset = archive.tellp();

        archive.write(reinterpret_cast<char *>(data.data()), result);
        if(!archive)
        {
            std::cerr << "Failed to write to archive: " << archive_path
                      << std::endl;
            return OFSTREAM_WRITE_FAIL;
        }

        if(archive.tellp() > max_archive_size)
        {
            archive.close();

            archive_path = folder_path.parent_path()
                           / ("Pak_" + std::to_string(archives) + ".rpk");
            archive.open(archive_path, std::ios::binary);
            if(!archive.is_open())
            {
                std::cerr << "Failed to open archive: " << archive_path
                          << std::endl;
                return OFSTREAM_FAIL;
            }
            archives++;
        }

        auto end = std::chrono::steady_clock::now();
        auto dur = end - start;
        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(dur)
                        .count();
        if(files < SPEED_CHECK_DURATION)
        {
            ms += time;
            mb += file.original_size / (1000.0f * 1000.0f);
            mb_s = mb / ms;
            std::cout << mb_s << "mb/s\n";
        }
        else
        {
            // Reset to show current average speed not total average after many
            // files
            ms = 0;
            mb = 0;
        }
    }
}
