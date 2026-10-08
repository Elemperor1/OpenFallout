#ifndef OPENFALLOUT_TEST_SUITE_NIF_SEQUENCE_H
#define OPENFALLOUT_TEST_SUITE_NIF_SEQUENCE_H

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

            ControlledBlock block;
            block.mNodeName = track.mNode;
            block.mInterpolator = NiInterpolatorPtr(&interpolator);
            block.mController = NiTimeControllerPtr(nullptr);
            block.mBlendInterpolator = NiBlendInterpolatorPtr(nullptr);
            block.mStringPalette = NiStringPalettePtr(nullptr);
            block.mPriority = 0;
            block.mControllerType = "NiTransformController";
            sequence.mControlledBlocks.push_back(std::move(block));
        }

        file.mRoots.push_back(&sequence);
        return sequence;
    }
}

#endif
