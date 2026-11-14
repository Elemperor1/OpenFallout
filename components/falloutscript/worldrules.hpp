#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WORLDRULES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WORLDRULES_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "interpreter.hpp"

/// The rules of the world's own commands (M16, "world"): what the sky, the cells, the objects, the machine and the odd
/// corners answer, as the executables work them out (FalloutNV.exe and Fallout3.exe; the addresses are in
/// docs/openfallout/m16-world-notes.md). Everything here is pure: the host gives it the facts and acts on the answers.
namespace FalloutScript::World
{
    // ---- Form types -------------------------------------------------------------------------------------------------

    /// The record signature of a form type number (GetIsObjectType takes the number, GetIsFormType the signature): the
    /// numbers are the indexes of the executables' table of signatures (FalloutNV.exe 0x1187004 with 12 bytes to an
    /// entry, Fallout3.exe 0xF4A74C). Fallout 3 has the numbers up to 0x66; New Vegas adds 0x67 to 0x78. Empty for a
    /// number that has none.
    std::string_view formTypeSignature(int type);
    /// The number of a signature (any case), -1 for one the table lacks
    int formTypeNumber(std::string_view signature);
    /// The signature that a record type holds: the four letters of its number, as the loaders write them (the bit that
    /// the engine sets in the number of a record of the games, which no letter has, is left out)
    std::string signatureOfRecordType(std::uint32_t recordType);

    // ---- Cells ------------------------------------------------------------------------------------------------------

    /// GetInCellParam: the cell of the reference is the cell asked about when its name begins with the whole name of that
    /// cell, with no regard to case (the executable compares the first strlen(wanted) characters of the two names with
    /// _strnicmp, FalloutNV.exe 0x59EF60): not an identity test. A cell without a name has the name that the setting of
    /// default names gives an exterior one and none for an interior; the empty name of the asked cell matches every cell.
    bool cellNameMatches(std::string_view cellName, std::string_view askedName);

    // ---- Weather ----------------------------------------------------------------------------------------------------

    /// What the sky holds, for the weather queries: the weather being shown (the one the sky changes to) and the one it
    /// changes from, by the bytes of a weather record's DATA, and how far into the change the sky is
    struct SkyView
    {
        struct Weather
        {
            bool mPresent = false;
            FormId mForm = 0;
            /// 0x01 pleasant, 0x02 cloudy, 0x04 rainy, 0x08 snow
            std::uint8_t mClassification = 0;
            std::uint8_t mPrecipitationBeginFadeIn = 0;
            std::uint8_t mPrecipitationEndFadeOut = 0;
            /// The byte of the record's wind speed (0 to 255)
            std::uint8_t mWindSpeed = 0;
        };
        Weather mCurrent;
        Weather mOutgoing;
        /// How much of the change is done, 0 at its start and 1 when no change goes on (the field the executables read
        /// as the percent of the weather, FalloutNV.exe Sky+0xF4)
        double mPercent = 1.0;
    };

    /// GetIsCurrentWeather: the weather being shown is that one; the outgoing one is not looked at (0x59EBB0)
    bool isCurrentWeather(const SkyView& sky, FormId weather);
    /// GetCurrentWeatherPercent (0x59EB60): how far into a change the sky is, from 0 to 1; 1 when none goes on and 0
    /// when the sky has no weather
    double weatherPercent(const SkyView& sky);
    /// IsCloudy (0x59E7F0) and IsPleasant (0x59E700): not a yes or a no but a share from 0 to 1, the share of the change
    /// that is done when the weather shown has the bit plus the share left when the weather it comes from has it. The
    /// bits are 0x02 and 0x01.
    double classificationShare(const SkyView& sky, std::uint8_t bit);
    /// IsSnowing (0x59EA10): the weather shown is snow and the change has passed where its precipitation begins (the
    /// byte of the record over 255, times 0.999), or the weather it comes from is snow and the change has not got to
    /// where its precipitation ends (0.001 plus the byte over 255, times 0.999)
    bool isSnowing(const SkyView& sky);
    /// GetWindSpeed (0x5A1880): the wind of the weather shown from 0 to 1 (the byte over 255), blended with that of the
    /// weather it comes from by the share of the change that is done (the sky's own update does the blend, 0x63C490)
    double windSpeed(const SkyView& sky);

    // ---- Line of sight ----------------------------------------------------------------------------------------------

    using Vec3 = std::array<double, 3>;

    /// What the rule of GetLineOfSight is given about the two references (the work of FalloutNV.exe 0x59C990, the
    /// condition, and 0x88B880, the function of an actor that casts its rays)
    struct SightFacts
    {
        bool mViewerIsPlayer = false;
        bool mTargetIsPlayer = false;
        /// The viewer's combat target is the target (the executable casts three rays for that, as for the player)
        bool mTargetIsViewerTarget = false;
        /// The target is enabled and its model is in the scene
        bool mTargetLoaded = false;
        bool mTargetDead = false;
        /// Same cell, or exterior cells of one worldspace; the distance is only worked out then
        bool mSameSpace = false;
        double mDistance = 0;
        /// Where the viewer's eyes are, which way it faces (radians, the games' turn: 0 faces +y, growing towards +x)
        Vec3 mEye{};
        double mFacing = 0;
        /// The target's position and its height (the height of its bound, scaled)
        Vec3 mTarget{};
        double mTargetHeight = 0;
        /// fDetectionViewCone, in degrees across (190 in the files of both games)
        double mViewCone = 190;
        /// SetAllVisible is on: every target that is in the cone is seen
        bool mAllVisible = false;
        /// The player's case: the target is in the view of the camera (the frustum)
        bool mInFrustum = false;
    };

    /// The fractions of the target's height that the rays aim at, in the order they are tried (FalloutNV.exe 0x101DE30,
    /// 0x1011588 and 0x10290B0 in the player's case, 0x1016264, 0x1016248 and 0x101622C in the actor's)
    constexpr std::array<double, 3> sightFractions{ 0.75, 0.5, 0.25 };
    /// A target this near (units) is seen whatever lies between (0x1011590, a double that holds 2)
    constexpr double sightNearDistance = 2.0;

    /// Whether the ray from a place to a place reaches the target (nothing of the world is in the way)
    using SightRay = std::function<bool(const Vec3& from, const Vec3& to)>;

    /// GetLineOfSight:
    /// - the player as the viewer: the target has to be in the view of the camera, else 0 (no ray is cast); then up to
    ///   three rays from the camera's place, at 0.75, 0.5 and 0.25 of the target's height, and the first that reaches it
    ///   gives 1; else the actor's own test, which for the player is only the near distance;
    /// - any other viewer: a dead target is not seen (but by the player); a target within 2 units is; the view cone is
    ///   asked (|bearing - facing| below half of fDetectionViewCone); with SetAllVisible that is enough; else the rays
    ///   from the viewer's eyes, three of them when the target is the player or the viewer's combat target, otherwise
    ///   the first only.
    bool seesTarget(const SightFacts& facts, const SightRay& ray);

    // ---- The player's last actions ----------------------------------------------------------------------------------

    /// The player's list of actions that IsPlayerActionActive and GetLastPlayerAction answer from (the list at the
    /// player's +0x60C, FalloutNV.exe 0x963CE0, 0x963EB0, 0x963BF0): records of an id, the seconds it has left and the
    /// reference it concerns, the newest first. Raising an id that is in the list takes the old record away. Every
    /// frame takes the frame's seconds off every record and drops those that have none left, but ids 5 and 8, which
    /// stay until they are removed. It can be used from any thread.
    class PlayerActions
    {
    public:
        void clear();
        void raise(int id, double seconds, FormId reference = 0);
        void remove(int id);
        void update(double seconds);
        bool active(int id) const;
        /// The id of the newest record, 0 for none
        int last() const;

        /// The ids that outlast their seconds
        static bool persistent(int id) { return id == 5 || id == 8; }

    private:
        struct Record
        {
            int mId = 0;
            double mSeconds = 0;
            FormId mReference = 0;
        };
        mutable std::mutex mMutex;
        std::vector<Record> mRecords;
    };

    // ---- Splatters on the screen --------------------------------------------------------------------------------------

    /// The numbers of the blood on the screen (the settings of the games' interface file, with the defaults the
    /// executable gives them: FalloutNV.exe 0x11CF9A4 and the neighbouring objects)
    struct SplatterSettings
    {
        double mDuration = 5.0; // fBloodSplatterDuration
        double mMinSize = 0.025; // fBloodSplatterMinSize
        double mMaxSize = 0.125; // fBloodSplatterMaxSize
        double mMinOpacity = 0.1;
        double mMaxOpacity = 1.0;
        double mOpacityChance = 0.8; // fBloodSplatterOpacityChance: the share that takes the second pair of opacities
        double mMinOpacity2 = 0.1;
        double mMaxOpacity2 = 1.0;
        double mFadeStart = 0.8; // fBloodSplatterFadeStart
        int mMaxCount = 10; // iBloodSplatterMaxCount (the blood of TriggerScreenBlood only)
        bool mCustomEnabled = true; // bCustomSplatterEnabled
        bool mBloodEnabled = true; // bBloodSplatterEnabled
    };

    /// A draw of a number from 0 up to but not including 1
    using Unit = std::function<double()>;

    /// The blood that scripts put on the screen: TriggerScreenSplatter makes a group of quads of a count, a duration and
    /// multipliers of their size and opacity (FalloutNV.exe 0x4DE8E0); TriggerScreenBlood makes the plain kind (0x4E0330,
    /// Fallout3.exe 0x492E70), which has no multipliers and may not run over iBloodSplatterMaxCount quads at a time. The
    /// groups are timed in the frame (0x4DE600): their alpha falls over the end of their time by the fade the script set
    /// (SetScreenSplatterFade) and they go when it is over. Nothing here draws a quad: this is what a renderer would be
    /// told, and the answers to what a script can ask. It can be used from any thread.
    class ScreenSplatter
    {
    public:
        struct Quad
        {
            /// Place in the screen (-0.5 to 0.5 from the middle), half of its size and its opacity
            double mX = 0;
            double mY = 0;
            double mHalfSize = 0;
            double mOpacity = 0;
        };
        struct Group
        {
            bool mCustom = true;
            double mElapsed = 0;
            double mDuration = 0;
            double mAlpha = 1;
            std::vector<Quad> mQuads;
        };

        explicit ScreenSplatter(SplatterSettings settings = SplatterSettings());

        void setSettings(const SplatterSettings& settings);
        SplatterSettings settings() const;
        /// Forgets everything, as for a new game
        void reset();

        /// TriggerScreenSplatter: false (nothing is made) for a count that is 0 or less, or when the custom splatters are
        /// turned off. A duration of 0 or less is the setting's.
        bool triggerCustom(int count, double duration, double sizeMultiplier, double opacityMultiplier, const Unit& random);
        /// TriggerScreenBlood: the count is taken down to what the maximum leaves; the number of quads made
        int triggerBlood(int count, const Unit& random);
        /// SetScreenSplatterFade: a mode other than 0 fades over the last `fade` seconds of a group's time; mode 0 ramps
        /// between fBloodSplatterFadeStart of its time and its end once `fade` of its time has gone (the number is a
        /// threshold, not the start: the executable's own quirk, 0x4DE600)
        void setFade(double fade, int mode);
        /// SetScreenSplatterTexture: slot 0 the colour, 1 the alpha, 2 the flare; any other slot is ignored (0x4DEED0).
        /// The texture applies to the groups made afterwards.
        bool setTexture(int slot, std::string_view path);
        std::string texture(int slot) const;
        /// ClearScreenSplatter (0x4DEFB0): every group goes, the fade is 0 with mode 0 and the textures are the settings'
        void clear();
        /// Sets the textures a clear goes back to
        void setDefaultTextures(const std::array<std::string, 3>& textures);
        /// A frame of the seconds: the groups age, fade and go
        void update(double seconds);

        std::vector<Group> groups() const;
        /// The quads on the screen now (what the executables keep as the total count)
        int quadCount() const;
        double fade() const;
        int fadeMode() const;

    private:
        void spawn(Group group);
        mutable std::mutex mMutex;
        SplatterSettings mSettings;
        std::vector<Group> mGroups;
        double mFade = 0.8;
        int mFadeMode = 0;
        std::array<std::string, 3> mTextures;
        std::array<std::string, 3> mDefaultTextures;
    };

    /// TriggerHUDShudder: the interface shakes sideways for a time (FalloutNV.exe 0x7F7880, 0x7F7970; the fourth number
    /// of the command is read and not used). The offset of the interface in x is sin(t * frequency) * amplitude * (1 - t /
    /// duration) for t milliseconds into it, and 0 in y (the command gives no amplitude for it).
    class HudShudder
    {
    public:
        /// The defaults of the executable when the script gives none: 500 milliseconds, an amplitude of 0.1 and a
        /// frequency of 0.05 radians per millisecond
        static constexpr double defaultDuration = 500.0;
        static constexpr double defaultAmplitude = 0.1;
        static constexpr double defaultFrequency = 0.05;

        void start(double durationMs, double amplitude, double frequency);
        /// The time since the start in milliseconds moves on; the offset follows
        void update(double milliseconds);
        bool active() const { return mActive; }
        /// The offset of the interface now (x, y)
        std::array<double, 2> offset() const { return mOffset; }

    private:
        bool mActive = false;
        double mTime = 0;
        double mDuration = 0;
        double mAmplitude = 0;
        double mFrequency = 0;
        std::array<double, 2> mOffset{ 0, 0 };
    };

    // ---- The pose a reference started in ----------------------------------------------------------------------------

    struct Pose
    {
        Vec3 mPosition{};
        /// radians
        Vec3 mAngle{};
    };

    /// GetStartingPos and GetStartingAngle: the pose a reference had when its record was loaded (a reference that was
    /// made while the game runs has the pose it had when it was first asked). SetAtStart sets it to the pose the
    /// reference has now (the executable's own effect, FalloutNV.exe 0x5C0BF0: the start extra is rewritten from the
    /// current pose). It can be used from any thread.
    class StartingPoses
    {
    public:
        void clear();
        /// The pose the reference started in; `current` tells where it is now and is only called for a reference that has
        /// not been looked at, and that the record does not place
        std::optional<Pose> poseOf(FormId reference, const std::optional<Pose>& recorded, const std::function<std::optional<Pose>()>& current);
        void setToNow(FormId reference, const Pose& now);

    private:
        std::mutex mMutex;
        std::map<FormId, Pose> mPoses;
    };

    /// Degrees of an angle in radians (the executable's 180/pi, a double that holds 57.2957763671875)
    double degreesOf(double radians);

    // ---- Controls the player does not have -----------------------------------------------------------------------------

    /// GetPlayerControlsDisabled: the bit of the player's control byte for each of the seven flags the command takes, in
    /// the order of the command (movement, the Pip-Boy, fighting, point of view, looking, the text of what is under the
    /// crosshair, sneaking): 0x01, 0x04, 0x08, 0x10, 0x02, 0x20 and 0x40 (FalloutNV.exe 0x5CAF40, the labels at
    /// 0x118C608). A flag the call leaves out is 1. The answer is whether any of the chosen groups is disabled.
    std::uint8_t controlMask(const std::array<bool, 7>& chosen);
    constexpr std::array<std::uint8_t, 7> controlBits{ 0x01, 0x04, 0x08, 0x10, 0x02, 0x20, 0x40 };

    // ---- Small answers of the executables -------------------------------------------------------------------------------

    /// IsWin32 (FalloutNV.exe 0x5A3870): 1, unless a pad of the Xbox 360 kind is connected and the setting
    /// bDisable360Controller of the interface does not turn it off
    bool isWin32(bool padConnected, bool padDisabled);

    /// The integer that a setting of an ini file holds, as the Windows call that the games read their ini files with
    /// gives it: spaces first, an optional sign, then digits up to the first character that is not one. Nothing when the
    /// text has no digit (the setting then keeps the default the executable registered it with).
    std::optional<int> iniInteger(std::string_view text);

    /// IsGoreDisabled (FalloutNV.exe 0x5A5A50, Fallout3.exe 0x5143B0): the setting bDisableAllGore of the General section
    /// (a byte at 0x11DF7F8, 0x107ADC4) is exactly 1. Where the file has no such setting the answer is 0.
    bool goreDisabledBy(std::optional<int> setting);

    /// The path SetSecuritronExpression builds for the screen of a securitron (0x437F90): the two words the script gives
    /// joined by an underscore, in the folder of the securitron's textures; the shape it goes on is named by
    /// securitronScreenShape()
    std::string securitronTexture(std::string_view first, std::string_view second);
    std::string_view securitronScreenShape();

    /// SetPackDuration (FalloutNV.exe 0x8F4020): the package the character follows is over when the game hours since it
    /// began, times 100, reach the duration (so the duration is in hundredths of an hour; 0 is no limit). The hours are
    /// not wrapped over midnight.
    bool packageTimeIsUp(double durationHundredths, double hoursNow, double hoursStarted);

    /// SetDefaultOpen (FalloutNV.exe 0x5CEBF0): the bits of the flag byte of the "open state" extra of a reference: 8 is
    /// opened by default, 4 is open now
    constexpr unsigned defaultOpenBit = 0x08;
    constexpr unsigned openNowBit = 0x04;

    /// ModScale (0x5C1C40): the scale a reference has after the call is the scale it has now plus the argument, between
    /// 0.5 and 2, which the scale command then rounds to two decimals and keeps between 0.01 and 10 (0x5C1C10, 0x567490)
    double modScaleResult(double current, double added);
    /// SetSize (0xC708A0): the size of an actor's capsule is 2 for a size of 0 or less, and at most half of the
    /// controller's 10, so 5
    double actorSizeResult(double size);

    /// The mask that RedeemCode, RevokeCode and IsCodeRedeemed work on: a code is kept by its name in lower case
    std::string codeKey(std::string_view code);
}

#endif
