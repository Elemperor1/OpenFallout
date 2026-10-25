#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_AIMEMORY_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_AIMEMORY_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "interpreter.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// What the AI of a character keeps that a saved game holds: which of its packages it has done, the one it
    /// follows, and the order of a script to follow somebody. The records are told by their form ids.
    struct AiState
    {
        /// The packages it has done and has not begun again since
        std::vector<FormId> mDone;
        /// The package it follows, 0 for none
        FormId mPackage = 0;
        /// A package that is done and has nothing after it is over, and its Change script has been told
        bool mChangeTold = false;
        /// A script told it to follow, the player or a reference, keeping within the distance (0: the usual one)
        bool mFollows = false;
        bool mFollowsPlayer = false;
        FormId mFollowTarget = 0;
        float mFollowDistance = 0.f;

        /// Whether there is nothing to keep
        bool empty() const;
        bool operator==(const AiState& other) const;
    };

    /// The AI state of the characters, by the reference of the character. It outlives the cell the character is in
    /// and goes into a saved game. It can be used from any thread.
    class AiMemory
    {
    public:
        /// Forgets everything, as for a new game
        void clear();

        /// Keeps the state of a character; a state with nothing to keep makes it forgotten
        void set(FormId actor, AiState state);
        std::optional<AiState> find(FormId actor) const;
        std::size_t size() const;

        /// Every state in order of the reference, for a saved game
        std::vector<std::pair<FormId, AiState>> all() const;

    private:
        mutable std::mutex mMutex;
        std::map<FormId, AiState> mStates;
    };

    /// The section of a saved game that holds the AI state
    constexpr std::uint32_t aiSectionVersion = 1;
    void writeAi(const AiMemory& memory, SaveWriter& out);
    /// A character or a package whose plugin is no longer loaded is left out
    void readAi(AiMemory& memory, SaveReader& in, std::uint32_t version);
}

#endif
