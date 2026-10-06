#ifndef OPENFALLOUT_MWWORLD_WORLDSPACESTART_H
#define OPENFALLOUT_MWWORLD_WORLDSPACESTART_H

#include <optional>
#include <string_view>

namespace OFWorld
{
    /// An exterior cell of a worldspace of a Fallout game, named by the editor id of the worldspace and the grid
    /// coordinates of the cell.
    struct WorldspaceStart
    {
        std::string_view mWorldspace;
        int mX = 0;
        int mY = 0;
    };

    /// Read "WastelandNV:-2,-3": the editor id of a worldspace, a colon and the two grid coordinates of an exterior
    /// cell separated by a comma. Returns nothing for any other text, including a cell name that has no coordinates, so
    /// the caller can treat it as one. Throws std::runtime_error when the coordinates do not fit in an int.
    std::optional<WorldspaceStart> parseWorldspaceStart(std::string_view text);
}

#endif
