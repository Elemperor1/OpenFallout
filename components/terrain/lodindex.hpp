#ifndef OPENFALLOUT_COMPONENTS_TERRAIN_LODINDEX_H
#define OPENFALLOUT_COMPONENTS_TERRAIN_LODINDEX_H

#include <components/vfs/pathutil.hpp>

#include <map>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

namespace VFS
{
    class Manager;
}

namespace Terrain
{
    /// The files of the distant land that Fallout 3 and New Vegas ship per worldspace, under
    /// meshes/landscape/lod/<worldspace>/: terrain tiles named <worldspace>.level<L>.x<X>.y<Y>.nif that cover
    /// L by L cells from the cell X, Y, and in the folder blocks/ the blocks of distant objects named
    /// <worldspace>.level4[.high].x<X>.y<Y>.nif (four by four cells).
    struct LodTileName
    {
        enum class Kind
        {
            Terrain,
            Objects,
        };

        Kind mKind;
        int mLevel;
        bool mHigh; // an object block of the second, "high", set
        int mX;
        int mY;
    };

    /// @param stem the name of the file without folders and extension, lower case
    /// @return nothing when the name is not of the form <worldspace>.level<L>[.high].x<X>.y<Y>
    std::optional<LodTileName> parseLodTileName(
        std::string_view stem, std::string_view worldspace, LodTileName::Kind kind);

    struct LodTile
    {
        LodTileName mName;
        VFS::Path::Normalized mPath;
    };

    /// Which of those files exist for one worldspace.
    class LodIndex
    {
    public:
        static constexpr int sSmallestLevel = 4;
        static constexpr int sLargestLevel = 32;

        LodIndex() = default;

        /// @param worldspace the editor id of the worldspace, in any case
        LodIndex(const VFS::Manager& vfs, std::string_view worldspace);

        /// Adds the file when it is one of the tiles of the worldspace.
        /// @return whether the file was a tile
        bool add(const VFS::Path::Normalized& path, std::string_view worldspace);

        bool empty() const { return mTiles.empty(); }
        std::size_t getTerrainCount() const { return mTerrainCount; }
        std::size_t getObjectCount() const { return mTiles.size() - mTerrainCount; }

        const LodTile* find(LodTileName::Kind kind, int level, bool high, int x, int y) const;

        /// The terrain tiles that fill the square of the given size in cells from the cell x, y. A tile of exactly
        /// that size is the answer when there is one, otherwise the four squares of half the size are looked at,
        /// down to the smallest level, and above the largest level the squares of that level.
        std::vector<const LodTile*> getTerrain(int x, int y, int size) const;

        /// How much of the square of the given size in cells from the cell x, y the terrain tiles fill.
        enum class Coverage
        {
            None,
            Partial,
            Full,
        };
        Coverage getTerrainCoverage(int x, int y, int size) const;

        /// The same for the blocks of distant objects of the set "high" or of the other set.
        std::vector<const LodTile*> getObjects(int x, int y, int size, bool high) const;

    private:
        using Key = std::tuple<LodTileName::Kind, int, bool, int, int>;

        void collect(LodTileName::Kind kind, bool high, int x, int y, int size,
            std::vector<const LodTile*>& out) const;

        std::map<Key, LodTile> mTiles;
        std::size_t mTerrainCount = 0;
    };
}

#endif
