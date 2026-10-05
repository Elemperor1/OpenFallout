#ifndef GAME_MWMECHANICS_MECHANICSMANAGERIMP_H
#define GAME_MWMECHANICS_MECHANICSMANAGERIMP_H

#include <components/settings/settings.hpp>

#include "../mwbase/mechanicsmanager.hpp"

#include "../mwworld/ptr.hpp"

#include "actors.hpp"
#include "npcstats.hpp"
#include "objects.hpp"

namespace OFSound
{
    enum class MusicType;
}

namespace OFWorld
{
    class CellStore;
}

namespace OFMechanics
{
    class MechanicsManager : public OFBase::MechanicsManager
    {
        bool mUpdatePlayer;
        bool mClassSelected;
        bool mRaceSelected;
        bool mAI; ///< is AI active?

        Objects mObjects;
        Actors mActors;

        typedef std::pair<ESM::RefId, bool> Owner; // < Owner id, bool isFaction >
        typedef std::map<Owner, int> OwnerMap; // < Owner, number of stolen items with this id from this owner >
        typedef std::map<ESM::RefId, OwnerMap> StolenItemsMap;
        StolenItemsMap mStolenItems;

    public:
        void buildPlayer();
        ///< build player according to stored class/race/birthsign information. Will
        /// default to the values of the ESM::NPC object, if no explicit information is given.

        MechanicsManager();

        void add(const OFWorld::Ptr& ptr) override;
        ///< Register an object for management

        void remove(const OFWorld::Ptr& ptr, bool keepActive) override;
        ///< Deregister an object for management

        void updateCell(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr) override;
        ///< Moves an object to a new cell

        void drop(const OFWorld::CellStore* cellStore) override;
        ///< Deregister all objects in the given cell.

        void update(float duration, bool paused);
        ///< Update objects
        ///
        /// \param paused In game type does not currently advance (this usually means some GUI
        /// component is up).

        void setPlayerName(const std::string& name) override;
        ///< Set player name.

        void setPlayerRace(const ESM::RefId& id, bool male, const ESM::RefId& head, const ESM::RefId& hair) override;
        ///< Set player race.

        void setPlayerBirthsign(const ESM::RefId& id) override;
        ///< Set player birthsign.

        void setPlayerClass(const ESM::RefId& id) override;
        ///< Set player class to stock class.

        void setPlayerClass(const ESM::Class& value) override;
        ///< Set player class to custom class.

        void restoreDynamicStats(const OFWorld::Ptr& actor, double hours, bool sleep) override;

        void rest(double hours, bool sleep) override;
        ///< If the player is sleeping or waiting, this should be called every hour.
        /// @param sleep is the player sleeping or waiting?

        int getHoursToRest() const override;
        ///< Calculate how many hours the player needs to rest in order to be fully healed

        int getBarterOffer(const OFWorld::Ptr& ptr, int basePrice, bool buying) override;
        ///< This is used by every service to determine the price of objects given the trading skills of the player and
        ///< NPC.

        int getDerivedDisposition(const OFWorld::Ptr& ptr, bool clamp = true) override;
        ///< Calculate the diposition of an NPC toward the player.

        int countDeaths(const ESM::RefId& id) const override;
        ///< Return the number of deaths for actors with the given ID.

        void getPersuasionDispositionChange(
            const OFWorld::Ptr& npc, PersuasionType type, bool& success, int& tempChange, int& permChange) override;
        ///< Perform a persuasion action on NPC

        /// Check if \a observer is potentially aware of \a ptr. Does not do a line of sight check!
        bool awarenessCheck(const OFWorld::Ptr& ptr, const OFWorld::Ptr& observer, bool useCache = true) override;

        /// Makes \a ptr fight \a target. Also shouts a combat taunt.
        void startCombat(
            const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, const std::set<OFWorld::Ptr>* targetAllies) override;

        void stopCombat(const OFWorld::Ptr& ptr) override;

        /**
         * @note victim may be empty
         * @param arg Depends on \a type, e.g. for Theft, the value of the item that was stolen.
         * @param victimAware Is the victim already aware of the crime?
         *                    If this parameter is false, it will be determined by a line-of-sight and awareness check.
         * @return was the crime seen?
         */
        bool commitCrime(const OFWorld::Ptr& ptr, const OFWorld::Ptr& victim, OffenseType type,
            const ESM::RefId& factionId = ESM::RefId(), int arg = 0, bool victimAware = false) override;
        /// @return false if the attack was considered a "friendly hit" and forgiven
        bool actorAttacked(const OFWorld::Ptr& victim, const OFWorld::Ptr& attacker) override;

        /// Notify that actor was killed, add a murder bounty if applicable
        /// @note No-op for non-player attackers
        void actorKilled(const OFWorld::Ptr& victim, const OFWorld::Ptr& attacker) override;

        /// Utility to check if taking this item is illegal and calling commitCrime if so
        /// @param container The container the item is in; may be empty for an item in the world
        void itemTaken(const OFWorld::Ptr& ptr, const OFWorld::Ptr& item, const OFWorld::Ptr& container, int count,
            bool alarm = true) override;
        /// Utility to check if unlocking this object is illegal and calling commitCrime if so
        void unlockAttempted(const OFWorld::Ptr& ptr, const OFWorld::Ptr& item) override;
        /// Attempt sleeping in a bed. If this is illegal, call commitCrime.
        /// @return was it illegal, and someone saw you doing it? Also returns fail when enemies are nearby
        bool sleepInBed(const OFWorld::Ptr& ptr, const OFWorld::Ptr& bed) override;

        void forceStateUpdate(const OFWorld::Ptr& ptr) override;

        /// Attempt to play an animation group
        /// @return Success or error
        bool playAnimationGroup(const OFWorld::Ptr& ptr, std::string_view groupName, int mode, uint32_t number,
            bool scripted = false) override;
        bool playAnimationGroupLua(const OFWorld::Ptr& ptr, std::string_view groupName, uint32_t loops, float speed,
            std::string_view startKey, std::string_view stopKey, bool forceLoop) override;
        void enableLuaAnimations(const OFWorld::Ptr& ptr, bool enable) override;
        void skipAnimation(const OFWorld::Ptr& ptr) override;
        bool checkAnimationPlaying(const OFWorld::Ptr& ptr, std::string_view groupName) override;
        bool checkScriptedAnimationPlaying(const OFWorld::Ptr& ptr) const override;
        void persistAnimationStates() override;
        void clearAnimationQueue(const OFWorld::Ptr& ptr, bool clearScripted) override;

        /// Update magic effects for an actor. Usually done automatically once per frame, but if we're currently
        /// paused we may want to do it manually (after equipping permanent enchantment)
        void updateMagicEffects(const OFWorld::Ptr& ptr) override;

        void getObjectsInRange(const osg::Vec3f& position, float radius, std::vector<OFWorld::Ptr>& objects) override;
        void getActorsInRange(const osg::Vec3f& position, float radius, std::vector<OFWorld::Ptr>& objects) override;

        /// Check if there are actors in selected range
        bool isAnyActorInRange(const osg::Vec3f& position, float radius) override;

        std::vector<OFWorld::Ptr> getActorsSidingWith(const OFWorld::Ptr& actor) override;
        std::vector<OFWorld::Ptr> getActorsFollowing(const OFWorld::Ptr& actor) override;
        std::vector<int> getActorsFollowingIndices(const OFWorld::Ptr& actor) override;
        std::map<int, OFWorld::Ptr> getActorsFollowingByIndex(const OFWorld::Ptr& actor) override;

        std::vector<OFWorld::Ptr> getActorsFighting(const OFWorld::Ptr& actor) override;
        std::vector<OFWorld::Ptr> getEnemiesNearby(const OFWorld::Ptr& actor) override;

        /// Recursive version of getActorsFollowing
        void getActorsFollowing(const OFWorld::Ptr& actor, std::set<OFWorld::Ptr>& out) override;
        /// Recursive version of getActorsSidingWith
        void getActorsSidingWith(const OFWorld::Ptr& actor, std::set<OFWorld::Ptr>& out) override;

        bool toggleAI() override;
        bool isAIActive() override;

        void playerLoaded() override;

        bool onOpen(const OFWorld::Ptr& ptr) override;
        void onClose(const OFWorld::Ptr& ptr) override;

        size_t countSavedGameRecords() const override;

        void write(ESM::ESMWriter& writer, Loading::Listener& listener) const override;

        void readRecord(ESM::ESMReader& reader, uint32_t type) override;

        void clear() override;

        void resurrect(const OFWorld::Ptr& ptr) override;

        bool isCastingSpell(const OFWorld::Ptr& ptr) const override;

        bool isReadyToBlock(const OFWorld::Ptr& ptr) const override;
        /// Is \a ptr casting spell or using weapon now?
        bool isAttackingOrSpell(const OFWorld::Ptr& ptr) const override;

        void castSpell(const OFWorld::Ptr& ptr, const ESM::RefId& spellId, bool scriptedSpell = false) override;

        void processChangedSettings(const Settings::CategorySettingVector& settings) override;

        void notifyDied(const OFWorld::Ptr& actor) override;

        /// Check if the target actor was detected by an observer
        /// If the observer is a non-NPC, check all actors in AI processing distance as observers
        bool isActorDetected(const OFWorld::Ptr& actor, const OFWorld::Ptr& observer) override;

        void confiscateStolenItems(const OFWorld::Ptr& player, const OFWorld::Ptr& targetContainer) override;

        /// List the owners that the player has stolen this item from (the owner can be an NPC or a faction).
        /// <Owner, item count>
        std::vector<std::pair<ESM::RefId, int>> getStolenItemOwners(const ESM::RefId& itemid) override;

        /// Has the player stolen this item from the given owner?
        bool isItemStolenFrom(const ESM::RefId& itemid, const OFWorld::Ptr& ptr) override;

        bool isBoundItem(const OFWorld::Ptr& item) override;

        /// @return is \a ptr allowed to take/use \a target or is it a crime?
        bool isAllowedToUse(const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, OFWorld::Ptr& victim) override;

        void setWerewolf(const OFWorld::Ptr& actor, bool werewolf) override;
        void applyWerewolfAcrobatics(const OFWorld::Ptr& actor) override;

        void cleanupSummonedCreature(ESM::RefNum creature) override;

        void confiscateStolenItemToOwner(
            const OFWorld::Ptr& player, const OFWorld::Ptr& item, const OFWorld::Ptr& victim, int count) override;

        bool isAttackPreparing(const OFWorld::Ptr& ptr) override;
        bool isRunning(const OFWorld::Ptr& ptr) override;
        bool isSneaking(const OFWorld::Ptr& ptr) override;

        void reportStats(unsigned int frameNumber, osg::Stats& stats) const override;

        int getGreetingTimer(const OFWorld::Ptr& ptr) const override;
        float getAngleToPlayer(const OFWorld::Ptr& ptr) const override;
        GreetingState getGreetingState(const OFWorld::Ptr& ptr) const override;
        void fastForwardAi() const override;

    private:
        bool canCommitCrimeAgainst(const OFWorld::Ptr& victim, const OFWorld::Ptr& attacker);
        bool canReportCrime(
            const OFWorld::Ptr& actor, const OFWorld::Ptr& victim, std::set<OFWorld::Ptr>& playerFollowers);

        bool reportCrime(const OFWorld::Ptr& ptr, const OFWorld::Ptr& victim, OffenseType type,
            const ESM::RefId& factionId, int arg = 0);
    };
}

#endif
