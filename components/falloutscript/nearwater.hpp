#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_NEARWATER_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_NEARWATER_H

#include <vector>

namespace FalloutScript::NearWater
{
    /// The sound of the water that is near the player, as the executable makes it (the sampler at 0x4E6A80, the one that
    /// runs every frame): the sound of the water of the cell (the SNAM of its WATR record) is played from a place that moves
    /// with the player, once the player is near enough to the water. No sound is made here: the engine asks these rules how
    /// near the water is and where the sound comes from. The numbers are the defaults of the [Water] settings of the games'
    /// own INI file (uNearWaterRadius, uNearWaterPoints, fNearWaterOutdoorTolerance, fNearWaterIndoorTolerance).

    struct Settings
    {
        /// How far from the player the water is looked for, in units
        int mRadius = 1000;
        /// How many points are looked at along each side of the square around the player (so there are points squared)
        int mPoints = 8;
        /// How far above or below the water the player can be and still hear it, out of doors and indoors
        float mOutdoorTolerance = 1024.f;
        float mIndoorTolerance = 512.f;
    };

    struct Point
    {
        float mX = 0.f;
        float mY = 0.f;
    };

    /// The points that an exterior look samples: a square of points times points around the player, from a radius before
    /// to a radius after, the first along x and then along y.
    std::vector<Point> samplePoints(Point player, const Settings& settings = {});

    /// The part of the water that is near inside a cell: 0 when the player is as far from the water as the tolerance or
    /// farther (or is under it), 1 at the surface, in a line between.
    float interiorStrength(float waterLevel, float playerZ, bool listenerUnderwater, const Settings& settings = {});

    /// What an exterior look found: how many of the points have terrain under the water of their cell, and where those
    /// points add up to
    struct Wet
    {
        int mCount = 0;
        float mSumX = 0.f;
        float mSumY = 0.f;

        void add(Point point)
        {
            ++mCount;
            mSumX += point.mX;
            mSumY += point.mY;
        }
    };

    /// The part of the water that is near out of doors: twice the share of the points that are wet (so that half of them is
    /// everything), times how near the player is to the water in height; 0 when the player is as far from the water as
    /// the tolerance or farther (or is under it). At most 1.
    float exteriorStrength(
        const Wet& wet, float waterLevel, float playerZ, bool listenerUnderwater, const Settings& settings = {});

    /// Where the sound comes from: from the player in the direction of the wet points (their middle), as far out as
    /// the radius times how far from full the strength is, so that it is on top of the player when all of it is near.
    /// Indoors, and when no point is wet, it is where the player is.
    Point soundPosition(Point player, const Wet& wet, float strength, bool interior, const Settings& settings = {});
}

#endif
