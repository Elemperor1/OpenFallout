#ifndef MWMECHANICS_SECURITY_H
#define MWMECHANICS_SECURITY_H

#include "../mwworld/ptr.hpp"

namespace OFMechanics
{

    /// @brief implementation of Security skill
    class Security
    {
    public:
        Security(const OFWorld::Ptr& actor);

        void pickLock(const OFWorld::Ptr& lock, const OFWorld::Ptr& lockpick, std::string_view& resultMessage,
            std::string_view& resultSound);
        void probeTrap(const OFWorld::Ptr& trap, const OFWorld::Ptr& probe, std::string_view& resultMessage,
            std::string_view& resultSound);

    private:
        float mAgility, mLuck, mSecuritySkill, mFatigueTerm;
        OFWorld::Ptr mActor;
    };

}

#endif
