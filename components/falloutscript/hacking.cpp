#include "hacking.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace FalloutScript::Hacking
{
    namespace
    {
        constexpr std::size_t shortest = shortestWord;
        constexpr std::size_t longest = longestWord;

        /// The symbols that fill the dump (FalloutNV.exe 0x1072AE0): no letters and no digits
        constexpr std::string_view junk = "!@#$%^*()_+=-`[]{}|;':,./<>?\\\"";
        static_assert(junk.size() == 30);

        bool isUpper(char c)
        {
            return c >= 'A' && c <= 'Z';
        }

        bool isSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        }

        /// The bracket that closes this one, 0 for a character that is not one
        char closer(char c)
        {
            switch (c)
            {
                case '(':
                    return ')';
                case '[':
                    return ']';
                case '{':
                    return '}';
                case '<':
                    return '>';
                default:
                    return 0;
            }
        }

        /// The words of each length from 4 to 12 letters
        const std::array<std::vector<std::string>, longest - shortest + 1>& table()
        {
            static const std::array<std::vector<std::string>, longest - shortest + 1> words = { {
                // 4 letters
                { "ABLE", "ACID", "AGED", "ARMY", "BACK", "BAND", "BANK", "BASE", "BEAR", "BEST", "BLUE", "BOAT",
                    "BOLT", "BONE", "BOOK", "CALM", "CAMP", "CARD", "CARE", "CASE", "CITY", "CLAY", "COAL", "COLD",
                    "CORE", "DARK", "DATA", "DEAD", "DEAL", "DOOR", "DUST", "EAST", "FARM", "FEAR", "FIRE", "FOOD",
                    "GATE", "GOLD", "HAND", "HARD", "HEAD", "HOLD", "IRON", "JUMP", "KEEP", "LAND", "LAST", "LOCK",
                    "LONG", "MAIN", "MINE", "NEWS", "NOTE", "OPEN", "PACK", "RAIN", "ROAD", "ROCK", "SAFE", "STAR",
                    "TEAM", "TOWN", "WALL", "WIND", "WIRE" },
                // 5 letters
                { "ALARM", "ARMOR", "AUDIT", "BADGE", "BASIC", "BLOCK", "BRAIN", "CABLE", "CHAIN", "CHAIR", "CLEAR",
                    "CLOCK", "CODER", "DEPOT", "DRIVE", "EARLY", "ENTRY", "FIELD", "FLAME", "FOUND", "GHOST", "GLASS",
                    "GRAIN", "GUARD", "HEART", "LASER", "LEVEL", "LIGHT", "METAL", "NIGHT", "ORDER", "PANEL", "POWER",
                    "RADIO", "SHELL", "STEEL", "STORM", "TOWER", "VAULT", "WATER", "WORLD" },
                // 6 letters
                { "ACCESS", "AGENCY", "ANCHOR", "BATTLE", "BEACON", "BUNKER", "CAMERA", "CIPHER", "CLOSET", "DAMAGE",
                    "DOMAIN", "ENGINE", "ESCAPE", "FRIEND", "GARDEN", "HUNTER", "LIGHTS", "MEMORY", "NARROW", "ORANGE",
                    "PACKET", "READER", "SECURE", "SHIELD", "SIGNAL", "SILVER", "SYSTEM", "TUNNEL", "UNLOCK",
                    "WINTER" },
                // 7 letters
                { "BALANCE", "BARRIER", "BATTERY", "CAPITAL", "CAPTAIN", "CHAMBER", "CONTROL", "COUNTER", "DEFENSE",
                    "DISPLAY", "FREEDOM", "GENERAL", "HARVEST", "JOURNEY", "KITCHEN", "LANTERN", "MACHINE", "MESSAGE",
                    "NETWORK", "OUTPOST", "PASSAGE", "PROCESS", "PROJECT", "PROTECT", "REACTOR", "SANCTUM", "SERVICE",
                    "SHOTGUN", "STATION", "SURVIVE", "WARRIOR", "WEAPONS" },
                // 8 letters
                { "APERTURE", "BACKBONE", "BUILDING", "COMPUTER", "DATABASE", "DEFENDER", "DOCUMENT", "ELEVATOR",
                    "ENGINEER", "FACILITY", "HARDWARE", "KEYBOARD", "MAGAZINE", "MONITORS", "MOUNTAIN", "MOVEMENT",
                    "OPERATOR", "OVERSEER", "PASSWORD", "QUANTITY", "RESEARCH", "SECURITY", "SETTINGS", "TERMINAL",
                    "TRIGGERS" },
                // 9 letters
                { "AUTHORITY", "BROADCAST", "CHARACTER", "COMMUNITY", "DEFENSIVE", "DETECTION", "EMERGENCY",
                    "EQUIPMENT", "GENERATOR", "HAZARDOUS", "KNOWLEDGE", "LAUNCHING", "MECHANISM", "OPERATING",
                    "PASSWORDS", "PERSONNEL", "PROTOCOLS", "RECORDING", "RESOURCES", "SATELLITE", "SCIENTIST",
                    "VIOLATION", "WASTELAND" },
                // 10 letters
                { "ACTIVATION", "ATMOSPHERE", "BACKGROUND", "COMMISSION", "CONNECTION", "DEPARTMENT", "EVACUATION",
                    "GENERATORS", "GOVERNMENT", "INSPECTION", "LABORATORY", "MANAGEMENT", "NAVIGATION", "PARTICULAR",
                    "PERMISSION", "PROTECTION", "QUARANTINE", "REINFORCED", "SUPERVISOR", "TECHNICIAN", "TECHNOLOGY",
                    "WASTELANDS" },
                // 11 letters
                { "ACCELERATOR", "ANTIBIOTICS", "ARRANGEMENT", "ASSOCIATION", "AUTHORITIES", "CALIBRATION",
                    "CONTAINMENT", "COOPERATION", "DESTRUCTION", "DEVELOPMENT", "ENGINEERING", "ENVIRONMENT",
                    "EXPLORATION", "IDENTIFIERS", "INFORMATION", "MAINTENANCE", "OBSERVATION", "PREPARATION",
                    "RADIOACTIVE", "REGULATIONS", "REPLACEMENT", "RESTRICTION", "TEMPERATURE" },
                // 12 letters
                { "ANTICIPATION", "CONSTRUCTION", "DEVELOPMENTS", "DISCONNECTED", "DISTRIBUTION", "ENVIRONMENTS",
                    "EXPLOITATION", "HEADQUARTERS", "IMMUNIZATION", "INSTALLATION", "INTELLIGENCE", "LABORATORIES",
                    "ORGANIZATION", "OVERWHELMING", "RADIOLOGICAL", "SURVEILLANCE", "TRANSMISSION", "UNAUTHORIZED",
                    "UNRESTRICTED" },
            } };
            return words;
        }

        std::array<std::vector<std::string>, longest - shortest + 1> builtInWords()
        {
            return table();
        }
    }

    Dictionary Dictionary::parse(std::string_view text)
    {
        Dictionary result;
        std::array<std::set<std::string>, longest - shortest + 1> seen;
        std::size_t at = 0;
        while (at < text.size())
        {
            while (at < text.size() && isSpace(text[at]))
                ++at;
            const std::size_t first = at;
            while (at < text.size() && !isSpace(text[at]))
                ++at;
            const std::string_view token = text.substr(first, at - first);
            if (token.size() < shortest || token.size() > longest)
                continue;
            std::string word(token);
            bool letters = true;
            for (char& c : word)
            {
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
                letters = letters && isUpper(c);
            }
            if (!letters || !seen[word.size() - shortest].insert(word).second)
                continue;
            result.mWords[word.size() - shortest].push_back(std::move(word));
        }
        return result;
    }

    const Dictionary& Dictionary::builtIn()
    {
        static const Dictionary dictionary = [] {
            Dictionary result;
            result.mWords = builtInWords();
            return result;
        }();
        return dictionary;
    }

    const std::vector<std::string>& Dictionary::ofLength(std::size_t length) const
    {
        static const std::vector<std::string> none;
        if (length < shortest || length > longest)
            return none;
        return mWords[length - shortest];
    }

    bool Dictionary::empty() const
    {
        for (const std::vector<std::string>& words : mWords)
            if (!words.empty())
                return false;
        return true;
    }

    const std::vector<std::string>& wordsOfLength(std::size_t length)
    {
        return Dictionary::builtIn().ofLength(length);
    }

    std::size_t wordLength(int level, int parity)
    {
        return std::min(longest,
            shortest + 2 * static_cast<std::size_t>(std::clamp(level, 0, 4)) + static_cast<std::size_t>(parity != 0));
    }

    namespace
    {
        constexpr std::array<const char*, 5> skillNames = { "fHackingMinSkillVeryEasy", "fHackingMinSkillEasy",
            "fHackingMinSkillAverage", "fHackingMinSkillHard", "fHackingMinSkillVeryHard" };
        constexpr std::array<double, 5> skillDefaults = { 0, 25, 50, 75, 100 };
    }

    double minimumScience(int level, const Interaction::Setting& setting)
    {
        const std::size_t index = static_cast<std::size_t>(std::clamp(level, 0, 4));
        return setting ? setting(skillNames[index], skillDefaults[index]) : skillDefaults[index];
    }

    std::size_t wordCount(int level, double science, const Interaction::Setting& setting)
    {
        const auto value
            = [&](std::string_view name, double fallback) { return setting ? setting(name, fallback) : fallback; };
        const double least = std::max(1.0, value("iHackingMinWords", 5));
        const double most = std::max(least, std::min(20.0, value("iHackingMaxWords", 20)));
        const double lowest = minimumScience(level, setting);
        // The executable divides by 100 less the lowest Science: for a terminal that asks 100 that is nothing, and it
        // has all the words
        const double span = 100 - lowest;
        const double share = span > 0 ? std::clamp(std::max(0.0, science - lowest) / span, 0.0, 1.0) : 0.0;
        return static_cast<std::size_t>(std::ceil(least + (most - least) * (1 - share) - 1e-6));
    }

    int likeness(const std::string& first, const std::string& second)
    {
        int same = 0;
        for (std::size_t i = 0; i < first.size() && i < second.size(); ++i)
            same += first[i] == second[i];
        return same;
    }

    namespace
    {
        /// The likeness of every word to every other: 0xFF on the diagonal
        using Matrix = std::vector<std::vector<unsigned char>>;

        /// The most words that have the same likeness to the word of a row (the diagonal is not counted)
        std::size_t largestGroup(const std::vector<unsigned char>& row)
        {
            std::array<std::size_t, 0x20> histogram{};
            for (const unsigned char value : row)
                if (value < histogram.size())
                    ++histogram[value];
            return *std::max_element(histogram.begin(), histogram.end());
        }

        /// FalloutNV.exe 0x769520: a count of the guesses that a player who picks well needs at the worst. The word
        /// whose largest group is smallest is the best guess; if that is no more than 2 words, one guess more
        /// settles it. Otherwise the groups of that size are searched in turn.
        std::size_t solve(const Matrix& matrix)
        {
            if (matrix.empty())
                return 0;
            std::size_t best = 255;
            for (const std::vector<unsigned char>& row : matrix)
                best = std::min(best, largestGroup(row));
            if (best <= 2)
                return best + 1;
            std::size_t result = matrix.size();
            for (const std::vector<unsigned char>& row : matrix)
            {
                if (largestGroup(row) != best)
                    continue;
                std::size_t worst = 0;
                // (the executable counts the values from 0 up to the number of words less one, not up to the length)
                for (std::size_t value = 0; value < matrix.size(); ++value)
                {
                    std::vector<std::size_t> group;
                    for (std::size_t other = 0; other < row.size(); ++other)
                        if (row[other] == value)
                            group.push_back(other);
                    if (group.size() != best)
                        continue;
                    Matrix inner;
                    for (const std::size_t first : group)
                    {
                        std::vector<unsigned char>& line = inner.emplace_back();
                        for (const std::size_t second : group)
                            line.push_back(matrix[first][second]);
                    }
                    worst = std::max(worst, solve(inner));
                }
                if (worst != 0)
                {
                    result = std::min(result, worst);
                    if (result <= 2)
                        return result + 1;
                }
            }
            return result + 1;
        }
    }

    int guessesNeeded(const std::vector<std::string>& words)
    {
        Matrix matrix(words.size(), std::vector<unsigned char>(words.size(), 0xFF));
        for (std::size_t first = 0; first < words.size(); ++first)
            for (std::size_t second = 0; second < words.size(); ++second)
                if (first != second)
                    matrix[first][second] = static_cast<unsigned char>(likeness(words[first], words[second]));
        return static_cast<int>(solve(matrix));
    }

    std::size_t Puzzle::random(std::size_t below)
    {
        return below == 0 ? 0 : static_cast<std::size_t>(mRandom() % below);
    }

    Puzzle::Puzzle(std::uint32_t seed, int level, int parity, double science, const Interaction::Setting& setting,
        const Dictionary& dictionary)
        : mRandom(seed)
    {
        build(dictionary, level, parity, science, setting);
    }

    Puzzle::Puzzle(std::uint32_t seed, int level, const Interaction::Setting& setting)
        : mRandom(seed)
    {
        build(Dictionary::builtIn(), level, 0, 0, setting);
    }

    std::vector<std::string> Puzzle::choose(const std::vector<std::string>& pool, std::size_t count)
    {
        count = std::min(count, pool.size());
        if (count == 0)
            return {};
        const std::size_t length = pool.front().size();
        const auto shuffle = [&](std::vector<std::string>& words) {
            for (std::size_t i = words.size(); i > 1; --i)
                std::swap(words[i - 1], words[random(i)]);
        };
        std::vector<std::string> shuffled = pool;
        for (int i = 0; i < 3; ++i)
            shuffle(shuffled);
        // FalloutNV.exe 0x768070: passes over the shuffled pool, each taking the words that fit with the ones taken. A
        // word that has no letter in the same place as one taken is let in with a chance of retry in ten, and retry
        // is 0 at first, so that the first passes want every word to have something in common with every other; the
        // last word has to be such that no likeness is the same for more than `cap` of the others. The executable
        // makes passes for a tenth of a second and then lets retry grow by one (at 10 it starts again from 0 with a
        // cap one more, and when the cap is over half the number of words it gives up with the words it has); here a
        // number of passes stands for the tenth of a second.
        constexpr int passesPerRound = 100;
        std::size_t retry = 0;
        std::size_t cap = count >= 2 ? (count - 2) / length + 1 : count;
        std::vector<std::string> chosen;
        const auto fits = [&](const std::string& candidate) {
            std::array<std::size_t, 8> buckets{};
            for (const std::string& other : chosen)
            {
                const std::size_t same = static_cast<std::size_t>(likeness(candidate, other));
                if (same == length)
                    return false;
                if (chosen.size() + 1 < count)
                {
                    if (same == 0 && random(10) >= retry)
                        return false;
                }
                else if (same < buckets.size() && ++buckets[same] > cap)
                    return false;
            }
            return true;
        };
        for (;;)
        {
            for (int pass = 0; pass < passesPerRound; ++pass)
            {
                chosen.clear();
                shuffle(shuffled);
                for (const std::string& candidate : shuffled)
                {
                    if (chosen.size() >= count)
                        break;
                    if (fits(candidate))
                        chosen.push_back(candidate);
                }
                if (chosen.size() >= count)
                    return chosen;
            }
            if (++retry >= 10)
            {
                retry = 0;
                ++cap;
            }
            if (cap > count / 2)
            {
                // The executable goes on with the words it has, however few; a board with none is no board
                if (chosen.empty())
                    chosen.assign(shuffled.begin(), shuffled.begin() + static_cast<std::ptrdiff_t>(count));
                return chosen;
            }
        }
    }

    void Puzzle::build(
        const Dictionary& dictionary, int level, int parity, double science, const Interaction::Setting& setting)
    {
        const std::size_t length = wordLength(level, parity);
        const std::vector<std::string>* pool = &dictionary.ofLength(length);
        if (pool->empty())
            pool = &Dictionary::builtIn().ofLength(length);
        const std::vector<std::string> chosen = choose(*pool, wordCount(level, science, setting));

        // The words are laid in a dump of symbols with two symbols on each side of them; a word that does not find
        // a place in a hundred tries has the whole dump made again, up to five times, and after that the words that
        // are left out are left out
        constexpr int rounds = 5;
        constexpr int tries = 100;
        for (int round = 0; round < rounds; ++round)
        {
            mDump.assign(dumpSize, ' ');
            for (char& c : mDump)
                c = junk[random(junk.size())];
            mWords.clear();
            mPlaces.clear();
            bool complete = true;
            for (const std::string& word : chosen)
            {
                bool placed = false;
                for (int attempt = 0; attempt < tries && !placed; ++attempt)
                {
                    const std::size_t first = random(dumpSize + 1 - word.size() - 2);
                    const std::size_t from = first >= 2 ? first - 2 : 0;
                    const std::size_t to = std::min(dumpSize, first + word.size() + 2);
                    bool free = true;
                    for (std::size_t i = from; i < to && free; ++i)
                        free = !isUpper(mDump[i]);
                    if (!free)
                        continue;
                    mDump.replace(first, word.size(), word);
                    mWords.push_back(word);
                    mPlaces.push_back(first);
                    placed = true;
                }
                complete = complete && placed;
                if (!placed && round + 1 < rounds)
                    break;
            }
            if (complete)
                break;
        }
        mWordsLeft = mWords.size();
        mPassword = random(mWords.size());
        mAddress = static_cast<unsigned>(random(0x10000));
        // At least four, and more for a list that is hard to tell apart (0x768721)
        mMaxAttempts = mAttemptsLeft = std::max(attempts, guessesNeeded(chosen));
    }

    std::string Puzzle::row(std::size_t index) const
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        const unsigned address = (mAddress + static_cast<unsigned>(rowSize * index)) & 0xFFFF;
        std::string text = "0x";
        for (int shift = 12; shift >= 0; shift -= 4)
            text += digits[(address >> shift) & 0xF];
        text += ' ';
        if (index < rowCount)
            text.append(mDump, index * rowSize, rowSize);
        return text;
    }

    Puzzle::Unit Puzzle::unitAt(std::size_t position) const
    {
        Unit unit;
        if (position >= mDump.size())
            return unit;
        unit.mFirst = position;
        unit.mLength = 1;
        if (isUpper(mDump[position]))
        {
            for (std::size_t word = 0; word < mWords.size(); ++word)
                if (!removed(word) && position >= mPlaces[word] && position < mPlaces[word] + mWords[word].size())
                {
                    unit.mKind = Unit::Kind::Word;
                    unit.mFirst = mPlaces[word];
                    unit.mLength = mWords[word].size();
                    unit.mWord = word;
                    return unit;
                }
            return unit;
        }
        // An opening bracket begins a pair when its matching closer is further along in the same row with no letter
        // before it (other brackets between do not count); the cursor has to be on the opening bracket
        const char closing = closer(mDump[position]);
        // (a pair that was used is a single character from then on)
        if (closing == 0 || mUsed.count(position) != 0)
            return unit;
        const std::size_t rowEnd = position - position % rowSize + rowSize;
        for (std::size_t close = position + 1; close < rowEnd && !isUpper(mDump[close]); ++close)
            if (mDump[close] == closing)
            {
                unit.mKind = Unit::Kind::Brackets;
                unit.mLength = close - position + 1;
                break;
            }
        return unit;
    }

    Puzzle::Pick Puzzle::pick(std::size_t position)
    {
        Pick result;
        result.mAttemptsLeft = mAttemptsLeft;
        if (mSolved || mAttemptsLeft <= 0 || position >= mDump.size())
        {
            result.mIgnored = true;
            return result;
        }
        result.mUnit = unitAt(position);
        result.mText = mDump.substr(result.mUnit.mFirst, result.mUnit.mLength);
        switch (result.mUnit.mKind)
        {
            case Unit::Kind::Character:
            {
                // A text of one character is compared with the password like a word, and is never the password
                result.mLikeness = likeness(result.mText, mWords[mPassword]);
                result.mAttemptsLeft = --mAttemptsLeft;
                break;
            }
            case Unit::Kind::Word:
            {
                const Guess guessed = guess(result.mUnit.mWord);
                result.mIgnored = guessed.mIgnored;
                result.mCorrect = guessed.mCorrect;
                result.mLikeness = guessed.mLikeness;
                result.mAttemptsLeft = guessed.mAttemptsLeft;
                break;
            }
            case Unit::Kind::Brackets:
            {
                if (mWordsLeft <= 1)
                {
                    result.mEffect = Pick::Effect::Denied;
                    break;
                }
                if (!mReplenished && random(4) == 0)
                {
                    mReplenished = true;
                    mAttemptsLeft = mMaxAttempts;
                    result.mEffect = Pick::Effect::Replenished;
                }
                else
                {
                    // A word of those that are left, but not the password (the one after it when the draw falls on
                    // it), is taken out: its place in the dump is dots
                    std::vector<std::size_t> left;
                    for (std::size_t word = 0; word < mWords.size(); ++word)
                        if (!removed(word))
                            left.push_back(word);
                    std::size_t at = random(left.size());
                    if (left[at] == mPassword)
                        at = (at + 1) % left.size();
                    const std::size_t word = left[at];
                    mRemoved.insert(word);
                    --mWordsLeft;
                    mDump.replace(mPlaces[word], mWords[word].size(), mWords[word].size(), '.');
                    result.mEffect = Pick::Effect::Removed;
                }
                // The pair stays in the dump, and cannot be used again
                mUsed.insert(result.mUnit.mFirst);
                result.mAttemptsLeft = mAttemptsLeft;
                break;
            }
        }
        return result;
    }

    Puzzle::Guess Puzzle::guess(std::size_t word)
    {
        Guess result;
        result.mAttemptsLeft = mAttemptsLeft;
        // A word that was guessed before is a guess again, and costs the same (the executable keeps no mark)
        if (word >= mWords.size() || removed(word) || mSolved || mAttemptsLeft <= 0)
        {
            result.mIgnored = true;
            return result;
        }
        mGuessed.insert(word);
        result.mLikeness = likeness(mWords[word], mWords[mPassword]);
        if (word == mPassword)
        {
            mSolved = true;
            result.mCorrect = true;
            return result;
        }
        result.mAttemptsLeft = --mAttemptsLeft;
        return result;
    }
}
