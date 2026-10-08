#ifndef OPENFALLOUT_TEST_SUITE_NIF_SEQUENCE_H
#define OPENFALLOUT_TEST_SUITE_NIF_SEQUENCE_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <components/nif/controller.hpp>
#include <components/nif/data.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/niffile.hpp>

#include "node.hpp"

namespace Nif::Testing
{
    /// The translation keys of one node that a sequence drives (linear keys)
    struct TranslationTrack
    {
        std::string mNode;
        std::vector<std::pair<float, osg::Vec3f>> mKeys;
        /// What the interpolator holds for a channel that has no keys (the identity when not given)
        std::optional<NiQuatTransform> mDefault = std::nullopt;
    };

    /// The control points of a B-spline interpolator that drives a node. The handles say where the points of a channel
    /// start in the array of the data (the shorts when the points are compact, else the floats); the channel has no
    /// points when its handle is the invalid one.
    struct SplineTrack
    {
        static constexpr uint32_t sNoHandle = 0xFFFF;

        std::string mNode;
        float mStart = 0.f;
        float mStop = 1.f;
        uint32_t mNumControlPoints = 0;
        bool mCompact = true;
        std::vector<int16_t> mCompactPoints;
        std::vector<float> mFloatPoints;
        uint32_t mTranslationHandle = sNoHandle;
        uint32_t mRotationHandle = sNoHandle;
        uint32_t mScaleHandle = sNoHandle;
        float mTranslationOffset = 0.f;
        float mTranslationHalfRange = 1.f;
        float mRotationOffset = 0.f;
        float mRotationHalfRange = 1.f;
        float mScaleOffset = 0.f;
        float mScaleHalfRange = 1.f;
        /// What the interpolator holds for a channel without points
        NiQuatTransform mValue = NiQuatTransform::getIdentity();
        bool mHasBasis = true;
    };

    template <class Record>
    Record& addRecord(NIFFile& file, const char* name, RecordType type)
    {
        auto record = std::make_unique<Record>();
        Record& result = *record;
        result.mRecordType = type;
        result.mRecordName = name;
        result.mRecordIndex = static_cast<unsigned int>(file.mRecords.size());
        file.mRecords.push_back(std::move(record));
        return result;
    }

    inline void addControlledBlock(
        NiControllerSequence& sequence, const std::string& node, NiInterpolator& interpolator)
    {
        ControlledBlock block;
        block.mNodeName = node;
        block.mInterpolator = NiInterpolatorPtr(&interpolator);
        block.mController = NiTimeControllerPtr(nullptr);
        block.mBlendInterpolator = NiBlendInterpolatorPtr(nullptr);
        block.mStringPalette = NiStringPalettePtr(nullptr);
        block.mPriority = 0;
        block.mControllerType = "NiTransformController";
        sequence.mControlledBlocks.push_back(std::move(block));
    }

    /// Adds a block to the sequence that a B-spline interpolator drives a node with
    inline void addSplineBlock(NIFFile& file, NiControllerSequence& sequence, const SplineTrack& track)
    {
        auto& data = addRecord<NiBSplineData>(file, "NiBSplineData", RC_NiBSplineData);
        data.mCompactControlPoints = track.mCompactPoints;
        data.mFloatControlPoints = track.mFloatPoints;

        auto& basis = addRecord<NiBSplineBasisData>(file, "NiBSplineBasisData", RC_NiBSplineBasisData);
        basis.mNumControlPoints = track.mNumControlPoints;

        const auto fill = [&](NiBSplineTransformInterpolator& interpolator) {
            interpolator.mStartTime = track.mStart;
            interpolator.mStopTime = track.mStop;
            interpolator.mSplineData = NiBSplineDataPtr(&data);
            interpolator.mBasisData = track.mHasBasis ? NiBSplineBasisDataPtr(&basis) : NiBSplineBasisDataPtr(nullptr);
            interpolator.mValue = track.mValue;
            interpolator.mTranslationHandle = track.mTranslationHandle;
            interpolator.mRotationHandle = track.mRotationHandle;
            interpolator.mScaleHandle = track.mScaleHandle;
        };

        if (track.mCompact)
        {
            auto& interpolator = addRecord<NiBSplineCompTransformInterpolator>(
                file, "NiBSplineCompTransformInterpolator", RC_NiBSplineCompTransformInterpolator);
            fill(interpolator);
            interpolator.mTranslationOffset = track.mTranslationOffset;
            interpolator.mTranslationHalfRange = track.mTranslationHalfRange;
            interpolator.mRotationOffset = track.mRotationOffset;
            interpolator.mRotationHalfRange = track.mRotationHalfRange;
            interpolator.mScaleOffset = track.mScaleOffset;
            interpolator.mScaleHalfRange = track.mScaleHalfRange;
            addControlledBlock(sequence, track.mNode, interpolator);
        }
        else
        {
            auto& interpolator = addRecord<NiBSplineTransformInterpolator>(
                file, "NiBSplineTransformInterpolator", RC_NiBSplineTransformInterpolator);
            fill(interpolator);
            addControlledBlock(sequence, track.mNode, interpolator);
        }
    }

    /// Adds a sequence of Oblivion and later to the file, as a root: it drives the nodes of the tracks with a transform
    /// interpolator each, and has the text keys. The records are the file's.
    inline NiControllerSequence& addSequence(NIFFile& file, const std::string& name, float start, float stop,
        NiTimeController::ExtrapolationMode mode, const std::vector<TranslationTrack>& tracks,
        const std::vector<std::pair<float, std::string>>& textKeys = {}, const std::string& accumRoot = {})
    {
        auto& texts = addRecord<NiTextKeyExtraData>(file, "NiTextKeyExtraData", RC_NiTextKeyExtraData);
        init(texts);
        for (const auto& [time, text] : textKeys)
            texts.mList.push_back({ time, text });

        auto& sequence = addRecord<NiControllerSequence>(file, "NiControllerSequence", RC_NiControllerSequence);
        sequence.mName = name;
        sequence.mAccumRootName = accumRoot;
        sequence.mTextKeys = ExtraPtr(&texts);
        sequence.mManager = NiControllerManagerPtr(nullptr);
        sequence.mStringPalette = NiStringPalettePtr(nullptr);
        sequence.mExtrapolationMode = mode;
        sequence.mFrequency = 1.f;
        sequence.mWeight = 1.f;
        sequence.mStartTime = start;
        sequence.mStopTime = stop;

        for (const TranslationTrack& track : tracks)
        {
            auto& data = addRecord<NiKeyframeData>(file, "NiTransformData", RC_NiKeyframeData);
            data.mTranslations = std::make_shared<Vector3KeyMap>();
            data.mTranslations->mInterpolationType = InterpolationType_Linear;
            for (const auto& [time, translation] : track.mKeys)
            {
                KeyT<osg::Vec3f> key;
                key.mValue = translation;
                data.mTranslations->mKeys.emplace_back(time, key);
            }

            auto& interpolator
                = addRecord<NiTransformInterpolator>(file, "NiTransformInterpolator", RC_NiTransformInterpolator);
            interpolator.mDefaultValue = track.mDefault.value_or(NiQuatTransform::getIdentity());
            interpolator.mData = NiKeyframeDataPtr(&data);

            addControlledBlock(sequence, track.mNode, interpolator);
        }

        file.mRoots.push_back(&sequence);
        return sequence;
    }
}

#endif
