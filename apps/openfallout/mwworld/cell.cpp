#include "cell.hpp"

#include "esmstore.hpp"

#include "../mwbase/environment.hpp"

#include <components/esm3/loadcell.hpp>
#include <components/esm4/lighting.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadlgtm.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/misc/algorithm.hpp>

#include <set>
#include <stdexcept>
#include <string>

namespace OFWorld
{
    ESM::FormId resolveClimate(
        const ESM4::World& world, const std::function<const ESM4::World*(ESM::FormId)>& findWorld)
    {
        // The worldspaces on the way up, so that a loop in the data ends the walk and a long chain does not
        std::set<ESM::FormId> visited{ world.mId };
        const ESM4::World* current = &world;
        while (!current->mParent.isZeroOrUnset() && (current->mParentUseFlags & ESM4::World::UseFlag_Climate))
        {
            const ESM4::World* parent = findWorld(current->mParent);
            if (parent == nullptr || !visited.insert(parent->mId).second)
                break;
            current = parent;
        }
        // A parent that has no climate either leaves the worldspace with its own
        return !current->mClimate.isZeroOrUnset() ? current->mClimate : world.mClimate;
    }

    ESM::FormId resolveCellClimate(const ESM4::Cell& cell, const ESM4::World* world,
        const std::function<const ESM4::World*(ESM::FormId)>& findWorld)
    {
        const bool isExterior = !(cell.mCellFlags & ESM4::CELL_Interior);
        const bool showsSky = cell.mCellFlags & ESM4::CELL_QuasiExt;
        if (!isExterior && !showsSky)
            return ESM::FormId();
        if (!cell.mClimate.isZeroOrUnset())
            return cell.mClimate;
        if (isExterior && world != nullptr)
            return resolveClimate(*world, findWorld);
        return ESM::FormId();
    }

    ESM4::Lighting resolveLighting(
        const ESM4::Lighting& own, const ESM4::Lighting* lightingTemplate, std::uint32_t inheritFlags)
    {
        ESM4::Lighting result = own;
        if (lightingTemplate == nullptr)
            return result;

        if (inheritFlags & 0x001)
            result.ambient = lightingTemplate->ambient;
        if (inheritFlags & 0x002)
            result.directional = lightingTemplate->directional;
        if (inheritFlags & 0x004)
            result.fogColor = lightingTemplate->fogColor;
        if (inheritFlags & 0x008)
            result.fogNear = lightingTemplate->fogNear;
        if (inheritFlags & 0x010)
            result.fogFar = lightingTemplate->fogFar;
        if (inheritFlags & 0x020)
        {
            result.rotationXY = lightingTemplate->rotationXY;
            result.rotationZ = lightingTemplate->rotationZ;
        }
        if (inheritFlags & 0x040)
            result.fogDirFade = lightingTemplate->fogDirFade;
        if (inheritFlags & 0x080)
            result.fogClipDist = lightingTemplate->fogClipDist;
        if (inheritFlags & 0x100)
            result.fogPower = lightingTemplate->fogPower;
        return result;
    }

    namespace
    {
        MoodData readMood(const ESM4::Cell& cell)
        {
            const ESM4::LightingTemplate* lightingTemplate = nullptr;
            if (cell.mLightingTemplateFlags != 0 && !cell.mLightingTemplate.isZeroOrUnset())
                lightingTemplate = OFBase::Environment::get().getESMStore()->get<ESM4::LightingTemplate>().search(
                    ESM::RefId(cell.mLightingTemplate));

            const ESM4::Lighting lighting = resolveLighting(cell.mLighting,
                lightingTemplate != nullptr ? &lightingTemplate->mLighting : nullptr, cell.mLightingTemplateFlags);

            return MoodData{
                .mAmbiantColor = lighting.ambient,
                .mDirectionalColor = lighting.directional,
                .mFogColor = lighting.fogColor,
                // Fallout fog is a range, not a share of the view distance
                .mFogDensity = 1.f,
                .mFogNear = lighting.fogNear,
                .mFogFar = lighting.fogFar,
            };
        }

        std::string getDescription(const ESM4::World& value)
        {
            if (!value.mEditorId.empty())
                return value.mEditorId;

            return ESM::RefId(value.mId).serializeText();
        }

        std::string getCellDescription(const ESM4::Cell& cell, const ESM4::World* world)
        {
            std::string result;

            if (!cell.mEditorId.empty())
                result = cell.mEditorId;
            else if (world != nullptr && cell.isExterior())
                result = getDescription(*world);
            else
                result = cell.mId.serializeText();

            if (cell.isExterior())
                result += " (" + std::to_string(cell.mX) + ", " + std::to_string(cell.mY) + ")";

            return result;
        }
    }

    Cell::Cell(const ESM4::Cell& cell)
        : ESM::CellVariant(cell)
        , mIsExterior(!(cell.mCellFlags & ESM4::CELL_Interior))
        , mIsQuasiExterior(cell.mCellFlags & ESM4::CELL_QuasiExt)
        , mHasWater(cell.mCellFlags & ESM4::CELL_HasWater)
        , mNoSleep(false) // No such notion in ESM4
        , mGridPos(cell.mX, cell.mY)
        , mDisplayname(cell.mFullName)
        , mNameID(cell.mEditorId)
        , mRegion() // a cell that shows the sky gets the id of its climate below, an interior cell has no region
        , mId(cell.mId)
        , mParent(cell.mParent)
        , mWaterHeight(cell.mWaterHeight)
        , mMood(readMood(cell))
    {
        const auto& worlds = OFBase::Environment::get().getESMStore()->get<ESM4::World>();
        const ESM4::World* world = worlds.search(mParent);
        if (isExterior())
        {
            if (world == nullptr)
                throw std::runtime_error(
                    "Cell " + cell.mId.toDebugString() + " parent world " + mParent.toDebugString() + " is not found");
            mWaterHeight = world->mWaterLevel;
        }

        // The weather of a cell is the one of its climate, where a cell of Morrowind has the weather of its region. A
        // cell with no climate has no region, as a cell of Morrowind without one: the weather manager leaves the
        // weather as it is.
        const ESM::FormId climate
            = resolveCellClimate(cell, world, [&](ESM::FormId id) { return worlds.search(ESM::RefId(id)); });
        if (!climate.isZeroOrUnset())
            mRegion = ESM::RefId(climate);
        mDescription = getCellDescription(cell, world);
    }

    Cell::Cell(const ESM::Cell& cell)
        : ESM::CellVariant(cell)
        , mIsExterior(!(cell.mData.mFlags & ESM::Cell::Interior))
        , mIsQuasiExterior(cell.mData.mFlags & ESM::Cell::QuasiEx)
        , mHasWater(cell.mData.mFlags & ESM::Cell::HasWater)
        , mNoSleep(cell.mData.mFlags & ESM::Cell::NoSleep)
        , mGridPos(cell.getGridX(), cell.getGridY())
        , mDisplayname(cell.mName)
        , mNameID(cell.mName)
        , mRegion(cell.mRegion)
        , mId(cell.mId)
        , mParent(ESM::Cell::sDefaultWorldspaceId)
        , mWaterHeight(cell.mWater)
        , mDescription(cell.getDescription())
        , mMood{
            .mAmbiantColor = cell.mAmbi.mAmbient,
            .mDirectionalColor = cell.mAmbi.mSunlight,
            .mFogColor = cell.mAmbi.mFog,
            .mFogDensity = cell.mAmbi.mFogDensity,
        }
    {
        if (isExterior())
        {
            mWaterHeight = -1.f;
            mHasWater = true;
        }
        else
            mGridPos = {};
    }
}
