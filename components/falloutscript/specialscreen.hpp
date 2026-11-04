#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SPECIALSCREEN_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SPECIALSCREEN_H

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace FalloutScript::SpecialScreen
{
    /// The two screens on which the player sets the seven attributes by turning the pages of a model: the Vigor Tester
    /// of New Vegas (LoveTesterMenu, menu 1074, FalloutNV.exe 0x7915F0) and the SPECIAL book of New Vegas and Fallout 3
    /// (SPECIALBookMenu, menu 1060, FalloutNV.exe 0x7C5FC0). The pictures are the model's: the screen only says which
    /// page the model is on and what each of its named shapes shows. The rules are those of the executables (Mac
    /// rounds 12AW S5 and S6, 12AY R1); a thing the Mac did not read is marked `guess`.
    enum class Kind
    {
        Tester,
        Book,
    };

    /// The sounds the screen makes, by the editor ids of the games' sound records
    enum class Sound
    {
        None,
        PageTurn, // OBJBookSpecialPageTurn
        Number, // OBJBookSpecialNumber
        NumberDown, // OBJBookSpecialNumberDown (the Vigor Tester only)
        Finished, // OBJBookSpecialNumberFinished (the Vigor Tester only)
        Focus, // OBJBookSpecialFocus
        Nothing, // UIActivateNothing
    };

    const char* soundName(Sound sound);

    /// What an action of the player did
    struct Outcome
    {
        Sound mSound = Sound::None;
        /// The attribute that changed (0 to 6, in the order of SPECIAL) and what it is now, -1 for none
        int mAttribute = -1;
        int mValue = 0;
        /// The player is done and the screen goes
        bool mClose = false;
    };

    /// A shape of the model that the screen decides for: whether it is shown, and the picture to put on it ("" for
    /// the one the model has)
    struct ShapeState
    {
        std::string mShape;
        bool mVisible = true;
        std::string mTexture;
    };

    class Screen
    {
    public:
        static constexpr int attributeCount = 7;
        static constexpr int minimum = 1;
        static constexpr int maximum = 10;

        /// `limit` is the points that all the attributes together have to come to (35 when a script gives none, 40 for
        /// the scripts of the games), `values` what the attributes are when the screen opens. `pad` is true when a
        /// pad is the device in use (the buttons show the pad's pictures and the index page has a cursor).
        Screen(Kind kind, int limit, const std::array<int, attributeCount>& values, bool pad = false);

        Kind kind() const { return mKind; }
        int limit() const { return mLimit; }
        int total() const;
        /// The points still to give (negative when there are too many)
        int left() const { return mLimit - total(); }
        int value(int attribute) const;
        bool pad() const { return mPad; }
        /// The device in use changes (a pad's first button, then a key or the pointer): the index page has its cursor with a pad
        void setPad(bool pad) { mPad = pad; }

        /// The page the model shows: 0 the cover (a button that opens the book), 1 to 7 one attribute each, 8 the index
        /// of all seven, and in the book a 9th that is empty
        int page() const { return mPage; }
        int lastPage() const { return mKind == Kind::Book ? 9 : 8; }
        /// The row of the index that the pad's cursor is on (0 to 6)
        int cursor() const { return mCursor; }

        /// The sound that comes with the opening
        Sound opening() const;

        /// The sequence of the model that plays, and where it is (seconds from its start, held at its end)
        std::string_view sequence() const { return mSequence; }
        float sequenceTime() const;
        /// A page can be turned only when the clip of the last turn has played to its end (the cover is always ready)
        bool ready() const;
        void update(float seconds);

        /// The ids of the menu file: 0 next page, 1 previous page, 2 and 3 more and less of the attribute of the page,
        /// 4 the exit, 6 and 7 the cursor of the index
        Outcome next();
        Outcome previous();
        /// `fromKey` is true for the keys, which give nothing when the attribute cannot go up (or down); a button gives
        /// its sound whatever it does
        Outcome increase(bool fromKey = false);
        Outcome decrease(bool fromKey = false);
        Outcome exitAttempt();
        Outcome moveCursor(int delta);

        /// What the player did on the shape of the model that the pointer found; none if the screen has no such button
        Outcome press(std::string_view shape);
        /// The buttons of the page, which the pointer can find (the names of the shapes of the model)
        std::vector<std::string> buttons() const;
        /// Every shape the screen decides for and what it shows now
        std::vector<ShapeState> shapes() const;

        /// The names of the attributes in the shapes of the index (guess: the model names them in full)
        static const std::array<const char*, attributeCount>& attributeNames();

    private:
        /// The attribute of the page the player is on, -1 when there is none
        int pageAttribute() const;
        Outcome turn(int newPage, bool forward);
        float duration() const;

        Kind mKind;
        int mLimit;
        std::array<int, attributeCount> mValues;
        bool mPad;
        int mPage = 0;
        int mCursor = 0;
        std::string mSequence;
        float mElapsed = 0.f;
    };
}

#endif
