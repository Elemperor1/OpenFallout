#include "weatherstate.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"
#include "loadregn.hpp"

namespace ESM
{
    namespace
    {
        constexpr NAME currentRegionRecord = "CREG";
        constexpr NAME timePassedRecord = "TMPS";
        constexpr NAME fastForwardRecord = "FAST";
        constexpr NAME weatherUpdateTimeRecord = "WUPD";
        constexpr NAME transitionFactorRecord = "TRFC";
        constexpr NAME currentWeatherRecord = "CWTH";
        constexpr NAME nextWeatherRecord = "NWTH";
        constexpr NAME queuedWeatherRecord = "QWTH";
        constexpr NAME regionNameRecord = "RGNN";
        constexpr NAME regionWeatherRecord = "RGNW";
        constexpr NAME regionChanceRecord = "RGNC";
        // Weathers other than the ten of Morrowind (those of Fallout) have no index, they are saved by id after the
        // index that stands for them: the record of an id, and for the chances of a region the id and the chance
        constexpr NAME currentWeatherIdRecord = "CWTX";
        constexpr NAME nextWeatherIdRecord = "NWTX";
        constexpr NAME queuedWeatherIdRecord = "QWTX";
        constexpr NAME regionWeatherIdRecord = "RGNX";
        constexpr NAME regionChanceIdRecord = "RGNF";
        constexpr NAME regionChanceOtherRecord = "RGNG";

        RefId withoutIndex(const RefId& weather)
        {
            return Weather::refIdToIndex(weather) < 0 ? weather : RefId();
        }

        RefId readWeather(ESMReader& esm, NAME indexRecord, NAME idRecord)
        {
            int index;
            esm.getHNT(index, indexRecord);
            const RefId id = esm.getHNORefId(idRecord);
            return id.empty() ? Weather::indexToRefId(index) : id;
        }
    }
}

namespace ESM
{
    void WeatherState::load(ESMReader& esm)
    {
        mCurrentRegion = esm.getHNRefId(currentRegionRecord);
        esm.getHNT(mTimePassed, timePassedRecord);
        esm.getHNT(mFastForward, fastForwardRecord);
        esm.getHNT(mWeatherUpdateTime, weatherUpdateTimeRecord);
        esm.getHNT(mTransitionFactor, transitionFactorRecord);
        mCurrentWeather = readWeather(esm, currentWeatherRecord, currentWeatherIdRecord);
        mNextWeather = readWeather(esm, nextWeatherRecord, nextWeatherIdRecord);
        mQueuedWeather = readWeather(esm, queuedWeatherRecord, queuedWeatherIdRecord);

        while (esm.isNextSub(regionNameRecord))
        {
            ESM::RefId regionID = esm.getRefId();
            RegionWeatherState region;
            region.mWeather = readWeather(esm, regionWeatherRecord, regionWeatherIdRecord);
            int index = 0;
            while (esm.isNextSub(regionChanceRecord))
            {
                uint8_t chance;
                esm.getHT(chance);
                ESM::RefId id = Weather::indexToRefId(index++);
                region.mChances.emplace(id, chance);
            }
            while (esm.isNextSub(regionChanceIdRecord))
            {
                const ESM::RefId id = esm.getRefId();
                uint8_t chance;
                esm.getHNT(chance, regionChanceOtherRecord);
                region.mChances.emplace(id, chance);
            }

            mRegions.insert(std::make_pair(regionID, region));
        }
    }

    void WeatherState::save(ESMWriter& esm) const
    {
        esm.writeHNCRefId(currentRegionRecord, mCurrentRegion);
        esm.writeHNT(timePassedRecord, mTimePassed);
        esm.writeHNT(fastForwardRecord, mFastForward);
        esm.writeHNT(weatherUpdateTimeRecord, mWeatherUpdateTime);
        esm.writeHNT(transitionFactorRecord, mTransitionFactor);
        esm.writeHNT(currentWeatherRecord, Weather::refIdToIndex(mCurrentWeather));
        esm.writeHNOCRefId(currentWeatherIdRecord, withoutIndex(mCurrentWeather));
        esm.writeHNT(nextWeatherRecord, Weather::refIdToIndex(mNextWeather));
        esm.writeHNOCRefId(nextWeatherIdRecord, withoutIndex(mNextWeather));
        esm.writeHNT(queuedWeatherRecord, Weather::refIdToIndex(mQueuedWeather));
        esm.writeHNOCRefId(queuedWeatherIdRecord, withoutIndex(mQueuedWeather));

        for (const auto& [region, weather] : mRegions)
        {
            esm.writeHNCRefId(regionNameRecord, region);
            esm.writeHNT(regionWeatherRecord, Weather::refIdToIndex(weather.mWeather));
            esm.writeHNOCRefId(regionWeatherIdRecord, withoutIndex(weather.mWeather));
            for (int i = 0; i < Weather::Length; ++i)
            {
                ESM::RefId id = Weather::indexToRefId(i);
                uint8_t chance = 0;
                const auto found = weather.mChances.find(id);
                if (found != weather.mChances.end())
                    chance = found->second;
                esm.writeHNT(regionChanceRecord, chance);
            }
            for (const auto& [id, chance] : weather.mChances)
            {
                if (Weather::refIdToIndex(id) >= 0)
                    continue;
                esm.writeHNCRefId(regionChanceIdRecord, id);
                esm.writeHNT(regionChanceOtherRecord, chance);
            }
        }
    }
}
