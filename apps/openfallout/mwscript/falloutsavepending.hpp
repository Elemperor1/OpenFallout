#ifndef OPENFALLOUT_SCRIPT_FALLOUTSAVEPENDING_H
#define OPENFALLOUT_SCRIPT_FALLOUTSAVEPENDING_H

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/falloutscript/script.hpp>

#include <osg/Vec3f>

namespace OFScript
{
    /// What a loaded game says that waits for a cell to be loaded or for the next frame, as the world is not ready for
    /// it while the records of the save are read. It is empty for a game that was not loaded.
    struct PendingSave
    {
        /// What the world does to a reference of a content file once its cell is loaded (where it is and whether it is
        /// enabled are the host's maps of the objects that scripts moved and disabled in cells that were not loaded)
        struct Reference
        {
            bool mDeleted = false;
            std::optional<float> mScale;
        };
        std::map<FalloutScript::FormId, Reference> mReferences;

        /// An object that was made while the game ran (by a script or by the player), which is made again
        struct Object
        {
            /// The number scripts know it by, 0 for an object that no script was told of
            FalloutScript::FormId mId = 0;
            FalloutScript::FormId mBase = 0;
            ESM::RefId mCell;
            osg::Vec3f mPosition;
            osg::Vec3f mAngle;
            bool mEnabled = true;
            int mCount = 1;
            float mScale = 1;
        };
        std::vector<Object> mObjects;

        /// Who stood in the volume of each trigger, until the cell of the trigger is loaded and its volume is put in
        std::map<FalloutScript::FormId, std::set<FalloutScript::FormId>> mOccupants;

        /// The references of loaded cells that the next frame deletes or scales
        std::vector<FalloutScript::FormId> mToDelete;
        std::vector<std::pair<FalloutScript::FormId, float>> mToScale;
    };
}

#endif
