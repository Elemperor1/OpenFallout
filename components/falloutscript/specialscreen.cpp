#include "specialscreen.hpp"

#include <algorithm>
#include <cctype>

namespace FalloutScript::SpecialScreen
{
    namespace
    {
        // The sequence of the model that turns to a page (Mac round 12AW S5 and S6): the page that is turned to picks the
        // clip, forwards or back
        constexpr std::array<const char*, 8> testerForward{ "Forward", "Backward", "Left", "Right", "FastForward",
            "FastBackward", "FastLeft", "FastRight" };
        constexpr std::array<const char*, 8> testerBack{ "AimUp", "Aim", "Holster", "JumpLand", "JumpLoop", "JumpStart",
            "TurnRight", "TurnLeft" };
        constexpr std::array<const char*, 9> bookForward{ "Forward", "Backward", "Left", "Right", "FastForward",
            "FastBackward", "FastLeft", "FastRight", "TurnLeft" };
        constexpr std::array<const char*, 9> bookBack{ "Attack2", "Attack1", "AimUp", "Aim", "Holster", "JumpLand",
            "JumpLoop", "JumpStart", "TurnRight" };

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            for (char& c : result)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        }

        std::string numberTexture(int number)
        {
            return "textures/terminals/bbnumber" + std::to_string(std::clamp(number, 0, 10)) + ".dds";
        }

        /// A face of a button, from the folder of the device the screen was opened for (Mac round 12BA C2)
        std::string faceTexture(bool pad, const char* name)
        {
            return std::string("textures/terminals/") + (pad ? "xbox" : "pc") + "/" + name + ".dds";
        }

        /// The picture of the book's message by the points that are left (the executable takes the folder of the language
        /// in use, English here): more than one point is the second picture, one the third, none the fourth; below none
        /// nothing is assigned and the strip of the model stays (FalloutNV.exe 0x7CA530 and the loader 0x7CADD0, Mac round
        /// 12BB G2: the first picture is the model's own and no code assigns it)
        std::string messageTexture(int left)
        {
            if (left > 1)
                return "textures/terminals/english/bbmessage02.dds";
            if (left == 1)
                return "textures/terminals/english/bbmessage03.dds";
            if (left == 0)
                return "textures/terminals/english/bbmessage04.dds";
            return {};
        }

        std::string indexShape(int attribute, const char* part)
        {
            return std::string("Index_") + Screen::attributeNames()[attribute] + part;
        }
    }

    const char* soundName(Sound sound)
    {
        switch (sound)
        {
            case Sound::PageTurn:
                return "OBJBookSpecialPageTurn";
            case Sound::Number:
                return "OBJBookSpecialNumber";
            case Sound::NumberDown:
                return "OBJBookSpecialNumberDown";
            case Sound::Finished:
                return "OBJBookSpecialNumberFinished";
            case Sound::Focus:
                return "OBJBookSpecialFocus";
            case Sound::Nothing:
                return "UIActivateNothing";
            case Sound::None:
                break;
        }
        return "";
    }

    const std::array<const char*, Screen::attributeCount>& Screen::attributeNames()
    {
        static const std::array<const char*, attributeCount> names{ "Strength", "Perception", "Endurance", "Charisma",
            "Intelligence", "Agility", "Luck" };
        return names;
    }

    Screen::Screen(Kind kind, int limit, const std::array<int, attributeCount>& values, bool pad)
        : mKind(kind)
        , mLimit(limit)
        , mValues(values)
        , mPad(pad)
        , mFacePad(pad)
    {
        // The Vigor Tester opens on its first attribute with the turn that leads to it playing (the cover is reached
        // only by going back); the book opens on its cover with its own clip
        mSequence = "Forward";
        mPage = kind == Kind::Tester ? 1 : 0;
    }

    int Screen::total() const
    {
        int sum = 0;
        for (const int value : mValues)
            sum += value;
        return sum;
    }

    int Screen::value(int attribute) const
    {
        return attribute >= 0 && attribute < attributeCount ? mValues[attribute] : 0;
    }

    Sound Screen::opening() const
    {
        return mKind == Kind::Tester ? Sound::PageTurn : Sound::None;
    }

    float Screen::duration() const
    {
        const bool forward = mSequence == "Forward";
        if (mKind == Kind::Tester)
            return forward ? 1.3f : 1.2667f;
        return forward ? 0.5f : 0.4667f;
    }

    float Screen::sequenceTime() const
    {
        return std::min(mElapsed, duration());
    }

    bool Screen::ready() const
    {
        return mPage == 0 || mElapsed >= duration();
    }

    void Screen::update(float seconds)
    {
        if (seconds > 0.f)
            mElapsed = std::min(mElapsed + seconds, duration());
    }

    int Screen::pageAttribute() const
    {
        if (mPage >= 1 && mPage <= attributeCount)
            return mPage - 1;
        // The index has no attribute of its own unless a pad's cursor is on one of its rows (the book's index with no
        // pad reads past the attributes in the executable, which is taken here as none)
        if (mPage == 8 && mPad)
            return mCursor;
        return -1;
    }

    Outcome Screen::turn(int newPage, bool forward)
    {
        mPage = newPage;
        mElapsed = 0.f;
        if (mKind == Kind::Tester)
            mSequence = forward ? testerForward[newPage - 1] : testerBack[newPage];
        else
            mSequence = forward ? bookForward[newPage - 1] : bookBack[newPage];
        Outcome outcome;
        outcome.mSound = Sound::PageTurn;
        return outcome;
    }

    Outcome Screen::next()
    {
        if (!ready())
            return {};
        // The right trigger on the last page of the Vigor Tester is its exit
        if (mKind == Kind::Tester && mPage == 8)
            return exitAttempt();
        if (mPage >= lastPage())
            return {};
        return turn(mPage + 1, true);
    }

    Outcome Screen::previous()
    {
        if (!ready() || mPage <= 0)
            return {};
        return turn(mPage - 1, false);
    }

    namespace
    {
        /// The change of one attribute that a button makes (Mac round 12AY R1 section 2 and 6). The Vigor Tester plays
        /// the sound of a rise before it looks at the points, so a button gives it when the attribute is below the top
        /// even though there are no points to give; the book plays it only when something changed.
        Outcome raiseValue(Kind kind, std::array<int, Screen::attributeCount>& values, int limit, int attribute,
            bool fromKey)
        {
            Outcome outcome;
            if (attribute < 0 || attribute >= Screen::attributeCount)
                return outcome;
            int sum = 0;
            for (const int value : values)
                sum += value;
            const int old = values[attribute];
            const bool room = sum < limit && old < Screen::maximum;
            if (fromKey && !room)
                return outcome;
            if (kind == Kind::Tester && old < Screen::maximum)
                outcome.mSound = Sound::Number;
            if (room)
            {
                values[attribute] = old + 1;
                outcome.mAttribute = attribute;
                outcome.mValue = old + 1;
                outcome.mSound = Sound::Number;
            }
            return outcome;
        }

        Outcome lowerValue(Kind kind, std::array<int, Screen::attributeCount>& values, int attribute)
        {
            Outcome outcome;
            if (attribute < 0 || attribute >= Screen::attributeCount)
                return outcome;
            const int old = values[attribute];
            if (old <= Screen::minimum)
                return outcome;
            values[attribute] = old - 1;
            outcome.mAttribute = attribute;
            outcome.mValue = old - 1;
            outcome.mSound = kind == Kind::Tester ? Sound::NumberDown : Sound::Number;
            return outcome;
        }
    }

    Outcome Screen::increase(bool fromKey)
    {
        return raiseValue(mKind, mValues, mLimit, pageAttribute(), fromKey);
    }

    Outcome Screen::decrease(bool /*fromKey*/)
    {
        return lowerValue(mKind, mValues, pageAttribute());
    }

    Outcome Screen::exitAttempt()
    {
        Outcome outcome;
        if (total() == mLimit)
        {
            outcome.mClose = true;
            outcome.mSound = mKind == Kind::Tester ? Sound::Finished : Sound::None;
        }
        else
            outcome.mSound = Sound::Nothing;
        return outcome;
    }

    Outcome Screen::moveCursor(int delta)
    {
        Outcome outcome;
        if (!mPad || mPage != 8)
            return outcome;
        const int moved = std::clamp(mCursor + delta, 0, attributeCount - 1);
        if (moved != mCursor)
        {
            mCursor = moved;
            outcome.mSound = Sound::Focus;
        }
        return outcome;
    }

    std::vector<std::string> Screen::buttons() const
    {
        std::vector<std::string> result;
        if (mKind == Kind::Tester)
        {
            // The buttons of the panel of one attribute stay in the list on every page (they do nothing off its pages)
            result = { "P1_Increase_Btn:0", "P1_Decrease_Btn:0", "P1_RT_Btn:0", "P1_LT_Btn:0" };
            if (mPage == 0)
                result.push_back("LookInside_Btn:0");
        }
        else if (mPage == 0)
            result = { "LookInside_Btn:0" };
        else if (mPage >= 1 && mPage <= attributeCount)
        {
            const std::string panel = "P" + std::to_string(mPage);
            result = { panel + "_Increase_Btn:0", panel + "_Decrease_Btn:0", panel + "_RT_Btn:0", panel + "_LT_Btn:0" };
        }
        if (mPage == 8)
        {
            for (int attribute = 0; attribute < attributeCount; ++attribute)
            {
                result.push_back(indexShape(attribute, "Increase_Btn:0"));
                result.push_back(indexShape(attribute, "Decrease_Btn:0"));
            }
            result.push_back("AllDone_Btn:0");
            if (mKind == Kind::Book)
                result.push_back("Index_LT_Btn:0");
        }
        return result;
    }

    Outcome Screen::press(std::string_view shape)
    {
        const std::string name = lowered(shape);
        const auto ends = [&](const std::string& tail) {
            return name.size() >= tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0;
        };

        if (name.starts_with("index_"))
        {
            for (int attribute = 0; attribute < attributeCount; ++attribute)
            {
                if (name == lowered(indexShape(attribute, "Increase_Btn:0")))
                    return raiseValue(mKind, mValues, mLimit, attribute, false);
                if (name == lowered(indexShape(attribute, "Decrease_Btn:0")))
                    return lowerValue(mKind, mValues, attribute);
            }
            if (name == "index_lt_btn:0")
                return previous();
            return {};
        }
        if (name == "alldone_btn:0")
            return exitAttempt();
        if (name == "lookinside_btn:0" || ends("_rt_btn:0"))
            return next();
        if (ends("_lt_btn:0"))
            return previous();
        if (ends("_increase_btn:0"))
            return increase(false);
        if (ends("_decrease_btn:0"))
            return decrease(false);
        return {};
    }

    std::vector<ShapeState> Screen::shapes() const
    {
        std::vector<ShapeState> result;
        const int left = this->left();
        const int tens = left < 0 ? 0 : std::min(left / 10, 9);
        const int ones = left < 0 ? 0 : left % 10;
        const int sum = total();

        const auto canRaise = [&](int attribute) { return mValues[attribute] < maximum && sum < mLimit; };
        const auto canLower = [&](int attribute) { return mValues[attribute] > minimum; };
        const auto add = [&](std::string shape, bool visible, std::string texture = {}) {
            result.push_back({ std::move(shape), visible, std::move(texture) });
        };

        // What the refresh of the executable does and nothing more (FalloutNV.exe 0x795E20 for the tester, 0x7CACD0 for the
        // book; Mac round 12BA C2): a shape it never touches is not in the list
        const bool spend = sum < mLimit;
        if (mKind == Kind::Tester)
        {
            // The buttons that raise and lower the attribute of the panel have none to look at (-1): they are never
            // touched, and show the picture of the file whatever the value is
            add("P1_PointsRemainDigit1:0", true, numberTexture(tens));
            add("P1_PointsRemainDigit2:0", true, numberTexture(ones));
            // The lamps of the cabinet: the glow of a lamp is cut when the attribute is below its number. The index has
            // the attribute under the cursor of a pad (none without one).
            const int shown = mPage >= 1 && mPage <= attributeCount ? mPage - 1 : (mPage == 8 && mPad ? mCursor : -1);
            const int lit = shown >= 0 ? mValues[shown] : 0;
            for (int lamp = 1; lamp <= maximum; ++lamp)
            {
                const std::string number = lamp < 10 ? "0" + std::to_string(lamp) : std::to_string(lamp);
                for (const char* variant : { ":4", ":5" })
                    add("P1_PointVal_" + number + "_GLOW" + variant, lit >= lamp);
            }
            // The faces: the right trigger is lit on the index while there are points to spend, and so is the button that
            // leaves; the left one and the one that opens the cover never
            add("P1_RT_Btn:0", true, faceTexture(mFacePad, mPage == 8 && spend ? "bbrton" : "bbrtoff"));
            add("P1_LT_Btn:0", true, faceTexture(mFacePad, "bbltoff"));
            add("LookInside_Btn:0", true, faceTexture(mFacePad, "bbrtoff"));
            add("AllDone_Btn:0", true, faceTexture(mFacePad, spend ? "bbrton" : "bbrtoff"));
        }
        else
        {
            // The digits of the points left: none when there are none, and the tens only from ten
            const bool tensShown = left >= 10;
            const bool onesShown = left > 0;
            for (int panel = 1; panel <= attributeCount; ++panel)
            {
                const int attribute = panel - 1;
                const std::string prefix = "P" + std::to_string(panel);
                add(prefix + "_Decrease_Btn:0", canLower(attribute));
                add(prefix + "_Increase_Btn:0", canRaise(attribute));
                add(prefix + "_PointVal:0", true, numberTexture(mValues[attribute]));
                add(prefix + "_PointsRemainDigit1:0", tensShown, numberTexture(tens));
                add(prefix + "_PointsRemainDigit2:0", onesShown, numberTexture(ones));
                add(prefix + "_Message:0", true, messageTexture(left));
                add(prefix + "_RT_Btn:0", true, faceTexture(mFacePad, "bbrtoff"));
                add(prefix + "_LT_Btn:0", true, faceTexture(mFacePad, "bbltoff"));
            }
            add("LookInside_Btn:0", true, faceTexture(mFacePad, "bbrtoff"));
            add("Index_PointsRemainDigit1:0", tensShown, numberTexture(tens));
            add("Index_PointsRemainDigit2:0", onesShown, numberTexture(ones));
            add("Index_Message:0", true, messageTexture(left));
            add("Index_LT_Btn:0", true, faceTexture(mFacePad, "bbltoff"));
            // The button that leaves is lit while there are points to spend
            add("AllDone_Btn:0", true, faceTexture(mFacePad, spend ? "bbxon" : "bbxoff"));
        }

        for (int attribute = 0; attribute < attributeCount; ++attribute)
        {
            // With a pad only the row under the cursor has its buttons
            const bool row = !(mPad && mPage == 8) || attribute == mCursor;
            add(indexShape(attribute, "PointVal:0"), true, numberTexture(mValues[attribute]));
            add(indexShape(attribute, "Decrease_Btn:0"), row && canLower(attribute));
            add(indexShape(attribute, "Increase_Btn:0"), row && canRaise(attribute));
        }
        return result;
    }
}
