#include "vatsvalue.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::VatsValue
{
    bool readsAction(int mode)
    {
        return mode == Playback || mode == 2 || mode == 3;
    }

    double value(int mode, const Action* action, int index, std::uint32_t form, const InList& inList)
    {
        // The executable takes the action out of the mode (the first of the list in the playback, the data of the screen in
        // the selection) and answers 0 for everything when there is none, and for an index outside 0 to 17
        if (!readsAction(mode) || action == nullptr || index < 0 || index > 17)
            return 0.0;
        const auto flag = [action](std::uint32_t bit) { return (action->mFlags & bit) != 0 ? 1.0 : 0.0; };
        const auto listed = [&inList](std::uint32_t list, std::uint32_t item) {
            return list != 0 && item != 0 && inList && inList(list, item) ? 1.0 : 0.0;
        };
        switch (index)
        {
            case 0:
                return form != 0 && form == action->mWeapon ? 1.0 : 0.0;
            case 1:
                return listed(form, action->mWeapon);
            case 2:
                return action->mHasTarget && action->mTargetBase != 0 && form != 0 && form == action->mTargetBase ? 1.0
                                                                                                                   : 0.0;
            case 3:
                return action->mHasTarget ? listed(form, action->mTargetBase) : 0.0;
            case 4:
                return action->mHasTarget ? action->mDistance : 0.0;
            case 5:
                return static_cast<std::int32_t>(form) == action->mPart ? 1.0 : 0.0;
            case 6:
                return form == action->mKind ? 1.0 : 0.0;
            case 7:
                return action->mHits ? 1.0 : 0.0;
            case 8:
                return flag(Flag4);
            case 9:
                return action->mAmmo != 0 && form != 0 && form == action->mAmmo ? 1.0 : 0.0;
            case 10:
                return listed(form, action->mAmmo);
            case 11:
                return flag(Fatal);
            case 12:
                return flag(LimbBurst);
            case 13:
                return flag(LimbGone);
            case 14:
                return flag(LimbCrippled);
            case 15:
                // A hit with a weapon compares the weapon's kind; one with none is true for 0 only (the executable
                // tests the form that was asked for against nothing)
                if (action->mWeapon != 0)
                    return form == action->mWeaponKind ? 1.0 : 0.0;
                return form == 0 ? 1.0 : 0.0;
            case 16:
                return action->mFlag6 || action->mFlag24 ? 1.0 : 0.0;
            case 17:
                return action->mFlag7 ? 1.0 : 0.0;
            default:
                return 0.0;
        }
    }

    bool targetMark(bool wanted, bool recordTargetable)
    {
        return wanted != recordTargetable;
    }

    bool targetable(bool recordTargetable, bool marked)
    {
        return recordTargetable != marked;
    }

    void TargetMarks::set(FormId reference, bool mark)
    {
        if (mark)
            mMarked.insert(reference);
        else
            mMarked.erase(reference);
    }

    void writeTargetMarks(const TargetMarks& marks, SaveWriter& out)
    {
        out.count(marks.all().size());
        for (const FormId reference : marks.all())
            out.form(reference);
    }

    void readTargetMarks(TargetMarks& marks, SaveReader& in, std::uint32_t version)
    {
        if (version != targetMarksSectionVersion)
            throw SaveError("vats marks: unknown version");
        const std::size_t count = in.count(4);
        std::set<FormId> marked;
        for (std::size_t i = 0; i < count; ++i)
            if (const std::optional<FormId> reference = in.form(); reference && *reference != 0)
                marked.insert(*reference);
        marks.restore(std::move(marked));
    }

    double threat(double level, double health, double damageThreshold)
    {
        if (level <= 0.0 || health <= 0.0)
            return 0.0;
        const double stopped = std::min(std::max(damageThreshold, 0.0) / 30.0, 0.99);
        return level * health / (1.0 - stopped);
    }

    double threatRatio(double own, double other)
    {
        return own > 0.0 ? other / own : 0.0;
    }

    double sideAngle(Side side)
    {
        switch (side)
        {
            case Side::Front:
                return 0.0;
            case Side::Right:
                return 1.5707963705062866;
            case Side::Back:
                return 3.1415927410125732;
            case Side::Left:
                return 4.71238899230957;
        }
        return 0.0;
    }

    double freeArea(double hitFraction)
    {
        if (hitFraction < 0.0)
            return nothingHit;
        return hitFraction * areaRayLength;
    }

    double visibleDistance(double stepDistance, double stepCount, const std::function<bool(int)>& seen)
    {
        double result = 0.0;
        for (int step = 0; static_cast<double>(step) < stepCount; ++step)
        {
            if (!seen(step))
                break;
            result = static_cast<double>(step + 1) * stepDistance;
        }
        return result;
    }

    double targetHeight(int mode, bool attackKind, bool hasTarget, double partOrTargetHeight, double playerHeight)
    {
        if (mode != Playback || !attackKind || !hasTarget)
            return 0.0;
        return partOrTargetHeight - playerHeight;
    }

    bool isAttackKind(std::uint32_t kind)
    {
        // The table at 0x66DE14 (one byte for each kind from 0 to 0x15): the kinds 0 to 8, 0x10, 0x11 and 0x13 to 0x15
        return kind <= 8 || kind == 0x10 || kind == 0x11 || (kind >= 0x13 && kind <= 0x15);
    }
}
