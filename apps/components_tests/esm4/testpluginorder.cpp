#include <components/esm4/common.hpp>
#include <components/esm4/pluginorder.hpp>
#include <components/files/collections.hpp>
#include <components/testing/expecterror.hpp>
#include <components/testing/util.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    ESM4::PluginInfo plugin(std::string name, bool master, std::vector<std::string> masters = {})
    {
        ESM4::PluginInfo info;
        info.mName = std::move(name);
        info.mMaster = master;
        info.mMasters = std::move(masters);
        return info;
    }

    std::vector<std::string> names(const std::vector<ESM4::PluginInfo>& plugins, const ESM4::PluginOrder& order)
    {
        std::vector<std::string> result;
        for (const std::size_t index : order.mOrder)
            result.push_back(plugins[index].mName);
        return result;
    }

    TEST(ESM4PluginOrderTest, keepsAListThatIsInOrder)
    {
        const std::vector<ESM4::PluginInfo> list = { plugin("FalloutNV.esm", true),
            plugin("DeadMoney.esm", true, { "FalloutNV.esm" }), plugin("Mod.esp", false, { "FalloutNV.esm" }) };
        const ESM4::PluginOrder order = ESM4::orderPlugins(list);
        EXPECT_THAT(names(list, order), ElementsAre("FalloutNV.esm", "DeadMoney.esm", "Mod.esp"));
        EXPECT_THAT(order.mMoves, IsEmpty());
    }

    TEST(ESM4PluginOrderTest, loadsMastersBeforeThePluginsThatAreNot)
    {
        const std::vector<ESM4::PluginInfo> list = { plugin("Mod.esp", false), plugin("FalloutNV.esm", true),
            plugin("Other.esp", false), plugin("Pack.esm", true, { "FalloutNV.esm" }) };
        const ESM4::PluginOrder order = ESM4::orderPlugins(list);
        // Each group keeps the order of the list
        EXPECT_THAT(names(list, order), ElementsAre("FalloutNV.esm", "Pack.esm", "Mod.esp", "Other.esp"));
        // Only the masters moved
        ASSERT_THAT(order.mMoves, SizeIs(2));
        EXPECT_THAT(order.mMoves[0], HasSubstr("FalloutNV.esm moves from place 2 to place 1 of 4"));
        EXPECT_THAT(order.mMoves[0], HasSubstr("it is flagged as a master"));
        EXPECT_THAT(order.mMoves[1], HasSubstr("Pack.esm moves from place 4 to place 2 of 4"));
    }

    TEST(ESM4PluginOrderTest, loadsAMasterBeforeThePluginThatListsIt)
    {
        const std::vector<ESM4::PluginInfo> list = { plugin("Patch.esp", false, { "Mod.esp" }),
            plugin("FalloutNV.esm", true), plugin("Mod.esp", false, { "FalloutNV.esm" }) };
        const ESM4::PluginOrder order = ESM4::orderPlugins(list);
        EXPECT_THAT(names(list, order), ElementsAre("FalloutNV.esm", "Mod.esp", "Patch.esp"));
        ASSERT_THAT(order.mMoves, Not(IsEmpty()));
        EXPECT_THAT(
            order.mMoves, Contains(HasSubstr("Mod.esp moves from place 3 to place 2: it is a master of Patch.esp")));
    }

    TEST(ESM4PluginOrderTest, comparesNamesWithoutCase)
    {
        const std::vector<ESM4::PluginInfo> list
            = { plugin("Mod.esp", false, { "FALLOUTNV.ESM" }), plugin("falloutnv.esm", true) };
        EXPECT_THAT(names(list, ESM4::orderPlugins(list)), ElementsAre("falloutnv.esm", "Mod.esp"));
    }

    TEST(ESM4PluginOrderTest, namesEveryMissingMaster)
    {
        const std::vector<ESM4::PluginInfo> list = { plugin("FalloutNV.esm", true),
            plugin("A.esp", false, { "FalloutNV.esm", "Gone.esm" }), plugin("B.esp", false, { "Lost.esp" }) };
        try
        {
            ESM4::orderPlugins(list);
            FAIL() << "a missing master must be an error";
        }
        catch (const std::runtime_error& e)
        {
            EXPECT_THAT(std::string(e.what()), HasSubstr("A.esp needs Gone.esm, which is not among the plugins"));
            EXPECT_THAT(std::string(e.what()), HasSubstr("B.esp needs Lost.esp"));
            EXPECT_THAT(std::string(e.what()), Not(HasSubstr("FalloutNV.esm, which")));
        }
    }

    TEST(ESM4PluginOrderTest, refusesAPluginListedTwice)
    {
        const std::vector<ESM4::PluginInfo> list = { plugin("A.esm", true), plugin("a.ESM", true) };
        EXPECT_ERROR(ESM4::orderPlugins(list), "a.ESM is listed twice");
    }

    TEST(ESM4PluginOrderTest, refusesMastersThatDependOnEachOther)
    {
        const std::vector<ESM4::PluginInfo> list
            = { plugin("A.esm", true, { "B.esm" }), plugin("B.esm", true, { "A.esm" }) };
        EXPECT_ERROR(ESM4::orderPlugins(list), "circle: A.esm needs B.esm needs A.esm");
    }

    TEST(ESM4PluginOrderTest, refusesAPluginThatListsItself)
    {
        const std::vector<ESM4::PluginInfo> list = { plugin("A.esp", false, { "a.esp" }) };
        EXPECT_ERROR(ESM4::orderPlugins(list), "A.esp needs A.esp");
    }

    TEST(ESM4PluginOrderTest, refusesMorePluginsThanAFormIdHasRoomFor)
    {
        std::vector<ESM4::PluginInfo> list;
        for (std::size_t i = 0; i <= ESM4::sMaxPlugins; ++i)
            list.push_back(plugin("p" + std::to_string(i) + ".esp", false));
        EXPECT_ERROR(ESM4::orderPlugins(list), "at most 255 can load together");
        list.pop_back();
        EXPECT_EQ(ESM4::orderPlugins(list).mOrder.size(), ESM4::sMaxPlugins);
    }

    TEST(ESM4PluginOrderTest, readsTheNamesInAPluginsFile)
    {
        EXPECT_THAT(ESM4::parsePluginsFile("# comment\r\nFalloutNV.esm\r\n\r\n  DeadMoney.esm  \nMod.esp"),
            ElementsAre("FalloutNV.esm", "DeadMoney.esm", "Mod.esp"));
        EXPECT_THAT(ESM4::parsePluginsFile(""), IsEmpty());
        EXPECT_THAT(ESM4::parsePluginsFile("\xEF\xBB\xBFFalloutNV.esm\n"), ElementsAre("FalloutNV.esm"));
    }

    TEST(ESM4PluginOrderTest, readsOnlyTheMarkedNamesOfAPluginsFileThatMarksSome)
    {
        EXPECT_THAT(ESM4::parsePluginsFile("*FalloutNV.esm\nOff.esp\n* DeadMoney.esm\n*\n"),
            ElementsAre("FalloutNV.esm", "DeadMoney.esm"));
    }

    std::string headerWith(
        std::uint32_t flags, const std::vector<std::string>& masters, const std::vector<std::uint64_t>& sizes = {})
    {
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 0);
        append<std::uint32_t>(hedr, 0x800);
        std::string data = subRecord("HEDR", hedr);
        for (std::size_t i = 0; i < masters.size(); ++i)
            data
                += zString("MAST", masters[i]) + valueSubRecord<std::uint64_t>("DATA", i < sizes.size() ? sizes[i] : 0);
        return record("TES4", 0, data, flags);
    }

    void write(const std::filesystem::path& path, const std::string& bytes)
    {
        std::ofstream(path, std::ios::binary) << bytes;
    }

    TEST(ESM4PluginOrderTest, readsTheFlagAndTheMastersOfAPluginHeader)
    {
        const auto path = TestingOpenMW::outputFilePath("test_pluginorder_master.esm");
        write(path, headerWith(ESM4::Rec_ESM, { "FalloutNV.esm", "DeadMoney.esm" }, { 1234, 5678 }));
        const std::optional<ESM4::PluginInfo> info = ESM4::readPluginInfo(path, "Pack.esm");
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->mName, "Pack.esm");
        EXPECT_TRUE(info->mMaster);
        EXPECT_THAT(info->mMasters, ElementsAre("FalloutNV.esm", "DeadMoney.esm"));
        EXPECT_THAT(info->mMasterSizes, ElementsAre(1234, 5678));

        const auto addon = TestingOpenMW::outputFilePath("test_pluginorder_addon.esp");
        write(addon, headerWith(0, {}));
        const std::optional<ESM4::PluginInfo> plain = ESM4::readPluginInfo(addon, "Mod.esp");
        ASSERT_TRUE(plain.has_value());
        EXPECT_FALSE(plain->mMaster);
        EXPECT_THAT(plain->mMasters, IsEmpty());
    }

    TEST(ESM4PluginOrderTest, leavesAFileOfAnotherFormatAlone)
    {
        const auto path = TestingOpenMW::outputFilePath("test_pluginorder_morrowind.esp");
        write(path, std::string("TES3") + std::string(40, '\0'));
        EXPECT_FALSE(ESM4::readPluginInfo(path, "morrowind.esp").has_value());
    }

    TEST(ESM4PluginOrderTest, refusesAFileThatCannotBeRead)
    {
        EXPECT_THROW(ESM4::readPluginInfo(TestingOpenMW::outputFilePath("test_pluginorder_missing.esp"), "gone.esp"),
            std::runtime_error);
        const auto cut = TestingOpenMW::outputFilePath("test_pluginorder_cut.esp");
        write(cut, "TES4");
        EXPECT_THROW(ESM4::readPluginInfo(cut, "cut.esp"), std::runtime_error);
    }

    TEST(ESM4PluginOrderTest, ordersTheContentFilesAndLeavesTheOtherContentWhereItIs)
    {
        const auto dir = TestingOpenMW::outputDirPath("pluginorder_content");
        write(dir / "Base.esm", headerWith(ESM4::Rec_ESM, {}));
        write(dir / "Pack.esm", headerWith(ESM4::Rec_ESM, { "Base.esm" }));
        write(dir / "Mod.esp", headerWith(0, { "Pack.esm" }));
        write(dir / "Old.esp", std::string("TES3") + std::string(40, '\0'));
        const Files::Collections collections(Files::PathContainer{ dir });

        const std::vector<std::string> content
            = { "builtin.omwscripts", "Mod.esp", "Pack.esm", "Old.esp", "base.ESM", "scripts.omwscripts" };
        // The Morrowind plugin and the scripts keep their places
        EXPECT_THAT(ESM4::orderContentFiles(content, collections),
            ElementsAre("builtin.omwscripts", "base.ESM", "Pack.esm", "Old.esp", "Mod.esp", "scripts.omwscripts"));
    }

    TEST(ESM4PluginOrderTest, refusesAContentFileThatIsNotThere)
    {
        const auto dir = TestingOpenMW::outputDirPath("pluginorder_missing");
        write(dir / "Base.esm", headerWith(ESM4::Rec_ESM, {}));
        const Files::Collections collections(Files::PathContainer{ dir });
        EXPECT_ERROR(ESM4::orderContentFiles({ "Base.esm", "Lost.esp" }, collections),
            "Failed loading Lost.esp: the content file does not exist");
    }

    TEST(ESM4PluginOrderTest, addsAMasterThatTheListLacksAndTheDirectoriesHave)
    {
        const auto dir = TestingOpenMW::outputDirPath("pluginorder_addmaster");
        write(dir / "Base.esm", headerWith(ESM4::Rec_ESM, {}));
        write(dir / "Mod.esp", headerWith(0, { "base.esm" }));
        const Files::Collections collections(Files::PathContainer{ dir });
        // The master takes the place after the last plugin of the list, and loads first
        EXPECT_THAT(ESM4::orderContentFiles({ "Mod.esp" }, collections), ElementsAre("Base.esm", "Mod.esp"));
        EXPECT_THAT(ESM4::orderContentFiles({ "builtin.omwscripts", "Mod.esp", "scripts.omwscripts" }, collections),
            ElementsAre("builtin.omwscripts", "Base.esm", "Mod.esp", "scripts.omwscripts"));
    }

    TEST(ESM4PluginOrderTest, refusesAMasterThatIsNeitherListedNorInTheDirectories)
    {
        const auto dir = TestingOpenMW::outputDirPath("pluginorder_nomaster");
        write(dir / "Mod.esp", headerWith(0, { "Gone.esm" }));
        const Files::Collections collections(Files::PathContainer{ dir });
        EXPECT_ERROR(ESM4::orderContentFiles({ "Mod.esp" }, collections),
            "Mod.esp needs Gone.esm, which is neither listed nor in the data directories");
    }

    TEST(ESM4PluginOrderTest, ordersByTheTimeTheFilesWereWrittenWhenAsked)
    {
        const auto dir = TestingOpenMW::outputDirPath("pluginorder_time");
        write(dir / "A.esp", headerWith(0, {}));
        write(dir / "B.esp", headerWith(0, {}));
        write(dir / "C.esm", headerWith(ESM4::Rec_ESM, {}));
        write(dir / "D.esp", headerWith(0, {}));
        const auto now = std::filesystem::file_time_type::clock::now();
        std::filesystem::last_write_time(dir / "A.esp", now);
        std::filesystem::last_write_time(dir / "B.esp", now - std::chrono::hours(2));
        std::filesystem::last_write_time(dir / "C.esm", now - std::chrono::hours(1));
        std::filesystem::last_write_time(dir / "D.esp", now);
        const Files::Collections collections(Files::PathContainer{ dir });
        const std::vector<std::string> content = { "A.esp", "B.esp", "C.esm", "D.esp" };

        EXPECT_THAT(ESM4::orderContentFiles(content, collections), ElementsAre("C.esm", "A.esp", "B.esp", "D.esp"));
        // The master first, then the oldest; the files written at the same time keep the order of the list
        EXPECT_THAT(ESM4::orderContentFiles(content, collections, ESM4::ContentOrder::FileTime),
            ElementsAre("C.esm", "B.esp", "A.esp", "D.esp"));
    }

    TEST(ESM4PluginOrderTest, dropsThePluginsThatAreNotInTheDirectories)
    {
        const auto dir = TestingOpenMW::outputDirPath("pluginorder_drop");
        write(dir / "Base.esm", headerWith(ESM4::Rec_ESM, {}));
        const Files::Collections collections(Files::PathContainer{ dir });
        EXPECT_THAT(
            ESM4::dropMissingPlugins({ "base.ESM", "Gone.esp", "notes.txt" }, collections), ElementsAre("base.ESM"));
    }

    TEST(ESM4PluginOrderTest, warnsOfAPluginMadeForAnotherVersionOfItsMaster)
    {
        ESM4::PluginInfo mod = plugin("Mod.esp", false, { "A.esm", "B.esm", "C.esm", "D.esm" });
        mod.mMasterSizes = { 100, 200, 0, 400 };
        ESM4::PluginInfo pack = plugin("Pack.esm", true, { "A.esm" });
        pack.mMasterSizes = { 1 };
        const auto sizeOf = [](const std::string& name) -> std::optional<std::uint64_t> {
            if (name == "A.esm")
                return 120;
            if (name == "B.esm")
                return 200;
            if (name == "C.esm")
                return 300;
            return std::nullopt;
        };
        // Only A.esm differs: B.esm is the size the plugin expects, C.esm is not said, D.esm has no file, and a master
        // is not checked
        const std::vector<std::string> warnings = ESM4::masterSizeWarnings({ mod, pack }, sizeOf);
        ASSERT_THAT(warnings, SizeIs(1));
        EXPECT_THAT(warnings[0], HasSubstr("Mod.esp was made for another version of A.esm"));
        EXPECT_THAT(warnings[0], HasSubstr("120 bytes"));
        EXPECT_THAT(warnings[0], HasSubstr("expects 100"));
    }
}
