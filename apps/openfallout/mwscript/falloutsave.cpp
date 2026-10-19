#include "falloutscripts.hpp"

#include <algorithm>

#include <components/debug/debuglog.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/falloutscript/savescripts.hpp>
#include <components/falloutscript/savestate.hpp>

#include "../mwworld/esmstore.hpp"

namespace FalloutScript
{
    /// The state that the host itself keeps (the global variables the scripts set, the clock, the controls the player
    /// was refused, the characters that talking activators speak for, the packages that scripts gave)
    template <>
    struct SaveAccess<OFScript::FalloutScripts>
    {
        static void writeFormat(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.u32(saveFormatVersion);
            out.u8(static_cast<std::uint8_t>(host.mGame));
            out.form(host.mPlayer);
        }

        static void readFormat(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::uint32_t version = in.u32();
            if (version > saveFormatVersion)
                throw SaveTooNewError("The state of Fallout in this save", version);
            const std::uint8_t game = in.u8();
            in.form();
            if (game != static_cast<std::uint8_t>(host.mGame))
                Log(Debug::Warning) << "The save was made for the other game of Fallout (" << int(game)
                                    << "): the state of the scripts may not fit";
        }

        static void writeGlobals(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.count(host.mGlobals.size());
            for (const auto& [global, value] : host.mGlobals)
            {
                out.form(global);
                out.f64(value);
            }
            out.i64(host.mCalendarShift);
        }

        static void readGlobals(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> global = in.form();
                const double value = in.f64();
                if (global && host.mStore.get<ESM4::GlobalVariable>().search(OFScript::FalloutScripts::refIdOf(*global)))
                    host.mGlobals[*global] = value;
            }
            const std::lock_guard lock(host.mCalendarMutex);
            host.mCalendarShift = in.i64();
        }

        static void writeControls(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.u32(host.mControlsOff.load());
        }

        static void readControls(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            // The switches of the engine are restored with the input of the saved game; this is what the games have
            // beyond them (the Pip-Boy, rollover, sneaking)
            host.mControlsOff = in.u32() & ((1u << FalloutScript::playerControlCount) - 1);
        }

        static void writeTalking(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            const std::lock_guard lock(host.mTalkingActorMutex);
            out.count(host.mTalkingActors.size());
            for (const auto& [activator, actor] : host.mTalkingActors)
            {
                out.form(activator);
                out.form(actor);
            }
        }

        static void readTalking(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(8);
            const std::lock_guard lock(host.mTalkingActorMutex);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> activator = in.form();
                const std::optional<FormId> actor = in.form();
                if (activator && actor)
                    host.mTalkingActors[*activator] = *actor;
            }
        }

        static void writePackages(OFScript::FalloutScripts& host, SaveWriter& out)
        {
            out.count(host.mScriptPackages.size());
            for (const auto& [actor, packages] : host.mScriptPackages)
            {
                out.form(actor);
                out.count(packages.size());
                for (const FormId package : packages)
                    out.form(package);
            }
        }

        static void readPackages(OFScript::FalloutScripts& host, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                std::vector<FormId> packages;
                const std::size_t size = in.count(4);
                for (std::size_t j = 0; j < size; ++j)
                    if (const std::optional<FormId> package = in.form())
                        packages.push_back(*package);
                if (actor && !packages.empty())
                    host.mScriptPackages[*actor] = std::move(packages);
            }
        }
    };
}

namespace OFScript
{
    ESM::RefId FalloutScripts::refIdOf(FalloutScript::FormId id)
    {
        const ESM::FormId form = ESM::FormId::fromUint32(id);
        return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
    }

    std::vector<FalloutScript::SaveSection> FalloutScripts::saveSections()
    {
        using Access = FalloutScript::SaveAccess<FalloutScripts>;
        std::vector<FalloutScript::SaveSection> sections;
        // The first section tells the layout of the whole: the others are only read when it is one this version knows
        sections.push_back({ "format", 1, [this](FalloutScript::SaveWriter& out) { Access::writeFormat(*this, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) { Access::readFormat(*this, in, version); } });
        sections.push_back({ "globals", 1, [this](FalloutScript::SaveWriter& out) { Access::writeGlobals(*this, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) { Access::readGlobals(*this, in, version); } });
        sections.push_back({ "quests", FalloutScript::questsSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeQuests(mQuests, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readQuests(mQuests, in, version);
            } });
        sections.push_back({ "objects", FalloutScript::objectScriptsSectionVersion,
            [this](FalloutScript::SaveWriter& out) { FalloutScript::writeObjectScripts(mObjects, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) {
                FalloutScript::readObjectScripts(mObjects, in, version);
            } });
        sections.push_back({ "controls", 1, [this](FalloutScript::SaveWriter& out) { Access::writeControls(*this, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) { Access::readControls(*this, in, version); } });
        sections.push_back({ "talking", 1, [this](FalloutScript::SaveWriter& out) { Access::writeTalking(*this, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) { Access::readTalking(*this, in, version); } });
        sections.push_back({ "packages", 1, [this](FalloutScript::SaveWriter& out) { Access::writePackages(*this, out); },
            [this](FalloutScript::SaveReader& in, std::uint32_t version) { Access::readPackages(*this, in, version); } });
        return sections;
    }

    std::size_t FalloutScripts::countSavedGameRecords()
    {
        return saveSections().size();
    }

    void FalloutScripts::write(ESM::ESMWriter& writer, Loading::Listener&)
    {
        for (const FalloutScript::SaveSection& section : saveSections())
        {
            FalloutScript::SaveWriter out;
            section.mWrite(out);
            writer.startRecord(ESM::REC_FOSV);
            writer.writeHNString("NAME", section.mName);
            writer.writeHNT("VERS", section.mVersion);
            writer.writeHNString("DATA", out.bytes());
            writer.endRecord(ESM::REC_FOSV);
        }
    }

    void FalloutScripts::readRecord(ESM::ESMReader& reader, std::uint32_t type)
    {
        if (type != ESM::REC_FOSV)
        {
            reader.skipRecord();
            return;
        }
        const std::string name = reader.getHNString("NAME");
        std::uint32_t version = 0;
        reader.getHNT(version, "VERS");
        reader.getSubNameIs("DATA");
        const std::string_view data = reader.getHStringView();

        const std::vector<FalloutScript::SaveSection> sections = saveSections();
        const auto section = std::find_if(sections.begin(), sections.end(),
            [&name](const FalloutScript::SaveSection& candidate) { return candidate.mName == name; });
        if (section == sections.end())
        {
            Log(Debug::Warning) << "The save has a section of the state of Fallout that this version does not know: "
                                << name;
            return;
        }
        if (version > section->mVersion)
            throw FalloutScript::SaveTooNewError("The section " + name + " of the state of Fallout", version);

        // The plugin of a form of the save may be at another place in the load order now, or not loaded
        FalloutScript::SaveReader in(data, [&reader](FalloutScript::FormId saved) -> std::optional<FalloutScript::FormId> {
            ESM::FormId form = ESM::FormId::fromUint32(saved);
            if (!reader.applyContentFileMapping(form))
                return std::nullopt;
            return form.toUint32();
        });
        try
        {
            section->mRead(in, version);
        }
        catch (const FalloutScript::SaveTooNewError&)
        {
            throw;
        }
        catch (const FalloutScript::SaveError& error)
        {
            throw FalloutScript::SaveError("The section " + name + " of the state of Fallout is damaged: " + error.what());
        }
    }
}
