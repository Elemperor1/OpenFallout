#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HACKING_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HACKING_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "interaction.hpp"

namespace FalloutScript::Hacking
{
    /// The guesses a hacker has at least, before a terminal is locked (the executable gives more to a list of words
    /// that is hard to tell apart)
    constexpr int attempts = 4;

    /// The size of the memory dump of the game: 408 characters in rows of 12, 17 rows in a column, 2 columns
    constexpr std::size_t dumpSize = 408;
    constexpr std::size_t rowSize = 12;
    constexpr std::size_t rowsPerColumn = 17;
    constexpr std::size_t rowCount = 34;

    /// The shortest and the longest word
    constexpr std::size_t shortestWord = 4;
    constexpr std::size_t longestWord = 12;

    /// The words of the game's file (Data\Menus\FalloutDict.txt), by their length from 4 to 12 letters
    class Dictionary
    {
    public:
        /// The words of the text: separated by white space, kept when they are letters only (changed to upper case),
        /// each of them once, in the order they come in
        static Dictionary parse(std::string_view text);
        /// A small list of words of the project's own, for an install whose files have none
        static const Dictionary& builtIn();

        /// The words of that length, none for a length that is not from 4 to 12
        const std::vector<std::string>& ofLength(std::size_t length) const;
        bool empty() const;

    private:
        std::array<std::vector<std::string>, longestWord - shortestWord + 1> mWords;
    };

    /// The game of hacking a terminal, as the executable plays it (FalloutNV.exe 0x765B80-0x76B800, read by the Mac in
    /// rounds 12R and 12S): a dump of memory of 408 characters, with words laid in it among symbols; a guess of a word
    /// tells how many of its letters stand where the same letter stands in the password (the likeness) and a wrong one
    /// costs an attempt; a pair of brackets removes a word that is not the password, or, once, gives all the attempts
    /// back, and then is spent. Choosing a character that is no part of a word or a pair is a wrong guess too, with
    /// nothing in common with the password (0x767060: a text of one character goes to the compare of a guess).
    class Puzzle
    {
    public:
        /// `level` is the hacking level of the terminal (0 to 4), `parity` the bit of the terminal that lengthens the
        /// words by one (0 or 1), `science` the Science of the player together with the perk entry point for hacking.
        /// The words are of the dictionary, from iHackingMinWords to iHackingMaxWords of them: more for a lower
        /// Science, by the fHackingMinSkill* settings.
        Puzzle(std::uint32_t seed, int level, int parity, double science, const Interaction::Setting& setting,
            const Dictionary& dictionary);
        /// The same with the project's own words, no Science and the length of the words by the level only
        Puzzle(std::uint32_t seed, int level, const Interaction::Setting& setting);

        /// The words that are laid in the dump, in the order they were chosen
        const std::vector<std::string>& words() const { return mWords; }
        /// How many of them are still in the dump
        std::size_t wordsLeft() const { return mWordsLeft; }
        int attemptsLeft() const { return mAttemptsLeft; }
        int maxAttempts() const { return mMaxAttempts; }
        bool solved() const { return mSolved; }
        bool lockedOut() const { return !mSolved && mAttemptsLeft <= 0; }
        /// Whether the word was guessed (a word can be guessed again, and costs again)
        bool guessed(std::size_t word) const { return mGuessed.count(word) != 0; }

        /// The dump as it stands now (the words that were removed are dots)
        const std::string& dump() const { return mDump; }
        /// The 16 bits that the first row of the dump has as its address; each row is 12 more than the one before
        unsigned address() const { return mAddress; }
        /// The text of a row as the screen shows it: "0x" and four digits of its address, a space and its 12 characters
        std::string row(std::size_t index) const;

        /// What the cursor is on at a place of the dump: a word, a pair of brackets, or a single character
        struct Unit
        {
            enum class Kind
            {
                Character,
                Word,
                Brackets,
            };
            Kind mKind = Kind::Character;
            /// Where it begins and how many characters it has
            std::size_t mFirst = 0;
            std::size_t mLength = 0;
            /// For a word, its place in words()
            std::size_t mWord = 0;
        };
        Unit unitAt(std::size_t position) const;

        /// What choosing a place of the dump did
        struct Pick
        {
            Unit mUnit;
            /// The text that was chosen
            std::string mText;
            /// Nothing was done: the game is over or the place is not in the dump
            bool mIgnored = false;
            /// A word: it was the password, and how many letters are where they are in the password
            bool mCorrect = false;
            int mLikeness = 0;
            /// A pair of brackets: what came of it
            enum class Effect
            {
                None,
                /// Only one word was left
                Denied,
                /// A word that is not the password was taken out
                Removed,
                /// The attempts were given back
                Replenished,
            };
            Effect mEffect = Effect::None;
            int mAttemptsLeft = 0;
        };
        Pick pick(std::size_t position);

        struct Guess
        {
            /// The word was the password
            bool mCorrect = false;
            /// How many letters are where they are in the password
            int mLikeness = 0;
            int mAttemptsLeft = 0;
            /// No guess was made: the word is not on the list or was taken out, or the game is over
            bool mIgnored = false;
        };
        /// Guess the word that stands at that place in words()
        Guess guess(std::size_t word);

        /// The password, for tests
        const std::string& password() const { return mWords[mPassword]; }
        /// Where a word begins in the dump (a word that was removed is not there)
        std::size_t placeOf(std::size_t word) const { return mPlaces[word]; }
        bool removed(std::size_t word) const { return mRemoved.count(word) != 0; }

    private:
        std::size_t random(std::size_t below);
        /// `count` of the words of the pool, picked so that they tell each other apart (some letters in common, and
        /// no likeness that many of them share with the last)
        std::vector<std::string> choose(const std::vector<std::string>& pool, std::size_t count);
        void build(
            const Dictionary& dictionary, int level, int parity, double science, const Interaction::Setting& setting);

        std::mt19937 mRandom;
        std::vector<std::string> mWords;
        /// Where each word begins in the dump
        std::vector<std::size_t> mPlaces;
        std::string mDump;
        unsigned mAddress = 0;
        std::size_t mPassword = 0;
        std::size_t mWordsLeft = 0;
        int mMaxAttempts = attempts;
        int mAttemptsLeft = attempts;
        bool mReplenished = false;
        bool mSolved = false;
        std::set<std::size_t> mGuessed;
        std::set<std::size_t> mRemoved;
        /// Where the pairs of brackets that were used begin
        std::set<std::size_t> mUsed;
    };

    /// How many letters of the two words are the same and in the same place
    int likeness(const std::string& first, const std::string& second);

    /// How long the words of a terminal of that hacking level are, with the bit of the terminal that lengthens them
    /// (FalloutNV.exe 0x501270): 4 letters and 2 more for each level, and never more than 12
    std::size_t wordLength(int level, int parity = 0);

    /// The Science that a terminal of that hacking level asks of the player to try it (FalloutNV.exe 0x501760): the
    /// fHackingMinSkill* setting of the level, 0 for the easiest up to 100
    double minimumScience(int level, const Interaction::Setting& setting);

    /// How many words the dump has (0x768aa0): from iHackingMaxWords for a Science that is no more than the lowest
    /// that the level asks for, down to iHackingMinWords for a Science of 100, never more than 20
    std::size_t wordCount(int level, double science, const Interaction::Setting& setting);

    /// How many guesses a player who picks well needs at the worst for a list of words (0x769520): the game gives at
    /// least four, and more when this is more
    int guessesNeeded(const std::vector<std::string>& words);

    /// The words of the project's own list that there are of that length
    const std::vector<std::string>& wordsOfLength(std::size_t length);
}

#endif
