#ifndef GAME_SOUND_WATERSOUNDUPDATER_H
#define GAME_SOUND_WATERSOUNDUPDATER_H

#include <components/esm/formid.hpp>
#include <components/falloutscript/nearwater.hpp>

#include <osg/Vec3f>

namespace OFBase
{
    class World;
}

namespace OFWorld
{
    class ConstPtr;
    class ESMStore;
}

namespace OFSound
{
    /// What the water near the player asks for (the sampler of the executable at 0x4E6A80)
    struct WaterSoundUpdate
    {
        /// The sound of the water of the cell (the SNAM of its WATR record), zero when it has none
        ESM::FormId mSound;
        /// How near the water is, 0 to 1: the sound plays while it is above 0
        float mStrength = 0.f;
        /// Where the sound comes from (at the height of the water)
        osg::Vec3f mPosition;
    };

    class WaterSoundUpdater
    {
    public:
        WaterSoundUpdater() = default;

        /// How near the water is to the player and where its sound comes from. The cell of the player is the one whose
        /// water type is heard; out of doors the terrain around the player is looked at, point by point, against the
        /// water of the cell each point is in.
        WaterSoundUpdate update(
            const OFWorld::ConstPtr& player, const OFBase::World& world, const OFWorld::ESMStore& store) const;

        void setUnderwater(bool value) { mListenerUnderwater = value; }

    private:
        FalloutScript::NearWater::Settings mSettings;
        bool mListenerUnderwater = false;
    };
}

#endif
