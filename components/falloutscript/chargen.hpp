#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CHARGEN_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CHARGEN_H

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "script.hpp"
#include "vanillacommands.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// The screens that making a character goes through
    enum class ChargenPage
    {
        Name, // the name of the player
        Race, // the sex and the race
        Special, // the points of the seven attributes
        Skills, // the skills that are tagged
        Traits, // the traits (New Vegas)
    };

    /// A skill the player can tag, the number scripts know it by, and the attribute that governs it
    struct ChargenSkill
    {
        int mNumber = 0;
        int mAttribute = 0;
        /// The editor id of the actor value (AVIF) that has its name and description
        const char* mEditorId = "";
        /// The name when the data has none
        const char* mName = "";
    };

    /// The character of the player as the screens of its making build it. The rules of the screens are here: how the
    /// points of the attributes are given, how many skills and traits can be picked, what a name can be, and what each
    /// skill comes to. Nothing here shows a screen, and nothing of the character is in the rest of the game until the
    /// host puts it there when a screen is done. It can be used from any thread.
    class Chargen
    {
    public:
        /// The seven attributes of SPECIAL, as the games number them as actor values (5 to 11)
        static constexpr int firstAttribute = 5;
        static constexpr int attributeCount = 7;
        static constexpr int minimumAttribute = 1;
        static constexpr int maximumAttribute = 10;
        /// The points that all the attributes together have to come to (they start at 5 each, which leaves 5 to give).
        /// This is what the scripts of the games ask for; a screen opened with a count has that instead.
        static constexpr int attributePoints = 40;
        /// How many skills are tagged (the scripts of the games ask for 3) and how many traits are picked
        static constexpr int tagLimit = 3;
        static constexpr int traitLimit = 2;
        /// The longest name, in bytes
        static constexpr std::size_t nameLimit = 31;
        /// A skill starts at this plus twice its attribute plus half the luck (the fAVDSkillBase of the games), and a
        /// tagged skill has this more (fAVDTagSkillBonus)
        static constexpr int skillBase = 2;
        static constexpr int tagBonus = 15;

        explicit Chargen(Game game = Game::NewVegas);

        /// The skills the player picks from, in the order the games list them: 13 in each (Big Guns is a skill of
        /// Fallout 3 that New Vegas left out, and Survival one that New Vegas added)
        static std::vector<ChargenSkill> skillsOf(Game game);
        const std::vector<ChargenSkill>& skills() const { return mSkills; }
        /// The traits exist in New Vegas only
        bool hasTraits() const { return mGame == Game::NewVegas; }
        Game game() const { return mGame; }

        /// Forgets everything, as for a new game
        void clear();

        /// The flag that scripts set while the player is being made (SetInChargen, GetInCharGen)
        void setInChargen(bool inChargen);
        bool inChargen() const;

        /// A menu that a script asks for: its screens come one after the other, each when the player is done with the
        /// one before (the menu of another command waits for them). The values are those that the command was called
        /// with, and `menu` the number the games give that menu (what MenuMode asks for), 0 for none. `count` is what
        /// the command asked for: the points of SPECIAL that the screen gives out (all the attributes together come to
        /// that, or to what they are when the screen opens plus that, for `add`) or how many skills it lets the player
        /// tag (that many, or that many more than are tagged when it opens); 0 is the default of the games.
        void open(const std::vector<ChargenPage>& pages, const std::vector<double>& values = {}, int menu = 0,
            int count = 0, bool add = false);
        /// The screen the player is on, none when no menu is open
        std::optional<ChargenPage> page() const;
        /// The number of the menu the screen belongs to, 0 when no menu is open
        int menu() const;
        /// The values the menu was asked for with
        std::vector<double> values() const;
        /// How many screens have been opened (a screen of the same page again is another one)
        std::uint32_t serial() const;
        /// The player is done with the screen: the next one comes, if any. Gives the page that was done, or none when
        /// there is no screen or the attributes come to more than the screen gives out (the player has to take points
        /// back first).
        std::optional<ChargenPage> finishPage();
        /// Whether the screen finished last was opened to give more (AddSPECIALPoints, AddTagSkills), and for a screen
        /// of skills what it tagged and what it let go: a character that has been made already keeps what it has, and
        /// only what the player changed is changed
        bool lastFinishAdded() const;
        std::vector<int> lastTagged() const;
        std::vector<int> lastUntagged() const;
        /// Whether the player has been through a screen of SPECIAL: until then the character is a new one, whose
        /// attributes the screen starts at 5 each, whatever its record says
        bool specialMade() const;
        /// All the screens that are waiting or open
        bool menuOpen() const;

        /// The name. The one set is cleaned: control characters are dropped, the spaces at the ends too, and it is cut
        /// at the length that fits (in whole characters). Gives the name that was kept.
        std::string setName(std::string_view name);
        std::string name() const;

        /// The sex and race the player picked, none until it did
        void setFemale(bool female);
        std::optional<bool> female() const;
        void setRace(FormId race);
        FormId race() const;

        /// The attributes by their numbers as actor values (5 to 11); a number that is none gives 0
        int attribute(int number) const;
        /// Starts the attributes from the ones the player has now (each is kept within the limits)
        void setAttributes(const std::array<int, attributeCount>& values);
        /// The points of the attributes that are left to give (negative when more were given)
        int pointsLeft() const;
        /// What the attributes have to come to, by the last screen of SPECIAL that was opened
        int pointTotal() const;
        /// Gives or takes points of an attribute: false, and nothing changed, for a number that is not an attribute, a
        /// value outside the limits, or points that are not there to give. Taking points back always works down to the
        /// minimum.
        bool adjustAttribute(int number, int delta);

        /// What the skill is now, from the attributes and the tag
        int skillValue(int skill) const;
        bool isSkill(int number) const;
        bool tagged(int skill) const;
        /// The skills that are tagged, in the order of their numbers
        std::vector<int> tags() const;
        void setTags(const std::vector<int>& skills);
        /// How many skills the last screen of skills lets the player tag
        int tagsAllowed() const;
        /// Tags a skill that is not (false, and nothing changed, when that many are tagged already or it is no skill
        /// of the game), or takes the tag off one that is
        bool toggleTag(int skill);
        /// Puts a skill in the place of a tag (SetPlayerTagSkill): the games keep the tags in places, and the tests of
        /// the games fill them one after the other. The skill leaves the place it had; false, and nothing changed,
        /// for a place that is none or a number that is no skill of the game.
        bool setTagSlot(int slot, int skill);

        bool hasTrait(FormId trait) const;
        std::vector<FormId> traits() const;
        void setTraits(const std::vector<FormId>& traits);
        /// Picks a trait that is not (false, and nothing changed, when traitLimit are picked already), or lets one go
        bool toggleTrait(FormId trait);

        /// The traits that the host has given the player as perks, so that a change of the picks takes the others back
        std::vector<FormId> givenTraits() const;
        void setGivenTraits(std::vector<FormId> traits);

    private:
        /// The screen at the front has just become the one the player is on: it sets what it gives out
        void activate();
        int taggedCount() const;
        std::vector<int> tagsLocked() const;
        int attributeSum() const;

        Game mGame;
        std::vector<ChargenSkill> mSkills;
        mutable std::mutex mMutex;
        bool mInChargen = false;
        bool mSpecialMade = false;
        /// A screen that is open or waits for its turn
        struct Screen
        {
            ChargenPage mPage = ChargenPage::Name;
            int mMenu = 0;
            int mCount = 0;
            bool mAdd = false;
        };
        std::deque<Screen> mScreens;
        std::vector<int> mTagsAtStart; // the tags when the screen of skills opened
        bool mLastAdd = false;
        std::vector<int> mLastTagged;
        std::vector<int> mLastUntagged;
        int mPointTotal = attributePoints;
        int mTagsAllowed = tagLimit;
        std::vector<double> mValues;
        std::uint32_t mSerial = 0;
        std::string mName;
        std::optional<bool> mFemale;
        FormId mRace = 0;
        std::array<int, attributeCount> mAttributes;
        std::vector<int> mTagSlots; // the tagged skills in their places, 0 for a place with none
        std::vector<FormId> mTraits;
        std::vector<FormId> mGivenTraits;
    };

    /// Gives the commands that open the screens of the making of a character to the interpreter: ShowRaceMenu,
    /// GetPlayerName (which asks for the name, as ShowNameMenu does), SexChange, SetSPECIALPoints, AddSPECIALPoints,
    /// SetTagSkills, AddTagSkills, SetPlayerTagSkill, IsPlayerTagSkill, ShowNameMenu, ShowSPECIALBookMenu and
    /// ShowLoveTesterMenu (and their Params), ShowChargenMenu (and its Params and ModValues), ShowTraitMenu and
    /// ShowTraitSelectMenu, and SetInChargen and GetInCharGen. The screens of a menu that the game does not have are
    /// left out (traits in Fallout 3). `opened` is called after a menu is asked for, so that the host can start the
    /// screens from the state of the game. Those commands that the command table does not have are left out.
    void addChargenCommands(Interpreter& interpreter, Chargen& chargen, std::function<void()> opened = {});
}

#endif
