#ifndef OPENFALLOUT_COMPONENTS_ESM4_RECORDREADER_H
#define OPENFALLOUT_COMPONENTS_ESM4_RECORDREADER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <components/esm/common.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "reader.hpp"
#include "script.hpp"

namespace ESM4
{
    // A sub-record that a loader keeps as it is, with the code that names it.
    struct RawSubRecord
    {
        std::uint32_t mType = 0;
        std::vector<std::uint8_t> mData;
    };

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

        // A zero-terminated path, which is a string that is also kept normalized.
        void path(ESM::Path& value);

        // The object representation of a trivially copyable value, which must be what the whole sub-record holds. The
        // members named after it are form IDs in the value, which are adjusted to the load order the way
        // Reader::getFormId does:
        //
        //     in.value(mData, &Data::mOwner, &Data::mLight);
        template <class T, class... FormIds>
        void value(T& value, FormIds... formIds)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            bytes(&value, sizeof(T));
            (adjust(value, formIds), ...);
        }

        // A sub-record that holds a whole number of T, which are added to `values`. The size must be a multiple of
        // sizeof(T), no more is asked. The form IDs are the same as for value().
        template <class T, class... FormIds>
        void values(std::vector<T>& values, FormIds... formIds)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if (size() % sizeof(T) != 0)
                badSize();
            for (std::uint32_t count = size() / sizeof(T); count > 0; --count)
            {
                T& value = values.emplace_back();
                readExact(&value, sizeof(T));
                (adjust(value, formIds), ...);
            }
        }

        // A form ID, adjusted to the load order the way Reader::getFormId does, except that a null reference (zero)
        // stays null.
        void formId(ESM::FormId& value);

        // A sub-record that holds a whole number of form IDs, which are added to `values`.
        void formIds(std::vector<ESM::FormId>& values);

        // A CTDA of Fallout 3 and New Vegas: its reference, and its comparison value when that is a global variable,
        // are adjusted to the load order. The parameters are not, because which of them are form IDs depends on the
        // function.
        void condition(TargetCondition& value);

        // The same, and also the older form of 20 bytes, which has no run on and no reference. Those are left at zero,
        // which is the subject and none.
        void conditionOrOlder(TargetCondition& value);

        // Adjusts the form ID that a member of a packed struct holds, unless it is null. A reference cannot bind to
        // such a member.
        template <class T>
        void adjust(T& object, ESM::FormId32 T::*member) const
        {
            ESM::FormId32 id = object.*member;
            adjustReference(id);
            object.*member = id;
        }

        // Exactly `count` bytes, which must be what the whole sub-record holds.
        void bytes(void* data, std::size_t count);

        // The whole sub-record, whatever its size.
        void bytes(std::vector<std::uint8_t>& data);

        // The whole sub-record, which must have one of the sizes.
        void bytes(std::vector<std::uint8_t>& data, std::initializer_list<std::uint32_t> sizes);

        // The whole sub-record and its code, for sub-records that the loader lists without knowing their names.
        void raw(RawSubRecord& value);

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
        void readExact(void* data, std::size_t count);
        void adjustReference(ESM::FormId32& id) const;
        void adjustComparison(TargetCondition& value) const;

        Reader& mReader;
        std::string mName;
    };
}

#endif
