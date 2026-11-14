#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/esm4/readerutils.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    template <class T>
    T load(std::string_view group, const std::string& data, std::uint32_t id = 0x801)
    {
        const std::vector<T> result = loadRecords<T>(group, record(group, id, data));
        EXPECT_EQ(result.size(), 1u);
        return result.empty() ? T{} : result[0];
    }

    template <class T>
    std::string loadFailure(std::string_view group, const std::string& data)
    {
        try
        {
            loadRecords<T>(group, record(group, 0x801, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    std::string formId(std::uint32_t id)
    {
        std::string result;
        append(result, id);
        return result;
    }

    std::string floats(std::size_t count, float first)
    {
        std::string result;
        for (std::size_t i = 0; i < count; ++i)
            append(result, first + static_cast<float>(i));
        return result;
    }

    /// Sub-records of the given types, each with four bytes that nobody reads.
    std::string ignored(std::initializer_list<const char*> types)
    {
        std::string result;
        for (const char* type : types)
            result += subRecord(type, bytePattern(4, 0x40));
        return result;
    }

    std::string enableParent(std::uint32_t parent, std::uint32_t flags)
    {
        std::string data;
        append(data, parent);
        append(data, flags);
        return data;
    }

    // The sub-records that the loader of a reference does not read, whichever game writes them
    const std::string sIgnoredReferenceData = ignored({ "LNAM", "XLIG", "XEMI", "XRDS", "XRGB", "XRGD", "XALP", "XPCI",
        "XLCM", "VMAD", "INAM", "PDTO", "SCHR", "SCTX", "XAPD", "XAPR", "XCVL", "XCZA", "XCZC", "XEZN", "XFVC", "XHTW",
        "XIS2", "XLCN", "XLIB", "XLRM", "XLRT", "XLTW", "XMBO", "XMBP", "XMBR", "XNDP", "XOCP", "XPOD", "XPTL", "XPPA",
        "XPRD", "XPWR", "XRMR", "XSPC", "XTNM", "XTRI", "XWCN", "XWCU", "XATR", "XHLT", "XCHG", "XHLP", "XAMT", "XAMC",
        "XRAD", "XIBS", "XORD", "XCLP", "SCDA", "RCLR", "BNAM", "MMRK", "MNAM", "NNAM", "XATO", "SCRV", "SCVR", "SLSD",
        "XSRF", "XSRD", "WMI1", "XLRL", "XASP", "XATP", "XBSD", "XCVR", "XCZR", "XLKT", "XLYR", "XMSP", "XPDD", "XPLK",
        "XRFG", "XWPG", "XWPN" });

    TEST(ESM4ReferenceTest, aReferenceHasItsBaseObjectPlacementAndOwner)
    {
        const ESM4::Reference reference = load<ESM4::Reference>("REFR",
            zString("EDID", "OFTestRef") + zString("FULL", "A test reference")
                + valueSubRecord<std::uint32_t>("NAME", 0x900) + subRecord("DATA", floats(6, 1.f))
                + valueSubRecord<float>("XSCL", 2.5f) + valueSubRecord<std::uint32_t>("XOWN", 0x810)
                + valueSubRecord<std::uint32_t>("XGLB", 0x811) + valueSubRecord<std::int32_t>("XRNK", 3)
                + subRecord("XESP", enableParent(0x812, 3)) + valueSubRecord<std::int32_t>("XCNT", 5)
                + valueSubRecord<std::uint32_t>("XTRG", 0x813) + valueSubRecord<std::uint32_t>("CNAM", 0x814));

        EXPECT_EQ(reference.mId.toUint32(), 0x801u);
        EXPECT_EQ(reference.mEditorId, "OFTestRef");
        EXPECT_EQ(reference.mFullName, "A test reference");
        EXPECT_EQ(reference.mBaseObj.toUint32(), 0x900u);
        EXPECT_EQ(reference.mPos.pos[0], 1.f);
        EXPECT_EQ(reference.mPos.pos[2], 3.f);
        EXPECT_EQ(reference.mPos.rot[0], 4.f);
        EXPECT_EQ(reference.mPos.rot[2], 6.f);
        EXPECT_EQ(reference.mScale, 2.5f);
        EXPECT_EQ(reference.mOwner.toUint32(), 0x810u);
        EXPECT_EQ(reference.mGlobal.toUint32(), 0x811u);
        EXPECT_EQ(reference.mFactionRank, 3);
        EXPECT_EQ(reference.mEsp.parent.toUint32(), 0x812u);
        EXPECT_EQ(reference.mEsp.flags, 3u);
        EXPECT_EQ(reference.mCount, 5);
        EXPECT_EQ(reference.mTargetRef.toUint32(), 0x813u);
        EXPECT_EQ(reference.mAudioLocation.toUint32(), 0x814u);
    }

    TEST(ESM4ReferenceTest, aReferenceHasTheDefaultsOfAnUnnamedPlacement)
    {
        const ESM4::Reference reference = load<ESM4::Reference>("REFR", valueSubRecord<std::uint32_t>("NAME", 0x900));

        EXPECT_EQ(reference.mScale, 1.f);
        EXPECT_EQ(reference.mCount, 1);
        EXPECT_EQ(reference.mFactionRank, -1);
        EXPECT_FALSE(reference.mIsMapMarker);
        EXPECT_FALSE(reference.mIsLocked);
        EXPECT_FALSE(reference.mOpenByDefault);
        EXPECT_TRUE(reference.mLinkedRefs.empty());
        EXPECT_FALSE(reference.mPrimitive.has_value());
        EXPECT_TRUE(reference.mOwner.isZeroOrUnset());
    }

    TEST(ESM4ReferenceTest, theOwnerHasFourBytesOrTwelveAndAnythingElseIsSkipped)
    {
        std::string twelve = formId(0x820);
        append<std::uint32_t>(twelve, 1);
        append<std::uint32_t>(twelve, 1);

        EXPECT_EQ(load<ESM4::Reference>("REFR", subRecord("XOWN", twelve) + valueSubRecord<float>("XSCL", 4.f))
                      .mOwner.toUint32(),
            0x820u);

        const ESM4::Reference other
            = load<ESM4::Reference>("REFR", subRecord("XOWN", bytePattern(6, 1)) + valueSubRecord<float>("XSCL", 4.f));
        EXPECT_TRUE(other.mOwner.isZeroOrUnset());
        EXPECT_EQ(other.mScale, 4.f);
    }

    TEST(ESM4ReferenceTest, aDoorKnowsWhereItLeadsInEveryLayoutOfItsTeleportData)
    {
        std::string destination = formId(0x830);
        destination += floats(6, 10.f);
        ASSERT_EQ(destination.size(), 28u);

        // Oblivion: the door and the position
        ESM4::Reference reference = load<ESM4::Reference>("REFR", subRecord("XTEL", destination));
        EXPECT_EQ(reference.mDoor.destDoor.toUint32(), 0x830u);
        EXPECT_EQ(reference.mDoor.destPos.pos[0], 10.f);
        EXPECT_EQ(reference.mDoor.destPos.rot[2], 15.f);
        EXPECT_EQ(reference.mDoor.flags, 0u);

        // Fallout 3, New Vegas and Skyrim: and the flags
        std::string withFlags = destination;
        append<std::uint32_t>(withFlags, 0x01);
        reference = load<ESM4::Reference>("REFR", subRecord("XTEL", withFlags));
        EXPECT_EQ(reference.mDoor.flags, 1u);
        EXPECT_TRUE(reference.mDoor.transitionInterior.isZeroOrUnset());

        // Fallout 4: and the interior that the door is the transition to
        std::string withInterior = withFlags;
        append<std::uint32_t>(withInterior, 0x840);
        reference = load<ESM4::Reference>("REFR", subRecord("XTEL", withInterior));
        EXPECT_EQ(reference.mDoor.destDoor.toUint32(), 0x830u);
        EXPECT_EQ(reference.mDoor.flags, 1u);
        EXPECT_EQ(reference.mDoor.transitionInterior.toUint32(), 0x840u);

        // Any other size is not a destination
        reference = load<ESM4::Reference>("REFR", subRecord("XTEL", bytePattern(8, 1)));
        EXPECT_TRUE(reference.mDoor.destDoor.isZeroOrUnset());
    }

    TEST(ESM4ReferenceTest, theSeedTheDistantLevelAndTheActivationHaveASizeThatIsChecked)
    {
        // Each of them has the size that the game writes, and a size that is not that and is skipped
        const ESM4::Reference reference = load<ESM4::Reference>("REFR",
            subRecord("XSED", bytePattern(1, 1)) + subRecord("XSED", bytePattern(4, 2))
                + subRecord("XSED", bytePattern(3, 3)) + subRecord("XLOD", bytePattern(12, 4))
                + subRecord("XLOD", bytePattern(5, 5)) + subRecord("XACT", bytePattern(4, 6))
                + subRecord("XACT", bytePattern(2, 7)) + valueSubRecord<std::uint32_t>("XRTM", 0x850)
                + valueSubRecord<float>("XSCL", 3.f));

        EXPECT_EQ(reference.mScale, 3.f);
    }

    TEST(ESM4ReferenceTest, aMapMarkerHasItsTypeInTwoBytesOrAFormOfAnotherGame)
    {
        // Oblivion: the type of the marker is two bytes
        const ESM4::Reference oblivion
            = load<ESM4::Reference>("REFR", subRecord("XMRK", "") + valueSubRecord<std::uint16_t>("TNAM", 0x0309));
        EXPECT_TRUE(oblivion.mIsMapMarker);
        EXPECT_EQ(oblivion.mMapMarker, 0x0309u);

        // Fallout 3: a form, which is not kept in the marker
        const ESM4::Reference fallout
            = load<ESM4::Reference>("REFR", subRecord("XMRK", "") + valueSubRecord<std::uint32_t>("TNAM", 0x860));
        EXPECT_TRUE(fallout.mIsMapMarker);
        EXPECT_EQ(fallout.mMapMarker, 0u);
    }

    TEST(ESM4ReferenceTest, aRadioHasItsRangeItsStaticAndItsPosition)
    {
        std::string radio;
        append<float>(radio, 4500.f);
        append<std::uint32_t>(radio, 2);
        append<float>(radio, 0.25f);
        append<std::uint32_t>(radio, 0x870);

        const ESM4::Reference reference
            = load<ESM4::Reference>("REFR", subRecord("XRDO", radio) + valueSubRecord<std::uint32_t>("SCRO", 0x871));

        EXPECT_TRUE(reference.mRadio.present);
        EXPECT_EQ(reference.mRadio.rangeRadius, 4500.f);
        EXPECT_EQ(reference.mRadio.broadcastRange, 2u);
        EXPECT_EQ(reference.mRadio.staticPercentage, 0.25f);
        EXPECT_EQ(reference.mRadio.posReference.toUint32(), 0x870u);
    }

    TEST(ESM4ReferenceTest, aDoorThatStartsOpenHasAnEmptyOpenSubrecord)
    {
        EXPECT_TRUE(load<ESM4::Reference>("REFR", subRecord("ONAM", "")).mOpenByDefault);

        // Anything in the subrecord is not that flag
        EXPECT_FALSE(load<ESM4::Reference>("REFR", subRecord("ONAM", bytePattern(4, 1))).mOpenByDefault);
    }

    TEST(ESM4ReferenceTest, aLockOfOblivionHasFourBytesAfterItsFlags)
    {
        std::string lock;
        append<std::uint8_t>(lock, 25);
        lock.append(3, '\0');
        append<std::uint32_t>(lock, 0x880);
        append<std::uint32_t>(lock, 0x04);
        lock.append(4, '\1');
        ASSERT_EQ(lock.size(), 16u);

        const ESM4::Reference reference
            = load<ESM4::Reference>("REFR", subRecord("XLOC", lock) + valueSubRecord<float>("XSCL", 6.f));

        EXPECT_TRUE(reference.mIsLocked);
        EXPECT_EQ(static_cast<std::uint8_t>(reference.mLockLevel), 25);
        EXPECT_EQ(reference.mKey.toUint32(), 0x880u);
        EXPECT_EQ(reference.mLockFlags, 0x04u);
        EXPECT_EQ(reference.mScale, 6.f);
    }

    TEST(ESM4ReferenceTest, theSubrecordsOfTheOtherGamesAreSkipped)
    {
        const ESM4::Reference reference = load<ESM4::Reference>("REFR",
            valueSubRecord<std::uint32_t>("NAME", 0x900) + sIgnoredReferenceData + valueSubRecord<float>("XSCL", 7.f));

        EXPECT_EQ(reference.mBaseObj.toUint32(), 0x900u);
        EXPECT_EQ(reference.mScale, 7.f);
    }

    TEST(ESM4ReferenceTest, aSubrecordThatNoGameWritesIsAnError)
    {
        EXPECT_THAT(loadFailure<ESM4::Reference>("REFR", subRecord("ZZZZ", "")), HasSubstr("Unknown subrecord ZZZZ"));
    }

    TEST(ESM4ReferenceTest, aBlankReferenceKeepsItsDefaults)
    {
        ESM4::Reference reference;
        reference.blank();

        EXPECT_EQ(reference.mScale, 1.f);
        EXPECT_EQ(reference.mCount, 1);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // ACHR and ACRE

    TEST(ESM4ActorReferenceTest, aPlacedCharacterHasItsBaseObjectPlacementAndOwner)
    {
        const ESM4::ActorCharacter actor = load<ESM4::ActorCharacter>("ACHR",
            zString("EDID", "OFTestActor") + zString("FULL", "A placed character")
                + valueSubRecord<std::uint32_t>("NAME", 0x900) + subRecord("DATA", floats(6, 1.f))
                + valueSubRecord<float>("XSCL", 1.5f) + valueSubRecord<std::uint32_t>("XOWN", 0x810)
                + subRecord("XESP", enableParent(0x812, 1)) + valueSubRecord<std::int32_t>("XCNT", 3)
                + valueSubRecord<std::uint32_t>("XMRC", 0x890));

        EXPECT_EQ(actor.mId.toUint32(), 0x801u);
        EXPECT_EQ(actor.mEditorId, "OFTestActor");
        EXPECT_EQ(actor.mFullName, "A placed character");
        EXPECT_EQ(actor.mBaseObj.toUint32(), 0x900u);
        EXPECT_EQ(actor.mPos.pos[1], 2.f);
        EXPECT_EQ(actor.mPos.rot[1], 5.f);
        EXPECT_EQ(actor.mScale, 1.5f);
        EXPECT_EQ(actor.mOwner.toUint32(), 0x810u);
        EXPECT_EQ(actor.mEsp.parent.toUint32(), 0x812u);
        EXPECT_EQ(actor.mEsp.flags, 1u);
        EXPECT_EQ(actor.mCount, 3);
        EXPECT_EQ(actor.mMerchantContainer.toUint32(), 0x890u);
    }

    TEST(ESM4ActorReferenceTest, aPlacedCreatureReadsTheSameSubrecords)
    {
        const ESM4::ActorCreature creature = load<ESM4::ActorCreature>(
            "ACRE", valueSubRecord<std::uint32_t>("NAME", 0x901) + valueSubRecord<float>("XSCL", 0.5f));

        EXPECT_EQ(creature.mBaseObj.toUint32(), 0x901u);
        EXPECT_EQ(creature.mScale, 0.5f);
    }

    TEST(ESM4ActorReferenceTest, theOwnerHasFourBytesOrTwelveAndAnythingElseIsSkipped)
    {
        std::string twelve = formId(0x820);
        append<std::uint32_t>(twelve, 1);
        append<std::uint32_t>(twelve, 1);

        EXPECT_EQ(load<ESM4::ActorCharacter>("ACHR", subRecord("XOWN", twelve)).mOwner.toUint32(), 0x820u);
        EXPECT_TRUE(load<ESM4::ActorCharacter>("ACHR", subRecord("XOWN", bytePattern(6, 1))).mOwner.isZeroOrUnset());
    }

    TEST(ESM4ActorReferenceTest, aLinkNamesAKeywordAndAReferenceOrTheReferenceAlone)
    {
        std::string keyworded = formId(0x830);
        append<std::uint32_t>(keyworded, 0x831);

        const ESM4::ActorCharacter actor = load<ESM4::ActorCharacter>("ACHR",
            subRecord("XLKR", keyworded) + valueSubRecord<std::uint32_t>("XLKR", 0x832)
                + subRecord("XLKR", bytePattern(6, 1)));

        ASSERT_EQ(actor.mLinkedRefs.size(), 2u);
        EXPECT_EQ(actor.mLinkedRefs[0].keyword.toUint32(), 0x830u);
        EXPECT_EQ(actor.mLinkedRefs[0].reference.toUint32(), 0x831u);
        EXPECT_TRUE(actor.mLinkedRefs[1].keyword.isZeroOrUnset());
        EXPECT_EQ(actor.mLinkedRefs[1].reference.toUint32(), 0x832u);
    }

    TEST(ESM4ActorReferenceTest, aPlacedActorKeepsItsOwnEncounterZoneAndLevelModifier)
    {
        const ESM4::ActorCharacter actor = load<ESM4::ActorCharacter>("ACHR",
            valueSubRecord<std::uint32_t>("XEZN", 0x910) + valueSubRecord<std::int32_t>("XLCM", 2));
        EXPECT_EQ(actor.mEncounterZone.toUint32(), 0x910u);
        EXPECT_EQ(actor.mLevelModifier, 2);

        const ESM4::ActorCreature creature = load<ESM4::ActorCreature>("ACRE", valueSubRecord<std::int32_t>("XLCM", 3));
        EXPECT_TRUE(creature.mEncounterZone.isZeroOrUnset());
        EXPECT_EQ(creature.mLevelModifier, 3);

        // An actor with neither has no zone and no modifier
        const ESM4::ActorCharacter plain = load<ESM4::ActorCharacter>("ACHR", valueSubRecord<float>("XSCL", 1.f));
        EXPECT_TRUE(plain.mEncounterZone.isZeroOrUnset());
        EXPECT_EQ(plain.mLevelModifier, -1);
    }

    TEST(ESM4ActorReferenceTest, aMerchantContainerOfAnotherSizeIsSkipped)
    {
        const ESM4::ActorCharacter actor = load<ESM4::ActorCharacter>(
            "ACHR", subRecord("XMRC", bytePattern(8, 1)) + valueSubRecord<float>("XSCL", 2.f));

        EXPECT_TRUE(actor.mMerchantContainer.isZeroOrUnset());
        EXPECT_EQ(actor.mScale, 2.f);
    }

    TEST(ESM4ActorReferenceTest, theSubrecordsOfTheOtherGamesAreSkipped)
    {
        const ESM4::ActorCharacter actor = load<ESM4::ActorCharacter>("ACHR",
            ignored({ "XAPD", "XAPR", "XHOR", "XLCN", "XLRT", "XPRD", "XPPA", "INAM", "PDTO", "XIS2",
                "XPCI", "XLOD", "VMAD", "XLRL", "XRDS", "XIBS", "SCHR", "TNAM", "XATO", "MNAM", "XATP", "XEMI", "XFVC",
                "XHLT", "XHTW", "XLKT", "XLYR", "XMBR", "XMSP", "XPLK", "XRFG", "XRNK", "XRGD", "XRGB", "XHRS" })
                + valueSubRecord<float>("XSCL", 9.f));

        EXPECT_EQ(actor.mScale, 9.f);
    }

    TEST(ESM4ActorReferenceTest, aSubrecordThatNoGameWritesIsAnError)
    {
        EXPECT_THAT(
            loadFailure<ESM4::ActorCharacter>("ACHR", subRecord("ZZZZ", "")), HasSubstr("Unknown subrecord ZZZZ"));
    }

    // ----------------------------------------------------------------------------------------------------------------
    // CELL

    std::string lightingData(bool withFogPower)
    {
        std::string data;
        append<std::uint32_t>(data, 0x11223344); // ambient
        append<std::uint32_t>(data, 0x55667788); // directional
        append<std::uint32_t>(data, 0x99AABBCC); // fog colour
        append<float>(data, 1.5f); // fog near
        append<float>(data, 2.5f); // fog far
        append<std::int32_t>(data, 10); // rotation xy
        append<std::int32_t>(data, 20); // rotation z
        append<float>(data, 0.5f); // fog direction fade
        append<float>(data, 3500.f); // fog clip distance
        if (withFogPower)
            append<float>(data, 2.f);
        return data;
    }

    TEST(ESM4CellTest, anExteriorCellHasItsGridAndItsRegions)
    {
        std::string grid;
        append<std::int32_t>(grid, -4);
        append<std::int32_t>(grid, 7);
        std::string regions = formId(0x910);
        regions += formId(0x911);
        regions += formId(0x912);

        const ESM4::Cell cell = load<ESM4::Cell>("CELL",
            zString("EDID", "OFTestCell") + subRecord("XCLC", grid) + zString("FULL", "A test cell")
                + valueSubRecord<std::uint16_t>("DATA", 0x0020) + subRecord("XCLR", regions));

        EXPECT_EQ(cell.mId, ESM::RefId(ESM::FormId::fromUint32(0x801)));
        EXPECT_EQ(cell.mEditorId, "OFTestCell");
        EXPECT_EQ(cell.mFullName, "A test cell");
        EXPECT_EQ(cell.mX, -4);
        EXPECT_EQ(cell.mY, 7);
        EXPECT_EQ(cell.mCellFlags, 0x0020u);
        ASSERT_EQ(cell.mRegions.size(), 3u);
        EXPECT_EQ(cell.mRegions[0].toUint32(), 0x910u);
        EXPECT_EQ(cell.mRegions[2].toUint32(), 0x912u);
        EXPECT_TRUE(cell.isExterior());
        EXPECT_EQ(cell.getGridX(), -4);
        EXPECT_EQ(cell.getGridY(), 7);
    }

    TEST(ESM4CellTest, theGridOfACellMayCarryFlags)
    {
        std::string grid;
        append<std::int32_t>(grid, 3);
        append<std::int32_t>(grid, -9);
        append<std::uint32_t>(grid, 0x05);

        const ESM4::Cell cell = load<ESM4::Cell>("CELL", subRecord("XCLC", grid));

        EXPECT_EQ(cell.mX, 3);
        EXPECT_EQ(cell.mY, -9);
    }

    TEST(ESM4CellTest, theFlagsAreOneByteOrTwoAndAnyOtherSizeIsAnError)
    {
        const ESM4::Cell interior = load<ESM4::Cell>("CELL", valueSubRecord<std::uint8_t>("DATA", 0x01));
        EXPECT_EQ(interior.mCellFlags, 0x01u);
        EXPECT_FALSE(interior.isExterior());

        EXPECT_EQ(load<ESM4::Cell>("CELL", valueSubRecord<std::uint16_t>("DATA", 0x0181)).mCellFlags, 0x0181u);

        EXPECT_THAT(loadFailure<ESM4::Cell>("CELL", subRecord("DATA", bytePattern(3, 1))),
            HasSubstr("unexpected DATA flag size"));
    }

    TEST(ESM4CellTest, anInteriorCellIsLocatedByItsOwnForm)
    {
        const ESM4::Cell cell = load<ESM4::Cell>("CELL", valueSubRecord<std::uint8_t>("DATA", 0x01), 0x7A3);

        EXPECT_EQ(cell.getExteriorCellLocation().mWorldspace, ESM::RefId(ESM::FormId::fromUint32(0x7A3)));
    }

    TEST(ESM4CellTest, theOwnerHasFourBytesOrTwelveAndAnythingElseIsSkipped)
    {
        std::string twelve = formId(0x920);
        append<std::uint32_t>(twelve, 1);
        append<std::uint32_t>(twelve, 1);

        EXPECT_EQ(load<ESM4::Cell>("CELL", subRecord("XOWN", formId(0x921))).mOwner.toUint32(), 0x921u);
        EXPECT_EQ(load<ESM4::Cell>("CELL", subRecord("XOWN", twelve)).mOwner.toUint32(), 0x920u);
        EXPECT_TRUE(load<ESM4::Cell>("CELL", subRecord("XOWN", bytePattern(6, 1))).mOwner.isZeroOrUnset());
    }

    TEST(ESM4CellTest, aCellHasItsGlobalClimateWaterAndMusic)
    {
        const ESM4::Cell cell = load<ESM4::Cell>("CELL",
            valueSubRecord<std::uint32_t>("XGLB", 0x930) + valueSubRecord<std::uint32_t>("XCCM", 0x931)
                + valueSubRecord<std::uint32_t>("XCWT", 0x932) + valueSubRecord<float>("XCLW", 128.f)
                + valueSubRecord<std::uint8_t>("XCMT", 2) + valueSubRecord<std::uint32_t>("LTMP", 0x933)
                + valueSubRecord<std::uint32_t>("LNAM", 0x0F) + valueSubRecord<std::uint32_t>("XCMO", 0x934)
                + valueSubRecord<std::uint32_t>("XCAS", 0x935) + valueSubRecord<std::uint32_t>("XEZN", 0x936));

        EXPECT_EQ(cell.mGlobal.toUint32(), 0x930u);
        EXPECT_EQ(cell.mClimate.toUint32(), 0x931u);
        EXPECT_EQ(cell.mWater.toUint32(), 0x932u);
        EXPECT_EQ(cell.mWaterHeight, 128.f);
        EXPECT_EQ(cell.mMusicType, 2u);
        EXPECT_EQ(cell.mLightingTemplate.toUint32(), 0x933u);
        EXPECT_EQ(cell.mLightingTemplateFlags, 0x0Fu);
        EXPECT_EQ(cell.mMusic.toUint32(), 0x934u);
        EXPECT_EQ(cell.mAcousticSpace.toUint32(), 0x935u);
        EXPECT_EQ(cell.mEncounterZone.toUint32(), 0x936u);
    }

    TEST(ESM4CellTest, aCellHasNoWaterHeightUntilItIsGiven)
    {
        const ESM4::Cell cell = load<ESM4::Cell>("CELL", valueSubRecord<std::uint8_t>("DATA", 0x01));

        EXPECT_EQ(cell.mWaterHeight, ESM4::Cell::sInvalidWaterLevel);
        EXPECT_FALSE(cell.hasWaterHeight());
    }

    TEST(ESM4CellTest, theLightingOfOblivionHasNineFieldsAndKeepsTheDefaultFogPower)
    {
        const ESM4::Cell cell = load<ESM4::Cell>("CELL", subRecord("XCLL", lightingData(false)));

        EXPECT_EQ(cell.mLighting.ambient, 0x11223344u);
        EXPECT_EQ(cell.mLighting.directional, 0x55667788u);
        EXPECT_EQ(cell.mLighting.fogColor, 0x99AABBCCu);
        EXPECT_EQ(cell.mLighting.fogNear, 1.5f);
        EXPECT_EQ(cell.mLighting.fogFar, 2.5f);
        EXPECT_EQ(cell.mLighting.rotationXY, 10);
        EXPECT_EQ(cell.mLighting.rotationZ, 20);
        EXPECT_EQ(cell.mLighting.fogDirFade, 0.5f);
        EXPECT_EQ(cell.mLighting.fogClipDist, 3500.f);
        EXPECT_EQ(cell.mLighting.fogPower, 1.f);
    }

    TEST(ESM4CellTest, theLightingOfTheOtherGamesHasTheFogPowerAndAMoreWhichIsSkipped)
    {
        const ESM4::Cell fallout = load<ESM4::Cell>("CELL", subRecord("XCLL", lightingData(true)));
        EXPECT_EQ(fallout.mLighting.fogClipDist, 3500.f);
        EXPECT_EQ(fallout.mLighting.fogPower, 2.f);

        // Skyrim writes 92 bytes and Fallout 4 136, of which the loader reads 40; the sub-record after is still found
        const ESM4::Cell skyrim = load<ESM4::Cell>("CELL",
            subRecord("XCLL", lightingData(true) + std::string(52, '\x7F'))
                + valueSubRecord<std::uint32_t>("XCMO", 0x940));
        EXPECT_EQ(skyrim.mLighting.fogPower, 2.f);
        EXPECT_EQ(skyrim.mMusic.toUint32(), 0x940u);

        const ESM4::Cell fallout4 = load<ESM4::Cell>("CELL",
            subRecord("XCLL", lightingData(true) + std::string(96, '\x7F'))
                + valueSubRecord<std::uint32_t>("XCMO", 0x941));
        EXPECT_EQ(fallout4.mLighting.ambient, 0x11223344u);
        EXPECT_EQ(fallout4.mMusic.toUint32(), 0x941u);
    }

    TEST(ESM4CellTest, lightingOfAnotherSizeIsSkipped)
    {
        const ESM4::Cell cell = load<ESM4::Cell>(
            "CELL", subRecord("XCLL", bytePattern(12, 1)) + valueSubRecord<std::uint32_t>("XCMO", 0x942));

        EXPECT_EQ(cell.mLighting.ambient, 0u);
        EXPECT_EQ(cell.mMusic.toUint32(), 0x942u);
    }

    TEST(ESM4CellTest, theSubrecordsThatTheLoaderDoesNotReadAreSkipped)
    {
        const ESM4::Cell cell = load<ESM4::Cell>("CELL",
            ignored({ "TVDT", "MHDT", "XCGD", "XNAM", "XLCN", "XWCS", "XWCU", "XWCN", "XCIM", "XWEM", "XILL", "XRNK",
                "XCET", "IMPF", "CNAM", "PCMB", "RVIS", "VISI", "XGDR", "XILW", "XCRI", "XPRI", "ZNAM" })
                + valueSubRecord<std::uint32_t>("XCMO", 0x943));

        EXPECT_EQ(cell.mMusic.toUint32(), 0x943u);
    }

    TEST(ESM4CellTest, aSubrecordThatNoGameWritesIsAnError)
    {
        EXPECT_THAT(loadFailure<ESM4::Cell>("CELL", subRecord("ZZZZ", "")), HasSubstr("Unknown subrecord ZZZZ"));
    }

    TEST(ESM4CellTest, anEditorIdThatTheFileEndsInsideOfIsAnError)
    {
        // The record header promises the null terminator, which the file does not have
        std::string data = zString("EDID", "AB");
        EXPECT_THROW(loadRecords<ESM4::Cell>("CELL", record("CELL", 0x801, data), 1), std::runtime_error);
    }

    TEST(ESM4CellTest, aBlankCellKeepsItsDefaults)
    {
        ESM4::Cell cell;
        cell.blank();

        EXPECT_EQ(cell.mWaterHeight, ESM4::Cell::sInvalidWaterLevel);
    }

    /// Load a cell that is the child of a group of the given type and label (an exterior sub-block in a plugin of
    /// Oblivion), then say what the reader remembers about the grid.
    struct GroupedCell
    {
        ESM4::Cell mCell;
        bool mHasGrid = false;
        std::int16_t mGridX = 0;
        std::int16_t mGridY = 0;
    };

    GroupedCell loadCellInGroup(std::int32_t groupType, std::uint32_t label, const std::string& cellData)
    {
        const std::string cell = record("CELL", 0x801, cellData);
        std::string group("GRUP");
        append<std::uint32_t>(group, static_cast<std::uint32_t>(20 + cell.size()));
        append(group, label);
        append(group, groupType);
        append<std::uint16_t>(group, 0);
        append<std::uint16_t>(group, 0);
        group += cell;

        ESM4::Reader reader(std::make_unique<std::istringstream>(header() + group), "synthetic.esp", nullptr, nullptr);
        reader.setModIndex(0);
        GroupedCell result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.mCell.load(r);
                result.mHasGrid = r.hasCellGrid();
                if (result.mHasGrid)
                {
                    result.mGridX = std::get<ESM4::Grid>(r.currCellGrid()).x;
                    result.mGridY = std::get<ESM4::Grid>(r.currCellGrid()).y;
                }
                return true;
            },
            [](ESM4::Reader&) {});
        return result;
    }

    TEST(ESM4CellTest, theCellWithoutAGridAtTheOriginOfAWorldIsPlacedThere)
    {
        // Some plugins have no XCLC in the cell 0, 0; the group that holds it says where it is
        const GroupedCell result = loadCellInGroup(ESM4::Grp_ExteriorSubCell, 0, zString("EDID", "OFOriginCell"));

        EXPECT_TRUE(result.mHasGrid);
        EXPECT_EQ(result.mGridX, 0);
        EXPECT_EQ(result.mGridY, 0);
        EXPECT_EQ(result.mCell.mEditorId, "OFOriginCell");
    }

    TEST(ESM4CellTest, aCellOfAnotherGroupHasNoGridUntilItsXclc)
    {
        // A sub-block of another grid, and a group of another type
        EXPECT_FALSE(loadCellInGroup(ESM4::Grp_ExteriorSubCell, 0x00000001, zString("EDID", "A")).mHasGrid);
        EXPECT_FALSE(loadCellInGroup(ESM4::Grp_ExteriorSubCell, 0x00010000, zString("EDID", "B")).mHasGrid);
        EXPECT_FALSE(loadCellInGroup(ESM4::Grp_InteriorSubCell, 0, zString("EDID", "C")).mHasGrid);

        std::string grid;
        append<std::int32_t>(grid, 5);
        append<std::int32_t>(grid, -6);
        const GroupedCell result = loadCellInGroup(ESM4::Grp_ExteriorSubCell, 0x00000001, subRecord("XCLC", grid));
        EXPECT_TRUE(result.mHasGrid);
        EXPECT_EQ(result.mGridX, 5);
        EXPECT_EQ(result.mGridY, -6);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // WRLD

    std::string mapData(bool newFormat)
    {
        std::string data;
        append<std::uint32_t>(data, 4096); // width
        append<std::uint32_t>(data, 2048); // height
        append<std::int16_t>(data, -30); // NW cell x
        append<std::int16_t>(data, 40); // NW cell y
        append<std::int16_t>(data, 50); // SE cell x
        append<std::int16_t>(data, -60); // SE cell y
        if (newFormat)
        {
            append<float>(data, 50000.f);
            append<float>(data, 80000.f);
            append<float>(data, 0.5f);
        }
        return data;
    }

    TEST(ESM4WorldTest, aWorldOfOblivionHasItsParentMapAndLevels)
    {
        std::string minimum;
        append<std::int32_t>(minimum, -10);
        append<std::int32_t>(minimum, -20);
        std::string maximum;
        append<std::int32_t>(maximum, 30);
        append<std::int32_t>(maximum, 40);
        std::string levels;
        append<float>(levels, -3000.f);
        append<float>(levels, -15000.f);

        const ESM4::World world = load<ESM4::World>("WRLD",
            zString("EDID", "OFTestWorld") + zString("FULL", "A test world")
                + valueSubRecord<std::uint32_t>("WNAM", 0xA00) + valueSubRecord<std::int32_t>("SNAM", 2)
                + zString("ICON", "textures\\map.dds") + valueSubRecord<std::uint32_t>("CNAM", 0xA01)
                + valueSubRecord<std::uint32_t>("NAM2", 0xA02) + subRecord("NAM0", minimum) + subRecord("NAM9", maximum)
                + valueSubRecord<std::uint8_t>("DATA", 0x05) + subRecord("MNAM", mapData(false))
                + subRecord("DNAM", levels) + valueSubRecord<std::uint32_t>("ZNAM", 0xA03)
                + valueSubRecord<std::uint32_t>("XEZN", 0xA04));

        EXPECT_EQ(world.mId.toUint32(), 0x801u);
        EXPECT_EQ(world.mEditorId, "OFTestWorld");
        EXPECT_EQ(world.mFullName, "A test world");
        EXPECT_EQ(world.mParent.toUint32(), 0xA00u);
        EXPECT_EQ(world.mSound, 2);
        EXPECT_EQ(world.mMapFile, "textures\\map.dds");
        EXPECT_EQ(world.mClimate.toUint32(), 0xA01u);
        EXPECT_EQ(world.mWater.toUint32(), 0xA02u);
        EXPECT_EQ(world.mMinX, -10);
        EXPECT_EQ(world.mMinY, -20);
        EXPECT_EQ(world.mMaxX, 30);
        EXPECT_EQ(world.mMaxY, 40);
        EXPECT_EQ(world.mWorldFlags, 0x05);
        EXPECT_EQ(world.mMap.width, 4096u);
        EXPECT_EQ(world.mMap.height, 2048u);
        EXPECT_EQ(world.mMap.NWcellX, -30);
        EXPECT_EQ(world.mMap.NWcellY, 40);
        EXPECT_EQ(world.mMap.SEcellX, 50);
        EXPECT_EQ(world.mMap.SEcellY, -60);
        EXPECT_EQ(world.mLandLevel, -3000.f);
        EXPECT_EQ(world.mWaterLevel, -15000.f);
        EXPECT_EQ(world.mMusic.toUint32(), 0xA03u);
        EXPECT_EQ(world.mEncounterZone.toUint32(), 0xA04u);
    }

    TEST(ESM4WorldTest, aWorldOfOblivionWithoutLevelsHasThemAtZeroAndUsesAllOfItsParent)
    {
        const ESM4::World world = load<ESM4::World>("WRLD", zString("EDID", "OFTestWorld"));

        EXPECT_EQ(world.mLandLevel, 0.f);
        EXPECT_EQ(world.mWaterLevel, 0.f);
        EXPECT_EQ(world.mParentUseFlags, 0xFFFFu);
    }

    TEST(ESM4WorldTest, aWorldOfSkyrimHasItsCenterCellItsFlagsItsCameraAndTheDefaultLevels)
    {
        std::string center;
        append<std::int16_t>(center, 4);
        append<std::int16_t>(center, -5);

        const ESM4::World world = loadVersionedRecord<ESM4::World>("WRLD", 1.7f, 44,
            zString("EDID", "OFTestSkyrim") + subRecord("WCTR", center) + subRecord("MNAM", mapData(true))
                + valueSubRecord<std::uint16_t>("PNAM", 0x001F));

        EXPECT_EQ(world.mCenterCell.x, 4);
        EXPECT_EQ(world.mCenterCell.y, -5);
        EXPECT_EQ(world.mMap.width, 4096u);
        EXPECT_EQ(world.mMap.minHeight, 50000.f);
        EXPECT_EQ(world.mMap.maxHeight, 80000.f);
        EXPECT_EQ(world.mMap.initialPitch, 0.5f);
        EXPECT_EQ(world.mParentUseFlags, 0x001Fu);
        EXPECT_EQ(world.mLandLevel, -2700.f);
        EXPECT_EQ(world.mWaterLevel, -14000.f);
    }

    TEST(ESM4WorldTest, theLevelsThatAWorldOfSkyrimNamesAreKept)
    {
        std::string levels;
        append<float>(levels, -100.f);
        append<float>(levels, -200.f);

        const ESM4::World world = loadVersionedRecord<ESM4::World>(
            "WRLD", 1.7f, 44, zString("EDID", "OFTestSkyrim") + subRecord("DNAM", levels));

        EXPECT_EQ(world.mLandLevel, -100.f);
        EXPECT_EQ(world.mWaterLevel, -200.f);
    }

    TEST(ESM4WorldTest, aWorldOfNewVegasHasLevelsOfZeroUnlessItNamesThem)
    {
        const ESM4::World world = loadVersionedRecord<ESM4::World>("WRLD", 1.34f, 15, zString("EDID", "OFTestVegas"));

        EXPECT_EQ(world.mLandLevel, 0.f);
        EXPECT_EQ(world.mWaterLevel, 0.f);
    }

    TEST(ESM4WorldTest, theSubrecordsThatTheLoaderDoesNotReadAreSkipped)
    {
        const ESM4::World world = load<ESM4::World>("WRLD",
            ignored({ "OFST", "RNAM", "MHDT", "LTMP", "XLCN", "NAM3", "NAM4", "NAMA", "ONAM", "TNAM", "UNAM", "XWEM",
                "MODL", "MODT", "MODC", "MODS", "MODF", "INAM", "NNAM", "XNAM", "IMPS", "IMPF", "CLSZ", "WLEV" })
                + valueSubRecord<std::uint32_t>("ZNAM", 0xA05));

        EXPECT_EQ(world.mMusic.toUint32(), 0xA05u);
    }

    TEST(ESM4WorldTest, aSubrecordThatNoGameWritesIsAnError)
    {
        EXPECT_THAT(loadFailure<ESM4::World>("WRLD", subRecord("ZZZZ", "")), HasSubstr("Unknown subrecord ZZZZ"));
    }
}
