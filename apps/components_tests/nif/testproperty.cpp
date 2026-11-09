#include "niftestfile.hpp"

#include <components/nif/data.hpp>
#include <components/nif/effect.hpp>
#include <components/nif/exception.hpp>
#include <components/nif/node.hpp>
#include <components/nif/property.hpp>
#include <components/nif/texture.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>
#include <vector>

// The properties of the shapes: the fixed ones of the old games and the shaders of Fallout 3 and the games after it.
namespace
{
    using namespace NifTest;
    using namespace testing;

    /// The start of a property: the name, the extra data and the controller.
    Bytes property(Builder& file, std::string_view name = "")
    {
        Bytes bytes;
        file.named(bytes, name);
        return bytes;
    }

    TEST(NifPropertyTest, readsTheFixedPropertiesOfOblivion)
    {
        Builder file(oblivion);
        file.add("NiAlphaProperty", property(file).u16(0x1ED).u8(128));
        file.add("NiDitherProperty", property(file).u16(1));
        file.add("NiFogProperty", property(file).u16(1).f32(0.5f).vec3(0.1f, 0.2f, 0.3f));
        // the colours of a material in Oblivion are the ambient, the diffuse, the specular and the emissive
        file.add("NiMaterialProperty",
            property(file, "Material").vec3(1, 0, 0).vec3(0, 1, 0).vec3(0, 0, 1).vec3(1, 1, 0).f32(30).f32(0.5f));
        file.add("NiShadeProperty", property(file).u16(1));
        file.add("NiSpecularProperty", property(file).u16(1));
        file.add("NiWireframeProperty", property(file).u16(0));
        file.add("NiStencilProperty",
            property(file).u8(1).u32(7).u32(5).u32(0xFF).u32(0).u32(2).u32(3).u32(1));
        file.add("NiVertexColorProperty", property(file).u16(1).u32(2).u32(1));
        file.add("NiZBufferProperty", property(file).u16(3).u32(7));

        const Parsed parsed(file.build());

        const auto* alpha = parsed.as<Nif::NiAlphaProperty>(0);
        ASSERT_NE(alpha, nullptr);
        EXPECT_EQ(alpha->mFlags, 0x1EDu);
        EXPECT_EQ(alpha->mThreshold, 128u);
        EXPECT_EQ(parsed.as<Nif::NiDitherProperty>(1)->mFlags, 1u);
        const auto* fog = parsed.as<Nif::NiFogProperty>(2);
        EXPECT_EQ(fog->mFogDepth, 0.5f);
        EXPECT_EQ(fog->mColour, osg::Vec3f(0.1f, 0.2f, 0.3f));
        const auto* material = parsed.as<Nif::NiMaterialProperty>(3);
        EXPECT_EQ(material->mName, "Material");
        EXPECT_EQ(material->mAmbient, osg::Vec3f(1, 0, 0));
        EXPECT_EQ(material->mDiffuse, osg::Vec3f(0, 1, 0));
        EXPECT_EQ(material->mSpecular, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(material->mEmissive, osg::Vec3f(1, 1, 0));
        EXPECT_EQ(material->mGlossiness, 30.f);
        EXPECT_EQ(material->mAlpha, 0.5f);
        EXPECT_EQ(material->mEmissiveMult, 1.f);
        EXPECT_EQ(parsed.as<Nif::NiShadeProperty>(4)->mFlags, 1u);
        EXPECT_TRUE(parsed.as<Nif::NiSpecularProperty>(5)->mEnable);
        EXPECT_FALSE(parsed.as<Nif::NiWireframeProperty>(6)->mEnable);
        const auto* stencil = parsed.as<Nif::NiStencilProperty>(7);
        EXPECT_TRUE(stencil->mEnabled);
        EXPECT_EQ(stencil->mTestFunction, Nif::NiStencilProperty::TestFunc::Always);
        EXPECT_EQ(stencil->mStencilRef, 5u);
        EXPECT_EQ(stencil->mStencilMask, 0xFFu);
        EXPECT_EQ(stencil->mFailAction, Nif::NiStencilProperty::Action::Keep);
        EXPECT_EQ(stencil->mZFailAction, Nif::NiStencilProperty::Action::Replace);
        EXPECT_EQ(stencil->mPassAction, Nif::NiStencilProperty::Action::Increment);
        EXPECT_EQ(stencil->mDrawMode, Nif::NiStencilProperty::DrawMode::CounterClockwise);
        const auto* vertexColor = parsed.as<Nif::NiVertexColorProperty>(8);
        EXPECT_EQ(vertexColor->mVertexMode, Nif::NiVertexColorProperty::VertexMode::VertMode_SrcAmbDif);
        EXPECT_EQ(vertexColor->mLightingMode, Nif::NiVertexColorProperty::LightMode::LightMode_EmiAmbDif);
        const auto* depth = parsed.as<Nif::NiZBufferProperty>(9);
        EXPECT_TRUE(depth->depthTest());
        EXPECT_TRUE(depth->depthWrite());
        EXPECT_EQ(depth->mTestFunction, 7u);
    }

    TEST(NifPropertyTest, readsTheFixedPropertiesOfFallout3WithTheirFlags)
    {
        // The material has no ambient and diffuse from beth 26 on, and has the multiplier of the emissive; the stencil
        // and the vertex colour keep what they need in their flags.
        Builder file(fallout3);
        file.add("NiMaterialProperty", property(file).vec3(0, 0, 1).vec3(1, 1, 0).f32(30).f32(0.5f).f32(2));
        file.add("NiStencilProperty", property(file).u16(0x1 | (2 << 1) | (1 << 4) | (3 << 7) | (2 << 10) | (5 << 12))
                                          .u32(7)
                                          .u32(0xFF));
        file.add("NiVertexColorProperty", property(file).u16((2 << 4) | (1 << 3)));
        file.add("NiZBufferProperty", property(file).u16(0x1 | (3 << 2)));
        file.add("NiShadeProperty", property(file).u16(2));

        const Parsed parsed(file.build());

        const auto* material = parsed.as<Nif::NiMaterialProperty>(0);
        EXPECT_EQ(material->mAmbient, osg::Vec3f(1, 1, 1));
        EXPECT_EQ(material->mSpecular, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(material->mEmissiveMult, 2.f);
        const auto* stencil = parsed.as<Nif::NiStencilProperty>(1);
        EXPECT_TRUE(stencil->mEnabled);
        EXPECT_EQ(stencil->mFailAction, Nif::NiStencilProperty::Action::Replace);
        EXPECT_EQ(stencil->mZFailAction, Nif::NiStencilProperty::Action::Zero);
        EXPECT_EQ(stencil->mPassAction, Nif::NiStencilProperty::Action::Increment);
        EXPECT_EQ(stencil->mDrawMode, Nif::NiStencilProperty::DrawMode::Clockwise);
        EXPECT_EQ(stencil->mTestFunction, Nif::NiStencilProperty::TestFunc::NotEqual);
        EXPECT_EQ(stencil->mStencilRef, 7u);
        EXPECT_EQ(stencil->mStencilMask, 0xFFu);
        const auto* vertexColor = parsed.as<Nif::NiVertexColorProperty>(2);
        EXPECT_EQ(vertexColor->mVertexMode, Nif::NiVertexColorProperty::VertexMode::VertMode_SrcAmbDif);
        EXPECT_EQ(vertexColor->mLightingMode, Nif::NiVertexColorProperty::LightMode::LightMode_EmiAmbDif);
        EXPECT_EQ(parsed.as<Nif::NiZBufferProperty>(3)->mTestFunction, 3u);
    }

    TEST(NifPropertyTest, readsTheFixedPropertiesOfMorrowindWithTheFlagsOfTheirOwn)
    {
        Builder file(morrowind);
        file.add("NiMaterialProperty",
            property(file).u16(3).vec3(1, 0, 0).vec3(0, 1, 0).vec3(0, 0, 1).vec3(1, 1, 0).f32(30).f32(0.5f));
        file.add("NiStencilProperty", property(file).u16(1).u8(1).u32(3).u32(1).u32(2).u32(0).u32(1).u32(2).u32(3));
        file.add("NiZBufferProperty", property(file).u16(1 | (4 << 2)));

        const Parsed parsed(file.build());

        EXPECT_EQ(parsed.as<Nif::NiMaterialProperty>(0)->mFlags, 3u);
        EXPECT_EQ(parsed.as<Nif::NiStencilProperty>(1)->mFlags, 1u);
        EXPECT_EQ(parsed.as<Nif::NiStencilProperty>(1)->mDrawMode, Nif::NiStencilProperty::DrawMode::Both);
        EXPECT_EQ(parsed.as<Nif::NiZBufferProperty>(2)->mTestFunction, 4u);
    }

    Bytes texture(Builder& file, std::int32_t source, std::uint32_t clamp, std::uint32_t filter, bool transform)
    {
        Bytes bytes;
        const bool old = file.profile().mVersion <= 0x14000005;
        file.flag(bytes, true);
        bytes.ref(source);
        if (old)
            bytes.u32(clamp).u32(filter).u32(1); // the clamp, the filter and the set of the coordinates
        else
            bytes.u16(static_cast<std::uint16_t>(clamp | (filter << 4)));
        if (file.profile().mVersion <= 0x0A040001)
            bytes.u32(0); // the settings of the PS2
        if (file.profile().mVersion <= 0x04010C00)
            bytes.u16(0);
        if (file.profile().mVersion >= 0x0A010000)
        {
            bytes.boolean(transform);
            if (transform)
                bytes.vec2(1, 2).vec2(3, 4).f32(0.5f).u32(1).vec2(5, 6);
        }
        return bytes;
    }

    TEST(NifPropertyTest, readsTheTexturesOfAShapeOfOblivion)
    {
        Builder file(oblivion);
        // 0: a source texture of a file
        file.add("NiSourceTexture", property(file).u8(1).sized("a.dds").ref(-1).u32(0).u32(0).u32(0).u8(1).boolean(true));
        Bytes bytes = property(file);
        bytes.u32(1); // modulate
        bytes.u32(7);
        bytes.raw(texture(file, 0, 3, 2, true).str());
        bytes.u8(0); // no dark texture
        for (int i = 0; i < 3; ++i)
            bytes.u8(0);
        // the bump texture has the luma and the matrix of its own
        bytes.raw(texture(file, 0, 1, 1, false).str()).vec2(0.5f, 0.25f).vec4(1, 0, 0, 1);
        bytes.u8(0);
        bytes.u32(1).raw(texture(file, 0, 0, 0, false).str()).u32(9);
        file.add("NiTexturingProperty", bytes);

        const Parsed parsed(file.build());

        const auto* property = parsed.as<Nif::NiTexturingProperty>(1);
        ASSERT_NE(property, nullptr);
        EXPECT_EQ(property->mApplyMode, Nif::NiTexturingProperty::ApplyMode::Decal);
        ASSERT_EQ(property->mTextures.size(), 7u);
        const auto& base = property->mTextures[0];
        EXPECT_TRUE(base.mEnabled);
        EXPECT_EQ(base.mSourceTexture.getPtr(), parsed.as<Nif::NiSourceTexture>(0));
        EXPECT_EQ(base.mClamp, 3u);
        EXPECT_EQ(base.mFilter, 2u);
        EXPECT_EQ(base.mUVSet, 1u);
        EXPECT_TRUE(base.wrapS());
        EXPECT_TRUE(base.wrapT());
        EXPECT_TRUE(base.mHasTransform);
        EXPECT_EQ(base.mTransform.mOffset, osg::Vec2f(1, 2));
        EXPECT_EQ(base.mTransform.mScale, osg::Vec2f(3, 4));
        EXPECT_EQ(base.mTransform.mRotation, 0.5f);
        EXPECT_EQ(base.mTransform.mOrigin, osg::Vec2f(5, 6));
        EXPECT_FALSE(property->mTextures[1].mEnabled);
        EXPECT_TRUE(property->mTextures[5].mEnabled);
        EXPECT_EQ(property->mEnvMapLumaBias, osg::Vec2f(0.5f, 0.25f));
        EXPECT_EQ(property->mBumpMapMatrix, osg::Vec4f(1, 0, 0, 1));
        ASSERT_EQ(property->mShaderTextures.size(), 1u);
        EXPECT_THAT(property->mShaderIds, ElementsAre(9u));
    }

    TEST(NifPropertyTest, readsTheTexturesOfAShapeOfFallout3WithTheirFlagsAndTheirParallax)
    {
        Builder file(fallout3);
        file.add("NiSourceTexture", property(file).u8(1).u32(0xFFFFFFFF).ref(-1).u32(0).u32(0).u32(0).u8(1).boolean(true).boolean(false));
        Bytes bytes = property(file);
        bytes.u16(5); // the flags of the property
        bytes.u32(8);
        for (int i = 0; i < 7; ++i)
        {
            bytes.raw(texture(file, i == 0 ? 0 : -1, 2, 3, false).str());
            if (i == 5)
                bytes.vec2(0, 0).vec4(0, 0, 0, 0);
        }
        bytes.raw(texture(file, -1, 0, 0, false).str()).f32(0.75f);
        bytes.u32(0);
        file.add("NiTexturingProperty", bytes);

        const Parsed parsed(file.build());

        const auto* property = parsed.as<Nif::NiTexturingProperty>(1);
        ASSERT_NE(property, nullptr);
        EXPECT_EQ(property->mFlags, 5u);
        ASSERT_EQ(property->mTextures.size(), 8u);
        EXPECT_EQ(property->mTextures[0].mClamp, 2u);
        EXPECT_EQ(property->mTextures[0].mFilter, 3u);
        EXPECT_EQ(property->mParallaxOffset, 0.75f);
        EXPECT_TRUE(property->mShaderTextures.empty());
    }

    TEST(NifPropertyTest, readsTheTexturesOfAShapeOfMorrowind)
    {
        Builder file(morrowind);
        file.add("NiSourceTexture", property(file).u8(1).sized("tx_wood.tga").u32(0).u32(1).u32(2).u8(1));
        Bytes bytes = property(file);
        bytes.u16(0); // the flags of the property
        bytes.u32(0).u32(7);
        bytes.raw(texture(file, 0, 1, 2, false).str());
        for (int i = 0; i < 6; ++i)
            file.flag(bytes, false);
        file.add("NiTexturingProperty", bytes);

        const Parsed parsed(file.build());

        const auto* property = parsed.as<Nif::NiTexturingProperty>(1);
        ASSERT_NE(property, nullptr);
        EXPECT_EQ(property->mApplyMode, Nif::NiTexturingProperty::ApplyMode::Replace);
        EXPECT_EQ(property->mTextures[0].mFilter, 2u);
        EXPECT_FALSE(property->mTextures[6].mEnabled);
        const auto* source = parsed.as<Nif::NiSourceTexture>(0);
        EXPECT_EQ(source->mFile, "tx_wood.tga");
        EXPECT_EQ(source->mPrefs.mUseMipMaps, Nif::NiSourceTexture::MipMapFormat::Yes);
        EXPECT_TRUE(source->mData.empty());
    }

    /// The start of a shader property of Fallout 3: the property, its flags and what the shader keeps from the games
    /// before: the type, the two sets of flags and the scale of the environment map.
    Bytes shader(Builder& file, std::string_view name = "")
    {
        Bytes bytes = property(file, name);
        return bytes.u16(1).u32(4).u32(0x80000000).u32(0x1).f32(0.5f);
    }

    TEST(NifPropertyTest, readsTheShadersOfFallout3)
    {
        Builder file(fallout3);
        file.add("BSShaderTextureSet", Bytes().u32(3).sized("a_d.dds").sized("a_n.dds").sized("a_g.dds"));
        file.add("BSShaderPPLightingProperty",
            shader(file, "PP").u32(3).ref(0).f32(0.25f).i32(7).f32(6).f32(0.5f));
        file.add("BSShaderNoLightingProperty", shader(file).u32(1).sized("glow.dds").vec4(1, 2, 3, 4));
        file.add("SkyShaderProperty", shader(file).u32(2).sized("sky.dds").u32(3));
        file.add("TallGrassShaderProperty", shader(file).sized("grass.dds"));
        file.add("TileShaderProperty", shader(file).u32(0).sized("tile.dds"));
        file.add("WaterShaderProperty", shader(file));
        file.add("BSWaterShaderProperty", shader(file).u32(0x42));
        file.add("BSSkyShaderProperty", shader(file).sized("sky.dds").u32(5));

        const Parsed parsed(file.build());

        const auto* textures = parsed.as<Nif::BSShaderTextureSet>(0);
        ASSERT_NE(textures, nullptr);
        EXPECT_THAT(textures->mTextures, ElementsAre("a_d.dds", "a_n.dds", "a_g.dds"));
        const auto* pp = parsed.as<Nif::BSShaderPPLightingProperty>(1);
        ASSERT_NE(pp, nullptr);
        EXPECT_EQ(pp->mName, "PP");
        EXPECT_EQ(pp->mType, 4u);
        EXPECT_EQ(pp->mShaderFlags1, 0x80000000u);
        EXPECT_EQ(pp->mEnvMapScale, 0.5f);
        EXPECT_EQ(pp->mClamp, 3u);
        EXPECT_EQ(pp->mTextureSet.getPtr(), textures);
        EXPECT_EQ(pp->mRefraction.mStrength, 0.25f);
        EXPECT_EQ(pp->mRefraction.mPeriod, 7);
        EXPECT_EQ(pp->mParallax.mMaxPasses, 6.f);
        EXPECT_EQ(pp->mParallax.mScale, 0.5f);
        const auto* noLighting = parsed.as<Nif::BSShaderNoLightingProperty>(2);
        EXPECT_EQ(noLighting->mFilename, "glow.dds");
        EXPECT_EQ(noLighting->mFalloffParams, osg::Vec4f(1, 2, 3, 4));
        EXPECT_EQ(parsed.as<Nif::SkyShaderProperty>(3)->mFilename, "sky.dds");
        EXPECT_EQ(parsed.as<Nif::SkyShaderProperty>(3)->mSkyObjectType, Nif::SkyObjectType::SkyClouds);
        EXPECT_EQ(parsed.as<Nif::TallGrassShaderProperty>(4)->mFilename, "grass.dds");
        EXPECT_EQ(parsed.as<Nif::TileShaderProperty>(5)->mFilename, "tile.dds");
        EXPECT_EQ(parsed.as<Nif::BSWaterShaderProperty>(7)->mFlags, 0x42u);
        EXPECT_EQ(parsed.as<Nif::BSSkyShaderProperty>(8)->mSkyObjectType, Nif::SkyObjectType::SkyStars);
    }

    TEST(NifPropertyTest, readsTheEarlierShadersOfFallout3WithoutTheirLaterParameters)
    {
        // The shader of the first files of Fallout 3 has no refraction and no parallax.
        for (const std::uint32_t bethVersion : { 14u, 24u })
        {
            Builder file(Profile{ 0x14020007, 11, bethVersion });
            file.add("BSShaderTextureSet", Bytes().u32(0));
            file.add("BSShaderPPLightingProperty", shader(file).u32(3).ref(0).raw(bethVersion >= 15 ? std::string(8, '\0') : ""));
            file.add("BSShaderNoLightingProperty", shader(file).u32(1).sized("glow.dds"));

            const Parsed parsed(file.build());

            EXPECT_EQ(parsed.as<Nif::BSShaderPPLightingProperty>(1)->mRefraction.mStrength, 0.f);
            EXPECT_EQ(parsed.as<Nif::BSShaderNoLightingProperty>(2)->mFalloffParams, osg::Vec4f());
        }
    }

    TEST(NifPropertyTest, readsTheShadersOfSkyrim)
    {
        Builder file(skyrim);
        file.add("BSShaderTextureSet", Bytes().u32(1).sized("a_d.dds"));
        const auto lighting = [&](std::uint32_t type, Bytes tail) {
            Bytes bytes;
            bytes.u32(type);
            file.named(bytes, "Lighting");
            bytes.u32(0x80000000).u32(0x1).vec2(0.5f, 0.25f).vec2(2, 3);
            bytes.ref(0).vec3(0.1f, 0.2f, 0.3f).f32(2).u32(3).f32(0.9f).f32(0.1f).f32(25).vec3(1, 1, 1).f32(1.5f);
            bytes.f32(0.5f).f32(0.25f); // the effects of the lighting
            return file.add("BSLightingShaderProperty", bytes.raw(tail.str()));
        };
        lighting(0, Bytes());
        lighting(1, Bytes().f32(0.75f));
        lighting(6, Bytes().vec3(1, 0.5f, 0.25f));
        lighting(5, Bytes().vec3(0.5f, 0.5f, 0.5f));
        lighting(7, Bytes().f32(8).f32(0.5f));
        lighting(11, Bytes().f32(1).f32(2).vec2(3, 4).f32(5));
        lighting(14, Bytes().vec4(1, 2, 3, 4));
        lighting(16, Bytes().f32(2).vec3(1, 2, 3).vec3(4, 5, 6));
        Bytes effect;
        file.named(effect, "Effect");
        effect.u32(0x80000000).u32(0x1).vec2(0, 0).vec2(1, 1).sized("fire.dds").u32(0x030201).vec4(1, 2, 3, 4);
        effect.vec4(1, 0, 0, 1).f32(2).f32(0.5f).sized("grey.dds");
        file.add("BSEffectShaderProperty", effect);

        const Parsed parsed(file.build());

        const auto* plain = parsed.as<Nif::BSLightingShaderProperty>(1);
        ASSERT_NE(plain, nullptr);
        EXPECT_EQ(plain->mName, "Lighting");
        EXPECT_EQ(plain->mShaderFlags1, 0x80000000u);
        EXPECT_EQ(plain->mUVOffset, osg::Vec2f(0.5f, 0.25f));
        EXPECT_EQ(plain->mUVScale, osg::Vec2f(2, 3));
        EXPECT_EQ(plain->mTextureSet.getPtr(), parsed.as<Nif::BSShaderTextureSet>(0));
        EXPECT_EQ(plain->mEmissive, osg::Vec3f(0.1f, 0.2f, 0.3f));
        EXPECT_EQ(plain->mEmissiveMult, 2.f);
        EXPECT_EQ(plain->mClamp, 3u);
        EXPECT_EQ(plain->mAlpha, 0.9f);
        EXPECT_EQ(plain->mGlossiness, 25.f);
        EXPECT_EQ(plain->mSpecStrength, 1.5f);
        EXPECT_THAT(plain->mLightingEffects, ElementsAre(0.5f, 0.25f));
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(2)->mEnvMapScale, 0.75f);
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(3)->mHairTintColor, osg::Vec3f(1, 0.5f, 0.25f));
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(4)->mSkinTintColor, osg::Vec3f(0.5f, 0.5f, 0.5f));
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(5)->mParallax.mMaxPasses, 8.f);
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(6)->mMultiLayerParallax.mInnerLayerTextureScale,
            osg::Vec2f(3, 4));
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(7)->mSparkle, osg::Vec4f(1, 2, 3, 4));
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(8)->mRightEyeReflectionCenter, osg::Vec3f(4, 5, 6));
        const auto* fire = parsed.as<Nif::BSEffectShaderProperty>(9);
        ASSERT_NE(fire, nullptr);
        EXPECT_EQ(fire->mSourceTexture, "fire.dds");
        EXPECT_EQ(fire->mClamp, 1u);
        EXPECT_EQ(fire->mLightingInfluence, 2u);
        EXPECT_EQ(fire->mEnvMapMinLOD, 3u);
        EXPECT_EQ(fire->mFalloffParams, osg::Vec4f(1, 2, 3, 4));
        EXPECT_EQ(fire->mBaseColorScale, 2.f);
        EXPECT_EQ(fire->mGreyscaleTexture, "grey.dds");
    }

    TEST(NifPropertyTest, readsTheShadersOfFallout4)
    {
        Builder file(fallout4);
        file.add("BSShaderTextureSet", Bytes().u32(1).sized("a_d.dds"));
        const auto lighting = [&](std::uint32_t type, float rimlight, Bytes tail) {
            Bytes bytes;
            bytes.u32(type);
            file.named(bytes, "Lighting");
            bytes.u32(0x80000000).u32(0x1).vec2(0, 0).vec2(1, 1);
            bytes.ref(0).vec3(0.1f, 0.2f, 0.3f).f32(2);
            file.string(bytes, "materials\\a.bgsm");
            bytes.u32(3).f32(0.9f).f32(0.1f).f32(0.8f).vec3(1, 1, 1).f32(1.5f);
            bytes.f32(0.5f).f32(rimlight);
            if (rimlight == std::numeric_limits<float>::max())
                bytes.f32(0.125f);
            bytes.f32(2).f32(4); // the scale of the palette and the power of the fresnel
            bytes.f32(1).f32(2).f32(3).f32(4).f32(5).f32(6); // the wetness
            return file.add("BSLightingShaderProperty", bytes.raw(tail.str()));
        };
        lighting(0, 1.f, Bytes());
        lighting(1, std::numeric_limits<float>::max(), Bytes().f32(0.75f).boolean(true).boolean(false));
        lighting(5, 1.f, Bytes().vec3(0.5f, 0.5f, 0.5f).f32(0.25f));
        Bytes effect;
        file.named(effect, "Effect");
        effect.u32(0x80000000).u32(0x1).vec2(0, 0).vec2(1, 1).sized("fire.dds").u32(0).vec4(1, 2, 3, 4);
        effect.vec4(1, 0, 0, 1).f32(2).f32(0.5f).sized("grey.dds");
        effect.sized("env.dds").sized("normal.dds").sized("mask.dds").f32(0.375f);
        file.add("BSEffectShaderProperty", effect);

        const Parsed parsed(file.build());

        const auto* plain = parsed.as<Nif::BSLightingShaderProperty>(1);
        ASSERT_NE(plain, nullptr);
        EXPECT_EQ(plain->mRootMaterial, "materials\\a.bgsm");
        EXPECT_EQ(plain->mSmoothness, 0.8f);
        EXPECT_EQ(plain->mSubsurfaceRolloff, 0.5f);
        EXPECT_EQ(plain->mGrayscaleToPaletteScale, 2.f);
        EXPECT_EQ(plain->mFresnelPower, 4.f);
        EXPECT_EQ(plain->mWetness.mSpecScale, 1.f);
        EXPECT_EQ(plain->mWetness.mEnvMapScale, 4.f);
        EXPECT_EQ(plain->mWetness.mMetalness, 6.f);
        const auto* envMap = parsed.as<Nif::BSLightingShaderProperty>(2);
        EXPECT_EQ(envMap->mBacklightPower, 0.125f);
        EXPECT_EQ(envMap->mEnvMapScale, 0.75f);
        EXPECT_TRUE(envMap->mUseSSR);
        EXPECT_FALSE(envMap->mWetnessUseSSR);
        EXPECT_EQ(parsed.as<Nif::BSLightingShaderProperty>(3)->mSkinTintAlpha, 0.25f);
        const auto* fire = parsed.as<Nif::BSEffectShaderProperty>(4);
        EXPECT_EQ(fire->mEnvMapTexture, "env.dds");
        EXPECT_EQ(fire->mNormalTexture, "normal.dds");
        EXPECT_EQ(fire->mEnvMaskTexture, "mask.dds");
        EXPECT_EQ(fire->mEnvMapScale, 0.375f);
    }

    TEST(NifTextureTest, readsTheSourcesOfTexturesOfTheGames)
    {
        // Morrowind has the pixels in the file only when the texture is not external.
        Builder file(morrowind);
        // a texture of the file that has pixels inside it
        file.add("NiSourceTexture", property(file).u8(0).u8(1).ref(1).u32(0).u32(1).u32(2).u8(1));
        file.add("NiPixelData",
            Bytes().u32(1).u32(0xFF).u32(0xFF00).u32(0xFF0000).u32(0xFF000000).u32(32).u32(0).u32(0).ref(-1).u32(1).u32(4)
                .u32(1).u32(1).u32(0).u32(4).u8(1).u8(2).u8(3).u8(4));

        const Parsed parsed(file.build());

        const auto* source = parsed.as<Nif::NiSourceTexture>(0);
        ASSERT_NE(source, nullptr);
        EXPECT_EQ(source->mExternal, 0);
        EXPECT_TRUE(source->mFile.empty());
        EXPECT_EQ(source->mData.getPtr(), parsed.as<Nif::NiPixelData>(1));
        EXPECT_EQ(source->mPrefs.mAlphaFormat, Nif::NiSourceTexture::AlphaFormat::Smooth);
    }

    TEST(NifEffectTest, readsTheLightsAndTheProjectedTexturesOfTheGames)
    {
        // The nodes that a light affects are skipped, and Fallout 4 does not have them.
        for (const Profile& profile : { morrowind, oblivion, fallout3, fallout4 })
        {
            Builder file(profile);
            const auto dynamic = [&](Bytes& bytes) {
                if (profile.mBethVersion >= 130)
                    return;
                if (profile.mVersion >= 0x0A01006A)
                    bytes.boolean(true);
                bytes.u32(2).ref(0).ref(0);
            };
            const auto light = [&](std::string_view name) {
                Bytes bytes;
                file.place(bytes, { .mName = std::string(name) });
                dynamic(bytes);
                bytes.f32(0.5f).vec3(0.1f, 0.1f, 0.1f).vec3(1, 1, 1).vec3(0.5f, 0.5f, 0.5f);
                return bytes;
            };
            file.add("NiAmbientLight", light("ambient"));
            file.add("NiPointLight", light("point").f32(1).f32(0.5f).f32(0.25f));
            Bytes spot = light("spot").f32(1).f32(0.5f).f32(0.25f).f32(0.75f);
            if (profile.mVersion >= 0x14020005)
                spot.f32(0.5f);
            file.add("NiSpotLight", spot.f32(2));
            Bytes effect;
            file.place(effect, { .mName = "projection" });
            dynamic(effect);
            effect.identity3().vec3(1, 2, 3).u32(2).u32(1).u32(2).u32(1).ref(-1).u8(1).vec4(0, 0, 1, -5);
            if (profile.mVersion <= 0x0A020000)
                effect.u32(0);
            if (profile.mVersion <= 0x04010C00)
                effect.u16(0);
            file.add("NiTextureEffect", effect);

            const Parsed parsed(file.build());

            const auto* ambient = parsed.as<Nif::NiLight>(0);
            ASSERT_NE(ambient, nullptr);
            EXPECT_EQ(ambient->mDimmer, 0.5f);
            EXPECT_EQ(ambient->mDiffuse, osg::Vec3f(1, 1, 1));
            EXPECT_EQ(parsed.as<Nif::NiPointLight>(1)->mQuadraticAttenuation, 0.25f);
            const auto* spotLight = parsed.as<Nif::NiSpotLight>(2);
            ASSERT_NE(spotLight, nullptr);
            EXPECT_EQ(spotLight->mOuterSpotAngle, 0.75f);
            EXPECT_EQ(spotLight->mInnerSpotAngle, profile.mVersion >= 0x14020005 ? 0.5f : 0.f);
            EXPECT_EQ(spotLight->mExponent, 2.f);
            const auto* projection = parsed.as<Nif::NiTextureEffect>(3);
            ASSERT_NE(projection, nullptr);
            EXPECT_EQ(projection->mProjectionPosition, osg::Vec3f(1, 2, 3));
            EXPECT_EQ(projection->mFilterMode, 2u);
            EXPECT_EQ(projection->mEnableClipPlane, 1u);
        }
    }
}
