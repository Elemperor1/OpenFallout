#ifndef OPENFALLOUT_MWSCRIPT_FALLOUTCHALLENGES_H
#define OPENFALLOUT_MWSCRIPT_FALLOUTCHALLENGES_H

#include <components/falloutscript/challenges.hpp>
#include <components/falloutscript/formlists.hpp>

namespace OFWorld
{
    class ESMStore;
}

namespace OFScript
{
    /// What the challenge records (CHAL) of the loaded plugins say, and which forms the lists they name hold
    class ChallengeRecordSource final : public FalloutScript::ChallengeRecords
    {
    public:
        ChallengeRecordSource(const OFWorld::ESMStore& store, FalloutScript::FormLists& lists)
            : mStore(store)
            , mLists(lists)
        {
        }

        void forEachChallenge(const std::function<void(const FalloutScript::ChallengeRecord&)>& function) override;
        bool inList(FalloutScript::FormId list, FalloutScript::FormId form) override;

    private:
        const OFWorld::ESMStore& mStore;
        FalloutScript::FormLists& mLists;
    };
}

#endif
