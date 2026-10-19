#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVESTATE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVESTATE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "script.hpp"

namespace FalloutScript
{
    /// The version of the part of a saved game that holds the state of Fallout (quests, scripts, characters ...), which
    /// every such save carries. It counts the layout of the part as a whole: the records it is made of and how a record
    /// is read. Each section has a version of its own too, which is raised when the layout of that section changes.
    /// A save of a newer version than this is not read.
    constexpr std::uint32_t saveFormatVersion = 1;

    /// A save that can not be read: it is cut short, or it holds a size that does not fit the rest of it
    class SaveError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /// A save that a newer version of the engine made
    class SaveTooNewError : public SaveError
    {
    public:
        SaveTooNewError(std::string_view what, std::uint32_t version)
            : SaveError(std::string(what) + " has version " + std::to_string(version) + ", which is newer than this engine reads")
            , mVersion(version)
        {
        }

        std::uint32_t version() const { return mVersion; }

    private:
        std::uint32_t mVersion;
    };

    /// Writes the state of a part of the game as bytes. Numbers are little endian whatever the machine is, and a state
    /// is written in a fixed order (maps in the order of their keys), so that the same state is always the same bytes.
    class SaveWriter
    {
    public:
        void boolean(bool value) { u8(value ? 1 : 0); }
        void u8(std::uint8_t value) { mBytes.push_back(static_cast<char>(value)); }
        void u16(std::uint16_t value);
        void u32(std::uint32_t value);
        void u64(std::uint64_t value);
        void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }
        void i64(std::int64_t value) { u64(static_cast<std::uint64_t>(value)); }
        void f32(float value);
        void f64(double value);
        /// A text, or any bytes, with the number of them in front
        void string(std::string_view value);
        /// A form id, as the load order of the game that saves it has it
        void form(FormId value) { u32(value); }
        /// The number of the entries of a list that follows
        void count(std::size_t size);

        const std::string& bytes() const { return mBytes; }
        std::string take() { return std::move(mBytes); }

    private:
        std::string mBytes;
    };

    /// Reads what a SaveWriter wrote. Every read is checked against the bytes that are left: a save that is cut short or
    /// that names more entries than it has room for throws SaveError.
    class SaveReader
    {
    public:
        /// How a form id of the save is told in the load order of this game: the plugin it names may be at another
        /// place now, or no longer be loaded (none). Without one the ids are what they were saved as.
        using FormMapping = std::function<std::optional<FormId>(FormId)>;

        explicit SaveReader(std::string_view bytes, FormMapping mapping = {})
            : mBytes(bytes)
            , mMapping(std::move(mapping))
        {
        }

        bool boolean() { return u8() != 0; }
        std::uint8_t u8();
        std::uint16_t u16();
        std::uint32_t u32();
        std::uint64_t u64();
        std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
        std::int64_t i64() { return static_cast<std::int64_t>(u64()); }
        float f32();
        double f64();
        std::string string();
        /// A form id in the load order of this game. None when the plugin of the form is no longer loaded: whatever the
        /// state says about such a form is dropped. The null form (0) and the forms that were made while playing (the
        /// plugin number 0xFF) are what they were saved as.
        std::optional<FormId> form();
        /// The number of the entries of a list that follows, each of which takes at least minimumBytes bytes of the
        /// save: a number that the rest of the save has no room for is an error, not an allocation
        std::size_t count(std::size_t minimumBytes = 1);

        std::size_t remaining() const { return mBytes.size() - mOffset; }
        bool atEnd() const { return mOffset == mBytes.size(); }

    private:
        const char* take(std::size_t size);

        std::string_view mBytes;
        std::size_t mOffset = 0;
        FormMapping mMapping;
    };

    /// A part of the state of the game that a save holds: its name, the version of its layout, and how it is written and
    /// read
    struct SaveSection
    {
        std::string mName;
        std::uint32_t mVersion = 1;
        std::function<void(SaveWriter&)> mWrite;
        /// Reads the section that was saved with the version given (not newer than mVersion)
        std::function<void(SaveReader&, std::uint32_t version)> mRead;
    };
}

#endif
