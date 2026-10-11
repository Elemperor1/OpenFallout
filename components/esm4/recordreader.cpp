#include "recordreader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "conditionparams.hpp"

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
        const bool found = mReader.getSubRecordHeader();
        if (!mReader.skippedExtendedSubRecords().empty())
            fail("sub-record with an extended size, which the reader does not read");
        if (!found)
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

    void RecordReader::adjustReference(ESM::FormId32& id) const
    {
        // Zero is the null reference. Adjusting it would give it the index of the file that holds it.
        if (id != 0)
            mReader.adjustFormId(id);
    }

    void RecordReader::formId(ESM::FormId& value)
    {
        expectSize(sizeof(ESM::FormId32));
        ESM::FormId32 id = 0;
        readExact(&id, sizeof(id));
        adjustReference(id);
        value = ESM::FormId::fromUint32(id);
    }

    void RecordReader::formIds(std::vector<ESM::FormId>& values)
    {
        if (size() % sizeof(ESM::FormId32) != 0)
            badSize();
        for (std::uint32_t count = size() / sizeof(ESM::FormId32); count > 0; --count)
        {
            ESM::FormId32 id = 0;
            readExact(&id, sizeof(id));
            adjustReference(id);
            values.push_back(ESM::FormId::fromUint32(id));
        }
    }

    void RecordReader::alternateTextures(std::vector<AlternateTexture>& values)
    {
        std::vector<std::uint8_t> data;
        bytes(data);

        std::size_t position = 0;
        const auto read = [&](void* out, std::size_t count) {
            if (data.size() - position < count)
                badSize();
            std::memcpy(out, data.data() + position, count);
            position += count;
        };

        std::uint32_t count = 0;
        read(&count, sizeof(count));
        // Each entry has at least the length of its name, the texture and the index.
        constexpr std::size_t minimumEntrySize = 3 * sizeof(std::uint32_t);
        if (count > (data.size() - position) / minimumEntrySize)
            badSize();
        for (; count > 0; --count)
        {
            AlternateTexture& value = values.emplace_back();
            std::uint32_t nameLength = 0;
            read(&nameLength, sizeof(nameLength));
            if (data.size() - position < nameLength)
                badSize();
            value.mName.assign(reinterpret_cast<const char*>(data.data()) + position, nameLength);
            position += nameLength;
            ESM::FormId32 texture = 0;
            read(&texture, sizeof(texture));
            adjustReference(texture);
            value.mTexture = ESM::FormId::fromUint32(texture);
            read(&value.mIndex, sizeof(value.mIndex));
        }
        if (position != data.size())
            badSize();
    }

    void RecordReader::condition(TargetCondition& value)
    {
        // The run on and the reference are optional in the format reference.
        constexpr std::size_t withoutRunOn = offsetof(TargetCondition, runOn);
        constexpr std::size_t withoutReference = offsetof(TargetCondition, reference);
        if (size() == withoutRunOn || size() == withoutReference)
        {
            value = {};
            readExact(&value, size());
        }
        else
            this->value(value, &TargetCondition::reference);
        // The older way to say that a condition is about the target, which the run on replaced. The format reference's
        // tool turns it into the run on, whatever the size, and so does this.
        if ((value.condition & CTF_RunOnTarget) != 0)
        {
            value.condition &= ~static_cast<std::uint32_t>(CTF_RunOnTarget);
            value.runOn = 1;
        }
        adjustComparison(value);
        adjustConditionParameters(mReader, value);
    }

    void RecordReader::adjustComparison(TargetCondition& value) const
    {
        adjustConditionComparison(mReader, value);
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

    void RecordReader::bytesBetween(std::vector<std::uint8_t>& data, std::uint32_t minimum, std::uint32_t maximum,
        std::uint32_t step, std::initializer_list<std::uint32_t> others)
    {
        const bool onBoundary = size() >= minimum && size() <= maximum && (size() - minimum) % step == 0;
        if (!onBoundary && std::find(others.begin(), others.end(), size()) == others.end())
            badSize();
        bytes(data);
    }

    void RecordReader::adjustFormIds(
        std::uint8_t* data, std::size_t size, std::initializer_list<std::size_t> offsets) const
    {
        for (const std::size_t offset : offsets)
        {
            if (size < sizeof(ESM::FormId32) || offset > size - sizeof(ESM::FormId32))
                continue;
            ESM::FormId32 id = 0;
            std::memcpy(&id, data + offset, sizeof(id));
            adjustReference(id);
            std::memcpy(data + offset, &id, sizeof(id));
        }
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
