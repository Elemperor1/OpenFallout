#ifndef OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_UPDATEGUARD_H
#define OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_UPDATEGUARD_H

#include <memory>
#include <mutex>

namespace DetourNavigator
{
    // A non-owning mutex token. TileCachedRecastMeshManager owns one bound to its mutex and returns its address from
    // makeUpdateGuard. ScopedUpdateGuard points to that manager-owned token; its deleter unlocks the referenced mutex
    // and does not delete the token. Keep the manager alive, use the token only with that manager, and destroy the
    // scoped handle on the locking thread.
    class UpdateGuard
    {
    public:
        explicit UpdateGuard(std::mutex& mutex)
            : mMutex(mutex)
        {
        }

    private:
        std::mutex& mMutex;

        friend struct UnlockUpdateGuard;
    };

    struct UnlockUpdateGuard
    {
        void operator()(UpdateGuard* value) const { value->mMutex.unlock(); }
    };

    using ScopedUpdateGuard = std::unique_ptr<UpdateGuard, UnlockUpdateGuard>;
}

#endif
