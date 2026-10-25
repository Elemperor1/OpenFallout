#include "hacking.hpp"

#include <algorithm>
#include <array>
#include <random>

namespace FalloutScript::Hacking
{
    namespace
    {
        constexpr std::size_t shortest = 4;
        constexpr std::size_t longest = 12;

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
    }

    const std::vector<std::string>& wordsOfLength(std::size_t length)
    {
        static const std::vector<std::string> none;
        if (length < shortest || length > longest)
            return none;
        return table()[length - shortest];
    }

    std::size_t wordLength(int level)
    {
        return shortest + 2 * static_cast<std::size_t>(std::clamp(level, 0, 4));
    }

    int likeness(const std::string& first, const std::string& second)
    {
        int same = 0;
        for (std::size_t i = 0; i < first.size() && i < second.size(); ++i)
            same += first[i] == second[i];
        return same;
    }

    Puzzle::Puzzle(std::uint32_t seed, int level, const Interaction::Setting& setting)
    {
        const auto value
            = [&](std::string_view name, double fallback) { return setting ? setting(name, fallback) : fallback; };
        const std::size_t least = static_cast<std::size_t>(std::max(2.0, value("iHackingMinWords", 5)));
        const std::size_t most = std::max(least, static_cast<std::size_t>(value("iHackingMaxWords", 20)));
        const std::size_t length = wordLength(level);
        const std::vector<std::string>& pool = wordsOfLength(length);
        const std::size_t count = std::min(
            pool.size(), std::clamp(least + 3 * static_cast<std::size_t>(std::clamp(level, 0, 4)), least, most));

        std::mt19937 random(seed);
        std::vector<std::size_t> order(pool.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::shuffle(order.begin(), order.end(), random);
        order.resize(count);
        for (const std::size_t index : order)
            mWords.push_back(pool[index]);
        mPassword = std::uniform_int_distribution<std::size_t>(0, mWords.size() - 1)(random);
    }

    Puzzle::Guess Puzzle::guess(std::size_t word)
    {
        Guess result;
        result.mAttemptsLeft = mAttemptsLeft;
        if (word >= mWords.size() || mSolved || mAttemptsLeft <= 0 || !mGuessed.insert(word).second)
        {
            result.mIgnored = true;
            return result;
        }
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
