#include "falloutscripts.hpp"

#include <mutex>
#include <string>
#include <vector>

#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadregn.hpp>

#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    std::vector<std::string> FalloutScripts::takeRegionNames()
    {
        std::vector<std::string> names;
        Place place;
        if (mPlayer == 0 || !whereIs(mPlayer, place))
            return names;
        const OFWorld::CellStore* cell = cellAt(place);
        if (cell == nullptr)
            return names;
        const ESM::RefId cellId = cell->getCell()->getId();

        const std::lock_guard lock(mRegionMutex);
        // The games look at the regions when the player's cell changes (Player::SetParentCell, FalloutNV.exe
        // 0x94CAE0), not on every frame; the first look is the one after a game was begun or loaded
        if (mRegionCellKnown && mRegionCell == cellId)
            return names;
        mRegionCell = cellId;
        mRegionCellKnown = true;

        const bool exterior = cell->isExterior();
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
            names.push_back(std::move(*name));
        return names;
    }
}
