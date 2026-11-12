#include "falloutfootsteps.hpp"

#include <random>
#include <string>
#include <string_view>

#include <components/debug/debuglog.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/refdata.hpp"

namespace OFSound
{
    void playFootstep(const FootstepRequest& request)
    {
        namespace Steps = FalloutScript::Footsteps;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (sound == nullptr || world == nullptr || request.mActor.isEmpty() || !sound->isEnabled())
            return;

        static const Steps::Settings settings;
        const float distance
            = (sound->getListenerPosition() - request.mActor.getRefData().getPosition().asVec3()).length();
        if (!Steps::isHeard(settings, distance))
            return;

        thread_local std::minstd_rand random{ 0x57e9 };
        const auto unit = [] { return std::uniform_real_distribution<float>(0.f, 1.f)(random); };

        // What the foot comes down on decides the sound (a character that is not on anything known is on concrete)
        bool stairs = false;
        const int material = world->getFloorMaterial(request.mActor, stairs);
        const Steps::Surface surface = Steps::surfaceOfMaterial(material, true);
        const Steps::Slot slot = request.mSlot ? *request.mSlot
                                               : Steps::chooseSlot(request.mFoot, request.mSneaking, stairs, request.mRunning);
        // New Vegas has the sounds of a jump, Fallout 3 does not (its table has no such slot)
        if (slot == Steps::Slot::Jump && !sound->hasFalloutSound(Steps::soundName(Steps::Surface::ConcSolid, slot)))
            return;
        const std::string name = Steps::resolveSound(
            surface, slot, [sound](std::string_view editorId) { return sound->hasFalloutSound(editorId); });
        if (name.empty())
            return;

        // Said for the first few steps of the player in the first person and of the rest
        static int sLoggedFirstPerson = 0;
        static int sLogged = 0;
        int& logged = request.mPlayerInFirstPerson ? sLoggedFirstPerson : sLogged;
        if (logged < 40)
        {
            ++logged;
            Log(Debug::Info) << "Footstep of " << request.mActor.getCellRef().getRefId()
                             << (request.mPlayerInFirstPerson ? " (first person)" : "") << ": material " << material
                             << ", " << Steps::surfaceName(surface) << ", " << Steps::slotName(slot) << " -> " << name << " at "
                             << static_cast<int>(distance) << " units";
        }

        sound->playFalloutSound(name, request.mActor, Steps::stepVolume(settings, request.mPlayerInFirstPerson, unit()),
            OFSound::Type::Foot, OFSound::PlayMode::Overlap);

        if (request.mArmor != 0 && (!request.mSlot || *request.mSlot == Steps::Slot::Jump))
        {
            const std::string armor = request.mSlot ? Steps::armorJumpSoundName(request.mArmor == 2)
                                                    : Steps::armorSoundName(request.mArmor == 2, request.mFoot);
            sound->playFalloutSound(armor, request.mActor,
                Steps::armorVolume(request.mPlayerInFirstPerson, settings, unit()), OFSound::Type::Foot,
                OFSound::PlayMode::Overlap);
        }
    }
}
