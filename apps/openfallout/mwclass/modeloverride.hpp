#ifndef OPENFALLOUT_MWCLASS_MODELOVERRIDE_H
#define OPENFALLOUT_MWCLASS_MODELOVERRIDE_H

#include <deque>
#include <map>
#include <mutex>
#include <string_view>

#include <components/esm/formid.hpp>
#include <components/vfs/pathutil.hpp>

namespace OFClass
{
    /// The models that take the place of those of the records for single references: what a destructible object shows
    /// once it has been damaged enough (the model of its stage). The models are looked up by the reference's number.
    /// A model that was given stays where it is for the life of the program, so that a view of it is never left behind by
    /// a later change.
    class ModelOverrides
    {
    public:
        static ModelOverrides& get()
        {
            static ModelOverrides sOverrides;
            return sOverrides;
        }

        /// The model of the reference, or none when it shows its record's. Valid for as long as the program runs.
        const VFS::Path::Normalized* find(const ESM::FormId& reference)
        {
            const std::lock_guard lock(mMutex);
            const auto found = mModels.find(reference);
            return found == mModels.end() ? nullptr : found->second;
        }

        /// Gives the reference the model (empty for its own again). True when that changed what it shows.
        bool set(const ESM::FormId& reference, std::string_view model)
        {
            const std::lock_guard lock(mMutex);
            const auto found = mModels.find(reference);
            if (model.empty())
            {
                if (found == mModels.end())
                    return false;
                mModels.erase(found);
                return true;
            }
            const VFS::Path::Normalized wanted{ model };
            if (found != mModels.end() && found->second->value() == wanted.value())
                return false;
            mPool.push_back(wanted);
            mModels[reference] = &mPool.back();
            return true;
        }

        void clear()
        {
            const std::lock_guard lock(mMutex);
            mModels.clear();
        }

    private:
        std::mutex mMutex;
        std::deque<VFS::Path::Normalized> mPool;
        std::map<ESM::FormId, const VFS::Path::Normalized*> mModels;
    };
}

#endif
