#include "quicksavemanager.hpp"

OFState::QuickSaveManager::QuickSaveManager(std::string& saveName, unsigned int maxSaves)
    : mSaveName(saveName)
    , mMaxSaves(maxSaves)
    , mSlotsVisited(0)
    , mOldestSlotVisited(nullptr)
{
}

void OFState::QuickSaveManager::visitSave(const Slot* saveSlot)
{
    if (mSaveName == saveSlot->mProfile.mDescription)
    {
        ++mSlotsVisited;
        if (isOldestSave(saveSlot))
            mOldestSlotVisited = saveSlot;
    }
}

bool OFState::QuickSaveManager::isOldestSave(const Slot* compare) const
{
    if (mOldestSlotVisited == nullptr)
        return true;
    return (compare->mTimeStamp <= mOldestSlotVisited->mTimeStamp);
}

bool OFState::QuickSaveManager::shouldCreateNewSlot() const
{
    return (mSlotsVisited < mMaxSaves);
}

const OFState::Slot* OFState::QuickSaveManager::getNextQuickSaveSlot()
{
    if (shouldCreateNewSlot())
        return nullptr;
    return mOldestSlotVisited;
}
