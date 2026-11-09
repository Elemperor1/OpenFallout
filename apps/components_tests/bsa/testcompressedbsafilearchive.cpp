#include <components/bsa/compressedbsafile.hpp>
#include <components/files/istreamptr.hpp>
#include <components/testing/util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <lz4frame.h>
#include <zlib.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

// Archives of the Oblivion, Fallout 3, New Vegas and Skyrim formats are built here byte by byte, and read back through
// the names of their files.
namespace Bsa
{
    namespace
    {
        using namespace ::testing;

        enum : std::uint32_t
        {
            Oblivion = CompressedBSAFile::Version_TES4,
            Fallout = CompressedBSAFile::Version_FO3,
            Skyrim = CompressedBSAFile::Version_SSE,
        };

        template <class T>
        void append(std::string& out, T value)
        {
            out.append(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        /// The hash of a name that the archive formats have: the first, the last and the second last characters and the
        /// length in the low half, a polynomial of the characters between in the high half, both changed by the
        /// extension.
        std::uint64_t hashOf(std::string_view name, std::string_view extension = {})
        {
            if (name.empty())
                return 0;
            const std::size_t length = name.size();
            std::uint32_t low = static_cast<unsigned char>(name.back());
            if (length >= 3)
                low |= static_cast<std::uint32_t>(static_cast<unsigned char>(name[length - 2])) << 8;
            low |= static_cast<std::uint32_t>(length) << 16;
            low |= static_cast<std::uint32_t>(static_cast<unsigned char>(name.front())) << 24;

            std::uint32_t high = 0;
            for (std::size_t i = 1; length >= 4 && i + 2 < length; ++i)
                high = high * 0x1003f + static_cast<unsigned char>(name[i]);

            if (extension == ".kf")
                low |= 0x80;
            else if (extension == ".nif")
                low |= 0x8000;
            else if (extension == ".dds")
                low |= 0x8080;
            else if (extension == ".wav")
                low |= 0x80000000;

            // The extension has a polynomial of its own, which is added
            std::uint32_t extensionHigh = 0;
            for (const char c : extension)
                extensionHigh = extensionHigh * 0x1003f + static_cast<unsigned char>(c);
            high += extensionHigh;

            return (static_cast<std::uint64_t>(high) << 32) | low;
        }

        std::uint64_t fileHash(std::string_view fileName)
        {
            const std::size_t dot = fileName.rfind('.');
            if (dot == std::string_view::npos)
                return hashOf(fileName);
            return hashOf(fileName.substr(0, dot), fileName.substr(dot));
        }

        struct FileSpec
        {
            std::string mName;
            /// What the archive holds for the file, with the name and the size that it has when they are there
            std::string mBlob;
            /// Whether the file does the opposite of what the archive says about compression
            bool mFlipCompression = false;
        };

        struct FolderSpec
        {
            std::string mName;
            std::vector<FileSpec> mFiles;
        };

        std::string deflate(const std::string& data)
        {
            uLongf size = compressBound(static_cast<uLong>(data.size()));
            std::string result(size, '\0');
            if (compress(reinterpret_cast<Bytef*>(result.data()), &size, reinterpret_cast<const Bytef*>(data.data()),
                    static_cast<uLong>(data.size()))
                != Z_OK)
                throw std::runtime_error("Failed to compress test data");
            result.resize(size);
            return result;
        }

        std::string lz4Frame(const std::string& data)
        {
            std::string result(LZ4F_compressFrameBound(data.size(), nullptr), '\0');
            const std::size_t size
                = LZ4F_compressFrame(result.data(), result.size(), data.data(), data.size(), nullptr);
            if (LZ4F_isError(size))
                throw std::runtime_error("Failed to compress test data");
            result.resize(size);
            return result;
        }

        /// The name that Fallout 3 and the games after it write in front of the data of a file
        std::string embeddedName(const std::string& name)
        {
            std::string result;
            append<std::uint8_t>(result, static_cast<std::uint8_t>(name.size()));
            return result + name;
        }

        /// The size of the data of a compressed file, then the data
        std::string packed(const std::string& data, const std::string& compressedData)
        {
            std::string result;
            append<std::uint32_t>(result, static_cast<std::uint32_t>(data.size()));
            return result + compressedData;
        }

        std::string buildArchive(std::uint32_t version, std::uint32_t flags, const std::vector<FolderSpec>& folders,
            std::uint32_t format = static_cast<std::uint32_t>(BsaVersion::Compressed))
        {
            const bool hasFolderNames = (flags & CompressedBSAFile::ArchiveFlag_FolderNames) != 0;
            const bool hasFileNames = (flags & CompressedBSAFile::ArchiveFlag_FileNames) != 0;
            const std::size_t folderRecordSize = version == Skyrim ? 24 : 16;

            std::uint32_t fileCount = 0;
            std::uint32_t folderNamesLength = 0;
            std::uint32_t fileNamesLength = 0;
            std::size_t blocksSize = 0;
            for (const FolderSpec& folder : folders)
            {
                fileCount += static_cast<std::uint32_t>(folder.mFiles.size());
                if (hasFolderNames)
                {
                    folderNamesLength += static_cast<std::uint32_t>(folder.mName.size() + 1);
                    blocksSize += 1 + folder.mName.size() + 1;
                }
                blocksSize += 16 * folder.mFiles.size();
                for (const FileSpec& file : folder.mFiles)
                    fileNamesLength += static_cast<std::uint32_t>(file.mName.size() + 1);
            }
            if (!hasFileNames)
                fileNamesLength = 0;

            const std::size_t foldersStart = 36;
            const std::size_t blocksStart = foldersStart + folderRecordSize * folders.size();
            std::size_t dataOffset = blocksStart + blocksSize + fileNamesLength;

            std::string header;
            append<std::uint32_t>(header, format);
            append<std::uint32_t>(header, version);
            append<std::uint32_t>(header, static_cast<std::uint32_t>(foldersStart));
            append<std::uint32_t>(header, flags);
            append<std::uint32_t>(header, static_cast<std::uint32_t>(folders.size()));
            append<std::uint32_t>(header, fileCount);
            append<std::uint32_t>(header, folderNamesLength);
            append<std::uint32_t>(header, fileNamesLength);
            append<std::uint32_t>(header, 0);

            std::string folderRecords;
            std::string blocks;
            std::string names;
            std::string data;
            for (const FolderSpec& folder : folders)
            {
                append<std::uint64_t>(folderRecords, hashOf(folder.mName));
                append<std::uint32_t>(folderRecords, static_cast<std::uint32_t>(folder.mFiles.size()));
                const std::size_t nameBlock = blocksStart + blocks.size() + fileNamesLength;
                if (version == Skyrim)
                {
                    append<std::uint32_t>(folderRecords, 0);
                    append<std::uint64_t>(folderRecords, nameBlock);
                }
                else
                    append<std::uint32_t>(folderRecords, static_cast<std::uint32_t>(nameBlock));

                if (hasFolderNames)
                {
                    append<std::uint8_t>(blocks, static_cast<std::uint8_t>(folder.mName.size() + 1));
                    blocks += folder.mName;
                    blocks.push_back('\0');
                }
                for (const FileSpec& file : folder.mFiles)
                {
                    append<std::uint64_t>(blocks, fileHash(file.mName));
                    append<std::uint32_t>(blocks,
                        static_cast<std::uint32_t>(file.mBlob.size())
                            | (file.mFlipCompression ? CompressedBSAFile::FileSizeFlag_Compression : 0u));
                    append<std::uint32_t>(blocks, static_cast<std::uint32_t>(dataOffset + data.size()));
                    data += file.mBlob;
                    names += file.mName;
                    names.push_back('\0');
                }
            }
            return header + folderRecords + blocks + (hasFileNames ? names : std::string()) + data;
        }

        std::filesystem::path write(const std::string& bytes)
        {
            const auto testInfo = UnitTest::GetInstance()->current_test_info();
            const std::filesystem::path path = TestingOpenMW::outputFilePath(
                std::format("{}.{}.bsa", testInfo->test_suite_name(), testInfo->name()));
            std::ofstream(path, std::ios::binary) << bytes;
            return path;
        }

        std::string openFailure(const std::string& bytes)
        {
            CompressedBSAFile file;
            try
            {
                file.open(write(bytes));
            }
            catch (const std::exception& e)
            {
                return e.what();
            }
            return {};
        }

        std::string contentOf(CompressedBSAFile& archive, std::string_view name)
        {
            for (const BSAFile::FileStruct& file : archive.getList())
            {
                if (file.name() == name)
                {
                    const Files::IStreamPtr stream = archive.getFile(&file);
                    return std::string(std::istreambuf_iterator<char>(*stream), std::istreambuf_iterator<char>());
                }
            }
            throw std::runtime_error("The archive has no file " + std::string(name));
        }

        std::string failureOf(CompressedBSAFile& archive, std::string_view name)
        {
            for (const BSAFile::FileStruct& file : archive.getList())
            {
                if (file.name() == name)
                {
                    try
                    {
                        archive.getFile(&file);
                    }
                    catch (const std::exception& e)
                    {
                        return e.what();
                    }
                    return {};
                }
            }
            return "The archive has no file " + std::string(name);
        }

        constexpr std::uint32_t sNames
            = CompressedBSAFile::ArchiveFlag_FolderNames | CompressedBSAFile::ArchiveFlag_FileNames;
        const std::string sMesh = "Not a mesh, but " + std::string(300, 'x');
        const std::string sTexture = "A texture.";

        TEST(CompressedBSAFileArchiveTest, anOblivionArchiveGivesTheFilesItHolds)
        {
            const std::string raw = std::string(11, 'r');
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, sNames,
                { { "meshes\\actors", { { "skeleton.nif", { sMesh } }, { "idle.kf", { raw } } } },
                    { "textures", { { "face.dds", { sTexture } } } } })));

            EXPECT_EQ(archive.getList().size(), 3u);
            EXPECT_EQ(contentOf(archive, "meshes\\actors\\skeleton.nif"), sMesh);
            EXPECT_EQ(contentOf(archive, "meshes\\actors\\idle.kf"), raw);
            EXPECT_EQ(contentOf(archive, "textures\\face.dds"), sTexture);
        }

        TEST(CompressedBSAFileArchiveTest, theHashOfAFileNameDependsOnTheLengthAndTheExtension)
        {
            // Names of every length that the hash treats differently, and the extensions that it knows
            const std::vector<FileSpec> files = {
                { "a.kf", { "kf" } },
                { "ab.nif", { "nif" } },
                { "abc.dds", { "dds" } },
                { "abcd.wav", { "wav" } },
                { "abcdef.txt", { "txt" } },
                { "readme", { "no extension" } },
            };
            CompressedBSAFile archive;
            archive.open(
                write(buildArchive(Oblivion, sNames, { { "x", files }, { "", { { "root.txt", { "root" } } } } })));

            EXPECT_EQ(contentOf(archive, "x\\a.kf"), "kf");
            EXPECT_EQ(contentOf(archive, "x\\ab.nif"), "nif");
            EXPECT_EQ(contentOf(archive, "x\\abc.dds"), "dds");
            EXPECT_EQ(contentOf(archive, "x\\abcd.wav"), "wav");
            EXPECT_EQ(contentOf(archive, "x\\abcdef.txt"), "txt");
            EXPECT_EQ(contentOf(archive, "x\\readme"), "no extension");
            EXPECT_EQ(contentOf(archive, "\\root.txt"), "root");
        }

        TEST(CompressedBSAFileArchiveTest, anArchiveThatCompressesInflatesEveryFileButTheOnesThatSayOtherwise)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, sNames | CompressedBSAFile::ArchiveFlag_Compress,
                { { "meshes",
                    { { "packed.nif", { packed(sMesh, deflate(sMesh)) } }, { "stored.nif", sTexture, true } } } })));

            EXPECT_EQ(contentOf(archive, "meshes\\packed.nif"), sMesh);
            EXPECT_EQ(contentOf(archive, "meshes\\stored.nif"), sTexture);
        }

        TEST(CompressedBSAFileArchiveTest, aFileOfAnArchiveThatDoesNotCompressCanSayThatItIsCompressed)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, sNames,
                { { "meshes",
                    { { "stored.nif", { sTexture } }, { "packed.nif", packed(sMesh, deflate(sMesh)), true } } } })));

            EXPECT_EQ(contentOf(archive, "meshes\\stored.nif"), sTexture);
            EXPECT_EQ(contentOf(archive, "meshes\\packed.nif"), sMesh);
        }

        TEST(CompressedBSAFileArchiveTest, aFalloutArchiveHasTheNameOfEachFileInFrontOfItsData)
        {
            const std::uint32_t flags
                = sNames | CompressedBSAFile::ArchiveFlag_Compress | CompressedBSAFile::ArchiveFlag_EmbeddedNames;
            const std::string name = "meshes\\clutter\\bottle.nif";
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Fallout, flags,
                { { "meshes\\clutter",
                    { { "bottle.nif", { embeddedName(name) + packed(sMesh, deflate(sMesh)) } },
                        { "cap.nif", embeddedName("meshes\\clutter\\cap.nif") + sTexture, true } } } })));

            EXPECT_EQ(contentOf(archive, "meshes\\clutter\\bottle.nif"), sMesh);
            // The buffer of a stored file has room for the name in front of it as well, so it ends in zeros
            EXPECT_THAT(contentOf(archive, "meshes\\clutter\\cap.nif"), StartsWith(sTexture));
        }

        TEST(CompressedBSAFileArchiveTest, aFalloutArchiveWithEmbeddedNamesAndNoCompressionIsRead)
        {
            const std::uint32_t flags = sNames | CompressedBSAFile::ArchiveFlag_EmbeddedNames;
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Fallout, flags,
                { { "sound", { { "voice.wav", { embeddedName("sound\\voice.wav") + sTexture } } } } })));

            EXPECT_THAT(contentOf(archive, "sound\\voice.wav"), StartsWith(sTexture));
        }

        TEST(CompressedBSAFileArchiveTest, anArchiveOfOblivionIgnoresTheFlagOfEmbeddedNames)
        {
            const std::uint32_t flags = sNames | CompressedBSAFile::ArchiveFlag_EmbeddedNames;
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, flags, { { "sound", { { "voice.wav", { sTexture } } } } })));

            EXPECT_EQ(contentOf(archive, "sound\\voice.wav"), sTexture);
        }

        TEST(CompressedBSAFileArchiveTest, aSkyrimArchiveHasWiderFolderRecordsAndFilesCompressedByLz4)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Skyrim, sNames | CompressedBSAFile::ArchiveFlag_Compress,
                { { "meshes\\armor", { { "iron.nif", { packed(sMesh, lz4Frame(sMesh)) } } } },
                    { "textures\\armor", { { "iron.dds", sTexture, true } } } })));

            EXPECT_EQ(archive.getList().size(), 2u);
            EXPECT_EQ(contentOf(archive, "meshes\\armor\\iron.nif"), sMesh);
            EXPECT_EQ(contentOf(archive, "textures\\armor\\iron.dds"), sTexture);
        }

        TEST(CompressedBSAFileArchiveTest, aFileThatDoesNotInflateIsAnError)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, sNames | CompressedBSAFile::ArchiveFlag_Compress,
                { { "meshes", { { "broken.nif", { packed(sMesh, std::string(40, '\x55')) } } } } })));

            EXPECT_THAT(failureOf(archive, "meshes\\broken.nif"), HasSubstr("zlib uncompress failed"));
        }

        TEST(CompressedBSAFileArchiveTest, aFileThatLz4DoesNotInflateIsAnError)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Skyrim, sNames | CompressedBSAFile::ArchiveFlag_Compress,
                { { "meshes", { { "broken.nif", { packed(sMesh, std::string(40, '\x55')) } } } } })));

            EXPECT_THAT(failureOf(archive, "meshes\\broken.nif"), HasSubstr("LZ4 decompression error"));
        }

        TEST(CompressedBSAFileArchiveTest, aFileThatTheArchiveDoesNotHaveIsAnError)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, sNames, { { "meshes", { { "there.nif", { sTexture } } } } })));

            std::vector<char> name;
            const auto fileOf = [&](std::string_view path) {
                name.assign(path.begin(), path.end());
                BSAFile::FileStruct file;
                file.mNamesBuffer = &name;
                file.mNameSize = static_cast<std::uint32_t>(name.size());
                return file;
            };

            // A folder that it does not have, a file of a folder that it has, a name in a language that the hash cannot
            // be made for
            for (const char* path : { "textures\\there.nif", "meshes\\gone.nif", "meshes\\there.dds" })
            {
                const BSAFile::FileStruct file = fileOf(path);
                EXPECT_THAT(
                    [&] { archive.getFile(&file); }, ThrowsMessage<std::runtime_error>(HasSubstr("File not found")))
                    << path;
            }

            const BSAFile::FileStruct unicode = fileOf("meshes\\th\xC3\xA9re.nif");
            EXPECT_THAT([&] { archive.getFile(&unicode); },
                ThrowsMessage<std::runtime_error>(HasSubstr("contains unicode characters")));
        }

        TEST(CompressedBSAFileArchiveTest, anArchiveWithoutFolderNamesHasOneFolder)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, CompressedBSAFile::ArchiveFlag_FileNames,
                { { "", { { "root.txt", { "root" } }, { "other.txt", { "other" } } } } })));

            EXPECT_EQ(archive.getList().size(), 2u);
        }

        TEST(CompressedBSAFileArchiveTest, anArchiveWithoutFileNamesHasFilesWithoutNames)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, CompressedBSAFile::ArchiveFlag_FolderNames,
                { { "meshes", { { "there.nif", { sTexture } } } } })));

            ASSERT_EQ(archive.getList().size(), 1u);
            EXPECT_EQ(archive.getList()[0].mNameSize, 0u);
        }

        TEST(CompressedBSAFileArchiveTest, aFileSizeDoesNotCountTheFlagOfCompression)
        {
            CompressedBSAFile archive;
            archive.open(write(buildArchive(Oblivion, sNames | CompressedBSAFile::ArchiveFlag_Compress,
                { { "meshes", { { "stored.nif", sTexture, true } } } })));

            ASSERT_EQ(archive.getList().size(), 1u);
            EXPECT_EQ(archive.getList()[0].mFileSize, sTexture.size());
        }

        TEST(CompressedBSAFileArchiveTest, anArchiveOfAnotherFormatOrVersionIsRefused)
        {
            const std::vector<FolderSpec> folders = { { "meshes", { { "there.nif", { sTexture } } } } };

            EXPECT_THAT(openFailure(buildArchive(Oblivion, sNames, folders, 0x00000100)),
                HasSubstr("Unrecognized compressed BSA format"));
            EXPECT_THAT(
                openFailure(buildArchive(0x70, sNames, folders)), HasSubstr("Unrecognized compressed BSA version"));
            EXPECT_THAT(openFailure(std::string(20, '\0')), HasSubstr("too small"));
        }

        TEST(CompressedBSAFileArchiveTest, anArchiveThatEndsInsideItsRecordsIsRefused)
        {
            const std::string whole = buildArchive(Oblivion, sNames,
                { { "meshes", { { "there.nif", { sTexture } } } }, { "textures", { { "face.dds", { sTexture } } } } });

            // Inside the folder records
            EXPECT_THAT(
                openFailure(whole.substr(0, 36 + 16 + 4)), HasSubstr("Failed to read compressed BSA folder record"));
            // Inside the name of the first folder
            EXPECT_THAT(openFailure(whole.substr(0, 36 + 32 + 4)), HasSubstr("Folder name size mismatch"));
            // Inside the names of the files
            EXPECT_THAT(openFailure(whole.substr(0, whole.size() - sTexture.size() - 12)), HasSubstr("Failed to read"));
        }

        TEST(CompressedBSAFileArchiveTest, aFolderOffsetThatIsNotInTheFileIsRefused)
        {
            std::string bytes = buildArchive(Oblivion, sNames, { { "meshes", { { "there.nif", { sTexture } } } } });
            // The offset of the folder records
            const std::uint32_t far = 0x7FFFFFF0;
            bytes.replace(8, sizeof(far), reinterpret_cast<const char*>(&far), sizeof(far));

            EXPECT_THAT(openFailure(bytes), HasSubstr("Failed to read"));
        }

        TEST(CompressedBSAFileArchiveTest, aFolderNameThatIsShorterThanItsSizeIsRefused)
        {
            std::string bytes = buildArchive(Oblivion, sNames, { { "meshes", { { "there.nif", { sTexture } } } } });
            // The size of the name of the folder is the byte after the folder record
            const std::size_t sizeOffset = 36 + 16;
            ASSERT_EQ(static_cast<std::uint8_t>(bytes[sizeOffset]), 7);
            bytes[sizeOffset] = 12;

            EXPECT_THAT(openFailure(bytes), HasSubstr("Folder name size mismatch"));
        }

        TEST(CompressedBSAFileArchiveTest, folderNamesThatAreLongerThanTheHeaderSaysAreRefused)
        {
            std::string bytes = buildArchive(Oblivion, sNames, { { "meshes", { { "there.nif", { sTexture } } } } });
            // The length of all the names of the folders, which is 7
            const std::uint32_t length = 3;
            bytes.replace(24, sizeof(length), reinterpret_cast<const char*>(&length), sizeof(length));

            EXPECT_THAT(openFailure(bytes), HasSubstr("Failed to read folder names"));
        }

        TEST(CompressedBSAFileArchiveTest, fileNamesThatAreLongerThanTheHeaderSaysAreRefused)
        {
            std::string bytes = buildArchive(Oblivion, sNames, { { "meshes", { { "there.nif", { sTexture } } } } });
            // The length of all the names of the files, which is 10
            const std::uint32_t length = 4;
            bytes.replace(28, sizeof(length), reinterpret_cast<const char*>(&length), sizeof(length));

            EXPECT_THAT(openFailure(bytes), HasSubstr("Failed to read file names"));
        }

        TEST(CompressedBSAFileArchiveTest, aFileNameThatDoesNotEndIsRefused)
        {
            const std::string longName(300, 'n');
            EXPECT_THAT(openFailure(buildArchive(Oblivion, sNames, { { "meshes", { { longName, { sTexture } } } } })),
                HasSubstr("filename is too long"));
        }
    }
}
