#include "mapmarkers.hpp"

#include "arguments.hpp"

namespace FalloutScript
{
    MapMarkers::MapMarkers(MapMarkerRecords& records)
        : mRecords(records)
    {
    }

    void MapMarkers::clear()
    {
        const std::lock_guard lock(mMutex);
        mFlags.clear();
        mSwitches = Switches();
    }

    std::uint8_t MapMarkers::flagsLocked(FormId marker, bool& known)
    {
        if (const auto found = mFlags.find(marker); found != mFlags.end())
        {
            known = true;
            return found->second;
        }
        std::uint8_t flags = 0;
        known = mRecords.markerFlags(marker, flags);
        return flags;
    }

    bool MapMarkers::isMarker(FormId marker)
    {
        const std::lock_guard lock(mMutex);
        bool known = false;
        flagsLocked(marker, known);
        return known;
    }

    int MapMarkers::state(FormId marker)
    {
        const std::lock_guard lock(mMutex);
        bool known = false;
        const std::uint8_t flags = flagsLocked(marker, known);
        if (!known || (flags & visibleFlag) == 0)
            return 0;
        return (flags & travelFlag) != 0 ? 2 : 1;
    }

    bool MapMarkers::show(FormId marker, bool withTravel)
    {
        const std::lock_guard lock(mMutex);
        bool known = false;
        const std::uint8_t before = flagsLocked(marker, known);
        if (!known)
            return false;
        std::uint8_t flags = before | visibleFlag;
        // A marker that is shown and could be travelled to stays so; one that is only shown becomes one to travel to
        // when the call asks
        if (withTravel)
            flags |= travelFlag;
        mFlags[marker] = flags;
        return flags != before;
    }

    void MapMarkers::enableFastTravel(bool enable, bool second, bool third)
    {
        const std::lock_guard lock(mMutex);
        mSwitches.mAllowed = enable;
        // Turning fast travel on allows resting too; turning it off lets the second switch say
        mSwitches.mRest = enable ? true : second;
        // The third switch is set when asked for, and cleared by a call that turns fast travel on without it
        if (third)
            mSwitches.mThird = true;
        else if (enable)
            mSwitches.mThird = false;
    }

    bool MapMarkers::fastTravelAllowed()
    {
        const std::lock_guard lock(mMutex);
        return mSwitches.mAllowed;
    }

    bool MapMarkers::restAllowed()
    {
        const std::lock_guard lock(mMutex);
        return mSwitches.mRest;
    }

    bool MapMarkers::travelSwitch()
    {
        const std::lock_guard lock(mMutex);
        return mSwitches.mThird;
    }

    template <>
    struct SaveAccess<MapMarkers>
    {
        static void write(MapMarkers& markers, SaveWriter& out)
        {
            const std::lock_guard lock(markers.mMutex);
            out.count(markers.mFlags.size());
            for (const auto& [marker, flags] : markers.mFlags)
            {
                out.form(marker);
                out.u8(flags);
            }
            out.boolean(markers.mSwitches.mAllowed);
            out.boolean(markers.mSwitches.mRest);
            out.boolean(markers.mSwitches.mThird);
        }

        static void read(MapMarkers& markers, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, std::uint8_t> flags;
            const std::size_t count = in.count(5);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> marker = in.form();
                const std::uint8_t value = in.u8();
                if (marker)
                    flags[*marker] = value;
            }
            MapMarkers::Switches switches;
            switches.mAllowed = in.boolean();
            switches.mRest = in.boolean();
            switches.mThird = in.boolean();
            const std::lock_guard lock(markers.mMutex);
            markers.mFlags = std::move(flags);
            markers.mSwitches = switches;
        }
    };

    void writeMapMarkers(MapMarkers& markers, SaveWriter& out)
    {
        SaveAccess<MapMarkers>::write(markers, out);
    }

    void readMapMarkers(MapMarkers& markers, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<MapMarkers>::read(markers, in, version);
    }

    void addMapMarkerCommands(Interpreter& interpreter, MapMarkers& markers, std::function<void(FormId)> message)
    {
        interpreter.setHandler("ShowMap", [&markers, message = std::move(message)](CallContext& call) {
            const FormId marker = formArgument(call, 0);
            if (markers.show(marker, intArgument(call, 1) != 0) && message)
                message(marker);
            return 0.0;
        });
        // The marker is the reference the command is called on
        interpreter.setHandler("GetMapMarkerVisible", [&markers](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(markers.state(call.mReference)) : 0.0;
        });
        // Without the switches that follow, a call that turns fast travel on allows resting and a call that turns it
        // off forbids it
        interpreter.setHandler("EnableFastTravel", [&markers](CallContext& call) {
            const bool second = call.mArguments.size() > 1 ? intArgument(call, 1) != 0 : true;
            const bool third = call.mArguments.size() > 2 && intArgument(call, 2) == 1;
            markers.enableFastTravel(intArgument(call, 0) != 0, second, third);
            return 0.0;
        });
    }
}
