#include "lodindex.hpp"

#include <charconv>
#include <system_error>

#include <components/misc/strings/lower.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

namespace Terrain
{
    namespace
    {
        bool consume(std::string_view& text, std::string_view prefix)
        {
            if (!text.starts_with(prefix))
                return false;
            text.remove_prefix(prefix.size());
            return true;
        }

        bool consumeInt(std::string_view& text, int& value)
        {
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end == text.data())
                return false;
            text.remove_prefix(static_cast<std::size_t>(end - text.data()));
            return true;
        }

        bool isLevel(int level)
        {
            return level >= LodIndex::sSmallestLevel && level <= LodIndex::sLargestLevel && (level & (level - 1)) == 0;
        }
    }

    std::optional<LodTileName> parseLodTileName(
        std::string_view stem, std::string_view worldspace, LodTileName::Kind kind)
    {
        if (!consume(stem, worldspace) || !consume(stem, ".level"))
            return std::nullopt;

        LodTileName name{ .mKind = kind, .mLevel = 0, .mHigh = false, .mX = 0, .mY = 0 };
        if (!consumeInt(stem, name.mLevel) || !isLevel(name.mLevel))
            return std::nullopt;
        name.mHigh = consume(stem, ".high");
        if (!consume(stem, ".x") || !consumeInt(stem, name.mX) || !consume(stem, ".y") || !consumeInt(stem, name.mY)
            || !stem.empty())
            return std::nullopt;
        // Only the blocks of objects come in a second set
        if (name.mHigh && kind == LodTileName::Kind::Terrain)
            return std::nullopt;
        return name;
    }

    LodIndex::LodIndex(const VFS::Manager& vfs, std::string_view worldspace)
    {
        std::string folder = "meshes/landscape/lod/";
        folder += Misc::StringUtils::lowerCase(worldspace);
        folder += '/';
        for (const VFS::Path::Normalized& path : vfs.getRecursiveDirectoryIterator(folder))
            add(path, worldspace);
    }

    bool LodIndex::add(const VFS::Path::Normalized& path, std::string_view worldspace)
    {
        const std::string name = Misc::StringUtils::lowerCase(worldspace);
        std::string_view rest = path.value();
        if (!consume(rest, "meshes/landscape/lod/") || !consume(rest, name) || !consume(rest, "/"))
            return false;

        LodTileName::Kind kind = LodTileName::Kind::Terrain;
        if (consume(rest, "blocks/"))
            kind = LodTileName::Kind::Objects;

        if (rest.find('/') != std::string_view::npos || !rest.ends_with(".nif"))
            return false;
        rest.remove_suffix(4);

        const std::optional<LodTileName> tile = parseLodTileName(rest, name, kind);
        if (!tile)
            return false;

        const Key key{ tile->mKind, tile->mLevel, tile->mHigh, tile->mX, tile->mY };
        if (mTiles.emplace(key, LodTile{ *tile, path }).second && kind == LodTileName::Kind::Terrain)
            ++mTerrainCount;
        return true;
    }

    const LodTile* LodIndex::find(LodTileName::Kind kind, int level, bool high, int x, int y) const
    {
        const auto it = mTiles.find(Key{ kind, level, high, x, y });
        return it == mTiles.end() ? nullptr : &it->second;
    }

    void LodIndex::collect(
        LodTileName::Kind kind, bool high, int x, int y, int size, std::vector<const LodTile*>& out) const
    {
        if (size < sSmallestLevel)
            return;
        if (size <= sLargestLevel)
        {
            if (const LodTile* tile = find(kind, size, high, x, y))
            {
                out.push_back(tile);
                return;
            }
            if (size == sSmallestLevel)
                return;
        }
        if (size % 2 != 0)
            return;
        const int half = size / 2;
        collect(kind, high, x, y, half, out);
        collect(kind, high, x + half, y, half, out);
        collect(kind, high, x, y + half, half, out);
        collect(kind, high, x + half, y + half, half, out);
    }

    std::vector<const LodTile*> LodIndex::getTerrain(int x, int y, int size) const
    {
        std::vector<const LodTile*> result;
        collect(LodTileName::Kind::Terrain, false, x, y, size, result);
        return result;
    }

    std::vector<const LodTile*> LodIndex::getObjects(int x, int y, int size, bool high) const
    {
        std::vector<const LodTile*> result;
        collect(LodTileName::Kind::Objects, high, x, y, size, result);
        return result;
    }
}
