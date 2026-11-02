#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

#include <components/esm4/reader.hpp>
#include <components/loadinglistener/loadinglistener.hpp>
#include <components/toutf8/toutf8.hpp>

#include "apps/components_tests/esm4/syntheticplugin.hpp"
#include "apps/openfallout/mwscript/falloutscripts.hpp"
#include "apps/openfallout/mwworld/esmstore.hpp"

namespace
{
    using namespace ESM4Test;

    std::string avif(std::uint32_t id, const std::string& editorId, const std::string& name, const std::string& text)
    {
        return record("AVIF", id,
            zString("EDID", editorId) + zString("FULL", name) + zString("DESC", text) + zString("ANAM", "Short"));
    }

    // The words the games' Pip-Boy shows for a skill or an attribute are in the plugins' AVIF records. This reads a
    // plugin the way the engine does (with an encoder, into the store) and asks the scripts for the words.
    TEST(FalloutActorValueText, theTextOfAnActorValueIsFoundByItsEditorId)
    {
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        const std::string plugin = header()
            + topGroup("AVIF",
                avif(0x100, "Strength", "Strength", "Strength is a measure of your raw physical power.")
                    + avif(0x101, "SmallGuns", "Guns", "Skill with pistols and rifles."));

        OFWorld::ESMStore store;
        Loading::Listener listener;
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esm", nullptr, &encoder);
        reader.setModIndex(0);
        store.loadESM4(reader, &listener);
        store.setUp();

        OFScript::FalloutScripts scripts(store, FalloutScript::Game::NewVegas, 0x14);
        const OFScript::FalloutScripts::ActorValueText strength = scripts.actorValueText("Strength", "Fallback");
        EXPECT_EQ(strength.mName, "Strength");
        EXPECT_EQ(strength.mDescription, "Strength is a measure of your raw physical power.");

        const OFScript::FalloutScripts::ActorValueText guns = scripts.actorValueText("smallguns", "Fallback");
        EXPECT_EQ(guns.mName, "Guns");
        EXPECT_EQ(guns.mDescription, "Skill with pistols and rifles.");

        const OFScript::FalloutScripts::ActorValueText none = scripts.actorValueText("Nothing", "Fallback");
        EXPECT_EQ(none.mName, "Fallback");
        EXPECT_EQ(none.mDescription, "");
    }
}
