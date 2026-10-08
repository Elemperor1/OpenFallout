#ifndef OPENFALLOUT_COMPONENTS_ESM4_CENSUSREADING_H
#define OPENFALLOUT_COMPONENTS_ESM4_CENSUSREADING_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "reader.hpp"

namespace ESM4::CensusReading
{
    // The first line of an error message, which holds offsets and sizes but no record contents.
    inline std::string firstLine(std::string_view message)
    {
        return std::string(message.substr(0, message.find('\n')));
    }

    // Calls visit with the type and the data size of each sub-record of the current record and reads the rest of the
    // sub-record when visit returns false. A sub-record that runs past the end of the record and a record that ends
    // inside a sub-record header are errors.
    template <class Visit>
    void forEachSubRecord(Reader& reader, Visit&& visit)
    {
        while (true)
        {
            const bool found = reader.getSubRecordHeader();
            if (!reader.subRecordFitsRecord())
                throw std::runtime_error("A sub-record runs past the end of its record");
            if (!found)
            {
                // The same answer comes at the end of the record and when a few bytes of a header are left.
                if (reader.unreadRecordBytes() != 0)
                    throw std::runtime_error("A record ends inside a sub-record header");
                return;
            }
            if (!visit(reader.subRecordHeader().typeId, reader.subRecordHeader().dataSize))
                reader.skipSubRecordData();
        }
    }

    template <class T>
    void readValue(Reader& reader, T& value)
    {
        if (!reader.getExact(value))
            throw std::runtime_error("The file ends inside a sub-record");
    }

    // The data of the current sub-record, size bytes of it, which may be less than the object has.
    template <std::size_t N>
    void readBytes(Reader& reader, std::array<char, N>& data, std::size_t size)
    {
        if (size > N || !reader.get(data.data(), size))
            throw std::runtime_error("The file ends inside a sub-record");
    }
}

#endif
