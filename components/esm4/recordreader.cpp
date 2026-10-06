#include "recordreader.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ESM4
{
    namespace
    {
        // The code of a sub-record as text, with an escape for the bytes that are not printable. Some codes start
        // with a byte below a space, and a message cannot hold a zero.
        std::string describe(std::uint32_t type)
        {
            std::string result;
            for (const char c : ESM::printName(type))
            {
                if (c >= 0x20 && c < 0x7f && c != '\\')
                    result += c;
                else
                {
                    constexpr char digits[] = "0123456789abcdef";
                    result += "\\x";
                    result += digits[(static_cast<unsigned char>(c) >> 4) & 0xf];
                    result += digits[static_cast<unsigned char>(c) & 0xf];
                }
            }
            return result;
        }
    }

    void RecordReader::fail(std::string_view message) const
    {
        throw std::runtime_error("ESM4::" + mName + "::load - " + std::string(message));
    }

    bool RecordReader::next()
    {
        if (!mReader.getSubRecordHeader())
            return false;
        if (!mReader.subRecordFitsRecord())
            fail("sub-record is longer than its record");
        return true;
    }

    void RecordReader::string(std::string& value)
    {
        if (size() == 0)
            value.clear();
        else if (!mReader.getZString(value))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::path(ESM::Path& value)
    {
        std::string text;
        string(text);
        value = std::move(text);
    }

    void RecordReader::formId(ESM::FormId& value)
    {
        expectSize(sizeof(ESM::FormId32));
        if (!mReader.getFormId(value))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::formIds(std::vector<ESM::FormId>& values)
    {
        if (size() % sizeof(ESM::FormId32) != 0)
            badSize();
        for (std::uint32_t count = size() / sizeof(ESM::FormId32); count > 0; --count)
        {
            if (!mReader.getFormId(values.emplace_back()))
                fail("sub-record is shorter than its size");
        }
    }

    void RecordReader::condition(TargetCondition& value)
    {
        this->value(value, &TargetCondition::reference);
        if ((value.condition & CTF_UseGlobal) != 0)
        {
            ESM::FormId32 global;
            static_assert(sizeof(global) == sizeof(value.comparison));
            std::memcpy(&global, &value.comparison, sizeof(global));
            mReader.adjustFormId(global);
            std::memcpy(&value.comparison, &global, sizeof(global));
        }
    }

    void RecordReader::bytes(void* data, std::size_t count)
    {
        expectSize(static_cast<std::uint32_t>(count));
        readExact(data, count);
    }

    void RecordReader::readExact(void* data, std::size_t count)
    {
        if (!mReader.get(data, count))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::bytes(std::vector<std::uint8_t>& data)
    {
        data.resize(size());
        if (!data.empty() && !mReader.get(data.data(), data.size()))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::bytes(std::vector<std::uint8_t>& data, std::initializer_list<std::uint32_t> sizes)
    {
        if (std::find(sizes.begin(), sizes.end(), size()) == sizes.end())
            badSize();
        bytes(data);
    }

    void RecordReader::raw(RawSubRecord& value)
    {
        value.mType = type();
        bytes(value.mData);
    }

    void RecordReader::expectSize(std::uint32_t expected) const
    {
        if (size() != expected)
            badSize();
    }

    void RecordReader::badSize() const
    {
        fail(describe(type()) + " has an unexpected size");
    }

    void RecordReader::unknown() const
    {
        throw std::runtime_error("ESM4::" + mName + "::load - Unknown subrecord " + describe(type()));
    }

    void RecordReader::finish() const
    {
        if (mReader.unreadRecordBytes() != 0)
            fail("record has unread bytes after its last sub-record");
    }
}
