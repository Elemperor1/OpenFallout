#ifndef OPENW_MWORLD_CELL
#define OPENW_MWORLD_CELL

#include <cstdint>
#include <functional>

#include <osg/Vec2i>

#include <components/esm/esmbridge.hpp>
#include <components/esm/exteriorcelllocation.hpp>
#include <components/esm/refid.hpp>

namespace ESM
{
    struct Cell;
}

namespace ESM4
{
    struct Cell;
    struct Lighting;
    struct World;
}

namespace OFWorld
{
    class CellStore;

    /// The lighting of a cell as the renderer needs it.
    struct MoodData
    {
        uint32_t mAmbiantColor;
        uint32_t mDirectionalColor;
        uint32_t mFogColor;
        /// Morrowind cells give the share of the view distance that is fogged; Fallout cells give a range instead.
        float mFogDensity;
        /// The distances in game units where the fog starts and where it is complete, both 0 when the cell has none.
        float mFogNear = 0.f;
        float mFogFar = 0.f;

        bool hasFogRange() const { return mFogFar > mFogNear; }
    };

    /// The lighting a Fallout cell is drawn with: its own values, except those that the inherit flags (bit 0 ambient
    /// colour to bit 8 fog power, as in the LNAM sub-record of the cell) take from its lighting template.
    ESM4::Lighting resolveLighting(
        const ESM4::Lighting& own, const ESM4::Lighting* lightingTemplate, std::uint32_t inheritFlags);

    /// The climate of a Fallout worldspace: its own, or the one of its parent worldspace (and so on up the chain) when
    /// the worldspace says that it uses the climate of its parent. Zero when there is none.
    ESM::FormId resolveClimate(
        const ESM4::World& world, const std::function<const ESM4::World*(ESM::FormId)>& findWorld);

    class Cell : public ESM::CellVariant
    {
    public:
        explicit Cell(const ESM4::Cell& cell);
        explicit Cell(const ESM::Cell& cell);

        int getGridX() const { return mGridPos.x(); }
        int getGridY() const { return mGridPos.y(); }
        bool isExterior() const { return mIsExterior; }
        bool isQuasiExterior() const { return mIsQuasiExterior; }
        bool hasWater() const { return mHasWater; }
        bool noSleep() const { return mNoSleep; }
        const ESM::RefId& getRegion() const { return mRegion; }
        std::string_view getNameId() const { return mNameID; }
        std::string_view getDisplayName() const { return mDisplayname; }
        std::string_view getDescription() const { return mDescription; }
        const MoodData& getMood() const { return mMood; }
        float getWaterHeight() const { return mWaterHeight; }
        const ESM::RefId& getId() const { return mId; }
        ESM::RefId getWorldSpace() const { return mIsExterior ? mParent : mId; }

        ESM::ExteriorCellLocation getExteriorCellLocation() const
        {
            return ESM::ExteriorCellLocation(mGridPos.x(), mGridPos.y(), getWorldSpace());
        }

    private:
        bool mIsExterior;
        bool mIsQuasiExterior;
        bool mHasWater;
        bool mNoSleep;

        osg::Vec2i mGridPos;
        std::string mDisplayname; // How the game displays it
        std::string mNameID; // The name that will be used by the script and console commands
        ESM::RefId mRegion;
        ESM::RefId mId;
        ESM::RefId mParent;
        float mWaterHeight;
        std::string mDescription;
        MoodData mMood;
    };
}

#endif
