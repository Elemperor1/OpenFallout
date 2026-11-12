#include "watersoundupdater.hpp"

#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

#include <components/esm/util.hpp>
#include <components/esm4/loadwatr.hpp>

#include <cmath>
#include <vector>

namespace OFSound
{
    namespace
    {
        // The water of an exterior cell that is loaded, by where it is in the grid
        struct WaterCell
        {
            int mX;
            int mY;
            float mLevel;
        };
    }

    WaterSoundUpdate WaterSoundUpdater::update(
        const OFWorld::ConstPtr& player, const OFBase::World& world, const OFWorld::ESMStore& store) const
    {
        namespace NearWater = FalloutScript::NearWater;
        WaterSoundUpdate result;

        if (!player.isInCell())
            return result;
        const OFWorld::CellStore& cellStore = *player.getCell();
        const OFWorld::Cell* cell = cellStore.getCell();
        // A cell of Fallout names its water type; without water the sound is not heard at all
        if (cell == nullptr || !cell->hasWater() || cell->getWaterType().isZeroOrUnset())
            return result;
        const ESM4::Water* water = store.get<ESM4::Water>().search(cell->getWaterType());
        if (water == nullptr || water->mSound.isZeroOrUnset())
            return result;

        const osg::Vec3f position = player.getRefData().getPosition().asVec3();
        const NearWater::Point where{ position.x(), position.y() };
        const float level = cellStore.getWaterLevel();
        const bool interior = !cell->isExterior();

        float strength = 0.f;
        NearWater::Wet wet;
        if (interior)
            strength = NearWater::interiorStrength(level, position.z(), mListenerUnderwater, mSettings);
        else
        {
            const ESM::RefId worldspace = cell->getWorldSpace();
            std::vector<WaterCell> cells;
            for (const OFWorld::CellStore* active : world.getActiveCells())
            {
                const OFWorld::Cell* other = active->getCell();
                if (other != nullptr && other->isExterior() && other->hasWater() && other->getWorldSpace() == worldspace)
                    cells.push_back({ other->getGridX(), other->getGridY(), active->getWaterLevel() });
            }
            for (const NearWater::Point point : NearWater::samplePoints(where, mSettings))
            {
                // The water of the cell that the point is in (a point in a cell that is not loaded or has none is dry)
                const ESM::ExteriorCellLocation location = ESM::positionToExteriorCellLocation(point.mX, point.mY, worldspace);
                const WaterCell* found = nullptr;
                for (const WaterCell& candidate : cells)
                    if (candidate.mX == location.mX && candidate.mY == location.mY)
                    {
                        found = &candidate;
                        break;
                    }
                if (found == nullptr)
                    continue;
                if (world.getTerrainHeightAt(osg::Vec3f(point.mX, point.mY, 0.f), worldspace) < found->mLevel)
                    wet.add(point);
            }
            strength = NearWater::exteriorStrength(wet, level, position.z(), mListenerUnderwater, mSettings);
        }

        result.mSound = water->mSound;
        result.mStrength = strength;
        const NearWater::Point at = NearWater::soundPosition(where, wet, strength, interior, mSettings);
        result.mPosition = osg::Vec3f(at.mX, at.mY, level);
        return result;
    }
}
