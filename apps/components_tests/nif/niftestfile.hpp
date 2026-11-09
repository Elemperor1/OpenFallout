#ifndef OPENFALLOUT_APPS_COMPONENTS_TESTS_NIF_NIFTESTFILE_H
#define OPENFALLOUT_APPS_COMPONENTS_TESTS_NIF_NIFTESTFILE_H

#include <components/nif/niffile.hpp>
#include <components/nif/nifstream.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Builds the bytes of small NIF files of the versions of the games, so that the readers of the records can be tested
// without any game data.
namespace NifTest
{
    /// The versions that a file has: of the format, of the user and of the Bethesda stream.
    struct Profile
    {
        std::uint32_t mVersion;
        std::uint32_t mUserVersion;
        std::uint32_t mBethVersion;

        /// Whether the strings of the records are indices of a table of the file, as from 20.1.0.1 on.
        bool hasStringTable() const { return mVersion >= Nif::NIFStream::generateVersion(20, 1, 0, 1); }
        /// Whether the file names its record types once in a list, as from 5.0.0.1 on.
        bool hasTypeList() const { return mVersion >= Nif::NIFStream::generateVersion(5, 0, 0, 1); }
    };

    constexpr Profile morrowind{ 0x04000002, 0, 0 };
    constexpr Profile oblivion{ 0x14000005, 11, 11 };
    constexpr Profile fallout3{ 0x14020007, 11, 34 };
    constexpr Profile skyrim{ 0x14020007, 12, 83 };
    constexpr Profile fallout4{ 0x14020007, 12, 130 };

    /// The bytes of the body of a record, written by little-endian value.
    class Bytes
    {
    public:
        Bytes() = default;

        Bytes& u8(std::uint8_t value) { return put(value); }
        Bytes& u16(std::uint16_t value) { return put(value); }
        Bytes& u32(std::uint32_t value) { return put(value); }
        Bytes& i32(std::int32_t value) { return put(value); }
        Bytes& f32(float value) { return put(value); }
        Bytes& boolean(bool value) { return put<std::uint8_t>(value ? 1 : 0); }

        /// A reference to a record by its index in the file; -1 is none.
        Bytes& ref(std::int32_t index) { return i32(index); }

        /// A count and then references.
        Bytes& refs(const std::vector<std::int32_t>& indices)
        {
            u32(static_cast<std::uint32_t>(indices.size()));
            for (const std::int32_t index : indices)
                ref(index);
            return *this;
        }

        Bytes& vec2(float x, float y) { return f32(x).f32(y); }
        Bytes& vec3(float x, float y, float z) { return f32(x).f32(y).f32(z); }
        Bytes& vec4(float x, float y, float z, float w) { return f32(x).f32(y).f32(z).f32(w); }
        /// A rotation as the four numbers of a quaternion: w, x, y, z.
        Bytes& quat(float w, float x, float y, float z) { return f32(w).f32(x).f32(y).f32(z); }

        Bytes& identity3()
        {
            return f32(1).f32(0).f32(0).f32(0).f32(1).f32(0).f32(0).f32(0).f32(1);
        }

        /// A transform of a node: the rotation matrix, the translation and the scale.
        Bytes& transform(float x = 0, float y = 0, float z = 0, float scale = 1)
        {
            identity3();
            return vec3(x, y, z).f32(scale);
        }

        Bytes& zeros(std::size_t count)
        {
            mData.append(count, '\0');
            return *this;
        }

        Bytes& raw(std::string_view data)
        {
            mData.append(data);
            return *this;
        }

        /// A string with a 32-bit length before it, as the files before 20.1.0.1 write their strings.
        Bytes& sized(std::string_view text)
        {
            u32(static_cast<std::uint32_t>(text.size()));
            return raw(text);
        }

        const std::string& str() const { return mData; }
        std::size_t size() const { return mData.size(); }

    private:
        template <class T>
        Bytes& put(T value)
        {
            mData.append(reinterpret_cast<const char*>(&value), sizeof(value));
            return *this;
        }

        std::string mData;
    };

    /// Builds a file of the given profile out of records, each with its type name and the bytes of its body.
    class Builder
    {
    public:
        explicit Builder(Profile profile)
            : mProfile(profile)
        {
        }

        const Profile& profile() const { return mProfile; }

        /// A string of a record: the index of the string table, which gets the text, or the text with its length.
        Bytes& string(Bytes& bytes, std::string_view text)
        {
            if (!mProfile.hasStringTable())
                return bytes.sized(text);
            if (text.empty())
                return bytes.u32(0xFFFFFFFF);
            for (std::size_t i = 0; i < mStrings.size(); ++i)
                if (mStrings[i] == text)
                    return bytes.u32(static_cast<std::uint32_t>(i));
            mStrings.emplace_back(text);
            return bytes.u32(static_cast<std::uint32_t>(mStrings.size() - 1));
        }

        /// The same for bytes that are not named, so that the call can be chained.
        Bytes string(Bytes&& bytes, std::string_view text)
        {
            string(bytes, text);
            return std::move(bytes);
        }

        /// Add a record and return its index.
        std::int32_t add(std::string_view type, const Bytes& body)
        {
            mBlocks.emplace_back(std::string(type), body.str());
            return static_cast<std::int32_t>(mBlocks.size() - 1);
        }

        void addRoot(std::int32_t index) { mRoots.push_back(index); }

        /// The bytes of the whole file.
        std::string build() const
        {
            Bytes out;
            const std::uint32_t version = mProfile.mVersion;
            using Nif::NIFStream;
            out.raw(mProfile.mVersion >= NIFStream::generateVersion(10, 0, 0, 0)
                    ? "Gamebryo File Format, Version "
                    : "NetImmerse File Format, Version ");
            out.raw(versionText(version)).raw("\n");
            out.u32(version);
            if (version >= NIFStream::generateVersion(20, 0, 0, 4))
                out.u8(1); // little endian
            if (version >= NIFStream::generateVersion(10, 0, 1, 8))
                out.u32(mProfile.mUserVersion);
            out.u32(static_cast<std::uint32_t>(mBlocks.size()));

            if (hasBethesdaStreamHeader())
            {
                out.u32(mProfile.mBethVersion);
                exportString(out, "author");
                if (mProfile.mBethVersion >= 131)
                    out.u32(0);
                else
                    exportString(out, "process_script");
                exportString(out, "export_script");
                if (mProfile.mBethVersion >= 103)
                    exportString(out, "max_file_path");
            }

            std::vector<std::string> types;
            std::vector<std::uint16_t> typeIndices;
            for (const auto& [type, body] : mBlocks)
            {
                std::size_t index = 0;
                while (index < types.size() && types[index] != type)
                    ++index;
                if (index == types.size())
                    types.push_back(type);
                typeIndices.push_back(static_cast<std::uint16_t>(index));
            }
            if (mProfile.hasTypeList())
            {
                out.u16(static_cast<std::uint16_t>(types.size()));
                for (const std::string& type : types)
                    out.sized(type);
                for (const std::uint16_t index : typeIndices)
                    out.u16(index);
            }
            if (version >= NIFStream::generateVersion(20, 2, 0, 5))
                for (const auto& [type, body] : mBlocks)
                    out.u32(static_cast<std::uint32_t>(body.size()));
            if (mProfile.hasStringTable())
            {
                std::size_t longest = 0;
                for (const std::string& text : mStrings)
                    longest = std::max(longest, text.size());
                out.u32(static_cast<std::uint32_t>(mStrings.size())).u32(static_cast<std::uint32_t>(longest));
                for (const std::string& text : mStrings)
                    out.sized(text);
            }
            if (version >= NIFStream::generateVersion(5, 0, 0, 6))
                out.u32(0); // no groups

            for (const auto& [type, body] : mBlocks)
            {
                if (!mProfile.hasTypeList())
                    out.sized(type);
                out.raw(body);
            }

            out.u32(static_cast<std::uint32_t>(mRoots.size()));
            for (const std::int32_t root : mRoots)
                out.i32(root);
            return out.str();
        }

    private:
        static std::string versionText(std::uint32_t version)
        {
            return std::to_string(version >> 24) + "." + std::to_string((version >> 16) & 0xFF) + "."
                + std::to_string((version >> 8) & 0xFF) + "." + std::to_string(version & 0xFF);
        }

        static void exportString(Bytes& out, std::string_view text)
        {
            out.u8(static_cast<std::uint8_t>(text.size()));
            out.raw(text);
        }

        bool hasBethesdaStreamHeader() const
        {
            using Nif::NIFFile;
            using Nif::NIFStream;
            if (mProfile.mVersion == NIFFile::VER_OB_OLD)
                return true;
            if (mProfile.mUserVersion < 3 || mProfile.mVersion < NIFStream::generateVersion(10, 1, 0, 0))
                return false;
            if (mProfile.mVersion <= NIFFile::VER_OB || mProfile.mVersion == NIFFile::VER_BGS)
                return mProfile.mUserVersion <= 11 || mProfile.mVersion >= NIFFile::VER_OB;
            return false;
        }

        Profile mProfile;
        std::vector<std::pair<std::string, std::string>> mBlocks;
        std::vector<std::string> mStrings;
        std::vector<std::int32_t> mRoots;
    };

    /// A parsed file. Parse errors propagate to the caller.
    class Parsed
    {
    public:
        explicit Parsed(const std::string& bytes)
            : mFile(VFS::Path::NormalizedView("test.nif"))
        {
            Nif::Reader reader(mFile, nullptr);
            auto input = std::make_unique<std::istringstream>(bytes);
            input->exceptions(std::ios::failbit | std::ios_base::badbit);
            reader.parse(std::move(input));
        }

        std::size_t size() const { return mFile.mRecords.size(); }

        const Nif::Record* record(std::size_t index) const { return mFile.mRecords.at(index).get(); }

        /// The record as the type, or null if it is not one.
        template <class T>
        const T* as(std::size_t index) const
        {
            return dynamic_cast<const T*>(record(index));
        }

        const std::vector<Nif::Record*>& roots() const { return mFile.mRoots; }
        const Nif::NIFFile& file() const { return mFile; }

    private:
        Nif::NIFFile mFile;
    };
}

#endif
