#include "action.hpp"

#include "../mwbase/environment.hpp"

#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

const OFWorld::Ptr& OFWorld::Action::getTarget() const
{
    return mTarget;
}

void OFWorld::Action::setTarget(const OFWorld::Ptr& target)
{
    mTarget = target;
}

OFWorld::Action::Action(bool keepSound, const Ptr& target)
    : mKeepSound(keepSound)
    , mSoundOffset(0)
    , mTarget(target)
{
}

OFWorld::Action::~Action() {}

void OFWorld::Action::execute(const Ptr& actor, bool noSound)
{
    if (!mSoundId.empty() && !noSound)
    {
        OFSound::PlayMode envType = OFSound::PlayMode::Normal;

        // Action sounds should not have a distortion in GUI mode
        // example: take an item or drink a potion underwater
        if (actor == OFMechanics::getPlayer() && OFBase::Environment::get().getWindowManager()->isGuiMode())
        {
            envType = OFSound::PlayMode::NoEnv;
        }

        if (mKeepSound && actor == OFMechanics::getPlayer())
            OFBase::Environment::get().getSoundManager()->playSound(
                mSoundId, 1.0, 1.0, OFSound::Type::Sfx, envType, mSoundOffset);
        else
        {
            bool local = mTarget.isEmpty() || !mTarget.isInCell(); // no usable target
            if (mKeepSound)
                OFBase::Environment::get().getSoundManager()->playSound3D(
                    (local ? actor : mTarget).getRefData().getPosition().asVec3(), mSoundId, 1.0, 1.0,
                    OFSound::Type::Sfx, envType, mSoundOffset);
            else
                OFBase::Environment::get().getSoundManager()->playSound3D(
                    local ? actor : mTarget, mSoundId, 1.0, 1.0, OFSound::Type::Sfx, envType, mSoundOffset);
        }
    }

    executeImp(actor);
}

void OFWorld::Action::setSound(const ESM::RefId& id)
{
    mSoundId = id;
}

void OFWorld::Action::setSoundOffset(float offset)
{
    mSoundOffset = offset;
}
