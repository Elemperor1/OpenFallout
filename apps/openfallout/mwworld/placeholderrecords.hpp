#ifndef OPENFALLOUT_MWWORLD_PLACEHOLDERRECORDS_H
#define OPENFALLOUT_MWWORLD_PLACEHOLDERRECORDS_H

namespace OFWorld
{
    class ESMStore;

    /// Whether the content that was loaded has no player record in the format of the engine's own record types, as a
    /// content list made only of Fallout 3, New Vegas or Tale of Two Wastelands plugins does.
    bool lacksPlayerRecord(const ESMStore& store);

    /// Insert the records the world cannot start without and that only a game file in the engine's own record format
    /// supplies: the player with a race and a class, the skills, the global variables of the clock and the game
    /// settings the engine reads. Their values are neutral placeholders, none of them is taken from a game. A record
    /// that is already in the store is left as it is. Call it after all content files are loaded and before
    /// ESMStore::setUp().
    void insertPlaceholderRecords(ESMStore& store);
}

#endif
