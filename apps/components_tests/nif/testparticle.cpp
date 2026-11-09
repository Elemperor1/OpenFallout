#include "geometry.hpp"
#include "niftestfile.hpp"

#include <components/nif/controller.hpp>
#include <components/nif/data.hpp>
#include <components/nif/exception.hpp>
#include <components/nif/node.hpp>
#include <components/nif/particle.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

// The particles of the games: the lists of particles and the modifiers that move them in Morrowind, and the particle
// systems with their emitters, modifiers and controllers in Oblivion and the games after it.
namespace
{
    using namespace NifTest;
    using namespace testing;

    /// The start of a modifier of a particle system: its name, the order in which it works, the system and whether it
    /// is active.
    Bytes modifier(Builder& file, std::string_view name, std::uint32_t order, std::int32_t target = -1)
    {
        Bytes bytes;
        file.string(bytes, name);
        return std::move(bytes.u32(order).ref(target).boolean(true));
    }

    /// The start of an emitter: the modifier, the speed, the angles, the colour, the radius and the lifespan.
    Bytes emitter(Builder& file, std::string_view name, std::int32_t target)
    {
        Bytes bytes = modifier(file, name, 1000, target);
        bytes.f32(1).f32(2).f32(3).f32(4).f32(5).f32(6); // speed and angles with their variations
        bytes.vec4(0.25f, 0.5f, 0.75f, 1); // initial colour
        bytes.f32(7); // initial radius
        if (file.profile().mVersion >= 0x0A040001)
            bytes.f32(8); // radius variation
        return std::move(bytes.f32(9).f32(10)); // lifespan and its variation
    }

    /// The start of a modifier of the old particle systems: the next modifier and the controller.
    Bytes oldModifier(std::int32_t next = -1)
    {
        return std::move(Bytes().ref(next).ref(-1));
    }

    TEST(NifParticleTest, readsTheParticlesAndTheirModifiersOfMorrowind)
    {
        Builder file(morrowind);
        file.add("NiNode", file.node({ .mName = "Root" }));
        // 1: the list of particles: the geometry and then the number of particles, one radius, the active ones
        // and the sizes of the vertices
        file.add("NiParticlesData",
            std::move(
                geometryData(file, 3, 0, false, false).u16(2).f32(0.5f).u16(1).boolean32(true).f32(1).f32(2).f32(3)));
        // 2: the same, with a rotation for every vertex
        file.add("NiRotatingParticlesData",
            std::move(geometryData(file, 2, 0, false, false)
                          .u16(2)
                          .f32(0.5f)
                          .u16(2)
                          .boolean32(false)
                          .boolean32(true)
                          .quat(1, 0, 0, 0)
                          .quat(0, 1, 0, 0)));
        file.add("NiParticles", geometry(file, { .mName = "Dust" }, 1));
        file.add("NiRotatingParticles", geometry(file, { .mName = "Sparks" }, 2));
        // 5 to 12: the modifiers, each one the next of the one before it
        file.add("NiParticleGrowFade", oldModifier(6).f32(0.25f).f32(0.75f));
        file.add("NiParticleColorModifier", oldModifier(7).ref(12));
        file.add("NiGravity", oldModifier(8).f32(0.5f).f32(9.8f).u32(1).vec3(1, 2, 3).vec3(0, 0, -1));
        file.add(
            "NiParticleBomb", oldModifier(9).f32(10).f32(2).f32(3).f32(4).u32(2).u32(1).vec3(4, 5, 6).vec3(0, 1, 0));
        file.add("NiPlanarCollider",
            oldModifier(10).f32(0.5f).vec2(4, 5).vec3(1, 1, 1).vec3(1, 0, 0).vec3(0, 1, 0).vec3(0, 0, 1).f32(8));
        file.add("NiSphericalCollider", oldModifier(11).f32(0.25f).f32(3).vec3(7, 8, 9));
        file.add("NiParticleRotation", oldModifier(-1).u8(1).vec3(0, 0, 1).f32(2.5f));
        file.add("NiColorData", Bytes().u32(1).u32(1).f32(0).vec4(1, 0.5f, 0.25f, 1));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* dust = parsed.as<Nif::NiParticlesData>(1);
        ASSERT_NE(dust, nullptr);
        EXPECT_EQ(dust->mNumParticles, 2u);
        EXPECT_THAT(dust->mRadii, ElementsAre(0.5f));
        EXPECT_EQ(dust->mActiveCount, 1u);
        EXPECT_THAT(dust->mSizes, ElementsAre(1.f, 2.f, 3.f));

        const auto* spinning = parsed.as<Nif::NiRotatingParticlesData>(2);
        ASSERT_NE(spinning, nullptr);
        EXPECT_THAT(spinning->mSizes, IsEmpty());
        ASSERT_EQ(spinning->mRotations.size(), 2u);
        EXPECT_EQ(spinning->mRotations[0].w(), 1.f);
        EXPECT_EQ(spinning->mRotations[1].x(), 1.f);

        const auto* growFade = parsed.as<Nif::NiParticleGrowFade>(5);
        ASSERT_NE(growFade, nullptr);
        EXPECT_EQ(growFade->mGrowTime, 0.25f);
        EXPECT_EQ(growFade->mFadeTime, 0.75f);
        EXPECT_EQ(growFade->mNext.getPtr(), parsed.as<Nif::NiParticleColorModifier>(6));

        const auto* color = parsed.as<Nif::NiParticleColorModifier>(6);
        ASSERT_NE(color, nullptr);
        EXPECT_EQ(color->mData.getPtr(), parsed.as<Nif::NiColorData>(12));

        const auto* gravity = parsed.as<Nif::NiGravity>(7);
        ASSERT_NE(gravity, nullptr);
        EXPECT_EQ(gravity->mDecay, 0.5f);
        EXPECT_EQ(gravity->mForce, 9.8f);
        EXPECT_EQ(gravity->mType, Nif::ForceType::Point);
        EXPECT_EQ(gravity->mPosition, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(gravity->mDirection, osg::Vec3f(0, 0, -1));

        const auto* bomb = parsed.as<Nif::NiParticleBomb>(8);
        ASSERT_NE(bomb, nullptr);
        EXPECT_EQ(bomb->mRange, 10.f);
        EXPECT_EQ(bomb->mDuration, 2.f);
        EXPECT_EQ(bomb->mStrength, 3.f);
        EXPECT_EQ(bomb->mStartTime, 4.f);
        EXPECT_EQ(bomb->mDecayType, Nif::DecayType::Exponential);
        EXPECT_EQ(bomb->mSymmetryType, Nif::SymmetryType::Cylindrical);
        EXPECT_EQ(bomb->mPosition, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(bomb->mDirection, osg::Vec3f(0, 1, 0));

        const auto* planar = parsed.as<Nif::NiPlanarCollider>(9);
        ASSERT_NE(planar, nullptr);
        EXPECT_EQ(planar->mBounceFactor, 0.5f);
        EXPECT_FALSE(planar->mSpawnOnCollision);
        EXPECT_FALSE(planar->mDieOnCollision);
        EXPECT_EQ(planar->mExtents, osg::Vec2f(4, 5));
        EXPECT_EQ(planar->mPosition, osg::Vec3f(1, 1, 1));
        EXPECT_EQ(planar->mXVector, osg::Vec3f(1, 0, 0));
        EXPECT_EQ(planar->mYVector, osg::Vec3f(0, 1, 0));
        EXPECT_EQ(planar->mPlaneNormal, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(planar->mPlaneDistance, 8.f);

        const auto* sphere = parsed.as<Nif::NiSphericalCollider>(10);
        ASSERT_NE(sphere, nullptr);
        EXPECT_EQ(sphere->mBounceFactor, 0.25f);
        EXPECT_EQ(sphere->mRadius, 3.f);
        EXPECT_EQ(sphere->mCenter, osg::Vec3f(7, 8, 9));

        const auto* rotation = parsed.as<Nif::NiParticleRotation>(11);
        ASSERT_NE(rotation, nullptr);
        EXPECT_EQ(rotation->mRandomInitialAxis, 1);
        EXPECT_EQ(rotation->mInitialAxis, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(rotation->mRotationSpeed, 2.5f);
    }

    TEST(NifParticleTest, theCollidersOfTheLaterFilesSayWhatHappensToAParticle)
    {
        Builder file(Profile{ 0x04020200, 0, 0 });
        file.add("NiNode", file.node({ .mName = "Root" }));
        file.add("NiPlanarCollider",
            oldModifier()
                .f32(0.5f)
                .boolean(true)
                .boolean(false)
                .vec2(1, 2)
                .vec3(0, 0, 0)
                .vec3(1, 0, 0)
                .vec3(0, 1, 0)
                .vec3(0, 0, 1)
                .f32(2));
        file.add("NiSphericalCollider", oldModifier().f32(0.5f).boolean(false).boolean(true).f32(1).vec3(0, 0, 0));
        file.addRoot(0);

        const Parsed parsed(file.build());

        EXPECT_TRUE(parsed.as<Nif::NiPlanarCollider>(1)->mSpawnOnCollision);
        EXPECT_FALSE(parsed.as<Nif::NiPlanarCollider>(1)->mDieOnCollision);
        EXPECT_FALSE(parsed.as<Nif::NiSphericalCollider>(2)->mSpawnOnCollision);
        EXPECT_TRUE(parsed.as<Nif::NiSphericalCollider>(2)->mDieOnCollision);
    }

    /// The data of a list of particles of a particle system as Oblivion writes it, for the given number of vertices.
    Bytes oblivionParticleData(Builder& file, std::uint16_t numVertices, bool rotations)
    {
        Bytes bytes = geometryData(file, numVertices, 0, false, false);
        bytes.boolean(true); // radii
        for (std::uint16_t i = 0; i < numVertices; ++i)
            bytes.f32(0.5f + i);
        bytes.u16(numVertices - 1); // active
        bytes.boolean(true); // sizes
        for (std::uint16_t i = 0; i < numVertices; ++i)
            bytes.f32(1.f + i);
        bytes.boolean(rotations); // rotations
        if (rotations)
            for (std::uint16_t i = 0; i < numVertices; ++i)
                bytes.quat(1, 0, 0, static_cast<float>(i));
        bytes.boolean(rotations); // rotation angles
        if (rotations)
            for (std::uint16_t i = 0; i < numVertices; ++i)
                bytes.f32(10.f + i);
        bytes.boolean(rotations); // rotation axes
        if (rotations)
            for (std::uint16_t i = 0; i < numVertices; ++i)
                bytes.vec3(0, 1, static_cast<float>(i));
        // the particles
        for (std::uint16_t i = 0; i < numVertices; ++i)
            bytes.vec3(0, 0, 1).f32(1.f + i).f32(5).f32(0.5f).u16(i).u16(3);
        return std::move(bytes.boolean(rotations)); // rotation speeds
    }

    TEST(NifParticleTest, readsTheParticleDataOfOblivion)
    {
        Builder file(oblivion);
        file.add("NiNode", file.node({ .mName = "Root" }));
        // 1: every particle has its radius, size, rotation and the rest
        Bytes full = oblivionParticleData(file, 2, true);
        for (std::uint16_t i = 0; i < 2; ++i)
            full.f32(0.5f + i);
        full.u16(4).u16(5);
        file.add("NiPSysData", full);
        // 2: the lists are not there
        Bytes bare = oblivionParticleData(file, 1, false);
        bare.u16(0).u16(0);
        file.add("NiPSysData", bare);
        // 3: the data of a particle system that shows meshes
        Bytes mesh = oblivionParticleData(file, 1, false);
        mesh.u16(0).u16(0).u32(64).boolean(true).u32(2).u32(7).u32(9).ref(0);
        file.add("NiMeshPSysData", mesh);
        // 4: the data of a strip
        Bytes strip = oblivionParticleData(file, 1, false);
        strip.u16(0).u16(0).u16(20).f32(0.5f).f32(1.5f).boolean(true);
        file.add("BSStripPSysData", strip);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* data = parsed.as<Nif::NiPSysData>(1);
        ASSERT_NE(data, nullptr);
        EXPECT_THAT(data->mRadii, ElementsAre(0.5f, 1.5f));
        EXPECT_EQ(data->mActiveCount, 1u);
        EXPECT_THAT(data->mSizes, ElementsAre(1.f, 2.f));
        ASSERT_EQ(data->mRotations.size(), 2u);
        EXPECT_EQ(data->mRotations[1].z(), 1.f);
        EXPECT_THAT(data->mRotationAngles, ElementsAre(10.f, 11.f));
        ASSERT_EQ(data->mRotationAxes.size(), 2u);
        EXPECT_EQ(data->mRotationAxes[1], osg::Vec3f(0, 1, 1));
        ASSERT_EQ(data->mParticles.size(), 2u);
        EXPECT_EQ(data->mParticles[0].mVelocity, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(data->mParticles[1].mAge, 2.f);
        EXPECT_EQ(data->mParticles[1].mLifespan, 5.f);
        EXPECT_EQ(data->mParticles[1].mLastUpdate, 0.5f);
        EXPECT_EQ(data->mParticles[1].mSpawnGeneration, 1u);
        EXPECT_EQ(data->mParticles[1].mCode, 3u);
        EXPECT_THAT(data->mRotationSpeeds, ElementsAre(0.5f, 1.5f));
        EXPECT_EQ(data->mNumAddedParticles, 4u);
        EXPECT_EQ(data->mAddedParticlesBase, 5u);

        const auto* bareData = parsed.as<Nif::NiPSysData>(2);
        ASSERT_NE(bareData, nullptr);
        EXPECT_THAT(bareData->mRotations, IsEmpty());
        EXPECT_THAT(bareData->mRotationSpeeds, IsEmpty());
        EXPECT_EQ(bareData->mParticles.size(), 1u);

        const auto* meshData = parsed.as<Nif::NiMeshPSysData>(3);
        ASSERT_NE(meshData, nullptr);
        EXPECT_EQ(meshData->mDefaultPoolSize, 64u);
        EXPECT_TRUE(meshData->mFillPoolsOnLoad);
        EXPECT_THAT(meshData->mGenerations, ElementsAre(7u, 9u));
        EXPECT_EQ(meshData->mParticleMeshes.getPtr(), parsed.as<Nif::NiNode>(0));

        const auto* stripData = parsed.as<Nif::BSStripPSysData>(4);
        ASSERT_NE(stripData, nullptr);
        EXPECT_EQ(stripData->mMaxPointCount, 20u);
        EXPECT_EQ(stripData->mStartCapSize, 0.5f);
        EXPECT_EQ(stripData->mEndCapSize, 1.5f);
        EXPECT_TRUE(stripData->mDoZPrepass);
    }

    /// The base of the data of a list of particles of the games from Fallout 3 on: it has the flags of the vertices,
    /// the normals and the colours, but the vertices themselves are not in the file.
    Bytes falloutParticleBase(Builder& file, std::uint16_t numVertices)
    {
        Bytes data;
        data.i32(0).u16(numVertices).u8(0).u8(0); // group, count, keep and compress flags
        file.flag(data, true); // the vertices, which this kind of data does not have
        data.u16(0); // data flags
        if (file.profile().mBethVersion > 34)
            data.u32(0); // material hash
        file.flag(data, false); // normals
        data.vec3(0, 0, 0).f32(1); // bounding sphere
        file.flag(data, false); // colours
        return std::move(data.u16(0).i32(-1)); // consistency and additional data
    }

    /// The data of a list of particles of the games from Fallout 3 on, which only have the vertices and the offsets
    /// of the textures.
    Bytes falloutParticleData(Builder& file, std::uint16_t numVertices, std::uint16_t offsets)
    {
        Bytes bytes = falloutParticleBase(file, numVertices);
        bytes.boolean(false); // radii
        bytes.u16(1); // active
        bytes.boolean(false).boolean(false).boolean(false).boolean(false); // sizes, rotations and the like
        bytes.boolean(offsets > 0); // texture indices
        if (file.profile().mBethVersion <= 34)
            bytes.u8(static_cast<std::uint8_t>(offsets));
        else
            bytes.u32(offsets);
        for (std::uint16_t i = 0; i < offsets; ++i)
            bytes.vec4(0, 0, 0.5f, 0.5f * (i + 1));
        if (file.profile().mBethVersion > 34)
            bytes.f32(2).u16(3).f32(4).f32(5).f32(6); // the aspect of the textures
        return bytes;
    }

    TEST(NifParticleTest, readsTheParticleDataOfFalloutAndSkyrim)
    {
        for (const Profile& profile : { fallout3, skyrim })
        {
            Builder file(profile);
            file.add("NiNode", file.node({ .mName = "Root" }));
            Bytes data = falloutParticleData(file, 4, 2);
            data.boolean(false); // rotation speeds
            file.add("NiPSysData", data);
            file.addRoot(0);

            const Parsed parsed(file.build());

            const auto* psys = parsed.as<Nif::NiPSysData>(1);
            ASSERT_NE(psys, nullptr) << profile.mBethVersion;
            EXPECT_TRUE(psys->mHasTextureIndices);
            EXPECT_THAT(psys->mSubtextureOffsets, ElementsAre(osg::Vec4f(0, 0, 0.5f, 0.5f), osg::Vec4f(0, 0, 0.5f, 1)));
            EXPECT_TRUE(psys->mRadii.empty());
            EXPECT_TRUE(psys->mParticles.empty());
            if (profile.mBethVersion > 34)
            {
                EXPECT_EQ(psys->mAspectRatio, 2.f);
                EXPECT_EQ(psys->mAspectFlags, 3u);
                EXPECT_EQ(psys->mAspectRatio2, 4.f);
                EXPECT_EQ(psys->mAspectSpeed, 5.f);
                EXPECT_EQ(psys->mAspectSpeed2, 6.f);
            }
        }
    }

    /// The records of a particle system of Fallout 3: its data and one of each modifier.
    TEST(NifParticleTest, readsTheModifiersOfAParticleSystem)
    {
        Builder file(fallout3);
        file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
        // 1: the system with its data and the modifiers, 2: the data, 3 to 21: the modifiers
        std::vector<std::int32_t> modifiers;
        for (std::int32_t i = 3; i <= 21; ++i)
            modifiers.push_back(i);
        file.add(
            "NiParticleSystem", geometry(file, { .mName = "Fire", .mFlags = 0x10 }, 2).boolean(false).refs(modifiers));
        Bytes data = falloutParticleData(file, 3, 0);
        data.boolean(false);
        file.add("NiPSysData", data);
        // 3, 4: the modifiers of the life of a particle
        file.add("NiPSysAgeDeathModifier", modifier(file, "Death", 0, 1).boolean(true).ref(4));
        file.add("NiPSysSpawnModifier",
            modifier(file, "Spawn", 2000, 1).u16(3).f32(0.5f).u16(1).u16(4).f32(0.1f).f32(0.2f).f32(6).f32(0.3f));
        // 5, 6, 7: the bounds, the colour and the growth
        file.add("NiPSysBoundUpdateModifier", modifier(file, "Bounds", 7000, 1).u16(20));
        file.add("NiPSysColorModifier", modifier(file, "Color", 3000, 1).ref(22));
        file.add("NiPSysGrowFadeModifier", modifier(file, "Grow", 3000, 1).f32(1).u16(0).f32(2).u16(1).f32(3));
        // 8, 9: the forces: the gravity of an object and a bomb
        file.add("NiPSysGravityModifier",
            modifier(file, "Gravity", 4000, 1)
                .ref(0)
                .vec3(0, 0, -1)
                .f32(0.5f)
                .f32(9.8f)
                .u32(0)
                .f32(0.25f)
                .f32(2)
                .boolean(true));
        file.add(
            "NiPSysBombModifier", modifier(file, "Bomb", 4000, 1).ref(0).vec3(1, 0, 0).f32(5).f32(6).u32(1).u32(2));
        // 10, 11, 12: drag, rotation and the plain position update
        file.add("NiPSysDragModifier", modifier(file, "Drag", 4000, 1).ref(0).vec3(0, 1, 0).f32(0.5f).f32(6).f32(7));
        file.add("NiPSysRotationModifier",
            modifier(file, "Rotation", 3000, 1).f32(1).f32(2).f32(3).f32(4).boolean(true).boolean(true).vec3(0, 0, 1));
        file.add("NiPSysPositionModifier", modifier(file, "Position", 6000, 1));
        // 13 to 17: the modifiers that Bethesda added
        file.add("BSPSysSimpleColorModifier",
            modifier(file, "Colors", 3000, 1)
                .f32(0.1f)
                .f32(0.2f)
                .f32(0.3f)
                .f32(0.4f)
                .f32(0.5f)
                .f32(0.6f)
                .vec4(1, 0, 0, 1)
                .vec4(0, 1, 0, 1)
                .vec4(0, 0, 1, 1));
        file.add("BSWindModifier", modifier(file, "Wind", 4000, 1).f32(3.5f));
        file.add("BSParentVelocityModifier", modifier(file, "Parent", 3000, 1).f32(0.75f));
        file.add("BSPSysScaleModifier", modifier(file, "Scale", 3000, 1).u32(3).f32(1).f32(2).f32(4));
        file.add("BSPSysStripUpdateModifier", modifier(file, "Strip", 2500, 1).f32(0.125f));
        // 18, 19, 20, 21: the emitters
        file.add("NiPSysBoxEmitter", emitter(file, "Box", 1).ref(0).f32(1).f32(2).f32(3));
        file.add("NiPSysCylinderEmitter", emitter(file, "Cylinder", 1).ref(0).f32(4).f32(5));
        file.add("NiPSysSphereEmitter", emitter(file, "Sphere", 1).ref(0).f32(6));
        file.add("NiPSysMeshEmitter", emitter(file, "Mesh", 1).refs({ 0 }).u32(1).u32(2).vec3(0, 1, 0));
        file.add("NiColorData", Bytes().u32(1).u32(1).f32(0).vec4(1, 0.5f, 0.25f, 1));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* system = parsed.as<Nif::NiParticleSystem>(1);
        ASSERT_NE(system, nullptr);
        EXPECT_FALSE(system->mWorldSpace);
        ASSERT_EQ(system->mModifiers.size(), 19u);
        EXPECT_EQ(system->mModifiers[0].getPtr(), parsed.as<Nif::NiPSysAgeDeathModifier>(3));
        EXPECT_EQ(system->mData.getPtr(), parsed.as<Nif::NiPSysData>(2));

        const auto* death = parsed.as<Nif::NiPSysAgeDeathModifier>(3);
        ASSERT_NE(death, nullptr);
        EXPECT_EQ(death->mName, "Death");
        EXPECT_EQ(death->mOrder, Nif::NiPSysModifier::NiPSysModifierOrder::KillOldParticles);
        EXPECT_EQ(death->mTarget.getPtr(), system);
        EXPECT_TRUE(death->mActive);
        EXPECT_TRUE(death->mSpawnOnDeath);
        EXPECT_EQ(death->mSpawnModifier.getPtr(), parsed.as<Nif::NiPSysSpawnModifier>(4));

        const auto* spawn = parsed.as<Nif::NiPSysSpawnModifier>(4);
        ASSERT_NE(spawn, nullptr);
        EXPECT_EQ(spawn->mOrder, Nif::NiPSysModifier::NiPSysModifierOrder::Spawn);
        EXPECT_EQ(spawn->mNumSpawnGenerations, 3u);
        EXPECT_EQ(spawn->mPercentageSpawned, 0.5f);
        EXPECT_EQ(spawn->mMinNumToSpawn, 1u);
        EXPECT_EQ(spawn->mMaxNumToSpawn, 4u);
        EXPECT_EQ(spawn->mSpawnSpeedVariation, 0.1f);
        EXPECT_EQ(spawn->mSpawnDirVariation, 0.2f);
        EXPECT_EQ(spawn->mLifespan, 6.f);
        EXPECT_EQ(spawn->mLifespanVariation, 0.3f);

        EXPECT_EQ(parsed.as<Nif::NiPSysBoundUpdateModifier>(5)->mUpdateSkip, 20u);
        EXPECT_EQ(parsed.as<Nif::NiPSysColorModifier>(6)->mData.getPtr(), parsed.as<Nif::NiColorData>(22));

        const auto* grow = parsed.as<Nif::NiPSysGrowFadeModifier>(7);
        ASSERT_NE(grow, nullptr);
        EXPECT_EQ(grow->mGrowTime, 1.f);
        EXPECT_EQ(grow->mGrowGeneration, 0u);
        EXPECT_EQ(grow->mFadeTime, 2.f);
        EXPECT_EQ(grow->mFadeGeneration, 1u);
        EXPECT_EQ(grow->mBaseScale, 3.f);

        const auto* gravity = parsed.as<Nif::NiPSysGravityModifier>(8);
        ASSERT_NE(gravity, nullptr);
        EXPECT_EQ(gravity->mGravityObject.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(gravity->mGravityAxis, osg::Vec3f(0, 0, -1));
        EXPECT_EQ(gravity->mDecay, 0.5f);
        EXPECT_EQ(gravity->mStrength, 9.8f);
        EXPECT_EQ(gravity->mForceType, Nif::ForceType::Wind);
        EXPECT_EQ(gravity->mTurbulence, 0.25f);
        EXPECT_EQ(gravity->mTurbulenceScale, 2.f);
        EXPECT_TRUE(gravity->mWorldAligned);

        const auto* bomb = parsed.as<Nif::NiPSysBombModifier>(9);
        ASSERT_NE(bomb, nullptr);
        EXPECT_EQ(bomb->mBombObject.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(bomb->mBombAxis, osg::Vec3f(1, 0, 0));
        EXPECT_EQ(bomb->mRange, 5.f);
        EXPECT_EQ(bomb->mStrength, 6.f);
        EXPECT_EQ(bomb->mDecayType, Nif::DecayType::Linear);
        EXPECT_EQ(bomb->mSymmetryType, Nif::SymmetryType::Planar);

        const auto* drag = parsed.as<Nif::NiPSysDragModifier>(10);
        ASSERT_NE(drag, nullptr);
        EXPECT_EQ(drag->mDragObject.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(drag->mDragAxis, osg::Vec3f(0, 1, 0));
        EXPECT_EQ(drag->mPercentage, 0.5f);
        EXPECT_EQ(drag->mRange, 6.f);
        EXPECT_EQ(drag->mRangeFalloff, 7.f);

        const auto* rotation = parsed.as<Nif::NiPSysRotationModifier>(11);
        ASSERT_NE(rotation, nullptr);
        EXPECT_EQ(rotation->mRotationSpeed, 1.f);
        EXPECT_EQ(rotation->mRotationSpeedVariation, 2.f);
        EXPECT_EQ(rotation->mRotationAngle, 3.f);
        EXPECT_EQ(rotation->mRotationAngleVariation, 4.f);
        EXPECT_TRUE(rotation->mRandomRotSpeedSign);
        EXPECT_TRUE(rotation->mRandomAxis);
        EXPECT_EQ(rotation->mAxis, osg::Vec3f(0, 0, 1));

        EXPECT_NE(parsed.as<Nif::NiPSysModifier>(12), nullptr);

        const auto* colors = parsed.as<Nif::BSPSysSimpleColorModifier>(13);
        ASSERT_NE(colors, nullptr);
        EXPECT_EQ(colors->mFadeInPercent, 0.1f);
        EXPECT_EQ(colors->mFadeOutPercent, 0.2f);
        EXPECT_EQ(colors->mColor1EndPercent, 0.3f);
        EXPECT_EQ(colors->mColor1StartPercent, 0.4f);
        EXPECT_EQ(colors->mColor2EndPercent, 0.5f);
        EXPECT_EQ(colors->mColor2StartPercent, 0.6f);
        EXPECT_THAT(
            colors->mColors, ElementsAre(osg::Vec4f(1, 0, 0, 1), osg::Vec4f(0, 1, 0, 1), osg::Vec4f(0, 0, 1, 1)));

        EXPECT_EQ(parsed.as<Nif::BSWindModifier>(14)->mStrength, 3.5f);
        EXPECT_EQ(parsed.as<Nif::BSParentVelocityModifier>(15)->mDamping, 0.75f);
        EXPECT_THAT(parsed.as<Nif::BSPSysScaleModifier>(16)->mScales, ElementsAre(1.f, 2.f, 4.f));
        EXPECT_EQ(parsed.as<Nif::BSPSysStripUpdateModifier>(17)->mUpdateDeltaTime, 0.125f);

        const auto* box = parsed.as<Nif::NiPSysBoxEmitter>(18);
        ASSERT_NE(box, nullptr);
        EXPECT_EQ(box->mSpeed, 1.f);
        EXPECT_EQ(box->mSpeedVariation, 2.f);
        EXPECT_EQ(box->mDeclination, 3.f);
        EXPECT_EQ(box->mDeclinationVariation, 4.f);
        EXPECT_EQ(box->mPlanarAngle, 5.f);
        EXPECT_EQ(box->mPlanarAngleVariation, 6.f);
        EXPECT_EQ(box->mInitialColor, osg::Vec4f(0.25f, 0.5f, 0.75f, 1));
        EXPECT_EQ(box->mInitialRadius, 7.f);
        EXPECT_EQ(box->mRadiusVariation, 8.f);
        EXPECT_EQ(box->mLifespan, 9.f);
        EXPECT_EQ(box->mLifespanVariation, 10.f);
        EXPECT_EQ(box->mEmitterObject.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(box->mWidth, 1.f);
        EXPECT_EQ(box->mHeight, 2.f);
        EXPECT_EQ(box->mDepth, 3.f);

        const auto* cylinder = parsed.as<Nif::NiPSysCylinderEmitter>(19);
        ASSERT_NE(cylinder, nullptr);
        EXPECT_EQ(cylinder->mRadius, 4.f);
        EXPECT_EQ(cylinder->mHeight, 5.f);
        EXPECT_EQ(parsed.as<Nif::NiPSysSphereEmitter>(20)->mRadius, 6.f);

        const auto* mesh = parsed.as<Nif::NiPSysMeshEmitter>(21);
        ASSERT_NE(mesh, nullptr);
        ASSERT_EQ(mesh->mEmitterMeshes.size(), 1u);
        EXPECT_EQ(mesh->mEmitterMeshes[0].getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(mesh->mInitialVelocityType, 1u);
        EXPECT_EQ(mesh->mEmissionType, 2u);
        EXPECT_EQ(mesh->mEmissionAxis, osg::Vec3f(0, 1, 0));
    }

    TEST(NifParticleTest, readsTheEmitterOfAnOlderFileWithoutAVariationOfTheRadius)
    {
        Builder file(oblivion);
        file.add("NiNode", file.node({ .mName = "Root" }));
        file.add("NiPSysBoxEmitter", emitter(file, "Box", -1).ref(-1).f32(1).f32(2).f32(3));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* box = parsed.as<Nif::NiPSysBoxEmitter>(1);
        ASSERT_NE(box, nullptr);
        EXPECT_EQ(box->mInitialRadius, 7.f);
        EXPECT_EQ(box->mRadiusVariation, 8.f);
        EXPECT_EQ(box->mWidth, 1.f);
    }

    TEST(NifParticleTest, readsTheColliersOfAParticleSystem)
    {
        Builder file(fallout3);
        file.add("NiNode", file.node({ .mName = "Root" }));
        file.add("NiParticleSystem", geometry(file, { .mName = "Rain" }, -1).boolean(true).refs({ 2 }));
        file.add("NiPSysColliderManager", modifier(file, "Colliders", 5000, 1).ref(3));
        // 3 and 4: the colliders, one after the other
        file.add("NiPSysPlanarCollider",
            Bytes()
                .f32(0.5f)
                .boolean(true)
                .boolean(false)
                .ref(5)
                .ref(2)
                .ref(4)
                .ref(0)
                .f32(10)
                .f32(20)
                .vec3(1, 0, 0)
                .vec3(0, 1, 0));
        file.add("NiPSysSphericalCollider",
            Bytes().f32(0.25f).boolean(false).boolean(true).ref(-1).ref(2).ref(-1).ref(0).f32(2));
        file.add("NiPSysSpawnModifier",
            modifier(file, "Spawn", 2000, 1).u16(1).f32(1).u16(1).u16(1).f32(0).f32(0).f32(1).f32(0));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* manager = parsed.as<Nif::NiPSysColliderManager>(2);
        ASSERT_NE(manager, nullptr);
        EXPECT_EQ(manager->mCollider.getPtr(), parsed.as<Nif::NiPSysPlanarCollider>(3));

        const auto* planar = parsed.as<Nif::NiPSysPlanarCollider>(3);
        ASSERT_NE(planar, nullptr);
        EXPECT_EQ(planar->mBounce, 0.5f);
        EXPECT_TRUE(planar->mCollideSpawn);
        EXPECT_FALSE(planar->mCollideDie);
        EXPECT_EQ(planar->mSpawnModifier.getPtr(), parsed.as<Nif::NiPSysSpawnModifier>(5));
        EXPECT_EQ(planar->mParent.getPtr(), manager);
        EXPECT_EQ(planar->mNextCollider.getPtr(), parsed.as<Nif::NiPSysSphericalCollider>(4));
        EXPECT_EQ(planar->mColliderObject.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(planar->mWidth, 10.f);
        EXPECT_EQ(planar->mHeight, 20.f);
        EXPECT_EQ(planar->mXAxis, osg::Vec3f(1, 0, 0));
        EXPECT_EQ(planar->mYAxis, osg::Vec3f(0, 1, 0));

        const auto* sphere = parsed.as<Nif::NiPSysSphericalCollider>(4);
        ASSERT_NE(sphere, nullptr);
        EXPECT_FALSE(sphere->mCollideSpawn);
        EXPECT_TRUE(sphere->mCollideDie);
        EXPECT_TRUE(sphere->mNextCollider.empty());
        EXPECT_EQ(sphere->mRadius, 2.f);
    }

    /// The start of a controller of a modifier: the controller, its interpolator and the name of the modifier.
    Bytes modifierController(Builder& file, std::string_view modifierName)
    {
        Bytes bytes;
        bytes.ref(-1).u16(0x8).f32(1).f32(0).f32(0).f32(2).ref(-1); // the time controller
        bytes.ref(-1); // the interpolator
        return std::move(file.string(bytes, modifierName));
    }

    TEST(NifParticleTest, readsTheControllersOfTheModifiers)
    {
        Builder file(fallout3);
        file.add("NiNode", file.node({ .mName = "Root" }));
        file.add("BSMasterParticleSystem", file.node({ .mName = "Master" }, { 2 }).u16(3).refs({ 2, 2 }));
        file.add("NiParticleSystem", geometry(file, { .mName = "Fire" }, -1).boolean(true).refs({}));
        file.add("NiPSysEmitterCtlr", modifierController(file, "Emitter").ref(-1));
        file.add("BSPSysMultiTargetEmitterCtlr", modifierController(file, "Multi").ref(-1).u16(5).ref(1));
        file.add("NiPSysEmitterSpeedCtlr", modifierController(file, "Speed"));
        file.add("NiPSysModifierActiveCtlr", modifierController(file, "Active"));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* master = parsed.as<Nif::BSMasterParticleSystem>(1);
        ASSERT_NE(master, nullptr);
        EXPECT_EQ(master->mMaxEmitters, 3u);
        ASSERT_EQ(master->mParticleSystems.size(), 2u);
        EXPECT_EQ(master->mParticleSystems[0].getPtr(), parsed.as<Nif::NiParticleSystem>(2));

        const auto* emitter = parsed.as<Nif::NiPSysEmitterCtlr>(3);
        ASSERT_NE(emitter, nullptr);
        EXPECT_EQ(emitter->mModifierName, "Emitter");
        EXPECT_TRUE(emitter->mVisInterpolator.empty());

        const auto* multi = parsed.as<Nif::BSPSysMultiTargetEmitterCtlr>(4);
        ASSERT_NE(multi, nullptr);
        EXPECT_EQ(multi->mModifierName, "Multi");
        EXPECT_EQ(multi->mMaxEmitters, 5u);
        EXPECT_EQ(multi->mMasterPSys.getPtr(), master);

        EXPECT_EQ(parsed.as<Nif::NiPSysModifierFloatCtlr>(5)->mModifierName, "Speed");
        EXPECT_EQ(parsed.as<Nif::NiPSysModifierBoolCtlr>(6)->mModifierName, "Active");
    }

    TEST(NifParticleTest, readsAParticleSystemOfSkyrimAndFallout4)
    {
        // Skyrim: the geometry and then the distances in which it is shown
        {
            Builder file(skyrim);
            file.add("NiNode", file.node({ .mName = "Root" }));
            file.add("NiParticleSystem",
                geometry(file, { .mName = "Smoke" }, -1).u16(1).u16(2).u16(3).u16(4).boolean(true).refs({ 2 }));
            file.add("NiPSysGravityModifier",
                modifier(file, "Gravity", 4000, 1)
                    .ref(0)
                    .vec3(0, 0, 1)
                    .f32(0)
                    .f32(1)
                    .u32(1)
                    .f32(0)
                    .f32(1)
                    .boolean(false));
            file.addRoot(0);

            const Parsed parsed(file.build());

            const auto* system = parsed.as<Nif::NiParticleSystem>(1);
            ASSERT_NE(system, nullptr);
            EXPECT_THAT(system->mNearFar, ElementsAre(1u, 2u, 3u, 4u));
            EXPECT_TRUE(system->mWorldSpace);
            ASSERT_EQ(system->mModifiers.size(), 1u);
            EXPECT_FALSE(parsed.as<Nif::NiPSysGravityModifier>(2)->mWorldAligned);
        }
        // Fallout 4: it is no longer a geometry, and its data is read as one of the particles
        {
            Builder file(fallout4);
            file.add("NiNode", file.node({ .mName = "Root" }));
            Bytes bytes;
            file.place(bytes, { .mName = "Embers" });
            bytes.vec3(1, 2, 3).f32(4); // the bounding sphere
            bytes.ref(-1).ref(-1).ref(-1); // the skin, the shader and the alpha property
            bytes.u32(0).u32(0x12345); // the description of the vertices
            bytes.u16(5).u16(6).u16(7).u16(8); // the distances
            bytes.ref(-1); // the data
            bytes.boolean(false).refs({});
            file.add("NiParticleSystem", bytes);
            file.addRoot(0);

            const Parsed parsed(file.build());

            const auto* system = parsed.as<Nif::NiParticleSystem>(1);
            ASSERT_NE(system, nullptr);
            EXPECT_EQ(system->mBoundingSphere.center(), osg::Vec3f(1, 2, 3));
            EXPECT_EQ(system->mBoundingSphere.radius(), 4.f);
            EXPECT_THAT(system->mNearFar, ElementsAre(5u, 6u, 7u, 8u));
            EXPECT_FALSE(system->mWorldSpace);
        }
    }

    TEST(NifParticleTest, readsTheModifiersThatOnlyTheLaterGamesHave)
    {
        Builder file(skyrim);
        file.add("NiNode", file.node({ .mName = "Root" }));
        file.add("NiParticleSystem",
            geometry(file, { .mName = "Magic" }, -1).u16(0).u16(0).u16(0).u16(0).boolean(true).refs({}));
        file.add("BSPSysInheritVelocityModifier", modifier(file, "Inherit", 3000, 1).ref(0).f32(0.5f).f32(1).f32(2));
        file.add("BSPSysLODModifier", modifier(file, "Lod", 1, 1).f32(1).f32(2).f32(3).f32(4));
        file.add("BSPSysRecycleBoundModifier", modifier(file, "Recycle", 3000, 1).vec3(1, 2, 3).vec3(4, 5, 6).ref(0));
        file.add("BSPSysSubTexModifier",
            modifier(file, "SubTexture", 3000, 1).f32(1).f32(2).f32(3).f32(4).f32(5).f32(6).f32(7));
        file.add("BSPSysHavokUpdateModifier", modifier(file, "Havok", 3000, 1).refs({ 0 }).ref(3));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* inherit = parsed.as<Nif::BSPSysInheritVelocityModifier>(2);
        ASSERT_NE(inherit, nullptr);
        EXPECT_EQ(inherit->mInheritObject.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(inherit->mInheritChance, 0.5f);
        EXPECT_EQ(inherit->mVelocityMult, 1.f);
        EXPECT_EQ(inherit->mVelcoityVariation, 2.f);

        const auto* lod = parsed.as<Nif::BSPSysLODModifier>(3);
        ASSERT_NE(lod, nullptr);
        EXPECT_EQ(lod->mOrder, Nif::NiPSysModifier::NiPSysModifierOrder::BSLOD);
        EXPECT_EQ(lod->mLODStartDistance, 1.f);
        EXPECT_EQ(lod->mLODEndDistance, 2.f);
        EXPECT_EQ(lod->mEndEmitScale, 3.f);
        EXPECT_EQ(lod->mEndSize, 4.f);

        const auto* recycle = parsed.as<Nif::BSPSysRecycleBoundModifier>(4);
        ASSERT_NE(recycle, nullptr);
        EXPECT_EQ(recycle->mBoundOffset, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(recycle->mBoundExtents, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(recycle->mBoundObject.getPtr(), parsed.as<Nif::NiNode>(0));

        const auto* subTexture = parsed.as<Nif::BSPSysSubTexModifier>(5);
        ASSERT_NE(subTexture, nullptr);
        EXPECT_EQ(subTexture->mStartFrame, 1.f);
        EXPECT_EQ(subTexture->mStartFrameFudge, 2.f);
        EXPECT_EQ(subTexture->mEndFrame, 3.f);
        EXPECT_EQ(subTexture->mLoopStartFrame, 4.f);
        EXPECT_EQ(subTexture->mLoopStartFrameFudge, 5.f);
        EXPECT_EQ(subTexture->mFrameCount, 6.f);
        EXPECT_EQ(subTexture->mFrameCountFudge, 7.f);

        const auto* havok = parsed.as<Nif::BSPSysHavokUpdateModifier>(6);
        ASSERT_NE(havok, nullptr);
        ASSERT_EQ(havok->mMeshes.size(), 1u);
        EXPECT_EQ(havok->mMeshes[0].getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(havok->mModifier.getPtr(), parsed.as<Nif::BSPSysLODModifier>(3));
    }

    TEST(NifParticleTest, aModifierThatNamesTheWrongKindOfRecordIsRefused)
    {
        Builder file(fallout3);
        file.add("NiNode", file.node({ .mName = "Root" }));
        // the target of a modifier is the node, which is not a particle system
        file.add("NiPSysBoundUpdateModifier", modifier(file, "Bounds", 7000, 0).u16(1));
        file.addRoot(0);

        EXPECT_THROW(Parsed(file.build()), std::runtime_error);
    }
}
