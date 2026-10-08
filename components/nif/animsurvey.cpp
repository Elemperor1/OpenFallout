#include "animsurvey.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include "base.hpp"
#include "bspline.hpp"
#include "controller.hpp"
#include "data.hpp"
#include "extra.hpp"
#include "niffile.hpp"
#include "node.hpp"

namespace Nif
{
    namespace
    {
        constexpr std::size_t sSkeletonMinNodes = 20;

        std::string_view fileName(std::string_view path)
        {
            const std::size_t slash = path.find_last_of("/\\");
            return slash == std::string_view::npos ? path : path.substr(slash + 1);
        }

        bool endsWith(std::string_view text, std::string_view suffix)
        {
            return Misc::StringUtils::ciEndsWith(text, suffix);
        }

        std::string textAnswer(std::string_view text)
        {
            return text.empty() ? "(none)" : Misc::StringUtils::lowerCase(text);
        }

        std::string cycleAnswer(NiTimeController::ExtrapolationMode mode)
        {
            switch (mode)
            {
                case NiTimeController::ExtrapolationMode::Cycle:
                    return "cycle";
                case NiTimeController::ExtrapolationMode::Reverse:
                    return "reverse";
                case NiTimeController::ExtrapolationMode::Constant:
                    return "constant";
                case NiTimeController::ExtrapolationMode::Mask:
                    break;
            }
            return "other";
        }

        std::string roundedAnswer(float value)
        {
            if (!std::isfinite(value))
                return "not a number";
            std::ostringstream out;
            out << std::fixed << std::setprecision(2) << value;
            return out.str();
        }

        const char* interpolationName(uint32_t type)
        {
            switch (type)
            {
                case InterpolationType_Linear:
                    return "linear";
                case InterpolationType_Quadratic:
                    return "quadratic";
                case InterpolationType_TCB:
                    return "tcb";
                case InterpolationType_XYZ:
                    return "xyz";
                case InterpolationType_Constant:
                    return "constant";
                default:
                    return "unknown";
            }
        }

        std::string axisOrderAnswer(NiKeyframeData::AxisOrder order)
        {
            static const char* const names[] = { "xyz", "xzy", "yzx", "yxz", "zxy", "zyx", "xyx", "yzy", "zxz" };
            const auto index = static_cast<std::size_t>(order);
            return index < std::size(names) ? names[index] : "other";
        }

        std::string travelAnswer(float units)
        {
            if (!(units > 0.5f))
                return "1: none";
            if (units < 20.f)
                return "2: under 20 units";
            if (units < 60.f)
                return "3: 20 to 60";
            if (units < 120.f)
                return "4: 60 to 120";
            if (units < 250.f)
                return "5: 120 to 250";
            if (units < 500.f)
                return "6: 250 to 500";
            return "7: over 500";
        }

        std::string speedAnswer(float unitsPerSecond)
        {
            if (!(unitsPerSecond > 1.f))
                return "1: none";
            if (unitsPerSecond < 50.f)
                return "2: under 50 units a second";
            if (unitsPerSecond < 100.f)
                return "3: 50 to 100";
            if (unitsPerSecond < 150.f)
                return "4: 100 to 150";
            if (unitsPerSecond < 200.f)
                return "5: 150 to 200";
            if (unitsPerSecond < 300.f)
                return "6: 200 to 300";
            if (unitsPerSecond < 500.f)
                return "7: 300 to 500";
            return "8: over 500";
        }

        std::string textKeyGroup(std::string_view key)
        {
            const std::size_t colon = key.find(':');
            if (colon == std::string_view::npos)
                return "(no colon)";
            return std::string(key.substr(0, colon));
        }

        // How far two rotations are apart, in degrees
        float angleBetween(const osg::Quat& a, const osg::Quat& b)
        {
            const double lengths = a.length() * b.length();
            if (!(lengths > 1e-12))
                return 180.f;
            const double dot = std::min(1.0, std::abs(a.asVec4() * b.asVec4()) / lengths);
            return static_cast<float>(2.0 * std::acos(dot) * 180.0 / osg::PI);
        }

        std::string closureAnswer(float degrees)
        {
            if (degrees < 2.f)
                return "1: under 2 degrees";
            if (degrees < 10.f)
                return "2: 2 to 10";
            if (degrees < 45.f)
                return "3: 10 to 45";
            return "4: over 45";
        }

        std::string lengthAnswer(float length)
        {
            const float error = std::abs(length - 1.f);
            if (error < 0.01f)
                return "1: within 1% of 1";
            if (error < 0.1f)
                return "2: within 10%";
            return "3: farther from 1";
        }

        osg::Quat toQuat(const std::array<float, BSplineCurve::sMaxComponents>& wxyz)
        {
            return osg::Quat(wxyz[1], wxyz[2], wxyz[3], wxyz[0]);
        }

        // The control points of a B-spline interpolator and how its curves behave, to see whether they are read right:
        // a rotation is a unit quaternion at every control point, and a sequence that cycles ends where it began
        void addSpline(AnimSurvey& survey, const NiBSplineTransformInterpolator& spline,
            const NiControllerSequence& seq, std::string_view file)
        {
            const BSplineTransform curves(spline);
            const auto channel = [&](std::string_view name, const BSplineCurve& curve, uint32_t handle) {
                const char* answer = !curve.empty()              ? "control points"
                    : (handle == 0xFFFF || handle == 0xFFFFFFFF) ? "no handle"
                                                                 : "a handle that reaches past the data";
                survey.add("B-spline " + std::string(name) + " of a block", answer, file);
            };
            channel("translation", curves.getTranslationCurve(), spline.mTranslationHandle);
            channel("rotation", curves.getRotationCurve(), spline.mRotationHandle);
            channel("scale", curves.getScaleCurve(), spline.mScaleHandle);

            if (!spline.mBasisData.empty())
                survey.add("B-spline control points in a block",
                    animSurveyCountAnswer(spline.mBasisData->mNumControlPoints), file);
            survey.add("B-spline interval compared with the sequence",
                spline.mStartTime == seq.mStartTime && spline.mStopTime == seq.mStopTime ? "the same" : "different",
                file);

            const BSplineCurve& rotation = curves.getRotationCurve();
            if (rotation.empty())
                return;
            for (std::size_t i = 0; i < rotation.size(); ++i)
            {
                const auto point = rotation.getPoint(i);
                survey.add("B-spline rotation control point, length of the quaternion",
                    lengthAnswer(std::sqrt(
                        point[0] * point[0] + point[1] * point[1] + point[2] * point[2] + point[3] * point[3])),
                    file);
            }

            if (seq.mExtrapolationMode != NiTimeController::ExtrapolationMode::Cycle || rotation.size() < 3)
                return;
            // Where the curve starts and ends, with the knots that make the first and last control points its ends
            // and, to compare, with uniform knots, where it is a blend of the three points near each end
            survey.add("Cycling B-spline rotation, angle between its ends (open knots)",
                closureAnswer(angleBetween(toQuat(rotation.evaluate(0.f)), toQuat(rotation.evaluate(1.f)))), file);
            std::array<float, BSplineCurve::sMaxComponents> start{}, end{};
            const std::size_t last = rotation.size() - 1;
            for (std::size_t c = 0; c < 4; ++c)
            {
                start[c] = (rotation.getPoint(0)[c] + 4.f * rotation.getPoint(1)[c] + rotation.getPoint(2)[c]) / 6.f;
                end[c] = (rotation.getPoint(last - 2)[c] + 4.f * rotation.getPoint(last - 1)[c]
                             + rotation.getPoint(last)[c])
                    / 6.f;
            }
            survey.add("Cycling B-spline rotation, angle between its ends (uniform knots)",
                closureAnswer(angleBetween(toQuat(start), toQuat(end))), file);
        }

        template <class KeyMap>
        void addTrack(
            AnimSurvey& survey, std::string_view name, const std::shared_ptr<KeyMap>& keys, std::string_view file)
        {
            const std::string typeSection = std::string(name) + " key type";
            const std::string countSection = std::string(name) + " keys in a track";
            if (keys == nullptr || keys->mKeys.empty())
            {
                survey.add(typeSection, "no keys", file);
                survey.add(countSection, animSurveyCountAnswer(0), file);
                return;
            }
            survey.add(typeSection, interpolationName(keys->mInterpolationType), file);
            survey.add(countSection, animSurveyCountAnswer(keys->mKeys.size()), file);
        }

        void addTransformData(AnimSurvey& survey, const NiKeyframeData& data, std::string_view file)
        {
            const bool euler
                = data.mRotations != nullptr && data.mRotations->mInterpolationType == InterpolationType_XYZ;
            if (euler)
            {
                survey.add("Rotation as Euler angles, order of the axes", axisOrderAnswer(data.mAxisOrder), file);
                addTrack(survey, "Rotation (x)", data.mXRotations, file);
                addTrack(survey, "Rotation (y)", data.mYRotations, file);
                addTrack(survey, "Rotation (z)", data.mZRotations, file);
            }
            else
                addTrack(survey, "Rotation", data.mRotations, file);
            addTrack(survey, "Translation", data.mTranslations, file);
            addTrack(survey, "Scale", data.mScales, file);
        }

        // How far the node that carries the movement travels in one run of the sequence
        void addTravel(AnimSurvey& survey, const osg::Vec3f& delta, float duration, std::string_view file)
        {
            const float travel = delta.length();
            const char* axis = std::abs(delta.x()) >= std::abs(delta.y()) && std::abs(delta.x()) >= std::abs(delta.z())
                ? "x"
                : std::abs(delta.y()) >= std::abs(delta.z()) ? "y"
                                                             : "z";
            survey.add("Travel of the accumulation root in a sequence", travelAnswer(travel), file);
            survey.add("Axis of that travel", travel > 0.5f ? axis : "none", file);
            if (duration > 0.f)
                survey.add("Speed of that travel", speedAnswer(travel / duration), file);
        }

        void addSequence(AnimSurvey& survey, const NiControllerSequence& seq, std::string_view file)
        {
            survey.add("Sequence name", textAnswer(seq.mName), file);
            survey.add("Cycle type", cycleAnswer(seq.mExtrapolationMode), file);
            survey.add("Frequency", roundedAnswer(seq.mFrequency), file);
            survey.add("Weight", roundedAnswer(seq.mWeight), file);
            survey.add("Start time", seq.mStartTime == 0.f ? "zero" : roundedAnswer(seq.mStartTime), file);
            const float duration = seq.mStopTime - seq.mStartTime;
            survey.add("Duration", animSurveyTimeAnswer(duration), file);
            survey.add("Accumulation root", textAnswer(seq.mAccumRootName), file);
            survey.add("Controlled blocks in a sequence", animSurveyCountAnswer(seq.mControlledBlocks.size()), file);
            survey.add("Controller manager of a sequence", seq.mManager.empty() ? "none" : "named", file);
            survey.add("Animation notes in a sequence", animSurveyCountAnswer(seq.mAnimNotesList.size()), file);

            std::size_t keyCount = 0;
            if (!seq.mTextKeys.empty() && seq.mTextKeys->mRecordType == RC_NiTextKeyExtraData)
            {
                const auto& keys = static_cast<const NiTextKeyExtraData&>(*seq.mTextKeys.getPtr()).mList;
                keyCount = keys.size();
                for (const NiTextKeyExtraData::TextKey& key : keys)
                {
                    const std::string text = textAnswer(key.mText);
                    survey.add("Text key", text, file);
                    survey.add("Text key group (before the colon)", textKeyGroup(text), file);
                    survey.add("Text key time",
                        seq.mStartTime == key.mTime                                   ? "at the start time"
                            : seq.mStopTime == key.mTime                              ? "at the stop time"
                            : key.mTime < seq.mStartTime || key.mTime > seq.mStopTime ? "outside the sequence"
                                                                                      : "inside the sequence",
                        file);
                }
            }
            survey.add("Text keys in a sequence", animSurveyCountAnswer(keyCount), file);
            if (!seq.mTextKeys.empty() && seq.mTextKeys->mRecordType != RC_NiTextKeyExtraData)
                survey.add("Text keys of another kind", std::string(seq.mTextKeys->mRecordName), file);

            const std::string accumRoot = Misc::StringUtils::lowerCase(seq.mAccumRootName);
            bool accumFound = false;
            bool accumSpline = false;
            for (const ControlledBlock& block : seq.mControlledBlocks)
            {
                survey.add("Controller type of a block", textAnswer(block.mControllerType), file);
                survey.add("Property type of a block", textAnswer(block.mPropertyType), file);
                survey.add("Controller id of a block", textAnswer(block.mControllerId), file);
                survey.add("Interpolator id of a block", textAnswer(block.mInterpolatorId), file);
                survey.add("Priority of a block", animSurveyNumberAnswer(block.mPriority), file);
                survey.add("Driven node", textAnswer(block.mNodeName), file);
                survey.add("Block with a controller record", block.mController.empty() ? "no" : "yes", file);

                const NiInterpolator* interp = block.mInterpolator.empty() ? nullptr : block.mInterpolator.getPtr();
                survey.add("Interpolator of a block", interp == nullptr ? "none" : interp->mRecordName, file);
                if (interp == nullptr)
                    continue;

                if (interp->mRecordType == RC_NiTransformInterpolator)
                {
                    const auto& transform = static_cast<const NiTransformInterpolator&>(*interp);
                    survey.add("Transform interpolator with data", transform.mData.empty() ? "no" : "yes", file);
                    if (!transform.mData.empty())
                    {
                        addTransformData(survey, *transform.mData.getPtr(), file);

                        const auto& rotations = transform.mData->mRotations;
                        if (seq.mExtrapolationMode == NiTimeController::ExtrapolationMode::Cycle && rotations != nullptr
                            && rotations->mInterpolationType != InterpolationType_XYZ && rotations->mKeys.size() >= 2)
                            survey.add("Cycling keyed rotation, angle between its first and last key",
                                closureAnswer(angleBetween(
                                    rotations->mKeys.front().second.mValue, rotations->mKeys.back().second.mValue)),
                                file);

                        // How far the node that carries the movement travels in one run of the sequence
                        if (!accumFound && !accumRoot.empty() && Misc::StringUtils::ciEqual(block.mNodeName, accumRoot)
                            && transform.mData->mTranslations != nullptr
                            && transform.mData->mTranslations->mKeys.size() >= 2)
                        {
                            accumFound = true;
                            const auto& keys = transform.mData->mTranslations->mKeys;
                            addTravel(survey, keys.back().second.mValue - keys.front().second.mValue, duration, file);
                        }
                    }
                }
                else if (interp->mRecordType == RC_NiBSplineCompTransformInterpolator
                    || interp->mRecordType == RC_NiBSplineTransformInterpolator)
                {
                    const auto& spline = static_cast<const NiBSplineTransformInterpolator&>(*interp);
                    survey.add("B-spline transform with data", spline.mSplineData.empty() ? "no" : "yes", file);
                    survey.add("B-spline transform with basis data", spline.mBasisData.empty() ? "no" : "yes", file);
                    if (!spline.mSplineData.empty() && !spline.mBasisData.empty())
                    {
                        addSpline(survey, spline, seq, file);

                        const BSplineTransform curves(spline);
                        if (!accumFound && !accumRoot.empty() && Misc::StringUtils::ciEqual(block.mNodeName, accumRoot)
                            && !curves.getTranslationCurve().empty())
                        {
                            accumFound = true;
                            accumSpline = true;
                            addTravel(survey,
                                *curves.getTranslation(spline.mStopTime) - *curves.getTranslation(spline.mStartTime),
                                duration, file);
                        }
                    }
                }
            }
            survey.add("Sequence whose accumulation root is driven",
                accumSpline      ? "with a B-spline translation"
                    : accumFound ? "with translation keys"
                                 : "no",
                file);
        }
    }

    void AnimSurvey::add(std::string_view section, std::string_view answer, std::string_view file)
    {
        auto it = std::ranges::find_if(mSections, [&](const auto& entry) { return entry.first == section; });
        if (it == mSections.end())
            it = mSections.insert(mSections.end(), { std::string(section), {} });

        Answer& entry = it->second[std::string(answer)];
        ++entry.mCount;
        // A file that has several sequences gives the same answer several times
        if (entry.mExamples.size() < mExamples && (entry.mExamples.empty() || entry.mExamples.back() != file))
            entry.mExamples.emplace_back(file);
    }

    std::size_t AnimSurvey::count(std::string_view section, std::string_view answer) const
    {
        const auto it = std::ranges::find_if(mSections, [&](const auto& entry) { return entry.first == section; });
        if (it == mSections.end())
            return 0;
        const auto answerIt = it->second.find(std::string(answer));
        return answerIt == it->second.end() ? 0 : answerIt->second.mCount;
    }

    void AnimSurvey::addFile(const NIFFile& file, std::string_view path)
    {
        const std::string_view name = fileName(path);
        const bool isKf = endsWith(name, ".kf");
        const std::string folder = animSurveyFolder(path);
        const std::string kind = isKf ? "kf" : "nif";

        add("Files", isKf ? "kf file" : "nif file", path);

        std::size_t sequences = 0;
        std::size_t managers = 0;
        std::set<std::string> drivenNodes;
        std::set<std::string> nodeNames;
        std::size_t nodeCount = 0;
        bool hasBip01 = false;

        for (const std::unique_ptr<Record>& record : file.mRecords)
        {
            if (record == nullptr)
                continue;
            const std::string& recordName = record->mRecordName;

            // Every interpolator and controller the file holds, to see which kinds the game uses where
            if (recordName.find("Interpolator") != std::string::npos)
                add("Interpolator records in " + kind + " files", recordName, path);
            else if (recordName.ends_with("Controller"))
                add("Controller records in " + kind + " files", recordName, path);

            if (record->mRecordType == RC_NiControllerSequence)
            {
                ++sequences;
                const auto& sequence = static_cast<const NiControllerSequence&>(*record);
                addSequence(*this, sequence, path);
                for (const ControlledBlock& block : sequence.mControlledBlocks)
                    if (!block.mNodeName.empty())
                        drivenNodes.insert(Misc::StringUtils::lowerCase(block.mNodeName));
            }
            else if (record->mRecordType == RC_NiControllerManager)
                ++managers;

            if (const auto* object = dynamic_cast<const NiNode*>(record.get()))
            {
                ++nodeCount;
                std::string lower = Misc::StringUtils::lowerCase(object->mName);
                if (lower == "bip01")
                    hasBip01 = true;
                if (!lower.empty())
                    nodeNames.insert(std::move(lower));
            }
        }

        if (isKf)
        {
            add("Sequences in a kf file", animSurveyNumberAnswer(sequences, 2), path);
            add("Kf files per folder", folder.empty() ? "(none)" : folder, name);
            if (!drivenNodes.empty())
                mDriven.push_back({ std::string(path), std::move(drivenNodes) });
        }
        else
        {
            if (sequences > 0)
                add("Nif files that hold sequences", animSurveyNumberAnswer(sequences, 2), path);
            if (managers > 0)
                add("Nif files that hold controller managers", animSurveyNumberAnswer(managers, 2), path);

            const bool skeleton
                = Misc::StringUtils::ciStartsWith(name, "skeleton") || (nodeCount >= sSkeletonMinNodes && hasBip01);
            if (skeleton)
            {
                add("Skeleton files", Misc::StringUtils::ciStartsWith(name, "skeleton") ? "named skeleton" : "other",
                    path);
                add("Nodes in a skeleton file", animSurveyCountAnswer(nodeCount), path);
                add("Bip01 node in a skeleton file", hasBip01 ? "yes" : "no", path);
                for (const std::string& node : nodeNames)
                    add("Node name in a skeleton file", node, path);
                mSkeletons[folder].insert(nodeNames.begin(), nodeNames.end());
            }
        }
    }

    void AnimSurvey::compareNames()
    {
        if (mCompared)
            return;
        mCompared = true;

        for (const Driven& driven : mDriven)
        {
            // The skeleton of the folder of the file or of the nearest folder above it that has one
            std::string folder = animSurveyFolder(driven.mFile);
            const std::set<std::string>* skeleton = nullptr;
            while (true)
            {
                const auto it = mSkeletons.find(folder);
                if (it != mSkeletons.end())
                {
                    skeleton = &it->second;
                    break;
                }
                if (folder.empty())
                    break;
                // One folder up: drop the slash at the end and then the name of the folder
                folder.pop_back();
                const std::size_t slash = folder.find_last_of('/');
                folder.resize(slash == std::string::npos ? 0 : slash + 1);
            }

            if (skeleton == nullptr)
            {
                add("Kf files by nodes that the skeleton of their folder lacks", "no skeleton in the folder or above",
                    driven.mFile);
                continue;
            }

            std::size_t missing = 0;
            for (const std::string& node : driven.mNodes)
            {
                if (skeleton->count(node) == 0)
                {
                    ++missing;
                    add("Driven node that the skeleton of the folder lacks", node, driven.mFile);
                }
            }
            add("Kf files by nodes that the skeleton of their folder lacks", animSurveyCountAnswer(missing),
                driven.mFile);
        }
    }

    void AnimSurvey::print(std::ostream& out)
    {
        compareNames();

        const std::ios_base::fmtflags flags = out.flags();
        const std::streamsize precision = out.precision();
        for (const auto& [section, answers] : mSections)
        {
            std::size_t total = 0;
            for (const auto& [text, answer] : answers)
                total += answer.mCount;

            // With a limit, the commonest answers are printed, in the order of their text
            std::size_t cut = 0;
            std::size_t shown = answers.size();
            std::size_t threshold = 0;
            if (mMaxAnswers > 0 && answers.size() > mMaxAnswers)
            {
                std::vector<std::size_t> counts;
                for (const auto& [text, answer] : answers)
                    counts.push_back(answer.mCount);
                std::ranges::sort(counts, std::greater<>());
                threshold = counts[mMaxAnswers - 1];
                shown = 0;
            }

            out << "\n" << section << " (" << total << ")\n";
            for (const auto& [text, answer] : answers)
            {
                if (threshold > 0 && (answer.mCount < threshold || shown >= mMaxAnswers))
                {
                    ++cut;
                    continue;
                }
                ++shown;
                out << std::setw(8) << answer.mCount << std::setw(7) << std::fixed << std::setprecision(1)
                    << (100.0 * answer.mCount / total) << "%  " << text;
                for (std::size_t i = 0; i < answer.mExamples.size(); ++i)
                    out << (i == 0 ? "   e.g. " : ", ") << answer.mExamples[i];
                out << "\n";
            }
            if (cut > 0)
                out << "         (" << cut << " rarer answers not shown)\n";
        }
        out.flags(flags);
        out.precision(precision);
        out.flush();
    }

    std::string animSurveyFolder(std::string_view path)
    {
        std::string folder = Misc::StringUtils::lowerCase(path);
        std::ranges::replace(folder, '\\', '/');
        const std::size_t slash = folder.find_last_of('/');
        if (slash == std::string::npos)
            return {};
        folder.resize(slash + 1);
        return folder;
    }

    std::string animSurveyNumberAnswer(std::size_t number, std::size_t width)
    {
        std::string result = std::to_string(number);
        if (result.size() < width)
            result.insert(0, width - result.size(), '0');
        return result;
    }

    std::string animSurveyCountAnswer(std::size_t count)
    {
        if (count == 0)
            return "1: none";
        if (count == 1)
            return "2: one";
        if (count < 10)
            return "3: 2 to 9";
        if (count < 100)
            return "4: 10 to 99";
        if (count < 1000)
            return "5: 100 to 999";
        return "6: 1000 or more";
    }

    std::string animSurveyTimeAnswer(float seconds)
    {
        if (!(seconds > 0.f))
            return "1: none";
        if (seconds < 0.5f)
            return "2: under 0.5 s";
        if (seconds < 1.f)
            return "3: 0.5 to 1 s";
        if (seconds < 2.f)
            return "4: 1 to 2 s";
        if (seconds < 4.f)
            return "5: 2 to 4 s";
        if (seconds < 8.f)
            return "6: 4 to 8 s";
        if (seconds < 16.f)
            return "7: 8 to 16 s";
        return "8: 16 s or more";
    }
}
