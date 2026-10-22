#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_QUERIES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_QUERIES_H

#include <cstdint>
#include <optional>
#include <string_view>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What the sky holds, for the weather in force and the one it is changing from. The numbers are the bytes of the
    /// `DATA` of a weather record.
    struct SkyWeather
    {
        struct Weather
        {
            bool mPresent = false;
            /// The bits of the classification: 0x01 pleasant, 0x02 cloudy, 0x04 rainy, 0x08 snow
            std::uint8_t mClassification = 0;
            /// How far into the change the rain begins and ends, in 255 steps
            std::uint8_t mPrecipitationBeginFadeIn = 0;
            std::uint8_t mPrecipitationEndFadeOut = 0;
        };

        /// The weather in force (the one the sky changes to, when it is changing) and the one it changes from
        Weather mCurrent;
        Weather mOutgoing;
        /// How much of the change is done: 1 when the weather in force is all there is
        double mFraction = 1.0;
    };

    /// Whether the games say it rains for this sky: the weather in force is rainy and the change has gone past where
    /// its rain begins, or the weather it changes from is rainy and the change has not got to where its rain ends.
    /// Snow does not count and neither does a cell: the answer is the sky's, in a building as well.
    bool isRaining(const SkyWeather& sky);

    /// What the commands that ask about the state of the world need from the game world
    class QueryWorld
    {
    public:
        virtual ~QueryWorld() = default;

        /// Whether the cell the reference stands in is a building. False for a reference that has no cell (an item in
        /// an inventory), and the host does not mind whether the reference is disabled.
        virtual bool inInterior(FormId reference) = 0;
        /// Whether the reference has its model in the world now: its cell is loaded and it is not hidden from the
        /// scene
        virtual bool hasLoaded3D(FormId reference) = 0;
        virtual SkyWeather sky() = 0;
        /// Whether the player sleeps (in a bed, waiting does not count)
        virtual bool playerSleeping() = 0;
        /// Whether the encounter zone of the cell the reference stands in is the zone (the zone of the worldspace
        /// when the cell has none). The zone of the reference itself is not asked.
        virtual bool inZone(FormId reference, FormId zone) = 0;
        /// Whether an activator speaks for the actor now (SetTalkingActivatorActor)
        virtual bool talksThroughActivator(FormId actor) = 0;
        /// The value of the game setting of that name (the names are not case sensitive), none for one that holds
        /// text or that no file has
        virtual std::optional<double> gameSetting(std::string_view name) = 0;
        /// The cell the reference is in, 0 for none (the form of the cell)
        virtual FormId parentCell(FormId reference) = 0;
        /// The worldspace of that cell, 0 for a cell inside a building
        virtual FormId parentWorldspace(FormId reference) = 0;
        /// True the first time the script asks after the game was started, and not again until it is started anew;
        /// the script is told apart by its address
        virtual bool gameRestarted(const void* script) = 0;
        /// True the first time the script asks after a saved game was loaded, and not again until the next load
        virtual bool gameLoaded(const void* script) = 0;
    };

    /// Gives the commands IsInInterior, HasLoaded3D, IsRaining, IsPCSleeping, GetInZone,
    /// IsActorTalkingThroughActivator and GetGameSetting to the interpreter, and the commands of the script extenders
    /// that Tale of Two Wastelands calls in its intro: GetParentCell, GetParentWorldspace, GetGameRestarted and
    /// GetGameLoaded. Those that the command table does not have are left out.
    void addQueryCommands(Interpreter& interpreter, QueryWorld& world);
}

#endif
