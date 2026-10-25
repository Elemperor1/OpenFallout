#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HACKING_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HACKING_H

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "interaction.hpp"

namespace FalloutScript::Hacking
{
    /// The guesses a hacker has at a terminal before it is locked
    constexpr int attempts = 4;

    /// The game of hacking a terminal: a list of words of which one is the password. A guess tells how many of its
    /// letters stand where the same letter stands in the password (the likeness), and a wrong one costs an attempt. The
    /// executable's version (the cursor in a block of symbols, the removal of a word, the refill of the attempts) was
    /// not read: this one has the words, the likeness and the four attempts.
    class Puzzle
    {
    public:
        /// The words for a terminal of that hacking level (0 to 4): longer and more of them for the harder ones, from
        /// iHackingMinWords to iHackingMaxWords, picked by the seed
        Puzzle(std::uint32_t seed, int level, const Interaction::Setting& setting);

        const std::vector<std::string>& words() const { return mWords; }
        int attemptsLeft() const { return mAttemptsLeft; }
        bool solved() const { return mSolved; }
        bool lockedOut() const { return !mSolved && mAttemptsLeft <= 0; }
        bool guessed(std::size_t word) const { return mGuessed.count(word) != 0; }

        struct Guess
        {
            /// The word was the password
            bool mCorrect = false;
            /// How many letters are where they are in the password
            int mLikeness = 0;
            int mAttemptsLeft = 0;
            /// No guess was made: the word is not on the list, was guessed already, or the game is over
            bool mIgnored = false;
        };
        Guess guess(std::size_t word);

        /// The password, for tests
        const std::string& password() const { return mWords[mPassword]; }

    private:
        std::vector<std::string> mWords;
        std::size_t mPassword = 0;
        int mAttemptsLeft = attempts;
        bool mSolved = false;
        std::set<std::size_t> mGuessed;
    };

    /// How many letters of the two words are the same and in the same place
    int likeness(const std::string& first, const std::string& second);

    /// How long the words of a terminal of that hacking level are
    std::size_t wordLength(int level);

    /// The words that there are of that length
    const std::vector<std::string>& wordsOfLength(std::size_t length);
}

#endif
