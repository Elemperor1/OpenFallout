#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MAPMARKERS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MAPMARKERS_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>

#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// What the records of the game say about map markers
    class MapMarkerRecords
    {
    public:
        virtual ~MapMarkerRecords() = default;

        /// The flags the record of a map marker gives it, 0x01 for visible and 0x02 for one that can be travelled to.
        /// False for a reference that is no map marker.
        virtual bool markerFlags(FormId marker, std::uint8_t& flags) = 0;
    };

    /// The markers of the world map that scripts uncover, and the switches of fast travel. A marker is what its record
    /// says until a script shows it. It can be used from any thread.
    class MapMarkers
    {
        template <class T>
        friend struct SaveAccess;

    public:
        static constexpr std::uint8_t visibleFlag = 0x01;
        static constexpr std::uint8_t travelFlag = 0x02;

        explicit MapMarkers(MapMarkerRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        /// Whether the reference is a map marker
        bool isMarker(FormId marker);
        /// 0 for a marker that is not shown (or a reference that is none), 1 for a shown one and 2 for a shown one
        /// that can be travelled to
        int state(FormId marker);
        /// Shows a marker, and makes it a place to travel to when that is asked for. True when that changed it, which
        /// is when the player is told. False for a reference that is no marker.
        bool show(FormId marker, bool withTravel);

        /// The switches of EnableFastTravel. Fast travel is allowed in a new game.
        void enableFastTravel(bool enable, bool second, bool third);
        /// Whether scripts allow fast travel (the first switch)
        bool fastTravelAllowed();
        /// The second switch, which the games read when the player rests or waits
        bool restAllowed();
        /// The bit that the third switch sets
        bool travelSwitch();

    private:
        struct Switches
        {
            bool mAllowed = true;
            bool mRest = true;
            bool mThird = false;
        };

        MapMarkerRecords& mRecords;
        std::mutex mMutex;
        /// The flags of the markers that scripts changed
        std::map<FormId, std::uint8_t> mFlags;
        Switches mSwitches;

        std::uint8_t flagsLocked(FormId marker, bool& known);
    };

    /// The section of a saved game that holds the markers scripts uncovered and the switches
    constexpr std::uint32_t mapMarkersSectionVersion = 1;
    void writeMapMarkers(MapMarkers& markers, SaveWriter& out);
    void readMapMarkers(MapMarkers& markers, SaveReader& in, std::uint32_t version);

    /// Gives the commands ShowMap, GetMapMarkerVisible and EnableFastTravel to the interpreter. The message is called
    /// with the marker when ShowMap uncovers it, to tell the player. Those commands that the command table does not
    /// have are left out.
    void addMapMarkerCommands(Interpreter& interpreter, MapMarkers& markers, std::function<void(FormId)> message);
}

#endif
