#ifndef OPENFALLOUT_COMPONENTS_ESM4_RECORDREADER_H
#define OPENFALLOUT_COMPONENTS_ESM4_RECORDREADER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <components/esm/common.hpp>
#include <components/esm/formid.hpp>

#include "reader.hpp"

namespace ESM4
{
    // Walks the sub-records of the current record for a loader, with the checks every loader of this kind needs: a
    // sub-record that runs past its record, a size that no known version of the record has, a file that ends inside
    // a field and bytes of the record that no sub-record accounts for all throw, with the record's name in the
    // message. A loader reads each sub-record with exactly one of the read functions, and ends with finish():
    //
    //     RecordReader in(reader, "WTHR");
    //     while (in.next())
    //     {
    //         switch (in.type())
    //         {
    //             case ESM::fourCC("EDID"): in.string(mEditorId); break;
    //             case ESM::fourCC("DATA"): in.value(mData); break;
    //             default: in.unknown();
    //         }
    //     }
    //     in.finish();
    //
    // The loader has called Reader::getRecordData() before, as all of them do.
    class RecordReader
    {
    public:
        // `name` is the four character record code, for the messages.
        RecordReader(Reader& reader, std::string_view name)
            : mReader(reader)
            , mName(name)
        {
        }

        Reader& reader() { return mReader; }

        // Reads the next sub-record header. Returns false when the record has no more sub-records. A sub-record whose
        // data would end after its record throws.
        bool next();

        std::uint32_t type() const { return mReader.subRecordHeader().typeId; }
        std::uint32_t size() const { return mReader.subRecordHeader().dataSize; }

        // A zero-terminated string. A sub-record without bytes is an empty string, the reader cannot read one.
        void string(std::string& value);

        // The object representation of a trivially copyable value, which must be what the whole sub-record holds.
        template <class T>
        void value(T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            bytes(&value, sizeof(T));
        }

        // A form ID, adjusted to the load order the way Reader::getFormId does.
        void formId(ESM::FormId& value);

        // Exactly `count` bytes, which must be what the whole sub-record holds.
        void bytes(void* data, std::size_t count);

        // The whole sub-record, whatever its size.
        void bytes(std::vector<std::uint8_t>& data);

        // Fails when the sub-record does not hold exactly the size.
        void expectSize(std::uint32_t size) const;

        // Fails: the sub-record has a size that the loader does not know.
        [[noreturn]] void badSize() const;

        // Fails: the loader does not know the sub-record.
        [[noreturn]] void unknown() const;

        // Fails when bytes of the record are left that no sub-record accounts for, which Reader::getSubRecordHeader
        // also reports as the end of the record, like too few bytes for another header or a file that ends in one.
        void finish() const;

        [[noreturn]] void fail(std::string_view message) const;

    private:
        Reader& mReader;
        std::string mName;
    };
}

#endif
