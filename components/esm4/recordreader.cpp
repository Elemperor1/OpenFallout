#include "recordreader.hpp"

#include <stdexcept>

namespace ESM4
{
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

    void RecordReader::formId(ESM::FormId& value)
    {
        expectSize(sizeof(ESM::FormId32));
        if (!mReader.getFormId(value))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::bytes(void* data, std::size_t count)
    {
        expectSize(static_cast<std::uint32_t>(count));
        if (!mReader.get(data, count))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::bytes(std::vector<std::uint8_t>& data)
    {
        data.resize(size());
        if (!data.empty() && !mReader.get(data.data(), data.size()))
            fail("sub-record is shorter than its size");
    }

    void RecordReader::expectSize(std::uint32_t expected) const
    {
        if (size() != expected)
            badSize();
    }

    void RecordReader::badSize() const
    {
        fail(ESM::printName(type()) + " has an unexpected size");
    }

    void RecordReader::unknown() const
    {
        throw std::runtime_error("ESM4::" + mName + "::load - Unknown subrecord " + ESM::printName(type()));
    }

    void RecordReader::finish() const
    {
        if (mReader.unreadRecordBytes() != 0)
            fail("record has unread bytes after its last sub-record");
    }
}
