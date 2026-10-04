#include "weatherbindings.hpp"

#include <type_traits>

#include <osg/Vec4f>

#include <components/esm3/loadregn.hpp>
#include <components/lua/util.hpp>
#include <components/misc/color.hpp>
#include <components/misc/finitevalues.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/scene.hpp"
#include "../mwworld/weather.hpp"

#include "context.hpp"
#include "object.hpp"
#include "recordstore.hpp"

namespace
{
    template <class Cell>
    bool hasWeather(const Cell& cell, bool requireExterior)
    {
        if (requireExterior && !cell.mStore->isQuasiExterior() && !cell.mStore->isExterior())
            return false;
        return OFBase::Environment::get().getWorldScene()->isCellActive(*cell.mStore);
    }

    template <class Getter>
    auto overloadForActiveCell(Getter&& getter, bool requireExterior = true)
    {
        using Result = std::invoke_result_t<Getter>;
        return sol::overload(
            [=](const OFLua::GCell& cell) -> Result {
                if (!hasWeather(cell, requireExterior))
                    return Result{};
                return getter();
            },
            [=](const OFLua::LCell& cell) -> Result {
                if (!hasWeather(cell, requireExterior))
                    return Result{};
                return getter();
            });
    }

    template <class T>
    using WeatherGetter = T (OFBase::World::*)() const;

    template <class T>
    auto overloadWeatherGetter(WeatherGetter<T> getter)
    {
        return overloadForActiveCell([=]() -> std::optional<T> {
            const OFBase::World& world = *OFBase::Environment::get().getWorld();
            return (world.*getter)();
        });
    }

    void createFloatInterpolator(sol::state_view lua)
    {
        using Misc::FiniteFloat;
        using T = OFWorld::TimeOfDayInterpolator<float>;

        auto interT = lua.new_usertype<T>("TimeOfDayInterpolatorFloat");

        interT["sunrise"] = sol::property([](const T& inter) { return inter.getSunriseValue(); },
            [](T& inter, FiniteFloat value) { inter.setSunriseValue(value); });
        interT["sunset"] = sol::property([](const T& inter) { return inter.getSunsetValue(); },
            [](T& inter, FiniteFloat value) { inter.setSunsetValue(value); });
        interT["day"] = sol::property([](const T& inter) { return inter.getDayValue(); },
            [](T& inter, FiniteFloat value) { inter.setDayValue(value); });
        interT["night"] = sol::property([](const T& inter) { return inter.getNightValue(); },
            [](T& inter, FiniteFloat value) { inter.setNightValue(value); });
    }

    void createColorInterpolator(sol::state_view lua)
    {
        using Misc::Color;
        using T = OFWorld::TimeOfDayInterpolator<osg::Vec4f>;

        auto interT = lua.new_usertype<T>("TimeOfDayInterpolatorColor");

        interT["sunrise"] = sol::property([](const T& inter) { return Color::fromVec(inter.getSunriseValue()); },
            [](T& inter, const Color& value) { inter.setSunriseValue(value.toVec()); });
        interT["sunset"] = sol::property([](const T& inter) { return Color::fromVec(inter.getSunsetValue()); },
            [](T& inter, const Color& value) { inter.setSunsetValue(value.toVec()); });
        interT["day"] = sol::property([](const T& inter) { return Color::fromVec(inter.getDayValue()); },
            [](T& inter, const Color& value) { inter.setDayValue(value.toVec()); });
        interT["night"] = sol::property([](const T& inter) { return Color::fromVec(inter.getNightValue()); },
            [](T& inter, const Color& value) { inter.setNightValue(value.toVec()); });
    }
}

namespace sol
{
    template <>
    struct is_automagical<OFWorld::TimeOfDayInterpolator<float>> : std::false_type
    {
    };
    template <>
    struct is_automagical<OFWorld::TimeOfDayInterpolator<osg::Vec4f>> : std::false_type
    {
    };
    template <>
    struct is_automagical<OFWorld::WeatherStore> : std::false_type
    {
    };
}

namespace OFLua
{
    sol::table initCoreWeatherBindings(const Context& context)
    {
        using Misc::FiniteFloat;
        using Misc::FiniteVec3f;

        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        auto vfs = OFBase::Environment::get().getResourceSystem()->getVFS();

        auto weatherT = lua.new_usertype<OFWorld::Weather>("Weather");
        createFloatInterpolator(lua);
        createColorInterpolator(lua);

        weatherT[sol::meta_function::to_string]
            = [](const OFWorld::Weather& w) -> std::string { return "Weather[" + w.mName + "]"; };
        weatherT["name"]
            = sol::readonly_property([](const OFWorld::Weather& w) -> std::string_view { return w.mName; });
        weatherT["scriptId"] = sol::readonly_property([](const OFWorld::Weather& w) { return w.mScriptId; });
        weatherT["recordId"] = sol::readonly_property([](const OFWorld::Weather& w) { return w.mId.serializeText(); });
        weatherT["thunderSoundID"] = sol::readonly_property([lua](const OFWorld::Weather& w) {
            sol::table result(lua, sol::create);
            for (const auto& soundId : w.mThunderSoundID)
                result.add(soundId.serializeText());
            return result;
        });

        weatherT["windSpeed"] = sol::property([](const OFWorld::Weather& w) { return w.mWindSpeed; },
            [](OFWorld::Weather& w, const FiniteFloat windSpeed) { w.mWindSpeed = windSpeed; });
        weatherT["cloudSpeed"] = sol::property([](const OFWorld::Weather& w) { return w.mCloudSpeed; },
            [](OFWorld::Weather& w, const FiniteFloat cloudSpeed) { w.mCloudSpeed = cloudSpeed; });
        weatherT["cloudTexture"] = sol::property(
            [vfs](const OFWorld::Weather& w) -> std::string {
                return Misc::ResourceHelpers::correctTexturePath(VFS::Path::toNormalized(w.mCloudTexture), *vfs);
            },
            [](OFWorld::Weather& w, std::string_view cloudTexture) { w.mCloudTexture = cloudTexture; });
        weatherT["cloudsMaximumPercent"]
            = sol::property([](const OFWorld::Weather& w) { return w.mCloudsMaximumPercent; },
                [](OFWorld::Weather& w, const FiniteFloat cloudsMaximumPercent) {
                    if (cloudsMaximumPercent <= 0.f)
                        throw std::runtime_error("Value must be greater than 0");
                    w.mCloudsMaximumPercent = cloudsMaximumPercent;
                });
        weatherT["isStorm"] = sol::property([](const OFWorld::Weather& w) { return w.mIsStorm; },
            [](OFWorld::Weather& w, bool isStorm) { w.mIsStorm = isStorm; });
        weatherT["stormDirection"] = sol::property([](const OFWorld::Weather& w) { return w.mStormDirection; },
            [](OFWorld::Weather& w, const FiniteVec3f& stormDirection) { w.mStormDirection = stormDirection; });
        weatherT["glareView"] = sol::property([](const OFWorld::Weather& w) { return w.mGlareView; },
            [](OFWorld::Weather& w, const FiniteFloat glareView) { w.mGlareView = glareView; });
        weatherT["rainSpeed"] = sol::property([](const OFWorld::Weather& w) { return w.mRainSpeed; },
            [](OFWorld::Weather& w, const FiniteFloat rainSpeed) { w.mRainSpeed = rainSpeed; });
        weatherT["rainEntranceSpeed"] = sol::property([](const OFWorld::Weather& w) { return w.mRainEntranceSpeed; },
            [](OFWorld::Weather& w, const FiniteFloat rainEntranceSpeed) {
                if (rainEntranceSpeed <= 0.f)
                    throw std::runtime_error("Value must be greater than 0");
                w.mRainEntranceSpeed = rainEntranceSpeed;
            });
        weatherT["rainEffect"] = sol::property(
            [](const OFWorld::Weather& w) -> sol::optional<std::string> {
                if (w.mRainEffect.empty())
                    return sol::nullopt;
                return w.mRainEffect;
            },
            [](OFWorld::Weather& w, sol::optional<std::string_view> rainEffect) {
                w.mRainEffect = rainEffect.value_or("");
            });
        weatherT["rainMaxRaindrops"] = sol::property([](const OFWorld::Weather& w) { return w.mRainMaxRaindrops; },
            [](OFWorld::Weather& w, int rainMaxRaindrops) { w.mRainMaxRaindrops = rainMaxRaindrops; });
        weatherT["rainDiameter"] = sol::property([](const OFWorld::Weather& w) { return w.mRainDiameter; },
            [](OFWorld::Weather& w, const FiniteFloat rainDiameter) { w.mRainDiameter = rainDiameter; });
        weatherT["rainThreshold"] = sol::property([](const OFWorld::Weather& w) { return w.mRainThreshold; },
            [](OFWorld::Weather& w, const FiniteFloat rainThreshold) { w.mRainThreshold = rainThreshold; });
        weatherT["rainMaxHeight"] = sol::property([](const OFWorld::Weather& w) { return w.mRainMaxHeight; },
            [](OFWorld::Weather& w, const FiniteFloat rainMaxHeight) { w.mRainMaxHeight = rainMaxHeight; });
        weatherT["rainMinHeight"] = sol::property([](const OFWorld::Weather& w) { return w.mRainMinHeight; },
            [](OFWorld::Weather& w, const FiniteFloat rainMinHeight) { w.mRainMinHeight = rainMinHeight; });
        weatherT["rainLoopSoundID"]
            = sol::property([](const OFWorld::Weather& w) -> ESM::RefId { return w.mRainLoopSoundID; },
                [](OFWorld::Weather& w, sol::optional<std::string_view> rainLoopSoundID) {
                    w.mRainLoopSoundID = ESM::RefId::deserializeText(rainLoopSoundID.value_or(""));
                });
        weatherT["sunDiscSunsetColor"]
            = sol::property([](const OFWorld::Weather& w) { return Misc::Color::fromVec(w.mSunDiscSunsetColor); },
                [](OFWorld::Weather& w, const Misc::Color& sunDiscSunsetColor) {
                    w.mSunDiscSunsetColor = sunDiscSunsetColor.toVec();
                });
        weatherT["ambientLoopSoundID"]
            = sol::property([](const OFWorld::Weather& w) -> ESM::RefId { return w.mAmbientLoopSoundID; },
                [](OFWorld::Weather& w, sol::optional<std::string_view> ambientLoopSoundId) {
                    w.mAmbientLoopSoundID = ESM::RefId::deserializeText(ambientLoopSoundId.value_or(""));
                });
        weatherT["ambientColor"] = sol::readonly_property([](const OFWorld::Weather& w) { return &w.mAmbientColor; });
        weatherT["fogColor"] = sol::readonly_property([](const OFWorld::Weather& w) { return &w.mFogColor; });
        weatherT["skyColor"] = sol::readonly_property([](const OFWorld::Weather& w) { return &w.mSkyColor; });
        weatherT["sunColor"] = sol::readonly_property([](const OFWorld::Weather& w) { return &w.mSunColor; });
        weatherT["landFogDepth"] = sol::readonly_property([](const OFWorld::Weather& w) { return &w.mLandFogDepth; });
        weatherT["particleEffect"] = sol::property(
            [](const OFWorld::Weather& w) -> sol::optional<std::string> {
                if (w.mParticleEffect.empty())
                    return sol::nullopt;
                return w.mParticleEffect;
            },
            [](OFWorld::Weather& w, sol::optional<std::string_view> particleEffect) {
                w.mParticleEffect = particleEffect.value_or("");
            });
        weatherT["distantLandFogFactor"] = sol::property([](const OFWorld::Weather& w) { return w.mDL.FogFactor; },
            [](OFWorld::Weather& w, const FiniteFloat fogFactor) { w.mDL.FogFactor = fogFactor; });
        weatherT["distantLandFogOffset"] = sol::property([](const OFWorld::Weather& w) { return w.mDL.FogOffset; },
            [](OFWorld::Weather& w, const FiniteFloat fogOffset) { w.mDL.FogOffset = fogOffset; });

        api["changeWeather"] = [](std::string_view regionId, const OFWorld::Weather& weather) {
            ESM::RefId region = ESM::RefId::deserializeText(regionId);
            OFBase::Environment::get().getESMStore()->get<ESM::Region>().find(region);
            OFBase::Environment::get().getWorld()->changeWeather(region, weather.mId);
        };

        addRecordStoreType<OFWorld::WeatherStore, OFWorld::Weather>(lua, "Weather");

        // Provide access to the store.
        api["records"] = &OFBase::Environment::get().getWorld()->getAllWeather();

        using Phase = OFRender::MoonState::Phase;
        api["MOON_PHASE"] = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, Phase>(lua,
            { { "Full", Phase::Full }, { "WaningGibbous", Phase::WaningGibbous },
                { "ThirdQuarter", Phase::ThirdQuarter }, { "WaningCrescent", Phase::WaningCrescent },
                { "New", Phase::New }, { "WaxingCrescent", Phase::WaxingCrescent },
                { "FirstQuarter", Phase::FirstQuarter }, { "WaxingGibbous", Phase::WaxingGibbous } }));

        api["getCurrent"] = overloadForActiveCell(
            []() -> const OFWorld::Weather* { return &OFBase::Environment::get().getWorld()->getCurrentWeather(); });
        api["getNext"] = overloadForActiveCell(
            []() -> const OFWorld::Weather* { return OFBase::Environment::get().getWorld()->getNextWeather(); });
        api["getTransition"] = overloadWeatherGetter(&OFBase::World::getWeatherTransition);
        api["getCurrentSunLightDirection"] = overloadForActiveCell(
            []() -> std::optional<osg::Vec4f> {
                osg::Vec4f sunPos = OFBase::Environment::get().getWorld()->getSunLightPosition();
                // normalize to get the direction towards the sun
                sunPos.normalize();

                // and invert it to get the direction of the sun light
                return -sunPos;
            },
            false);
        api["getCurrentSunVisibility"] = overloadWeatherGetter(&OFBase::World::getSunVisibility);
        api["getCurrentSunPercentage"] = overloadWeatherGetter(&OFBase::World::getSunPercentage);
        api["getCurrentWindSpeed"] = overloadWeatherGetter(&OFBase::World::getWindSpeed);
        api["getCurrentStormDirection"] = overloadWeatherGetter(&OFBase::World::getStormDirection);
        api["getCurrentMoons"] = overloadForActiveCell([lua]() {
            sol::table result(lua, sol::create);
            for (const OFWorld::Moon& moon : OFBase::Environment::get().getWorld()->getCurrentMoons())
            {
                sol::table moonTable(lua, sol::create);
                moonTable["name"] = moon.mName;
                moonTable["phase"] = moon.mPhase;
                moonTable["phaseValue"] = moon.mPhaseValue;
                moonTable["alpha"] = moon.mAlpha;
                result.add(LuaUtil::makeReadOnly(moonTable));
            }
            return LuaUtil::makeReadOnly(result);
        });

        return LuaUtil::makeReadOnly(api);
    }
}
