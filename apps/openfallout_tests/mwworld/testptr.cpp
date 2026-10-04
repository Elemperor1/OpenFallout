#include "apps/openfallout/mwclass/npc.hpp"
#include "apps/openfallout/mwworld/esmstore.hpp"
#include "apps/openfallout/mwworld/livecellref.hpp"
#include "apps/openfallout/mwworld/ptr.hpp"
#include "apps/openfallout/mwworld/worldmodel.hpp"

#include <components/esm3/loadnpc.hpp>
#include <components/esm3/readerscache.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace OFWorld
{
    namespace
    {
        using namespace testing;

        TEST(OFWorldPtrTest, toStringShouldReturnHumanReadableTextRepresentationOfPtrWithNullRef)
        {
            Ptr ptr;
            EXPECT_EQ(ptr.toString(), "null object");
        }

        TEST(OFWorldPtrTest, toStringShouldReturnHumanReadableTextRepresentationOfPtrWithDeletedRef)
        {
            OFClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("Player");
            ESMStore store;
            store.insert(npc);
            ESM::CellRef cellRef;
            cellRef.blank();
            cellRef.mRefID = npc.mId;
            cellRef.mRefNum = ESM::RefNum{ .mIndex = 0x2a, .mContentFile = 0xd };
            LiveCellRef<ESM::NPC> liveCellRef(cellRef, &npc);
            liveCellRef.mData.setDeletedByContentFile(true);
            Ptr ptr(&liveCellRef);
            EXPECT_THAT(ptr.toString(), StrCaseEq("deleted object0xd00002a (NPC, \"player\")"));
        }

        TEST(OFWorldPtrTest, toStringShouldReturnHumanReadableTextRepresentationOfPtr)
        {
            OFClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("Player");
            ESMStore store;
            store.insert(npc);
            ESM::CellRef cellRef;
            cellRef.blank();
            cellRef.mRefID = npc.mId;
            cellRef.mRefNum = ESM::RefNum{ .mIndex = 0x2a, .mContentFile = 0xd };
            LiveCellRef<ESM::NPC> liveCellRef(cellRef, &npc);
            Ptr ptr(&liveCellRef);
            EXPECT_THAT(ptr.toString(), StrCaseEq("object0xd00002a (NPC, \"player\")"));
        }

        TEST(OFWorldPtrTest, underlyingLiveCellRefShouldBeDeregisteredOnDestruction)
        {
            OFClass::Npc::registerSelf();
            ESM::NPC npc;
            npc.blank();
            npc.mId = ESM::RefId::stringRefId("Player");
            ESMStore store;
            store.insert(npc);
            ESM::ReadersCache readersCache;
            WorldModel worldModel(store, readersCache);
            ESM::CellRef cellRef;
            cellRef.blank();
            cellRef.mRefID = npc.mId;
            cellRef.mRefNum = ESM::FormId{ .mIndex = 0x2a, .mContentFile = 0xd };
            {
                LiveCellRef<ESM::NPC> liveCellRef(cellRef, &npc);
                Ptr ptr(&liveCellRef);
                worldModel.registerPtr(ptr);
                ASSERT_EQ(worldModel.getPtr(cellRef.mRefNum), ptr);
            }
            EXPECT_EQ(worldModel.getPtr(cellRef.mRefNum), Ptr());
        }
    }
}
