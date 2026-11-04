#include "facecontrolfile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace FalloutScript
{
    namespace
    {
        constexpr std::string_view magic = "FRCTL001";
        constexpr std::array<std::uint32_t, 4> lengths = { 50, 30, 50, 0 };
        // The fan controls: 5 types x 2 groups x 2 kinds of a row and an offset, 20 pairs of two rows and an offset,
        // and 5 records of two vectors and three matrices, which nothing reads
        constexpr std::size_t fanTypes = 5;
        constexpr std::size_t fanFloat = 4;
        constexpr std::size_t fanBRecord = (50 + 50 + 1) * fanFloat;
        constexpr std::size_t fanCRecord = (50 + 50 + 100 * 100 + 50 * 50 + 50 * 50) * fanFloat;

        class Reader
        {
        public:
            explicit Reader(std::string_view bytes)
                : mBytes(bytes)
            {
            }

            std::size_t left() const { return mBytes.size() - mPosition; }
            bool skip(std::size_t count)
            {
                if (count > left())
                    return false;
                mPosition += count;
                return true;
            }
            bool u32(std::uint32_t& value)
            {
                if (left() < sizeof(value))
                    return false;
                std::memcpy(&value, mBytes.data() + mPosition, sizeof(value));
                mPosition += sizeof(value);
                return true;
            }
            bool floats(std::size_t count, std::vector<float>& values)
            {
                if (count > left() / sizeof(float))
                    return false;
                values.resize(count);
                if (count > 0)
                    std::memcpy(values.data(), mBytes.data() + mPosition, count * sizeof(float));
                mPosition += count * sizeof(float);
                // A number that is not one (the file is data of the player's disk) would reach the face and the saved look
                return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
            }
            bool atEnd() const { return mPosition == mBytes.size(); }

        private:
            std::string_view mBytes;
            std::size_t mPosition = 0;
        };

        /// One list of controls: the rows are kept, the names are not wanted
        bool readList(Reader& reader, std::size_t length, std::vector<std::vector<float>>& rows)
        {
            std::uint32_t count = 0;
            if (!reader.u32(count))
                return false;
            // Each control has at least the u32 of its name's length: a count the rest of the file cannot hold is false
            if (count > reader.left() / sizeof(std::uint32_t))
                return false;
            rows.clear();
            rows.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                std::vector<float> row;
                if (!reader.floats(length, row))
                    return false;
                std::uint32_t nameLength = 0;
                if (!reader.u32(nameLength) || !reader.skip(nameLength))
                    return false;
                rows.push_back(std::move(row));
            }
            return true;
        }
    }

    std::optional<FaceControlFile> readFaceControlFile(std::string_view bytes)
    {
        Reader reader(bytes);
        if (bytes.size() < magic.size() || bytes.substr(0, magic.size()) != magic || !reader.skip(magic.size()))
            return std::nullopt;
        std::uint32_t geometry = 0;
        std::uint32_t texture = 0;
        if (!reader.u32(geometry) || !reader.u32(texture))
            return std::nullopt;
        for (const std::uint32_t expected : lengths)
        {
            std::uint32_t length = 0;
            if (!reader.u32(length) || length != expected)
                return std::nullopt;
        }

        FaceControlFile file;
        std::vector<std::vector<float>> shape;
        std::vector<std::vector<float>> asymmetric;
        std::vector<std::vector<float>> tone;
        std::vector<std::vector<float>> none;
        if (!readList(reader, lengths[0], shape) || !readList(reader, lengths[1], asymmetric)
            || !readList(reader, lengths[2], tone) || !readList(reader, lengths[3], none))
            return std::nullopt;
        file.mControls.set(FaceKind::Shape, std::move(shape));
        file.mControls.set(FaceKind::Tone, std::move(tone));
        file.mAsymmetricCount = static_cast<int>(asymmetric.size());

        // The fan controls of the first type: for each group and kind a row and the offset that makes its value an age
        for (std::size_t type = 0; type < fanTypes; ++type)
        {
            for (int group = 0; group < FaceAge::groups; ++group)
            {
                for (const FaceKind kind : { FaceKind::Shape, FaceKind::Tone })
                {
                    std::vector<float> row;
                    std::vector<float> offset;
                    if (!reader.floats(lengths[0], row) || !reader.floats(1, offset))
                        return std::nullopt;
                    if (type == 0)
                        file.mAge.set(kind, group, std::move(row), offset[0]);
                }
            }
        }
        file.mAge.finish();
        if (!reader.skip(20 * fanBRecord) || !reader.skip(fanTypes * fanCRecord) || !reader.atEnd())
            return std::nullopt;
        return file;
    }
}
