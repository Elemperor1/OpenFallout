#include "journalentry.hpp"

#include <stdexcept>

#include <components/esm3/journalentry.hpp>

#include <components/interpreter/defines.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"

#include "../mwscript/interpretercontext.hpp"

namespace OFDialogue
{
    Entry::Entry(const ESM::RefId& topic, const ESM::RefId& infoId, const OFWorld::Ptr& actor)
        : mInfoId(infoId)
    {
        const ESM::Dialogue* dialogue = OFBase::Environment::get().getESMStore()->get<ESM::Dialogue>().find(topic);

        for (ESM::Dialogue::InfoContainer::const_iterator iter(dialogue->mInfo.begin()); iter != dialogue->mInfo.end();
             ++iter)
            if (iter->mId == mInfoId)
            {
                if (actor.isEmpty())
                {
                    OFScript::InterpreterContext interpreterContext(nullptr, OFWorld::Ptr());
                    mText = Interpreter::fixDefinesDialog(iter->mResponse, interpreterContext);
                }
                else
                {
                    OFScript::InterpreterContext interpreterContext(&actor.getRefData().getLocals(), actor);
                    mText = Interpreter::fixDefinesDialog(iter->mResponse, interpreterContext);
                }

                return;
            }

        throw std::runtime_error("unknown info ID " + mInfoId.toDebugString() + " for topic " + topic.toDebugString());
    }

    Entry::Entry(const ESM::JournalEntry& record)
        : mInfoId(record.mInfo)
        , mText(record.mText)
        , mActorName(record.mActorName)
    {
    }

    const std::string& Entry::getText() const
    {
        return mText;
    }

    void Entry::write(ESM::JournalEntry& entry) const
    {
        entry.mInfo = mInfoId;
        entry.mText = mText;
        entry.mActorName = mActorName;
    }

    JournalEntry::JournalEntry(const ESM::RefId& topic, const ESM::RefId& infoId, const OFWorld::Ptr& actor)
        : Entry(topic, infoId, actor)
        , mTopic(topic)
    {
    }

    JournalEntry::JournalEntry(const ESM::JournalEntry& record)
        : Entry(record)
        , mTopic(record.mTopic)
    {
    }

    void JournalEntry::write(ESM::JournalEntry& entry) const
    {
        Entry::write(entry);
        entry.mTopic = mTopic;
    }

    JournalEntry JournalEntry::makeFromQuest(const ESM::RefId& topic, int index)
    {
        return JournalEntry(topic, idFromIndex(topic, index), OFWorld::Ptr());
    }

    const ESM::RefId& JournalEntry::idFromIndex(const ESM::RefId& topic, int index)
    {
        const ESM::Dialogue* dialogue = OFBase::Environment::get().getESMStore()->get<ESM::Dialogue>().find(topic);

        for (ESM::Dialogue::InfoContainer::const_iterator iter(dialogue->mInfo.begin()); iter != dialogue->mInfo.end();
             ++iter)
            if (iter->mData.mJournalIndex == index)
            {
                return iter->mId;
            }

        throw std::runtime_error("unknown journal index for topic " + topic.toDebugString());
    }

    StampedJournalEntry::StampedJournalEntry()
        : mDay(0)
        , mMonth(0)
        , mDayOfMonth(0)
    {
    }

    StampedJournalEntry::StampedJournalEntry(const ESM::RefId& topic, const ESM::RefId& infoId, int day, int month,
        int dayOfMonth, const OFWorld::Ptr& actor)
        : JournalEntry(topic, infoId, actor)
        , mDay(day)
        , mMonth(month)
        , mDayOfMonth(dayOfMonth)
    {
    }

    StampedJournalEntry::StampedJournalEntry(const ESM::JournalEntry& record)
        : JournalEntry(record)
        , mDay(record.mDay)
        , mMonth(record.mMonth)
        , mDayOfMonth(record.mDayOfMonth)
    {
    }

    void StampedJournalEntry::write(ESM::JournalEntry& entry) const
    {
        JournalEntry::write(entry);
        entry.mDay = mDay;
        entry.mMonth = mMonth;
        entry.mDayOfMonth = mDayOfMonth;
    }

    StampedJournalEntry StampedJournalEntry::makeFromQuest(
        const ESM::RefId& topic, int index, const OFWorld::Ptr& actor)
    {
        const int day = OFBase::Environment::get().getWorld()->getGlobalInt(OFWorld::Globals::sDaysPassed);
        const int month = OFBase::Environment::get().getWorld()->getGlobalInt(OFWorld::Globals::sMonth);
        const int dayOfMonth = OFBase::Environment::get().getWorld()->getGlobalInt(OFWorld::Globals::sDay);

        return StampedJournalEntry(topic, idFromIndex(topic, index), day, month, dayOfMonth, actor);
    }
}
