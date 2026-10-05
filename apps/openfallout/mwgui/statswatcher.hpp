#ifndef MWGUI_STATSWATCHER_H
#define MWGUI_STATSWATCHER_H

#include <map>
#include <set>
#include <span>

#include <components/esm/attr.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwmechanics/stat.hpp"

#include "../mwworld/ptr.hpp"

namespace OFGui
{
    class StatsListener
    {
    public:
        virtual ~StatsListener() = default;

        /// Set value for the given ID.
        virtual void setAttribute(ESM::RefId id, const OFMechanics::AttributeValue& value) {}
        virtual void setValue(std::string_view id, const OFMechanics::DynamicStat<float>& value) {}
        virtual void setValue(std::string_view, const std::string& value) {}
        virtual void setValue(std::string_view, int value) {}
        virtual void setValue(ESM::RefId id, const OFMechanics::SkillValue& value) {}
        virtual void configureSkills(std::span<const ESM::RefId> major, std::span<const ESM::RefId> minor) {}
    };

    class StatsWatcher
    {
        OFWorld::Ptr mWatched;

        std::map<ESM::RefId, OFMechanics::AttributeValue> mWatchedAttributes;
        std::map<ESM::RefId, OFMechanics::SkillValue> mWatchedSkills;

        OFMechanics::DynamicStat<float> mWatchedHealth;
        OFMechanics::DynamicStat<float> mWatchedMagicka;
        OFMechanics::DynamicStat<float> mWatchedFatigue;

        std::string mWatchedName;
        ESM::RefId mWatchedRace;
        ESM::RefId mWatchedClass;

        int mWatchedLevel;

        float mWatchedTimeToStartDrowning;

        bool mWatchedStatsEmpty;

        std::set<StatsListener*> mListeners;

        void setAttribute(ESM::RefId id, const OFMechanics::AttributeValue& value);
        void setValue(std::string_view id, const OFMechanics::DynamicStat<float>& value);
        void setValue(std::string_view id, const std::string& value);
        void setValue(std::string_view id, int value);
        void setValue(ESM::RefId id, const OFMechanics::SkillValue& value);
        void configureSkills(std::span<const ESM::RefId> major, std::span<const ESM::RefId> minor);

    public:
        StatsWatcher();

        void update();
        void addListener(StatsListener* listener);
        void removeListener(StatsListener* listener);

        void watchActor(const OFWorld::Ptr& ptr);
        OFWorld::Ptr getWatchedActor() const { return mWatched; }

        void forceUpdate() { mWatchedStatsEmpty = true; }
    };
}

#endif
