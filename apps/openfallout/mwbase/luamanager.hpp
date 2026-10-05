#ifndef GAME_MWBASE_LUAMANAGER_H
#define GAME_MWBASE_LUAMANAGER_H

#include <filesystem>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include <SDL_events.h>
#include <osg/Quat>
#include <osg/Vec3f>

#include <components/esm3/refnum.hpp>
#include <components/sdlutil/events.hpp>

#include "../mwmechanics/attacktype.hpp"
#include "../mwmechanics/damagesourcetype.hpp"
#include "../mwrender/animationpriority.hpp"

namespace OFWorld
{
    class CellStore;
    class Ptr;
}

namespace Loading
{
    class Listener;
}

namespace ESM
{
    class ESMReader;
    class ESMWriter;
    class RefId;
    struct LuaScripts;
    struct DialInfo;
    struct Dialogue;
}

namespace LuaUtil
{
    namespace InputAction
    {
        class Registry;
    }
}

namespace osg
{
    class Vec3f;
}

namespace OFBase
{
    // \brief LuaManager is the central interface through which the engine invokes lua scripts.
    //
    // The native side invokes functions on this interface, which queues events to be handled by the
    // scripts in the lua thread. Synchronous calls are not possible.
    //
    // The main implementation is in apps/openfallout/mwlua/luamanagerimp.cpp.
    // Lua logic in general lives under apps/openfallout/mwlua and this interface is
    // the main way for the rest of the engine to interact with the logic there.
    class LuaManager
    {
    public:
        virtual ~LuaManager() = default;

        virtual void contentFilesLoaded() = 0;
        virtual void newGameStarted() = 0;
        virtual void gameLoaded() = 0;
        virtual void gameEnded() = 0;
        virtual void noGame() = 0;
        virtual void objectAddedToScene(const OFWorld::Ptr& ptr) = 0;
        virtual void objectRemovedFromScene(const OFWorld::Ptr& ptr) = 0;
        virtual void objectTeleported(const OFWorld::Ptr& ptr) = 0;
        virtual void itemConsumed(const OFWorld::Ptr& consumable, const OFWorld::Ptr& actor) = 0;
        virtual void objectDropped(const OFWorld::Ptr& object, const OFWorld::Ptr& actor, const osg::Vec3f& position,
            const osg::Quat& rotation)
            = 0;
        virtual void objectPlaced(const OFWorld::Ptr& object, const OFWorld::Ptr& actor, const osg::Vec3f& position,
            const osg::Quat& rotation)
            = 0;
        virtual void objectActivated(const OFWorld::Ptr& object, const OFWorld::Ptr& actor) = 0;
        virtual void useItem(const OFWorld::Ptr& object, const OFWorld::Ptr& actor, bool force) = 0;
        virtual void animationTextKey(const OFWorld::Ptr& actor, const std::string& key) = 0;
        virtual void playAnimation(const OFWorld::Ptr& object, const std::string& groupname,
            const OFRender::AnimPriority& priority, int blendMask, bool autodisable, float speedmult,
            std::string_view start, std::string_view stop, float startpoint, uint32_t loops, bool loopfallback)
            = 0;
        virtual void animationEnded(const OFWorld::Ptr& actor, std::string_view groupname, float time, float completion,
            std::string_view startKey, std::string_view stopKey)
            = 0;
        virtual void jailTimeServed(const OFWorld::Ptr& actor, int days) = 0;
        virtual void skillLevelUp(const OFWorld::Ptr& actor, ESM::RefId skillId, std::string_view source) = 0;
        virtual void skillUse(const OFWorld::Ptr& actor, ESM::RefId skillId, int useType, float scale) = 0;
        virtual void onHit(const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim, const OFWorld::Ptr& weapon,
            const OFWorld::Ptr& ammo, int attackType, float attackStrength, float attackWindUp, float damage,
            bool isHealth, const osg::Vec3f& hitPos, bool successful, OFMechanics::DamageSourceType)
            = 0;
        virtual void exteriorCreated(OFWorld::CellStore& cell) = 0;
        virtual void actorDied(const OFWorld::Ptr& actor) = 0;
        virtual void onDialogueResponse(
            const OFWorld::Ptr& actor, const ESM::DialInfo& info, const ESM::Dialogue& record)
            = 0;
        virtual void questUpdated(const ESM::RefId& questId, int stage) = 0;
        // `arg` is either forwarded from OFGui::pushGuiMode or empty
        virtual void uiModeChanged(const OFWorld::Ptr& arg) = 0;
        virtual void viewportResized(int width, int height) = 0;
        virtual void savePermanentStorage(const std::filesystem::path& userConfigPath) = 0;
        virtual void applyMagicEffects(ESM::RefId id, const OFWorld::Ptr& caster, ESM::RefNum item,
            const OFWorld::Ptr& target, const std::vector<int>& effects, bool ignoreReflect, bool ignoreSpellAbsorption,
            bool stackable, bool isReflect)
            = 0;
        virtual void magicProjectileHit(ESM::RefId spellId, const OFWorld::Ptr& caster, ESM::RefNum item,
            const OFWorld::Ptr& victim, const osg::Vec3f& position, const osg::Vec3f& normal)
            = 0;
        // TODO: notify LuaManager about other events
        // virtual void objectOnHit(const OFWorld::Ptr &ptr, float damage, bool ishealth, const OFWorld::Ptr &object,
        //                          const OFWorld::Ptr &attacker, const osg::Vec3f &hitPosition, bool successful,
        //                          DamageSourceType sourceType) = 0;

        struct InputEvent
        {
            struct WheelChange
            {
                int x;
                int y;
            };

            enum
            {
                KeyPressed,
                KeyReleased,
                ControllerPressed,
                ControllerReleased,
                Action,
                TouchPressed,
                TouchReleased,
                TouchMoved,
                MouseButtonPressed,
                MouseButtonReleased,
                MouseWheel,
            } mType;
            std::variant<SDL_Keysym, int, SDLUtil::TouchEvent, WheelChange> mValue;
        };
        virtual void inputEvent(const InputEvent& event) = 0;

        struct ActorControls
        {
            bool mDisableAI = false;
            bool mChanged = false;

            bool mJump = false;
            bool mRun = false;
            bool mSneak = false;
            float mMovement = 0;
            float mSideMovement = 0;
            float mPitchChange = 0;
            float mYawChange = 0;
            OFMechanics::AttackType mUse = OFMechanics::AttackType::NoAttack;
        };

        virtual ActorControls* getActorControls(const OFWorld::Ptr&) const = 0;

        virtual void clear() = 0;
        virtual void setupPlayer(const OFWorld::Ptr&) = 0;

        // Saving
        size_t countSavedGameRecords() const { return 1; }
        virtual void write(ESM::ESMWriter& writer, Loading::Listener& progress) = 0;
        virtual void saveLocalScripts(const OFWorld::Ptr& ptr, ESM::LuaScripts& data) = 0;

        // Must be called before save, otherwise the world can be saved in an inconsistent state.
        virtual void applyDelayedActions() = 0;

        // Loading from a save
        virtual void readRecord(ESM::ESMReader& reader, uint32_t type) = 0;
        virtual void loadLocalScripts(const OFWorld::Ptr& ptr, const ESM::LuaScripts& data) = 0;

        // Should be called before loading. The map is used to fix refnums if the order of content files was changed.
        virtual void setContentFileMapping(const std::map<int, int>&) = 0;

        // Drops script cache and reloads all scripts. Calls `onSave` and `onLoad` for every script.
        virtual void reloadAllScripts() = 0;

        virtual void handleConsoleCommand(
            const std::string& consoleMode, const std::string& command, const OFWorld::Ptr& selectedPtr)
            = 0;

        virtual std::string formatResourceUsageStats() const = 0;
    };

}

#endif // GAME_MWBASE_LUAMANAGER_H
