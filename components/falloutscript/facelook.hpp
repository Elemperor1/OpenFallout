#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FACELOOK_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FACELOOK_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    /// How a character looks, as the screen of the sex, race and face (RaceSexMenu, menu 1036, FalloutNV.exe 0x7AC730)
    /// edits it: the NPC record's own fields, which the executable changes in place (race +0x10C, the sex bit of +0x30,
    /// hair +0x1B8, hair length +0x1BC, eyes +0x1C0, hair colour +0x1D8, head parts +0x1DC and the three FaceGen
    /// arrays; Mac round 12AY R2 section C). The face arrays are kept as the record has them, as the difference from the
    /// default face of the race for the sex (0x603AD0 adds the race's to the record's, 0x652E00 takes it off again).
    struct FaceLook
    {
        static constexpr int shapeCount = 50; // FGGS
        static constexpr int asymmetricCount = 30; // FGGA
        static constexpr int toneCount = 50; // FGTS

        FormId mRace = 0;
        bool mFemale = false;
        FormId mHair = 0;
        float mHairLength = 0.f;
        FormId mEyes = 0;
        /// Red, green and blue (npc+0x1D8 holds R | G << 8 | B << 16)
        std::array<std::uint8_t, 3> mHairColour{ 0, 0, 0 };
        /// The head parts the record names: the beard and the eyebrows
        std::vector<FormId> mHeadParts;
        std::vector<float> mShape = std::vector<float>(shapeCount, 0.f);
        std::vector<float> mAsymmetric = std::vector<float>(asymmetricCount, 0.f);
        std::vector<float> mTone = std::vector<float>(toneCount, 0.f);

        bool operator==(const FaceLook& other) const = default;
    };

    /// The two things a slider of the face screen moves: the shape of the head (FGGS) and the tone of the skin (FGTS)
    enum class FaceKind
    {
        Shape,
        Tone,
    };

    /// A standard normal number from a source of uniform numbers in [0, 1): the polar method of Marsaglia, which is what
    /// the executable does (FalloutNV.exe 0xAC6360 on 0xAC6220, the C library's rand divided by RAND_MAX)
    class Gauss
    {
    public:
        explicit Gauss(std::function<double()> uniform);
        double next();

    private:
        std::function<double()> mUniform;
        double mSpare = 0.0;
        bool mHasSpare = false;
    };

    /// The controls of the face screen: directions in the space of the coefficients (facegen\si.ctl, read by the
    /// executable at 0xAAA7D0). A slider's value is the dot product of its direction with the coefficients, and moving
    /// the slider adds the difference, along the direction, to every coefficient (0x652320: coef += (value - current) *
    /// row). The directions are what the file holds; this class only does the arithmetic, in the executable's own order
    /// and precision: the sum of the dot product is kept as a float after every term (0x658360), the product of the
    /// terms is not. The rows are not at right angles to each other, and the ones of the tone are not exactly of length
    /// 1 (12BB A): nothing here makes them so.
    class FaceControls
    {
    public:
        /// The directions of one kind, one row for each control. A row is as long as the coefficients of its kind.
        void set(FaceKind kind, std::vector<std::vector<float>> rows);
        int count(FaceKind kind) const;
        bool has(FaceKind kind, int control) const;

        /// The value of the control for these coefficients (0 when the control is not there)
        float value(FaceKind kind, int control, const std::vector<float>& coefficients) const;
        /// Moves the coefficients so that the control has this value; false and nothing changed when it is not there
        bool move(FaceKind kind, int control, float value, std::vector<float>& coefficients) const;

    private:
        std::array<std::vector<std::vector<float>>, 2> mRows;
    };

    /// The dot product of a direction and the coefficients, as the executable sums it (0x658360)
    float faceDot(const std::vector<float>& direction, const std::vector<float>& coefficients);

    /// The age of a face: two directions of the shape and two of the tone (the fan controls of facegen\si.ctl,
    /// FalloutNV.exe 0xAAAF10, Mac round 12BB A). Group 0 is the age in years (15 to 65), group 1 a second number that
    /// only the random face draws (-4 to 4). A group's value is the dot product of its direction with the coefficients
    /// of the kind plus an offset. To set one, the difference of both groups from what they should be is turned into a
    /// move along both directions with the inverse of the matrix of their dot products, so that the one that is not
    /// asked for keeps its value (0xAAC170).
    class FaceAge
    {
    public:
        static constexpr int groups = 2;
        static constexpr float minimumYears = 15.f;
        static constexpr float maximumYears = 65.f;
        static constexpr float minimumSecond = -4.f;
        static constexpr float maximumSecond = 4.f;

        /// The direction and the offset of a group for a kind of coefficients
        void set(FaceKind kind, int group, std::vector<float> direction, float offset);
        /// Works out the inverse matrices once the four directions are there
        void finish();
        bool loaded() const { return mLoaded; }

        float value(int group, FaceKind kind, const std::vector<float>& coefficients) const;
        /// Sets one group of a kind to a value (kept to the group's range; the other group keeps its value)
        void move(int group, FaceKind kind, float value, std::vector<float>& coefficients) const;
        /// The age slider (0x7B4B50): the age in years for the shape, the same age moved by what the tone is older or
        /// younger than the shape for the tone, both kept to 15 to 65. Nothing when the directions are not there.
        bool setAge(float years, std::vector<float>& shape, std::vector<float>& tone) const;

    private:
        struct Fan
        {
            std::vector<float> mDirection;
            float mOffset = 0.f;
        };
        std::array<std::array<Fan, groups>, 2> mFans;
        /// The inverse of the matrix of the dot products of the two directions of a kind, row by row
        std::array<std::array<float, 4>, 2> mInverse{};
        bool mLoaded = false;
    };

    /// The years the age slider's whole number (1 to 10) stands for: truncated, as FalloutNV.exe does (0x7B4E05)
    int faceAgeYears(int slider);

    /// What a slider of the face shows as a whole number: the control's value times ten, truncated towards zero, as
    /// FalloutNV.exe does (0x7B181A, Mac round 12BB B: not rounded, not floored). The slider's number divided by ten is
    /// what a move writes (0x7AF7E5).
    int faceSliderInteger(float value);
    float faceSliderValue(int integer);

    /// The numbers of the sliders of the face lists whose range is not the usual one, and the usual one: the executable
    /// reads fRSMFaceSliderDefaultMin and fRSMFaceSliderDefaultMax from the INI, and the INI files of both games say
    /// -3.5 and 3.5 (the executable's own defaults, -5 and 5, apply only when the keys are missing)
    constexpr int faceSliderDefaultMinimum = -35;
    constexpr int faceSliderDefaultMaximum = 35;

    /// One slider of the screen's face lists
    struct FaceSlider
    {
        FaceKind mKind = FaceKind::Shape;
        /// The control of facegen\si.ctl (12AY R2: the control index that the executable passes, not a coefficient index)
        int mControl = 0;
        /// The number of the list the slider is in (10 to 18 for the shape lists, 19 for the tone list)
        int mList = 0;
        /// The editor setting that names the slider (`sRSMShapeOptionNN` for NN = the number given, or one of the plain
        /// names of the tone list), and the number in it
        std::string mSetting;
        int mMinimum = faceSliderDefaultMinimum;
        int mMaximum = faceSliderDefaultMaximum;
    };
    /// The 30 sliders of the shape lists and the 12 of the tone list, in the order the screen shows them (12AW S7)
    const std::vector<FaceSlider>& faceSliders();

    /// A face of random coefficients around the race's (0x601830): each shape and tone coefficient is a standard normal
    /// number times the variation (the setting fNPCGeneticVariation, 0.5) times 1.5 (0xAAD6B0 and the double at
    /// 0x1016FF0), the asymmetric ones are 0 and the hair, eyes and colour stay. Because the record keeps the difference
    /// from the race's face, that number is the coefficient itself. The executable then puts the age sliders of the old
    /// face back on the new one (the fan controls, 0x652440 and 0x652470); that is for the caller.
    FaceLook randomFace(const FaceLook& current, double variation, Gauss& gauss);
}

#endif
