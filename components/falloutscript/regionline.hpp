#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REGIONLINE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REGIONLINE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "script.hpp"

namespace FalloutScript::RegionLine
{
    /// A region of the cell the player has entered that covers the place of the player, in the order of the cell's list
    struct Candidate
    {
        FormId mRegion = 0;
        /// The weather entry of the region: whether it has one, its override byte and its priority
        bool mHasWeather = false;
        bool mOverride = false;
        int mPriority = 0;
        /// The name of the region on the map (empty for none)
        std::string mName;
    };

    /// The one region that is the player's out of the ones that cover it (FalloutNV.exe 0x94CAE0): the first of the
    /// list, which a later one replaces only when it has a weather entry and the first has none, or when its override
    /// is set and the first's is not, or when the two overrides are the same and its priority is higher. The place in
    /// the list, nothing for an empty list.
    std::optional<std::size_t> pick(const std::vector<Candidate>& covering);

    /// The region line of the display: the name of the region the player is in, shown when the player comes to another
    /// (FalloutNV.exe 0x7728F0). It keeps the region of the player and the region it showed last.
    class Announcer
    {
    public:
        /// The player is in a cell (a new one, or the first after a game was begun or loaded). `exterior` says it is
        /// not a building (in a building the player's region is left as it was and nothing is shown), `covering` the
        /// regions of the cell that cover the place of the player. The name to show, when the region is another than
        /// the one that was shown last and has a name; nothing otherwise. A region that has no name, or none at all, is
        /// forgotten as the last shown, so that coming back to the one before shows it again.
        std::optional<std::string> enter(bool exterior, const std::vector<Candidate>& covering);

        /// A game was begun or loaded
        void reset();

        FormId region() const { return mRegion; }

    private:
        FormId mRegion = 0;
        FormId mLast = 0;
    };
}

#endif
