#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HUD_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HUD_H

#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "detection.hpp"
#include "script.hpp"

namespace FalloutScript::Hud
{
    /// A line of the message area of the head-up display: the text, the icon that goes with it (a path of a texture,
    /// empty for none) and the seconds it stays
    struct Message
    {
        std::string mText;
        std::string mIcon;
        float mSeconds = 0;
    };

    /// The seconds that a message without a time of its own stays (the karma messages of the games pass 2)
    constexpr float messageSeconds = 2.f;

    /// The seconds for a text: two, and a little more for each character past what is read in that time
    float secondsFor(const std::string& text);

    /// What the games tell the player in the corner of the screen, in the order it was said, until the display takes it
    /// to show it. It can be used from any thread. Only the newest few are kept: nobody reads a line that is queued
    /// behind a dozen others, and a display that is not there (a test, a menu) must not make it grow.
    class MessageLine
    {
    public:
        static constexpr std::size_t capacity = 8;

        void push(std::string text, std::string icon = {}, float seconds = 0);
        /// The messages that were said since the last time, oldest first
        std::vector<Message> take();
        std::size_t size();
        void clear();

    private:
        std::mutex mMutex;
        std::deque<Message> mMessages;
    };

    /// A line a character says, for the subtitles of the head-up display (the tile `Subtitles` of its menu file)
    struct Subtitle
    {
        std::string mText;
        /// How long it stays from the moment it was said
        float mSeconds = 0;
        /// How far the speaker is from the player: of several lines said at once the display shows the nearest
        float mDistance = 0;
        /// A script forced the text on whatever the setting says; such a line comes before the others
        bool mForced = false;
    };

    /// The lines that were said to the player since the display last took them, oldest first (FalloutNV.exe 0x774FD0
    /// makes the entry and 0x7711F4 and 0x775230 show it: Mac round 12AO B3). It can be used from any thread. Only the
    /// newest few are kept, as in MessageLine.
    class SubtitleLine
    {
    public:
        static constexpr std::size_t capacity = 8;

        void push(Subtitle line);
        std::vector<Subtitle> take();
        std::size_t size();
        void clear();

    private:
        std::mutex mMutex;
        std::deque<Subtitle> mLines;
    };

    /// The word the sneak meter shows
    enum class Sneak
    {
        /// Nobody sees or hears the player
        Hidden,
        /// Someone heard or noticed something
        Caution,
        /// Someone who is no enemy sees the player
        Detected,
        /// An enemy sees the player
        Danger,
    };

    /// What the characters around the player make of it, as the stealth equation told when each last looked. A look
    /// that is older than a few seconds does not count: the character is gone or has lost interest. It can be used from
    /// any thread.
    class Watchers
    {
    public:
        /// A watcher looked and saw this; `hostile` for a character that fights the player
        void note(FormId watcher, Detection::Sighting sighting, bool hostile, double now);
        void forget(FormId watcher);
        void clear();

        /// The strongest of the looks that are not older than `lasting` seconds
        Sneak status(double now, double lasting = 3.0);
        /// How many characters saw the player in a look that is not older than `lasting` seconds
        int seen(double now, double lasting = 3.0);

    private:
        struct Look
        {
            Detection::Sighting mSighting = Detection::Sighting::Unseen;
            bool mHostile = false;
            double mTime = 0;
        };

        std::mutex mMutex;
        std::map<FormId, Look> mLooks;
    };

    /// The bearing of a point that lies (dx, dy) from the player, in degrees clockwise from the y axis (north), from 0
    /// up to 360 (the way the games count the heading of a character)
    double bearing(double dx, double dy);

    /// Where a thing at the bearing lies in the window of the compass, as a share of its width from the left edge
    /// (0.5 is straight ahead), for a player that faces `heading` degrees. The window shows `field` degrees across; a
    /// thing outside of it has no place.
    std::optional<double> compassShare(double heading, double bearingOfThing, double field);

    /// How far the experience is between the start of the level and the start of the next one, from 0 to 1 (1 at the
    /// highest level, where there is no next)
    double experienceShare(int experience, int levelStart, int nextLevel);

    /// The name of the texture (under Interface\Stats) of the part of the body of that type (see BodyPart::mType), none
    /// for a type that has no icon
    std::optional<std::string> limbIcon(int partType);
}

#endif
