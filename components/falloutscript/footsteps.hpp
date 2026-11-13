#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FOOTSTEPS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FOOTSTEPS_H

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FalloutScript::Footsteps
{
    /// The ground a foot comes down on, as the executables name it. The first ten are in both games; the last three
    /// (chain link, tile, carpet) are New Vegas only. The order is that of the executable's table (FalloutNV.exe
    /// 0x11A1888, Fallout3.exe 0xF6C5B8), and the sound records are named after it: `FSTDirtWalkL`.
    enum class Surface
    {
        ConcSolid,
        ConcBroken,
        MetalSolid,
        MetalHollow,
        MetalSheet,
        Wood,
        Sand,
        Dirt,
        Grass,
        Water,
        ChainLink,
        Tile,
        Carpet,
    };

    constexpr int surfaceCount = 13;
    constexpr int surfaceCountFallout3 = 10;

    /// The ways a foot makes a sound, in the order of the executable's table of names (0x11A18BC): walking and running
    /// have the left and right foot in different orders (the table lists RunR before RunL), as the table does.
    enum class Slot
    {
        WalkL,
        WalkR,
        RunR,
        RunL,
        SneakL,
        SneakR,
        StairsL,
        StairsR,
        Land,
        Jump,
    };

    constexpr int slotCount = 10;
    constexpr int slotCountFallout3 = 9; // no jump sound in Fallout 3

    /// The values of the animation text key `Enum: Left`, `Right`, `BackLeft` and `BackRight` (the index in the
    /// executable's table of the words of the keys, 0x1199B50)
    enum class Foot
    {
        Left = 0,
        Right = 1,
        BackLeft = 2,
        BackRight = 3,
    };

    /// The words of the animation text key `Enum: <word>` and their values: the index in the executable's table of the
    /// words (FalloutNV.exe 0x1199B50). The first four are the feet, and the next five (idle, aware, attack, hit, death)
    /// are the sounds that a creature has by the same numbers (the type of a CSDT of its record).
    enum class EnumKey
    {
        Left = 0,
        Right = 1,
        BackLeft = 2,
        BackRight = 3,
        Idle = 4,
        Aware = 5,
        Attack = 6,
        Hit = 7,
        Death = 8,
        Equip = 9,
        Unequip = 10,
        CameraShake = 11,
        StopSounds = 12,
        HitShader = 13,
        RiderSoft = 14,
        RiderLoud = 15,
        Weapon1 = 16,
        FoleySneak = 17,
        FoleyWalk = 18,
        FoleyRun = 19,
        ArmorSneak = 20,
        ArmorWalk = 21,
        ArmorRun = 22,
        Auxiliary1 = 23, // to Auxiliary8 = 30
    };

    /// The value of the word of an `Enum:` key (lower case, as the text keys are kept), when the table has it
    std::optional<int> enumKeyIndex(std::string_view word);

    /// The sound that a list of chances (in percent, one for each sound of a creature's list for an event) gives for a
    /// number in [0, 1]: the chances count as shares of a hundred, a list that comes to less leaves the rest as no sound
    /// at all (-1) and one that comes to more is shared out by what it comes to
    int pickByChance(const std::vector<int>& chances, float unit);

    std::string_view surfaceName(Surface surface);
    std::string_view slotName(Slot slot);

    /// The foot that a text key stands for: its word (`left`, `right`, `backleft`, `backright`, lower case, as the
    /// text keys are kept)
    std::optional<Foot> footOfKey(std::string_view word);

    /// The surface for a Havok material (the number a collision shape or a land texture names; the executable's table
    /// at FalloutNV.exe 0x11A17F8 / Fallout3.exe 0xF6C538). No material, a material below 1, or a number the table does
    /// not have is concrete: Fallout 3's table has 32 entries and New Vegas's 36.
    Surface surfaceOfMaterial(int material, bool newVegas);

    /// The Havok material number of water, which a character standing in water has in place of the ground's
    constexpr int waterMaterial = 8;

    /// The surface whose sound stands in for one that has none (the parent of the executable's table 0x11A17C4), if
    /// the surface has such
    std::optional<Surface> parentOf(Surface surface);

    /// How the character moves, from the bits of the executable's movement flags (0x400 and not 0x800 is sneaking,
    /// 0x200 running) and the material flag of stairs: sneaking before stairs before running before walking
    Slot chooseSlot(Foot foot, bool sneaking, bool onStairs, bool running);

    /// Whether the ground under a foot is a stair. The executable asks the movement of the actor (a bit that the physics
    /// of Havok sets, 0x1000000 of the flags at +0x410 of its process, tested at 0x838750); the physics of the port has no
    /// such bit, so a stair is told by its shape, as the port's own rule: the ground under the foot is flat (a tread, not
    /// a slope) and the ground a stride ahead or behind it (`reach` units along where the actor faces, a stride of a
    /// stair being about that long) is a step higher or lower, by `minStep` to `maxStep` units.
    struct StairSettings
    {
        float mReach = 24.f;
        float mMinStep = 5.f;
        float mMaxStep = 30.f;
        /// How flat the ground has to be: the z of its normal. What a slope of that tilt rises over `mReach` does not count
        /// as a step, so a ramp is not taken for stairs.
        float mFlatNormal = 0.95f;
    };
    bool isStairs(const StairSettings& settings, float normalZ, float height, std::optional<float> aheadHeight,
        std::optional<float> behindHeight);

    /// The slot of the landing and of the jump
    constexpr Slot landSlot = Slot::Land;
    constexpr Slot jumpSlot = Slot::Jump;

    /// The editor id of the sound of a surface in a slot (`FSTDirtWalkL`)
    std::string soundName(Surface surface, Slot slot);

    /// The name of the sound that plays for a surface in a slot: the sound of the surface, else that of its parent,
    /// else that of the parent's parent, else `FSTConcSolidWalkR` (the executable's order when it builds its table of
    /// sounds, 0x839AB0). `exists` says whether the files have a sound by a name. Empty when not even the last
    /// resort is there.
    std::string resolveSound(Surface surface, Slot slot, const std::function<bool(std::string_view)>& exists);

    /// The sound the armor adds to a step (FalloutNV.exe 0x8382A0 to 0x838386): power armor has its own pair,
    /// `FSTArmorPowerL` and `FSTArmorPowerR`, other heavy armor `FSTArmorL` and `FSTArmorR`. The key `Left` (value 0, as
    /// for the ground) plays the one named R, and the key `Right` the one named L.
    std::string armorSoundName(bool powerArmor, Foot foot);
    std::string armorJumpSoundName(bool powerArmor);

    /// The sound of a child's step (`FSTBabyLand` on landing)
    constexpr std::string_view babyLandSound = "FSTBabyLand";

    /// The heavy and the light landing of a hard fall
    constexpr std::string_view hardLandHeavySound = "FSTLandHardHeavy";
    constexpr std::string_view hardLandLightSound = "FSTLandHardLight";

    /// What the INI says about the sound of steps (`[Audio]` of Fallout_default.ini, and the defaults in the
    /// executables, which are the same): how far a step is heard, how loud the player's own steps are
    struct Settings
    {
        float mMaxDistance = 1100.f; // fMaxFootstepDistance
        float mPlayerVolume = 0.65f; // fPlayerFootVolume: the player's own feet in the first person
        float mFootVolume = 0.5f; // fDefaultFootVolume: the volume setting of footsteps
    };

    /// Whether a step is heard: a listener no farther from the foot than the maximum distance
    bool isHeard(const Settings& settings, float distance);

    /// The volume of the step of a character: the player in the first person has the fixed one of the INI, everyone
    /// else a random one in 0.9 to 1 (`unit` is a number in [0, 1]; FalloutNV.exe 0x8385C3 to 0x8385D8)
    float stepVolume(const Settings& settings, bool playerInFirstPerson, float unit);

    /// The volume of the sound the armor adds (one minus a random number up to a tenth: 0x8386D6 to 0x838709)
    float armorVolume(bool playerInFirstPerson, const Settings& settings, float unit);

    /// Whether the step of a character other than the player in the first person is heard from the listener: the sound
    /// reaches no farther than its own maximum attenuation distance, a hundred units for each of the byte of its data
    /// (the comparison at 0x8384F0, 0x838608)
    bool reachesListener(float distance, int maxAttenuationByte);
}

#endif
