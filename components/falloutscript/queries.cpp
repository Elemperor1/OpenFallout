#include "queries.hpp"

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        constexpr std::uint8_t rainy = 0x04;

        double fractionOf(std::uint8_t steps)
        {
            return steps / 255.0;
        }
    }

    bool isRaining(const SkyWeather& sky)
    {
        // The rain of the weather the sky changes to starts a little after the change has, and the rain of the weather
        // it changes from stops a little before the change is over
        if (sky.mCurrent.mPresent && (sky.mCurrent.mClassification & rainy) != 0
            && sky.mFraction > 0.999 * fractionOf(sky.mCurrent.mPrecipitationBeginFadeIn))
            return true;
        return sky.mOutgoing.mPresent && (sky.mOutgoing.mClassification & rainy) != 0
            && sky.mFraction < 0.001 + 0.999 * fractionOf(sky.mOutgoing.mPrecipitationEndFadeOut);
    }

    void addQueryCommands(Interpreter& interpreter, QueryWorld& world)
    {
        interpreter.setHandler("IsInInterior", [&world](CallContext& call) {
            return call.mReference != 0 && world.inInterior(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("HasLoaded3D", [&world](CallContext& call) {
            return call.mReference != 0 && world.hasLoaded3D(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsRaining", [&world](CallContext&) { return isRaining(world.sky()) ? 1.0 : 0.0; });
        // The player is asked, whatever the command is called on
        interpreter.setHandler("IsPCSleeping", [&world](CallContext&) { return world.playerSleeping() ? 1.0 : 0.0; });
        interpreter.setHandler("GetInZone", [&world](CallContext& call) {
            const FormId zone = formArgument(call, 0);
            return call.mReference != 0 && zone != 0 && world.inZone(call.mReference, zone) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsActorTalkingThroughActivator", [&world](CallContext& call) {
            return call.mReference != 0 && world.talksThroughActivator(call.mReference) ? 1.0 : 0.0;
        });
        // The commands of the script extenders that the intro of Tale of Two Wastelands calls
        interpreter.setHandler("GetParentCell", [&world](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(world.parentCell(call.mReference)) : 0.0;
        });
        interpreter.setHandler("GetParentWorldspace", [&world](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(world.parentWorldspace(call.mReference)) : 0.0;
        });
        interpreter.setHandler("GetGameRestarted",
            [&world](CallContext& call) { return world.gameRestarted(&call.mInstance) ? 1.0 : 0.0; });
        interpreter.setHandler(
            "GetGameLoaded", [&world](CallContext& call) { return world.gameLoaded(&call.mInstance) ? 1.0 : 0.0; });
        // A setting that holds text, or that nobody has, is 0 as in the games
        interpreter.setHandler("GetGameSetting", [&world](CallContext& call) {
            if (call.mArguments.empty() || !call.mArguments[0].mIsText)
                return 0.0;
            return world.gameSetting(call.mArguments[0].mText).value_or(0.0);
        });
    }
}
