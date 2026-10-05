#include "soundbindings.hpp"
#include "recordstore.hpp"

#include "types/usertypeutil.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/esmstore.hpp"

#include <components/esm3/loadsoun.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/pathutil.hpp>

#include "luamanagerimp.hpp"
#include "objectvariant.hpp"

namespace
{
    struct PlaySoundArgs
    {
        bool mScale = true;
        bool mLoop = false;
        float mVolume = 1.f;
        float mPitch = 1.f;
        float mTimeOffset = 0.f;
    };

    struct StreamMusicArgs
    {
        float mFade = 1.f;
    };

    OFWorld::Ptr getMutablePtrOrThrow(const OFLua::ObjectVariant& variant)
    {
        if (variant.isLObject())
            throw std::runtime_error("Local scripts can only modify object they are attached to.");

        OFWorld::Ptr ptr = variant.ptr();
        if (ptr.isEmpty())
            throw std::runtime_error("Invalid object");

        return ptr;
    }

    OFWorld::Ptr getPtrOrThrow(const OFLua::ObjectVariant& variant)
    {
        OFWorld::Ptr ptr = variant.ptr();
        if (ptr.isEmpty())
            throw std::runtime_error("Invalid object");

        return ptr;
    }

    PlaySoundArgs getPlaySoundArgs(const sol::optional<sol::table>& options)
    {
        PlaySoundArgs args;

        if (options.has_value())
        {
            args.mLoop = options->get_or("loop", false);
            args.mVolume = options->get_or("volume", 1.f);
            args.mPitch = options->get_or("pitch", 1.f);
            args.mTimeOffset = options->get_or("timeOffset", 0.f);
            args.mScale = options->get_or("scale", true);
        }
        return args;
    }

    OFSound::PlayMode getPlayMode(const PlaySoundArgs& args, bool is3D)
    {
        if (is3D)
        {
            if (args.mLoop)
                return OFSound::PlayMode::LoopRemoveAtDistance;
            return OFSound::PlayMode::Normal;
        }

        if (args.mLoop && !args.mScale)
            return OFSound::PlayMode::LoopNoEnvNoScaling;
        else if (args.mLoop)
            return OFSound::PlayMode::LoopNoEnv;
        else if (!args.mScale)
            return OFSound::PlayMode::NoEnvNoScaling;
        return OFSound::PlayMode::NoEnv;
    }

    StreamMusicArgs getStreamMusicArgs(const sol::optional<sol::table>& options)
    {
        StreamMusicArgs args;

        if (options.has_value())
        {
            args.mFade = options->get_or("fadeOut", 1.f);
        }
        return args;
    }
}

namespace OFLua
{
    namespace
    {
        template <class T>
        void addUserType(sol::state_view& lua, std::string_view name)
        {
            sol::usertype<T> record = lua.new_usertype<T>(name);

            record[sol::meta_function::to_string]
                = [](const T& rec) -> std::string { return "ESM3_Sound[" + rec.mId.toDebugString() + "]"; };
            record["id"] = sol::readonly_property([](const T& rec) -> ESM::RefId { return rec.mId; });

            Types::addProperty(record, "volume", &ESM::Sound::mData, &ESM::SOUNstruct::mVolume);
            Types::addProperty(record, "minRange", &ESM::Sound::mData, &ESM::SOUNstruct::mMinRange);
            Types::addProperty(record, "maxRange", &ESM::Sound::mData, &ESM::SOUNstruct::mMaxRange);

            if constexpr (Types::RecordType<T>::isMutable)
            {
                record["fileName"] = sol::property(
                    [](const T& mutRec) -> std::string {
                        return Misc::ResourceHelpers::correctSoundPath(VFS::Path::Normalized(mutRec.find().mSound));
                    },
                    [](T& mutRec, std::string_view path) {
                        ESM::Sound& recordValue = mutRec.find();
                        recordValue.mSound = Misc::ResourceHelpers::soundPathForESM3(path);
                    });
            }
            else
            {
                record["fileName"] = sol::readonly_property([](const ESM::Sound& rec) -> std::string {
                    return Misc::ResourceHelpers::correctSoundPath(VFS::Path::Normalized(rec.mSound));
                });
            }
        }
    }

    sol::table initAmbientPackage(const Context& context)
    {
        sol::state_view lua = context.sol();
        if (lua["openfallout_ambient"] != sol::nil)
            return lua["openfallout_ambient"];

        sol::table api(lua, sol::create);

        api["playSound"] = [](std::string_view soundId, const sol::optional<sol::table>& options) {
            auto args = getPlaySoundArgs(options);
            auto playMode = getPlayMode(args, false);
            ESM::RefId sound = ESM::RefId::deserializeText(soundId);

            OFBase::Environment::get().getSoundManager()->playSound(
                sound, args.mVolume, args.mPitch, OFSound::Type::Sfx, playMode, args.mTimeOffset);
        };
        api["playSoundFile"] = [](std::string_view fileName, const sol::optional<sol::table>& options) {
            auto args = getPlaySoundArgs(options);
            auto playMode = getPlayMode(args, false);

            OFBase::Environment::get().getSoundManager()->playSound(VFS::Path::Normalized(fileName), args.mVolume,
                args.mPitch, OFSound::Type::Sfx, playMode, args.mTimeOffset);
        };

        api["stopSound"] = [](std::string_view soundId) {
            ESM::RefId sound = ESM::RefId::deserializeText(soundId);
            OFBase::Environment::get().getSoundManager()->stopSound3D(OFWorld::Ptr(), sound);
        };
        api["stopSoundFile"] = [](std::string_view fileName) {
            OFBase::Environment::get().getSoundManager()->stopSound3D(OFWorld::Ptr(), VFS::Path::Normalized(fileName));
        };

        api["isSoundPlaying"] = [](std::string_view soundId) {
            ESM::RefId sound = ESM::RefId::deserializeText(soundId);
            return OFBase::Environment::get().getSoundManager()->getSoundPlaying(OFWorld::Ptr(), sound);
        };
        api["isSoundFilePlaying"] = [](std::string_view fileName) {
            return OFBase::Environment::get().getSoundManager()->getSoundPlaying(
                OFWorld::Ptr(), VFS::Path::Normalized(fileName));
        };

        api["streamMusic"] = [](std::string_view fileName, const sol::optional<sol::table>& options) {
            auto args = getStreamMusicArgs(options);
            OFBase::SoundManager* sndMgr = OFBase::Environment::get().getSoundManager();
            sndMgr->streamMusic(VFS::Path::Normalized(fileName), OFSound::MusicType::Normal, args.mFade);
        };

        api["say"]
            = [luaManager = context.mLuaManager](std::string_view fileName, sol::optional<std::string_view> text) {
                  OFBase::Environment::get().getSoundManager()->say(VFS::Path::Normalized(fileName));
                  if (text && Settings::gui().mSubtitles)
                      luaManager->addUIMessage(*text);
              };

        api["stopSay"] = []() { OFBase::Environment::get().getSoundManager()->stopSay(OFWorld::ConstPtr()); };
        api["isSayActive"]
            = []() { return OFBase::Environment::get().getSoundManager()->sayActive(OFWorld::ConstPtr()); };

        api["isMusicPlaying"] = []() { return OFBase::Environment::get().getSoundManager()->isMusicPlaying(); };

        api["stopMusic"] = []() {
            OFBase::SoundManager* sndMgr = OFBase::Environment::get().getSoundManager();
            if (sndMgr->getMusicType() == OFSound::MusicType::MWScript)
                return;

            sndMgr->stopMusic();
        };

        lua["openfallout_ambient"] = LuaUtil::makeReadOnly(api);
        return lua["openfallout_ambient"];
    }

    sol::table initCoreSoundBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        api["isEnabled"] = []() { return OFBase::Environment::get().getSoundManager()->isEnabled(); };

        api["playSound3d"] = [](std::string_view soundId, const sol::object& objectOrPosition,
                                 const sol::optional<sol::table>& options) {
            auto args = getPlaySoundArgs(options);
            auto playMode = getPlayMode(args, true);

            ESM::RefId sound = ESM::RefId::deserializeText(soundId);

            if (objectOrPosition.is<Object>())
            {
                OFWorld::Ptr ptr = getMutablePtrOrThrow(ObjectVariant(objectOrPosition));
                OFBase::Environment::get().getSoundManager()->playSound3D(
                    ptr, sound, args.mVolume, args.mPitch, OFSound::Type::Sfx, playMode, args.mTimeOffset);
            }
            else if (objectOrPosition.is<osg::Vec3f>())
                OFBase::Environment::get().getSoundManager()->playSound3D(objectOrPosition.as<osg::Vec3f>(), sound,
                    args.mVolume, args.mPitch, OFSound::Type::Sfx, playMode, args.mTimeOffset);
        };
        api["playSoundFile3d"]
            = [](std::string_view fileName, const sol::object& object, const sol::optional<sol::table>& options) {
                  auto args = getPlaySoundArgs(options);
                  auto playMode = getPlayMode(args, true);
                  OFWorld::Ptr ptr = getMutablePtrOrThrow(ObjectVariant(object));

                  OFBase::Environment::get().getSoundManager()->playSound3D(ptr, VFS::Path::Normalized(fileName),
                      args.mVolume, args.mPitch, OFSound::Type::Sfx, playMode, args.mTimeOffset);
              };

        api["stopSound3d"] = [](std::string_view soundId, const sol::object& object) {
            ESM::RefId sound = ESM::RefId::deserializeText(soundId);
            OFWorld::Ptr ptr = getMutablePtrOrThrow(ObjectVariant(object));
            OFBase::Environment::get().getSoundManager()->stopSound3D(ptr, sound);
        };
        api["stopSoundFile3d"] = [](std::string_view fileName, const sol::object& object) {
            OFWorld::Ptr ptr = getMutablePtrOrThrow(ObjectVariant(object));
            OFBase::Environment::get().getSoundManager()->stopSound3D(ptr, VFS::Path::Normalized(fileName));
        };

        api["isSoundPlaying"] = [](std::string_view soundId, const sol::object& object) {
            ESM::RefId sound = ESM::RefId::deserializeText(soundId);
            const OFWorld::Ptr& ptr = getPtrOrThrow(ObjectVariant(object));
            return OFBase::Environment::get().getSoundManager()->getSoundPlaying(ptr, sound);
        };
        api["isSoundFilePlaying"] = [](std::string_view fileName, const sol::object& object) {
            const OFWorld::Ptr& ptr = getPtrOrThrow(ObjectVariant(object));
            return OFBase::Environment::get().getSoundManager()->getSoundPlaying(ptr, VFS::Path::Normalized(fileName));
        };

        api["say"] = [luaManager = context.mLuaManager](
                         std::string_view fileName, const sol::object& object, sol::optional<std::string_view> text) {
            OFWorld::Ptr ptr = getMutablePtrOrThrow(ObjectVariant(object));
            OFBase::Environment::get().getSoundManager()->say(ptr, VFS::Path::Normalized(fileName));
            if (text && Settings::gui().mSubtitles)
                luaManager->addUIMessage(*text);
        };
        api["stopSay"] = [](const sol::object& object) {
            OFWorld::Ptr ptr = getMutablePtrOrThrow(ObjectVariant(object));
            OFBase::Environment::get().getSoundManager()->stopSay(ptr);
        };
        api["isSayActive"] = [](const sol::object& object) {
            const OFWorld::Ptr& ptr = getPtrOrThrow(ObjectVariant(object));
            return OFBase::Environment::get().getSoundManager()->sayActive(ptr);
        };

        addRecordFunctionBinding<ESM::Sound>(api, context);

        // Sound record
        addUserType<ESM::Sound>(lua, "ESM3_Sound");

        return LuaUtil::makeReadOnly(api);
    }

    void addMutableSoundType(sol::state_view& lua)
    {
        addUserType<MutableRecord<ESM::Sound>>(lua, "ESM3_MutableSound");
    }

    ESM::Sound tableToSound(const sol::table& rec)
    {
        auto sound = Types::initFromTemplate<ESM::Sound>(rec);
        if (rec["volume"] != sol::nil)
            sound.mData.mVolume = rec["volume"];
        if (rec["minRange"] != sol::nil)
            sound.mData.mMinRange = rec["minRange"];
        if (rec["maxRange"] != sol::nil)
            sound.mData.mMaxRange = rec["maxRange"];
        if (rec["fileName"] != sol::nil)
            sound.mSound = Misc::ResourceHelpers::soundPathForESM3(rec["fileName"].get<std::string_view>());
        return sound;
    }
}
