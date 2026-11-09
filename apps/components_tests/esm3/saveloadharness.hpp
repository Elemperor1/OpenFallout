#ifndef OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM3_SAVELOADHARNESS_H
#define OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM3_SAVELOADHARNESS_H

#include <components/esm/fourcc.hpp>
#include <components/esm/refid.hpp>
#include <components/esm3/cellref.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/formatversion.hpp>
#include <components/esm3/loadland.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// Saves a record with the writer and loads it back with the reader, for every format version of the content files and
// of the saved games, so that the readers of the records can be tested without any game data.
namespace ESM::SaveLoadTest
{
    using namespace ::testing;

    inline std::vector<ESM::FormatVersion> getFormats()
    {
        std::vector<ESM::FormatVersion> result({
            CurrentContentFormatVersion,
            MaxLimitedSizeStringsFormatVersion,
            MaxStringRefIdFormatVersion,
        });
        for (ESM::FormatVersion v = result.back() + 1; v <= ESM::CurrentSaveGameFormatVersion; ++v)
            result.push_back(v);
        return result;
    }

    constexpr std::uint32_t fakeRecordId = fourCC("FAKE");

    template <class T>
    concept HasSave = requires(T v, ESMWriter& w)
    {
        v.save(w);
    };

    template <class T>
    concept NotHasSave = !HasSave<T>;

    template <HasSave T>
    auto save(const T& record, ESMWriter& writer)
    {
        record.save(writer);
    }

    inline void save(const CellRef& record, ESMWriter& writer)
    {
        record.save(writer, true);
    }

    template <NotHasSave T>
    auto save(const T& record, ESMWriter& writer)
    {
        writer.writeComposite(record);
    }

    template <typename T>
    std::unique_ptr<std::istream> makeEsmStream(const T& record, FormatVersion formatVersion)
    {
        ESMWriter writer;
        auto stream = std::make_unique<std::stringstream>();
        writer.setFormatVersion(formatVersion);
        writer.save(*stream);
        writer.startRecord(fakeRecordId);
        save(record, writer);
        writer.endRecord(fakeRecordId);
        return stream;
    }

    template <class T>
    concept HasLoad = requires(T v, ESMReader& r)
    {
        v.load(r);
    };

    template <class T>
    concept HasLoadWithDelete = requires(T v, ESMReader& r, bool& d)
    {
        v.load(r, d);
    };

    template <class T>
    concept NotHasLoad = !HasLoad<T> && !HasLoadWithDelete<T>;

    template <HasLoad T>
    void load(ESMReader& reader, T& record)
    {
        record.load(reader);
    }

    template <HasLoadWithDelete T>
    void load(ESMReader& reader, T& record)
    {
        bool deleted = false;
        record.load(reader, deleted);
    }

    inline void load(ESMReader& reader, CellRef& record)
    {
        bool deleted = false;
        record.load(reader, deleted, true);
    }

    template <NotHasLoad T>
    void load(ESMReader& reader, T& record)
    {
        reader.getComposite(record);
    }

    inline void load(ESMReader& reader, Land& record)
    {
        bool deleted = false;
        record.load(reader, deleted);
        if (deleted)
            return;
        record.mLandData = std::make_unique<LandRecordData>();
        reader.restoreContext(record.mContext);
        loadLandRecordData(record.mDataTypes, reader, *record.mLandData);
    }

    template <typename T>
    void saveAndLoadRecord(const T& record, FormatVersion formatVersion, T& result)
    {
        ESMReader reader;
        reader.open(makeEsmStream(record, formatVersion), "stream");
        ASSERT_TRUE(reader.hasMoreRecs());
        ASSERT_EQ(reader.getRecName().toInt(), fakeRecordId);
        reader.getRecHeader();
        load(reader, result);
    }

    struct Esm3SaveLoadRecordTest : public TestWithParam<FormatVersion>
    {
        std::minstd_rand mRandom;
        std::uniform_int_distribution<short> mRefIdDistribution{ 'a', 'z' };

        std::string generateRandomString(std::size_t size)
        {
            std::string value;
            while (value.size() < size)
                value.push_back(static_cast<char>(mRefIdDistribution(mRandom)));
            return value;
        }

        RefId generateRandomRefId(std::size_t size = 33) { return RefId::stringRefId(generateRandomString(size)); }

        template <class T, std::size_t n>
        void generateArray(T (&dst)[n])
        {
            for (auto& v : dst)
                v = std::uniform_real_distribution<float>{ -1.0f, 1.0f }(mRandom);
        }

        void generateBytes(auto iterator, std::size_t count)
        {
            std::uniform_int_distribution<unsigned short> distribution{ 0, std::numeric_limits<unsigned char>::max() };
            std::generate_n(iterator, count, [&] { return static_cast<unsigned char>(distribution(mRandom)); });
        }

        void generateStrings(auto iterator, std::size_t count)
        {
            std::uniform_int_distribution<std::size_t> distribution{ 1, 13 };
            std::generate_n(iterator, count, [&] { return generateRandomString(distribution(mRandom)); });
        }
    };

    // Write a record with the function, in a file of the format version, and read it with the other function.
    template <class Write, class Read>
    inline void roundTripWith(FormatVersion format, Write&& write, Read&& read)
    {
        ESMWriter writer;
        auto stream = std::make_unique<std::stringstream>();
        writer.setFormatVersion(format);
        writer.save(*stream);
        writer.startRecord(fakeRecordId);
        write(writer);
        writer.endRecord(fakeRecordId);

        ESMReader reader;
        reader.open(std::move(stream), "stream");
        ASSERT_TRUE(reader.hasMoreRecs());
        reader.getRecName();
        reader.getRecHeader();
        read(reader);
    }
}

#endif
