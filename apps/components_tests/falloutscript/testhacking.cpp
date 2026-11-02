#include <components/falloutscript/hacking.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <set>
#include <string>

namespace
{
    using FalloutScript::Hacking::Dictionary;
    using FalloutScript::Hacking::Puzzle;

    const FalloutScript::Interaction::Setting defaults = [](std::string_view, double fallback) { return fallback; };

    bool isLetter(char c)
    {
        return c >= 'A' && c <= 'Z';
    }

    /// The first place of the dump that is the start of a pair of brackets
    std::optional<std::size_t> bracketsOf(const Puzzle& puzzle)
    {
        for (std::size_t i = 0; i < puzzle.dump().size(); ++i)
        {
            const Puzzle::Unit unit = puzzle.unitAt(i);
            if (unit.mKind == Puzzle::Unit::Kind::Brackets)
                return unit.mFirst;
        }
        return std::nullopt;
    }

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
                    EXPECT_TRUE(isLetter(letter)) << word;
            }
        }
        EXPECT_TRUE(FalloutScript::Hacking::wordsOfLength(3).empty());
        EXPECT_TRUE(FalloutScript::Hacking::wordsOfLength(13).empty());
    }

    TEST(FalloutScriptHackingTest, the_dictionary_keeps_each_word_of_four_to_twelve_letters_once)
    {
        const Dictionary dictionary
            = Dictionary::parse("able ACID\nbad  ABLE\tvault\r\nABCDEFGHIJKLM X1YZ ABCDEFGHIJKL   \n");
        EXPECT_EQ(dictionary.ofLength(4), (std::vector<std::string>{ "ABLE", "ACID" }));
        EXPECT_EQ(dictionary.ofLength(5), (std::vector<std::string>{ "VAULT" }));
        EXPECT_EQ(dictionary.ofLength(12), (std::vector<std::string>{ "ABCDEFGHIJKL" }));
        EXPECT_TRUE(dictionary.ofLength(3).empty());
        EXPECT_TRUE(dictionary.ofLength(13).empty());
        EXPECT_TRUE(dictionary.ofLength(6).empty());
        EXPECT_FALSE(dictionary.empty());
        EXPECT_TRUE(Dictionary::parse("").empty());
        EXPECT_TRUE(Dictionary::parse("ab cd 12 ef").empty());
        EXPECT_FALSE(Dictionary::builtIn().empty());
    }

    TEST(FalloutScriptHackingTest, the_likeness_counts_the_letters_in_the_same_place)
    {
        using FalloutScript::Hacking::likeness;
        EXPECT_EQ(likeness("VAULT", "VAULT"), 5);
        EXPECT_EQ(likeness("VAULT", "VALET"), 3);
        EXPECT_EQ(likeness("VAULT", "TLUAV"), 1);
        EXPECT_EQ(likeness("VAULT", "AB"), 0);
        // Capitals are not small letters
        EXPECT_EQ(likeness("VAULT", "vault"), 0);
    }

    TEST(FalloutScriptHackingTest, the_words_are_longer_for_a_harder_level_and_a_terminal_with_the_bit)
    {
        using FalloutScript::Hacking::wordLength;
        EXPECT_EQ(wordLength(0), 4u);
        EXPECT_EQ(wordLength(1), 6u);
        EXPECT_EQ(wordLength(2), 8u);
        EXPECT_EQ(wordLength(3), 10u);
        EXPECT_EQ(wordLength(4), 12u);
        EXPECT_EQ(wordLength(0, 1), 5u);
        EXPECT_EQ(wordLength(3, 1), 11u);
        // Never more than twelve
        EXPECT_EQ(wordLength(4, 1), 12u);
        EXPECT_EQ(wordLength(9), 12u);
        EXPECT_EQ(wordLength(-2), 4u);
    }

    TEST(FalloutScriptHackingTest, the_words_are_fewer_for_a_higher_science)
    {
        using FalloutScript::Hacking::wordCount;
        // The most words for a Science that does not pass what the level asks for, the fewest for a Science of 100
        EXPECT_EQ(wordCount(0, 0, defaults), 20u);
        EXPECT_EQ(wordCount(2, 50, defaults), 20u);
        EXPECT_EQ(wordCount(2, 30, defaults), 20u);
        EXPECT_EQ(wordCount(0, 100, defaults), 5u);
        EXPECT_EQ(wordCount(2, 100, defaults), 5u);
        // Halfway: 5 + 15 / 2 = 12.5, up to 13
        EXPECT_EQ(wordCount(0, 50, defaults), 13u);
        EXPECT_EQ(wordCount(2, 75, defaults), 13u);
        // More Science (a perk) than 100 stays at the fewest; a level that asks 100 has all the words
        EXPECT_EQ(wordCount(0, 120, defaults), 5u);
        EXPECT_EQ(wordCount(4, 100, defaults), 20u);
        // The settings of the game give the numbers
        const FalloutScript::Interaction::Setting settings = [](std::string_view name, double fallback) {
            if (name == "iHackingMinWords")
                return 2.0;
            if (name == "iHackingMaxWords")
                return 8.0;
            if (name == "fHackingMinSkillAverage")
                return 10.0;
            return fallback;
        };
        EXPECT_EQ(wordCount(2, 0, settings), 8u);
        EXPECT_EQ(wordCount(2, 100, settings), 2u);
        EXPECT_EQ(wordCount(2, 55, settings), 5u);
        EXPECT_EQ(wordCount(0, 0, settings), 8u);
    }

    TEST(FalloutScriptHackingTest, a_puzzle_has_the_words_of_the_level_and_the_science)
    {
        const Dictionary& words = Dictionary::builtIn();
        const Puzzle easy(1, 0, 0, 100, defaults, words);
        EXPECT_EQ(easy.words().size(), 5u);
        for (const std::string& word : easy.words())
            EXPECT_EQ(word.size(), 4u);
        const Puzzle odd(1, 0, 1, 100, defaults, words);
        for (const std::string& word : odd.words())
            EXPECT_EQ(word.size(), 5u);
        // A pool of 19 words of 12 letters has no more words than that
        const Puzzle hard(1, 4, 0, 0, defaults, words);
        EXPECT_GE(hard.words().size(), 15u);
        EXPECT_LE(hard.words().size(), 19u);
        for (const std::string& word : hard.words())
            EXPECT_EQ(word.size(), 12u);
        // The short constructor knows no Science
        const Puzzle plain(1, 2, defaults);
        for (const std::string& word : plain.words())
            EXPECT_EQ(word.size(), 8u);
    }

    TEST(FalloutScriptHackingTest, a_dictionary_that_has_no_such_words_gives_the_built_in_ones)
    {
        const Puzzle puzzle(1, 1, 0, 0, defaults, Dictionary::parse("ABLE ACID"));
        EXPECT_GE(puzzle.words().size(), 5u);
        for (const std::string& word : puzzle.words())
            EXPECT_EQ(word.size(), 6u);
    }

    TEST(FalloutScriptHackingTest, the_words_are_all_different_and_the_password_is_one_of_them)
    {
        for (std::uint32_t seed = 0; seed < 8; ++seed)
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
        EXPECT_EQ(first.dump(), second.dump());
        EXPECT_EQ(first.address(), second.address());
        const Puzzle other(8, 2, defaults);
        EXPECT_NE(first.dump(), other.dump());
    }

    TEST(FalloutScriptHackingTest, the_dump_is_408_symbols_with_the_words_laid_in_it)
    {
        for (std::uint32_t seed = 0; seed < 10; ++seed)
            for (int level = 0; level <= 4; ++level)
            {
                const Puzzle puzzle(seed, level, defaults);
                const std::string& dump = puzzle.dump();
                ASSERT_EQ(dump.size(), FalloutScript::Hacking::dumpSize);
                std::vector<bool> inWord(dump.size(), false);
                for (std::size_t word = 0; word < puzzle.words().size(); ++word)
                {
                    const std::string& text = puzzle.words()[word];
                    const std::size_t first = puzzle.placeOf(word);
                    ASSERT_LE(first + text.size(), dump.size());
                    EXPECT_EQ(dump.substr(first, text.size()), text);
                    for (std::size_t i = first; i < first + text.size(); ++i)
                        inWord[i] = true;
                    // Two characters that are not letters on each side
                    for (std::size_t i = first >= 2 ? first - 2 : 0; i < first; ++i)
                        EXPECT_FALSE(isLetter(dump[i])) << i;
                    for (std::size_t i = first + text.size(); i < first + text.size() + 2 && i < dump.size(); ++i)
                        EXPECT_FALSE(isLetter(dump[i])) << i;
                }
                // Everything else is one of the 30 symbols
                const std::string symbols = "!@#$%^*()_+=-`[]{}|;':,./<>?\\\"";
                for (std::size_t i = 0; i < dump.size(); ++i)
                {
                    if (!inWord[i])
                    {
                        EXPECT_NE(symbols.find(dump[i]), std::string::npos) << dump[i];
                    }
                }
            }
    }

    TEST(FalloutScriptHackingTest, the_rows_have_an_address_that_grows_by_twelve)
    {
        const Puzzle puzzle(11, 1, defaults);
        std::set<std::string> seen;
        for (std::size_t i = 0; i < FalloutScript::Hacking::rowCount; ++i)
        {
            const std::string row = puzzle.row(i);
            ASSERT_EQ(row.size(), 2u + 4 + 1 + FalloutScript::Hacking::rowSize);
            EXPECT_EQ(row.substr(0, 2), "0x");
            EXPECT_EQ(row[6], ' ');
            const unsigned address = static_cast<unsigned>(std::stoul(row.substr(2, 4), nullptr, 16));
            EXPECT_EQ(address, (puzzle.address() + 12 * i) & 0xFFFF);
            for (const char digit : row.substr(2, 4))
                EXPECT_TRUE((digit >= '0' && digit <= '9') || (digit >= 'A' && digit <= 'F')) << row;
            EXPECT_EQ(row.substr(7), puzzle.dump().substr(i * FalloutScript::Hacking::rowSize, 12));
            seen.insert(row);
        }
        EXPECT_EQ(seen.size(), FalloutScript::Hacking::rowCount);
    }

    TEST(FalloutScriptHackingTest, the_cursor_is_on_a_whole_word_or_a_pair_of_brackets_or_one_character)
    {
        bool word = false, brackets = false, character = false;
        for (std::uint32_t seed = 0; seed < 20; ++seed)
        {
            const Puzzle puzzle(seed, 2, defaults);
            for (std::size_t i = 0; i < puzzle.dump().size(); ++i)
            {
                const Puzzle::Unit unit = puzzle.unitAt(i);
                ASSERT_GE(unit.mLength, 1u);
                ASSERT_LE(unit.mFirst, i);
                ASSERT_GT(unit.mFirst + unit.mLength, i);
                const std::string text = puzzle.dump().substr(unit.mFirst, unit.mLength);
                switch (unit.mKind)
                {
                    case Puzzle::Unit::Kind::Word:
                        word = true;
                        EXPECT_EQ(text, puzzle.words()[unit.mWord]);
                        EXPECT_EQ(unit.mFirst, puzzle.placeOf(unit.mWord));
                        break;
                    case Puzzle::Unit::Kind::Brackets:
                    {
                        brackets = true;
                        ASSERT_GE(text.size(), 2u);
                        const std::string open = "([{<";
                        const std::string close = ")]}>";
                        const std::size_t kind = open.find(text.front());
                        ASSERT_NE(kind, std::string::npos) << text;
                        EXPECT_EQ(text.back(), close[kind]);
                        // The cursor is on the opening bracket, in one row with the closing one, and no letter between
                        EXPECT_EQ(unit.mFirst, i);
                        EXPECT_EQ(unit.mFirst / 12, (unit.mFirst + unit.mLength - 1) / 12);
                        for (const char c : text)
                            EXPECT_FALSE(isLetter(c)) << text;
                        break;
                    }
                    case Puzzle::Unit::Kind::Character:
                        character = true;
                        EXPECT_EQ(unit.mLength, 1u);
                        break;
                }
            }
        }
        EXPECT_TRUE(word);
        EXPECT_TRUE(brackets);
        EXPECT_TRUE(character);
    }

    TEST(FalloutScriptHackingTest, only_the_opening_bracket_has_the_pair)
    {
        std::size_t pairs = 0;
        for (std::uint32_t seed = 0; seed < 20; ++seed)
        {
            const Puzzle puzzle(seed, 2, defaults);
            for (std::size_t i = 0; i < puzzle.dump().size(); ++i)
            {
                const Puzzle::Unit unit = puzzle.unitAt(i);
                if (unit.mKind != Puzzle::Unit::Kind::Brackets)
                    continue;
                ++pairs;
                // The closing bracket and what is between are single characters (an opening bracket between begins a
                // pair of its own, if it has a closer)
                for (std::size_t inside = i + 1; inside < i + unit.mLength; ++inside)
                {
                    const Puzzle::Unit other = puzzle.unitAt(inside);
                    EXPECT_NE(other.mKind, Puzzle::Unit::Kind::Word);
                    if (other.mKind == Puzzle::Unit::Kind::Brackets)
                        EXPECT_EQ(other.mFirst, inside);
                    else
                        EXPECT_EQ(other.mLength, 1u);
                }
            }
        }
        EXPECT_GT(pairs, 20u);
    }

    TEST(FalloutScriptHackingTest, choosing_a_character_is_a_wrong_guess)
    {
        Puzzle puzzle(4, 2, defaults);
        int made = 0;
        for (std::size_t i = 0; i < puzzle.dump().size() && !puzzle.lockedOut(); ++i)
            if (puzzle.unitAt(i).mKind == Puzzle::Unit::Kind::Character)
            {
                const std::string before = puzzle.dump();
                const int attemptsBefore = puzzle.attemptsLeft();
                const Puzzle::Pick pick = puzzle.pick(i);
                EXPECT_FALSE(pick.mIgnored);
                EXPECT_EQ(pick.mText, before.substr(i, 1));
                EXPECT_EQ(pick.mEffect, Puzzle::Pick::Effect::None);
                EXPECT_FALSE(pick.mCorrect);
                EXPECT_EQ(pick.mLikeness, 0);
                EXPECT_EQ(pick.mAttemptsLeft, attemptsBefore - 1);
                EXPECT_EQ(puzzle.dump(), before);
                ++made;
            }
        // The attempts run out on characters, and then nothing is chosen
        EXPECT_EQ(made, puzzle.maxAttempts());
        EXPECT_TRUE(puzzle.lockedOut());
        EXPECT_TRUE(puzzle.pick(0).mIgnored);
        EXPECT_TRUE(Puzzle(4, 2, defaults).pick(100000).mIgnored);
    }

    TEST(FalloutScriptHackingTest, a_level_asks_the_science_the_settings_give)
    {
        using FalloutScript::Hacking::minimumScience;
        EXPECT_EQ(minimumScience(0, defaults), 0);
        EXPECT_EQ(minimumScience(1, defaults), 25);
        EXPECT_EQ(minimumScience(2, defaults), 50);
        EXPECT_EQ(minimumScience(3, defaults), 75);
        EXPECT_EQ(minimumScience(4, defaults), 100);
        EXPECT_EQ(minimumScience(9, defaults), 100);
        const FalloutScript::Interaction::Setting settings
            = [](std::string_view name, double fallback) { return name == "fHackingMinSkillHard" ? 80.0 : fallback; };
        EXPECT_EQ(minimumScience(3, settings), 80);
    }

    TEST(FalloutScriptHackingTest, choosing_a_word_in_the_dump_is_a_guess)
    {
        Puzzle puzzle(4, 2, defaults);
        std::size_t wrong = 0;
        while (puzzle.words()[wrong] == puzzle.password())
            ++wrong;
        const Puzzle::Pick pick = puzzle.pick(puzzle.placeOf(wrong) + 2);
        EXPECT_EQ(pick.mUnit.mKind, Puzzle::Unit::Kind::Word);
        EXPECT_EQ(pick.mText, puzzle.words()[wrong]);
        EXPECT_FALSE(pick.mCorrect);
        EXPECT_EQ(pick.mLikeness, FalloutScript::Hacking::likeness(puzzle.words()[wrong], puzzle.password()));
        EXPECT_EQ(pick.mAttemptsLeft, puzzle.maxAttempts() - 1);
        EXPECT_EQ(puzzle.attemptsLeft(), puzzle.maxAttempts() - 1);
        // The word stays in the dump
        EXPECT_EQ(puzzle.dump().substr(puzzle.placeOf(wrong), puzzle.words()[wrong].size()), puzzle.words()[wrong]);
        // The same word again is a guess again
        const Puzzle::Pick again = puzzle.pick(puzzle.placeOf(wrong));
        EXPECT_FALSE(again.mIgnored);
        EXPECT_EQ(again.mLikeness, pick.mLikeness);
        EXPECT_EQ(puzzle.attemptsLeft(), puzzle.maxAttempts() - 2);
    }

    TEST(FalloutScriptHackingTest, a_pair_of_brackets_takes_a_word_out_or_gives_the_attempts_back)
    {
        int removed = 0;
        int replenished = 0;
        for (std::uint32_t seed = 0; seed < 400; ++seed)
        {
            Puzzle puzzle(seed, 1, 0, 100, defaults, Dictionary::builtIn());
            // One attempt is spent, so that a refill can be seen
            std::size_t wrong = 0;
            while (puzzle.words()[wrong] == puzzle.password())
                ++wrong;
            puzzle.guess(wrong);
            const std::optional<std::size_t> place = bracketsOf(puzzle);
            if (!place)
                continue;
            const std::string before = puzzle.dump();
            const std::size_t wordsBefore = puzzle.wordsLeft();
            const Puzzle::Pick pick = puzzle.pick(*place);
            ASSERT_EQ(pick.mUnit.mKind, Puzzle::Unit::Kind::Brackets);
            EXPECT_EQ(pick.mText.front(), before[*place]);
            // The brackets stay where they are, and are spent: the cursor on them is on one character now
            EXPECT_EQ(puzzle.dump().substr(*place, pick.mText.size()), pick.mText);
            EXPECT_EQ(puzzle.unitAt(*place).mKind, Puzzle::Unit::Kind::Character);
            const int attemptsAfter = puzzle.attemptsLeft();
            EXPECT_EQ(
                puzzle.wordsLeft(), pick.mEffect == Puzzle::Pick::Effect::Removed ? wordsBefore - 1 : wordsBefore);
            if (pick.mEffect == Puzzle::Pick::Effect::Removed)
            {
                ++removed;
                EXPECT_EQ(puzzle.wordsLeft(), wordsBefore - 1);
                EXPECT_EQ(attemptsAfter, puzzle.maxAttempts() - 1);
                std::size_t gone = 0;
                for (std::size_t word = 0; word < puzzle.words().size(); ++word)
                    if (puzzle.removed(word))
                    {
                        ++gone;
                        EXPECT_NE(puzzle.words()[word], puzzle.password());
                        EXPECT_EQ(puzzle.dump().substr(puzzle.placeOf(word), puzzle.words()[word].size()),
                            std::string(puzzle.words()[word].size(), '.'));
                        // What was cut out was not a letter anywhere else
                        EXPECT_EQ(puzzle.unitAt(puzzle.placeOf(word)).mKind != Puzzle::Unit::Kind::Word, true);
                    }
                EXPECT_EQ(gone, 1u);
            }
            else
            {
                ASSERT_EQ(pick.mEffect, Puzzle::Pick::Effect::Replenished);
                ++replenished;
                EXPECT_EQ(puzzle.wordsLeft(), wordsBefore);
                EXPECT_EQ(attemptsAfter, puzzle.maxAttempts());
            }
            // The spent brackets are single characters now, and choosing one is a wrong guess like any other
            EXPECT_EQ(puzzle.pick(*place).mEffect, Puzzle::Pick::Effect::None);
            EXPECT_EQ(puzzle.attemptsLeft(), attemptsAfter - 1);
        }
        // About one in four of the first brackets gives the attempts back
        EXPECT_GT(removed, 100);
        EXPECT_GT(replenished, 30);
        EXPECT_LT(replenished, removed);
    }

    TEST(FalloutScriptHackingTest, the_attempts_are_given_back_once_only)
    {
        int refilled = 0;
        for (std::uint32_t seed = 0; seed < 300; ++seed)
        {
            Puzzle puzzle(seed, 0, 0, 100, defaults, Dictionary::builtIn());
            int seen = 0;
            while (puzzle.wordsLeft() > 1)
            {
                const std::optional<std::size_t> place = bracketsOf(puzzle);
                if (!place)
                    break;
                if (puzzle.pick(*place).mEffect == Puzzle::Pick::Effect::Replenished)
                    ++seen;
            }
            EXPECT_LE(seen, 1) << seed;
            refilled += seen;
        }
        EXPECT_GT(refilled, 0);
    }

    TEST(FalloutScriptHackingTest, brackets_do_nothing_when_one_word_is_left)
    {
        bool tried = false;
        for (std::uint32_t seed = 0; seed < 600 && !tried; ++seed)
        {
            Puzzle puzzle(seed, 0, 0, 100, defaults, Dictionary::parse("ABLE ACID AGED ARMY BACK BAND"));
            // Four brackets take four words out of five (a refill does not take one)
            while (puzzle.wordsLeft() > 1)
            {
                const std::optional<std::size_t> place = bracketsOf(puzzle);
                if (!place)
                    break;
                puzzle.pick(*place);
            }
            const std::optional<std::size_t> place = bracketsOf(puzzle);
            if (puzzle.wordsLeft() != 1 || !place)
                continue;
            tried = true;
            const std::string before = puzzle.dump();
            const Puzzle::Pick pick = puzzle.pick(*place);
            EXPECT_EQ(pick.mEffect, Puzzle::Pick::Effect::Denied);
            EXPECT_EQ(puzzle.dump(), before);
            EXPECT_EQ(puzzle.wordsLeft(), 1u);
        }
        EXPECT_TRUE(tried);
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
        // The same word again is a guess again, and costs the same
        const Puzzle::Guess again = puzzle.guess(wrong);
        EXPECT_FALSE(again.mIgnored);
        EXPECT_EQ(again.mLikeness, guess.mLikeness);
        EXPECT_EQ(puzzle.attemptsLeft(), FalloutScript::Hacking::attempts - 2);
        EXPECT_TRUE(puzzle.guess(10000).mIgnored);
    }

    TEST(FalloutScriptHackingTest, a_list_that_is_hard_to_tell_apart_gives_more_attempts)
    {
        using FalloutScript::Hacking::guessesNeeded;
        EXPECT_EQ(guessesNeeded({}), 0);
        EXPECT_EQ(guessesNeeded({ "ABLE" }), 1);
        // Two words and no letter in common: one guess tells, and one more settles it
        EXPECT_EQ(guessesNeeded({ "ABLE", "BACK" }), 2);
        // Words that are all the same distance from each other: each guess tells nothing of the others
        EXPECT_EQ(guessesNeeded({ "ABCD", "BCDA", "CDAB", "DABC" }), 4);
        EXPECT_EQ(guessesNeeded({ "ABCDE", "BCDEA", "CDEAB", "DEABC", "EABCD" }), 5);
        // Words that all tell by their likeness need few
        EXPECT_EQ(guessesNeeded({ "AAAA", "AAAB", "AABB", "ABBB", "BBBB" }), 2);
        // The game has four at least, and more when the list needs more
        bool more = false;
        for (std::uint32_t seed = 1; seed <= 40; ++seed)
        {
            const Puzzle puzzle(seed, 0, 0, 0, defaults, Dictionary::builtIn());
            EXPECT_EQ(puzzle.maxAttempts(), std::max(4, guessesNeeded(puzzle.words())));
            EXPECT_EQ(puzzle.attemptsLeft(), puzzle.maxAttempts());
            EXPECT_GE(puzzle.maxAttempts(), 4);
            more = more || puzzle.maxAttempts() > 4;
        }
        EXPECT_TRUE(more);
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
        EXPECT_TRUE(puzzle.pick(puzzle.placeOf(right)).mIgnored);
    }
}
