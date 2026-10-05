#ifndef GAME_MWMECHANICS_AIBREATHE_H
#define GAME_MWMECHANICS_AIBREATHE_H

#include "typedaipackage.hpp"

namespace OFMechanics
{
    /// \brief AiPackage to have an actor resurface to breathe
    // The AI will go up if lesser than half breath left
    class AiBreathe final : public TypedAiPackage<AiBreathe>
    {
    public:
        bool execute(const OFWorld::Ptr& actor, CharacterController& characterController, AiState& state,
            float duration) override;

        static constexpr AiPackageTypeId getTypeId() { return AiPackageTypeId::Breathe; }

        static constexpr Options makeDefaultOptions()
        {
            AiPackage::Options options;
            options.mPriority = 2;
            options.mCanCancel = false;
            options.mShouldCancelPreviousAi = false;
            return options;
        }
    };
}
#endif
