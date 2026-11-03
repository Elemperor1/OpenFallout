#include "falloutscripts.hpp"

#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadregn.hpp>
#include <components/esm4/loadwrld.hpp>

#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    std::vector<FalloutScripts::RegionEvent> FalloutScripts::takeRegionEvents()
    {
        std::vector<RegionEvent> events;
        Place place;
        if (mPlayer == 0 || !whereIs(mPlayer, place))
            return events;
        const OFWorld::CellStore* cell = cellAt(place);
        if (cell == nullptr)
            return events;
        const ESM::RefId cellId = cell->getCell()->getId();

        const std::lock_guard lock(mRegionMutex);
        // The games look at the regions when the player's cell changes (Player::SetParentCell, FalloutNV.exe
        // 0x94CAE0), not on every frame; the first look is the one after a game was begun or loaded
        if (mRegionCellKnown && mRegionCell == cellId)
            return events;

        const bool exterior = cell->isExterior();
        // The executable tells the name of the world space at the end of a door, a teleport or a load (0x93CC9A), and does not
        // when the player walks over to the next cell of the outside: that is a step of one cell in the same world space
        const ESM::RefId world = cell->getCell()->getWorldSpace();
        const int gridX = cell->getCell()->getGridX();
        const int gridY = cell->getCell()->getGridY();
        const bool step = mRegionCellKnown && exterior && mRegionExterior && world == mRegionWorld
            && std::abs(gridX - mRegionGridX) <= 1 && std::abs(gridY - mRegionGridY) <= 1;
        mRegionCell = cellId;
        mRegionCellKnown = true;
        mRegionExterior = exterior;
        mRegionWorld = world;
        mRegionGridX = gridX;
        mRegionGridY = gridY;
        std::vector<FalloutScript::RegionLine::Candidate> covering;
        if (exterior)
        {
            const ESM::FormId* cellForm = cellId.getIf<ESM::FormId>();
            const ESM4::Cell* record = cellForm != nullptr ? mStore.get<ESM4::Cell>().search(*cellForm) : nullptr;
            const ESM::RefId space = cell->getCell()->getWorldSpace(); // a copy: the pointer into it needs it alive
            const ESM::FormId* worldspace = space.getIf<ESM::FormId>();
            if (record != nullptr && worldspace != nullptr)
                // The list of the cell is walked in order: the pick depends on it
                for (const ESM::FormId& id : record->mRegions)
                {
                    const ESM4::Region* region = mStore.get<ESM4::Region>().search(id);
                    if (region == nullptr || !region->covers(*worldspace, place.mPosition.x(), place.mPosition.y()))
                        continue;
                    FalloutScript::RegionLine::Candidate candidate;
                    candidate.mRegion = id.hasContentFile() && id.mContentFile <= 0xfe ? id.toUint32() : 0;
                    candidate.mHasWeather = region->mHasWeather;
                    candidate.mOverride = region->mWeatherOverride;
                    candidate.mPriority = region->mWeatherPriority;
                    candidate.mName = region->mMapName;
                    if (candidate.mRegion != 0)
                        covering.push_back(std::move(candidate));
                }
        }
        if (std::optional<std::string> name = mRegionLine.enter(exterior, covering))
        {
            // A line for the log, so that a run can tell that the name was found whoever takes it
            Log(Debug::Info) << "Region line: the player came into a region whose name has " << name->size()
                             << " characters";
            events.push_back({ std::move(*name), false });
        }
        if (!step && exterior)
        {
            // The line over the compass tells the name of the world space at the end of every placement of the player
            // into an exterior (a door, fast travel, a move, a load), and nothing for an interior; the name of the
            // exterior cell itself is never used (Mac round 12AW S10, FalloutNV.exe 0x93CC5D)
            const ESM::RefId space = cell->getCell()->getWorldSpace();
            const ESM::FormId* worldspace = space.getIf<ESM::FormId>();
            const ESM4::World* record = worldspace != nullptr ? mStore.get<ESM4::World>().search(*worldspace) : nullptr;
            if (record != nullptr)
            {
                // (the region comes first: in the executable it is looked for inside the change of the cell, the name of
                // the world space at the end of the transition)
                Log(Debug::Info) << "Region line: the player was placed in a world space whose name has "
                                 << record->mFullName.size() << " characters";
                events.push_back({ record->mFullName, true });
            }
        }
        return events;
    }
}
