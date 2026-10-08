#include "falloutanimation.hpp"

#include <algorithm>

#include <components/misc/strings/lower.hpp>

namespace OFRender
{
    namespace
    {
        constexpr std::string_view sExtension = ".kf";

        /// The name of a file in the folder without its extension, empty for a file of a folder below or not a .kf
        std::string_view stemInFolder(std::string_view folder, std::string_view path)
        {
            if (!path.starts_with(folder) || !path.ends_with(sExtension))
                return {};
            std::string_view name = path.substr(folder.size());
            name.remove_suffix(sExtension.size());
            if (name.empty() || name.find('/') != std::string_view::npos)
                return {};
            return name;
        }
    }

    std::string falloutAnimationFolder(std::string_view skeleton)
    {
        std::string folder = Misc::StringUtils::lowerCase(skeleton);
        std::ranges::replace(folder, '\\', '/');
        const std::size_t slash = folder.find_last_of('/');
        if (slash == std::string::npos)
            return {};
        folder.resize(slash + 1);
        return folder;
    }

    std::string chooseFalloutIdle(std::string_view folder, const std::vector<std::string>& files)
    {
        static constexpr std::string_view preferred[] = { "mtidle", "idle", "h2hidle" };
        for (std::string_view stem : preferred)
        {
            const std::string path = std::string(folder) + std::string(stem) + std::string(sExtension);
            if (std::ranges::find(files, path) != files.end())
                return path;
        }

        std::string best;
        std::string_view bestStem;
        for (const std::string& path : files)
        {
            const std::string_view stem = stemInFolder(folder, path);
            if (stem.empty() || stem.find("idle") == std::string_view::npos)
                continue;
            if (best.empty() || stem < bestStem)
            {
                best = path;
                bestStem = stemInFolder(folder, best);
            }
        }
        return best;
    }

    namespace
    {
        constexpr std::string_view sLocomotionFolder = "locomotion/";

        // Units a second at which a character starts to move, and below which it stops; and the speed that stands for
        // a run when the animations do not say how fast they travel
        constexpr float sStartSpeed = 10.f;
        constexpr float sStopSpeed = 5.f;
        constexpr float sDefaultRunSpeed = 250.f;
        constexpr float sRunHysteresis = 0.1f;

        // The animations of a character travel at a speed of their own, which one character may keep up with by a
        // little more or less than the animation does
        constexpr float sMinAnimationSpeed = 0.25f;
        constexpr float sMaxAnimationSpeed = 4.f;
    }

    FalloutLocomotion chooseFalloutLocomotion(
        std::string_view folder, const std::vector<std::string>& files, bool female)
    {
        const std::string base = std::string(folder) + std::string(sLocomotionFolder);

        const auto choose = [&](std::string_view wanted) {
            std::string best;
            int bestRank = 3;
            for (const std::string& path : files)
            {
                if (!path.starts_with(base) || !path.ends_with(sExtension))
                    continue;
                // What is left is "name.kf" or "folder/name.kf"
                std::string_view rest = std::string_view(path).substr(base.size());
                rest.remove_suffix(sExtension.size());
                std::string_view subfolder;
                const std::size_t slash = rest.find('/');
                if (slash != std::string_view::npos)
                {
                    subfolder = rest.substr(0, slash);
                    rest.remove_prefix(slash + 1);
                }
                if (rest != wanted || rest.find('/') != std::string_view::npos)
                    continue;

                int rank = 2;
                if (subfolder.empty())
                    rank = 1;
                else if (subfolder == (female ? "female" : "male"))
                    rank = 0;
                else if (subfolder == "child" || subfolder == "hurt")
                    continue;

                if (rank < bestRank || (rank == bestRank && path < best))
                {
                    best = path;
                    bestRank = rank;
                }
            }
            return best;
        };

        return { choose("mtforward"), choose("mtfastforward") };
    }

    FalloutGait chooseFalloutGait(float speed, FalloutGait current, const FalloutGaits& gaits)
    {
        if (!gaits.mHasWalk && !gaits.mHasRun)
            return FalloutGait::Idle;

        const bool moving = speed > (current == FalloutGait::Idle ? sStartSpeed : sStopSpeed);
        if (!moving)
            return FalloutGait::Idle;
        if (!gaits.mHasRun)
            return FalloutGait::Walk;
        if (!gaits.mHasWalk)
            return FalloutGait::Run;

        // The speed at which the two animations look the same: half way between the speeds they travel at
        float runSpeed = sDefaultRunSpeed;
        if (gaits.mWalkVelocity > 0.f && gaits.mRunVelocity > gaits.mWalkVelocity)
            runSpeed = 0.5f * (gaits.mWalkVelocity + gaits.mRunVelocity);
        const float threshold = runSpeed * (current == FalloutGait::Run ? 1.f - sRunHysteresis : 1.f + sRunHysteresis);
        return speed > threshold ? FalloutGait::Run : FalloutGait::Walk;
    }

    float falloutAnimationSpeed(float speed, float velocity)
    {
        if (!(velocity > 1.f))
            return 1.f;
        return std::clamp(speed / velocity, sMinAnimationSpeed, sMaxAnimationSpeed);
    }
}
