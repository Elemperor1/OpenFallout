#include <components/falloutscript/hacking.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>

namespace
{
    using FalloutScript::Hacking::Puzzle;

    const FalloutScript::Interaction::Setting defaults = [](std::string_view, double fallback) { return fallback; };

    TEST(FalloutScriptHackingTest, every_word_of_the_tables_has_the_length_it_is_listed_under)
    {
        for (std::size_t length = 4; length <= 12; ++length)
        {
            const std::vector<std::string>& words = FalloutScript::Hacking::wordsOfLength(length);
            EXPECT_GE(words.size(), 18u) << length;
            std::set<std::string> distinct;
            for (const std::string& word : words)
            {
                EXPECT_EQ(word.size(), length) << word;
                EXPECT_TRUE(distinct.insert(word).second) << word;
                for (const char letter : word)
                    EXPECT_TRUE(letter >= 'A' && letter <= 'Z') << word;
            }
        }
        EXPECT_TRUE(FalloutScript::Hacking::wordsOfLength(3).empty());
        EXPECT_TRUE(FalloutScript::Hacking::wordsOfLength(13).empty());
    }

    TEST(FalloutScriptHackingTest, the_likeness_counts_the_letters_in_the_same_place)
    {
        using FalloutScript::Hacking::likeness;
        EXPECT_EQ(likeness("VAULT", "VAULT"), 5);
        EXPECT_EQ(likeness("VAULT", "VALET"), 3);
        EXPECT_EQ(likeness("VAULT", "TLUAV"), 1);
        EXPECT_EQ(likeness("VAULT", "AB"), 0);
    }

    TEST(FalloutScriptHackingTest, harder_terminals_have_longer_words_and_more_of_them)
    {
        const Puzzle easy(1, 0, defaults);
        const Puzzle hard(1, 4, defaults);
        EXPECT_EQ(easy.words().front().size(), 4u);
        EXPECT_EQ(hard.words().front().size(), 12u);
        EXPECT_LT(easy.words().size(), hard.words().size());
        EXPECT_GE(easy.words().size(), 5u);
        EXPECT_LE(hard.words().size(), 20u);
        for (const std::string& word : hard.words())
            EXPECT_EQ(word.size(), 12u);
    }

    TEST(FalloutScriptHackingTest, the_words_are_all_different_and_the_password_is_one_of_them)
    {
        for (std::uint32_t seed = 0; seed < 20; ++seed)
            for (int level = 0; level <= 4; ++level)
            {
                const Puzzle puzzle(seed, level, defaults);
                const std::set<std::string> distinct(puzzle.words().begin(), puzzle.words().end());
                EXPECT_EQ(distinct.size(), puzzle.words().size());
                EXPECT_TRUE(distinct.count(puzzle.password()) != 0);
            }
    }

    TEST(FalloutScriptHackingTest, the_same_seed_makes_the_same_puzzle)
    {
        const Puzzle first(7, 2, defaults);
        const Puzzle second(7, 2, defaults);
        EXPECT_EQ(first.words(), second.words());
        EXPECT_EQ(first.password(), second.password());
        const Puzzle other(8, 2, defaults);
        EXPECT_NE(first.words(), other.words());
    }

    TEST(FalloutScriptHackingTest, a_wrong_guess_costs_an_attempt_and_tells_the_likeness)
    {
        Puzzle puzzle(3, 1, defaults);
        std::size_t wrong = 0;
        while (puzzle.words()[wrong] == puzzle.password())
            ++wrong;
        const Puzzle::Guess guess = puzzle.guess(wrong);
        EXPECT_FALSE(guess.mCorrect);
        EXPECT_FALSE(guess.mIgnored);
        EXPECT_EQ(guess.mLikeness, FalloutScript::Hacking::likeness(puzzle.words()[wrong], puzzle.password()));
        EXPECT_EQ(guess.mAttemptsLeft, FalloutScript::Hacking::attempts - 1);
        EXPECT_TRUE(puzzle.guessed(wrong));
        // The same word again is no guess
        EXPECT_TRUE(puzzle.guess(wrong).mIgnored);
        EXPECT_EQ(puzzle.attemptsLeft(), FalloutScript::Hacking::attempts - 1);
        EXPECT_TRUE(puzzle.guess(10000).mIgnored);
    }

    TEST(FalloutScriptHackingTest, the_password_solves_it_and_ends_the_game)
    {
        Puzzle puzzle(3, 1, defaults);
        std::size_t right = 0;
        while (puzzle.words()[right] != puzzle.password())
            ++right;
        const Puzzle::Guess guess = puzzle.guess(right);
        EXPECT_TRUE(guess.mCorrect);
        EXPECT_EQ(guess.mLikeness, static_cast<int>(puzzle.password().size()));
        EXPECT_EQ(guess.mAttemptsLeft, FalloutScript::Hacking::attempts);
        EXPECT_TRUE(puzzle.solved());
        EXPECT_FALSE(puzzle.lockedOut());
        EXPECT_TRUE(puzzle.guess(0).mIgnored);
    }

    TEST(FalloutScriptHackingTest, four_wrong_guesses_lock_the_terminal)
    {
        Puzzle puzzle(5, 2, defaults);
        int made = 0;
        for (std::size_t word = 0; word < puzzle.words().size() && made < FalloutScript::Hacking::attempts; ++word)
        {
            if (puzzle.words()[word] == puzzle.password())
                continue;
            EXPECT_FALSE(puzzle.guess(word).mIgnored);
            ++made;
        }
        EXPECT_EQ(made, FalloutScript::Hacking::attempts);
        EXPECT_TRUE(puzzle.lockedOut());
        EXPECT_EQ(puzzle.attemptsLeft(), 0);
        std::size_t right = 0;
        while (puzzle.words()[right] != puzzle.password())
            ++right;
        EXPECT_TRUE(puzzle.guess(right).mIgnored);
        EXPECT_FALSE(puzzle.solved());
    }
}
