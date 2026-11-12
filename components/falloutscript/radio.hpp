#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_RADIO_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_RADIO_H

#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <vector>

#include "savestate.hpp"

namespace FalloutScript::Radio
{
    /// The radio of the games: a station is a talking activator (TACT with the radio flag) that is placed in the world with
    /// a reach (the subrecord XRDO of its reference). What it says is the dialogue of the topic RadioHello and the topics it
    /// leads to, said by the station as a speaker; this file holds the rules of who hears which station.
    ///
    /// The values of the settings are the games' own (FalloutNV.exe, the [Audio] section: iRadioUpdateInterval 250 and
    /// fDefaultRadioVolume 0.5 and fRadioDialogMute 0.5 of Fallout_default.ini, iRadioStationTimeout 1000 and
    /// fRadioStaticAtOuterRadiusPct 0.1 from the executable's own defaults)
    struct Settings
    {
        /// How often the stations are looked at, in seconds (iRadioUpdateInterval, milliseconds in the games)
        float mUpdateInterval = 0.25f;
        /// How long a station that is out of reach stays tuned and discovered before the signal is lost, in seconds
        /// (iRadioStationTimeout, milliseconds)
        float mStationTimeout = 1.f;
        /// The share of static at the outer radius of a station that names none of its own (fRadioStaticAtOuterRadiusPct)
        float mStaticAtOuterRadius = 0.1f;
        /// The volume of the radio (fDefaultRadioVolume)
        float mVolume = 0.5f;
        /// What the radio is multiplied by while somebody speaks to the player (fRadioDialogMute)
        float mDialogMute = 0.5f;
    };

    /// How far a station is heard (the broadcast range of XRDO)
    enum class Reach : std::uint32_t
    {
        Radius = 0, // from the station (or from the reference it names) out to the radius, in exteriors only
        Everywhere = 1,
        Worldspace = 2, // the worldspace of the station and the interiors that are linked to it
        LinkedInteriors = 3, // the cell of the station and the interiors that are linked to it
        CurrentCell = 4,
    };

    /// The interiors that the doors of the world join to each other, and the worldspaces those doors lead out to. The
    /// reaches that name linked interiors are read from it: an interior is linked to the interiors that can be walked to
    /// from it without going out of doors, and to the worldspaces that the doors of those interiors open on. (`Id`
    /// names a cell or a worldspace, and is ordered.)
    template <class Id>
    class Links
    {
    public:
        struct Group
        {
            /// The interior itself and those that are linked to it
            std::set<Id> mInteriors;
            /// The worldspaces that a door of one of them opens on
            std::set<Id> mWorldspaces;
        };

        /// A door of an interior that leads into another interior (the doors of the games go both ways: each has its own)
        void addDoor(const Id& interior, const Id& other)
        {
            mDoors[interior].insert(other);
            mDoors[other].insert(interior);
        }

        /// A door of an interior that leads out into a worldspace
        void addExit(const Id& interior, const Id& worldspace) { mExits[interior].insert(worldspace); }

        Group groupOf(const Id& interior) const
        {
            Group group;
            std::vector<Id> open{ interior };
            group.mInteriors.insert(interior);
            while (!open.empty())
            {
                const Id cell = open.back();
                open.pop_back();
                if (const auto exits = mExits.find(cell); exits != mExits.end())
                    group.mWorldspaces.insert(exits->second.begin(), exits->second.end());
                const auto doors = mDoors.find(cell);
                if (doors == mDoors.end())
                    continue;
                for (const Id& other : doors->second)
                    if (group.mInteriors.insert(other).second)
                        open.push_back(other);
            }
            return group;
        }

        bool empty() const { return mDoors.empty() && mExits.empty(); }

    private:
        std::map<Id, std::set<Id>> mDoors;
        std::map<Id, std::set<Id>> mExits;
    };

    /// Where the player is, in relation to the station
    struct Where
    {
        bool mExterior = true;
        /// Out of doors: in the worldspace of the station. Indoors: in an interior that is linked to that worldspace
        bool mSameWorldspace = false;
        bool mSameCell = false;
        /// Indoors: in an interior that is linked to the (interior) cell of the station
        bool mLinkedInterior = false;
        /// The distance from the player to the place the radius is measured from (the station, or the reference that it
        /// names), or a negative number when it cannot be measured (the other cell is not in the player's worldspace)
        float mDistance = -1.f;
    };

    struct Signal
    {
        bool mInRange = false;
        /// 1 at the station, falling to 0 at the outer radius (1 where the range has no radius)
        float mStrength = 0.f;
        /// The share of static in what is heard: none at the station and the station's own share at the outer radius
        float mStatic = 0.f;
    };

    /// Whether a station is heard and how well. `staticPercent` is the station's own share of static at its outer radius
    /// (a percentage, 0 to 100; with none the setting's share is taken). A range that is not the radius has no static.
    Signal signalOf(Reach reach, float radius, float staticPercent, const Where& where, const Settings& settings);

    /// How loud the voice of a station is: the volume of the radio, less the static, and quieter while somebody speaks to
    /// the player
    float gainOf(const Signal& signal, bool dialogue, const Settings& settings);

    /// What the radio keeps in a save: the stations that scripts turned on or off (SetBroadcastState) and those that the
    /// player found
    struct Saved
    {
        std::map<std::uint32_t, bool> mBroadcast;
        std::set<std::uint32_t> mDiscovered;
    };
    constexpr std::uint32_t savedSectionVersion = 1;
    void writeSaved(const Saved& saved, SaveWriter& out);
    /// A station whose plugin is no longer loaded is dropped
    void readSaved(Saved& saved, SaveReader& in, std::uint32_t version);

    /// A station as the receiver is told of it in an update
    struct Sighting
    {
        std::uint32_t mStation = 0;
        Signal mSignal;
    };

    struct Event
    {
        enum class Kind
        {
            Discovered, // in reach for the first time
            Found, // in reach again after the signal was lost
            Lost, // out of reach for the length of the timeout
        };
        Kind mKind = Kind::Found;
        std::uint32_t mStation = 0;
    };

    /// The radio of the Pip-Boy: which stations are in reach, which the player has found, and which one is tuned
    class Receiver
    {
    public:
        explicit Receiver(Settings settings = {})
            : mSettings(settings)
        {
        }

        /// Called with all the stations that are looked at (every update interval). A station that is not told of is out of
        /// reach. Returns what changed.
        std::vector<Event> update(float seconds, std::span<const Sighting> sightings);

        /// Tunes the station. Only one that is in reach can be: it returns whether it is tuned now
        bool tune(std::uint32_t station);
        void untune() { mTuned = 0; }
        std::uint32_t tuned() const { return mTuned; }

        bool inReach(std::uint32_t station) const { return mReach.count(station) != 0; }
        bool discovered(std::uint32_t station) const { return mDiscovered.count(station) != 0; }
        /// The signal of a station that is in reach (not in range: none)
        Signal signal(std::uint32_t station) const;
        /// The stations that are in reach, in the order of their numbers
        std::vector<std::uint32_t> stations() const;
        const Settings& settings() const { return mSettings; }

        /// The stations that were found are forgotten (a new game, a load)
        void reset();
        /// A station was discovered in an earlier game of the save
        void markDiscovered(std::uint32_t station) { mDiscovered.insert(station); }
        const std::set<std::uint32_t>& discoveredStations() const { return mDiscovered; }

    private:
        struct Held
        {
            Signal mSignal;
            float mOut = 0.f; // seconds out of range
            bool mLost = false; // the loss was told
        };

        Settings mSettings;
        std::map<std::uint32_t, Held> mReach;
        std::set<std::uint32_t> mDiscovered;
        std::uint32_t mTuned = 0;
    };
}

#endif
