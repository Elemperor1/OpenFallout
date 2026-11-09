#include <components/bgsm/file.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Bgsm
{
    namespace
    {
        // The bytes of a material file, as Fallout 4 writes them: little endian, strings with their length first
        class Writer
        {
        public:
            Writer& u8(std::uint8_t value)
            {
                mBytes.push_back(static_cast<char>(value));
                return *this;
            }

            Writer& u32(std::uint32_t value)
            {
                for (int i = 0; i < 4; ++i)
                    mBytes.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
                return *this;
            }

            Writer& f32(float value)
            {
                std::uint32_t bits;
                std::memcpy(&bits, &value, sizeof(bits));
                return u32(bits);
            }

            Writer& vec2(float x, float y) { return f32(x).f32(y); }

            Writer& vec3(float x, float y, float z) { return f32(x).f32(y).f32(z); }

            Writer& vec4(float x, float y, float z, float w) { return f32(x).f32(y).f32(z).f32(w); }

            Writer& raw(const std::string& value)
            {
                mBytes += value;
                return *this;
            }

            Writer& str(const std::string& value, bool terminated = true)
            {
                u32(static_cast<std::uint32_t>(value.size() + (terminated ? 1 : 0)));
                mBytes += value;
                if (terminated)
                    mBytes.push_back('\0');
                return *this;
            }

            const std::string& bytes() const { return mBytes; }

        private:
            std::string mBytes;
        };

        Files::IStreamPtr stream(const std::string& bytes)
        {
            return std::make_unique<std::istringstream>(bytes, std::ios::binary);
        }

        // The part that both kinds of file start with
        void writeCommon(Writer& w, std::uint32_t version)
        {
            w.u32(version).u32(3).vec2(0.25f, 0.5f).vec2(2.f, 4.f).f32(0.75f);
            w.u8(1).u32(5).u32(6).u8(128).u8(1); // alpha blend, blend modes, alpha test threshold, alpha test
            w.u8(1).u8(0).u8(1).u8(0).u8(1).u8(0).u8(1).u8(
                0); // depth write, test, SSR, wetness SSR, decal, two sided...
            w.u8(1).u8(0).f32(1.5f); // refraction, falloff, power
            if (version < 10)
                w.u8(1).f32(0.25f); // environment map
            else
                w.u8(1); // depth bias
            w.u8(1); // grayscale to palette colour
            if (version >= 6)
                w.u8(7); // mask writes
        }

        Writer lighting(std::uint32_t version, bool emit = true, bool terrain = true)
        {
            Writer w;
            w.raw("BGSM");
            writeCommon(w, version);
            w.str("textures/diffuse.dds")
                .str("textures/normal.dds")
                .str("textures/smooth.dds")
                .str("textures/gray.dds");
            if (version >= 3)
            {
                w.str("textures/glow.dds").str("textures/wrinkle.dds").str("textures/spec.dds");
                w.str("textures/lighting.dds").str("textures/flow.dds");
                if (version >= 17)
                    w.str("textures/field.dds");
            }
            else
            {
                w.str("textures/env.dds").str("textures/glow.dds").str("textures/inner.dds");
                w.str("textures/wrinkle.dds").str("textures/displacement.dds");
            }
            w.u8(1); // editor alpha threshold
            if (version >= 8)
                w.u8(1).u8(0).u8(1).vec3(0.1f, 0.2f, 0.3f).f32(0.4f).f32(0.5f);
            else
                w.u8(1).f32(2.f).f32(3.f).u8(1).f32(4.f);
            w.u8(1).vec3(0.5f, 0.6f, 0.7f).f32(8.f).f32(9.f).f32(10.f); // specular
            w.f32(11.f).f32(12.f).f32(13.f); // wetness control spec scale, power scale, minvar
            if (version < 10)
                w.f32(14.f);
            w.f32(15.f).f32(16.f);
            if (version >= 3)
            {
                w.u8(1);
                if (version >= 9)
                    w.u8(1).f32(17.f);
            }
            w.str("materials/root.bgsm").u8(1);
            w.u8(emit ? 1 : 0);
            if (emit)
                w.vec3(0.3f, 0.4f, 0.5f);
            w.f32(18.f).u8(1).u8(0);
            if (version >= 12)
            {
                w.f32(19.f);
                if (version >= 13)
                    w.u8(1).vec3(1.f, 2.f, 3.f);
            }
            else if (version < 8)
                w.u8(1);
            w.u8(1).u8(0).u8(1).u8(0).u8(1).u8(0); // shadows...glow map enabled
            if (version < 7)
                w.u8(1).u8(1);
            w.u8(1).vec3(0.9f, 0.8f, 0.7f).u8(1).u8(0).u8(1).u8(0); // hair, tint, tree, facegen, skin tint, tessellate
            if (version < 3)
                w.vec2(5.f, 6.f).vec3(7.f, 8.f, 9.f);
            w.f32(20.f);
            if (version >= 1)
            {
                w.u8(1);
                if (version >= 3)
                {
                    w.u8(terrain ? 1 : 0);
                    if (terrain)
                    {
                        if (version == 3)
                            w.u32(0xdeadbeef);
                        w.vec3(21.f, 22.f, 23.f);
                    }
                }
            }
            return w;
        }

        Writer effect(std::uint32_t version, bool glass = true)
        {
            Writer w;
            w.raw("BGEM");
            writeCommon(w, version);
            w.str("textures/base.dds").str("textures/gray.dds").str("textures/env.dds").str("textures/normal.dds");
            w.str("textures/mask.dds");
            if (version >= 11)
                w.str("textures/spec.dds").str("textures/lighting.dds").str("textures/glow.dds");
            if (version >= 21)
            {
                w.str("textures/scratch.dds").str("textures/dirt.dds");
                w.u8(glass ? 1 : 0);
                if (glass)
                {
                    w.vec3(0.1f, 0.2f, 0.3f).f32(0.4f);
                    if (version >= 22)
                        w.f32(0.5f);
                    w.f32(0.6f);
                }
            }
            if (version >= 10)
                w.u8(1).f32(0.7f);
            w.u8(1).u8(0).u8(1).u8(0).u8(1).u8(0); // blood, lighting, falloff, falloff colour, palette alpha, soft
            w.vec3(0.4f, 0.5f, 0.6f).f32(0.8f).vec4(1.f, 2.f, 3.f, 4.f).f32(0.9f).u8(9).f32(1.25f);
            if (version >= 11)
                w.vec3(2.f, 3.f, 4.f);
            if (version >= 15)
                w.vec3(5.f, 6.f, 7.f);
            if (version >= 16)
                w.u8(1);
            if (version >= 20)
                w.u8(1);
            return w;
        }

        template <class T>
        const T& as(const MaterialFilePtr& file)
        {
            const T* result = dynamic_cast<const T*>(file.get());
            if (result == nullptr)
                throw std::logic_error("Unexpected kind of material file");
            return *result;
        }

        TEST(BgsmFileTest, a_lighting_material_has_the_common_fields_and_the_texture_paths)
        {
            const MaterialFilePtr file = parse(stream(lighting(20).bytes()));

            ASSERT_NE(file, nullptr);
            EXPECT_EQ(file->mShaderType, ShaderType::Lighting);
            EXPECT_EQ(file->mVersion, 20u);
            EXPECT_EQ(file->mClamp, 3u);
            EXPECT_TRUE(file->wrapS());
            EXPECT_TRUE(file->wrapT());
            EXPECT_EQ(file->mUVOffset, osg::Vec2f(0.25f, 0.5f));
            EXPECT_EQ(file->mUVScale, osg::Vec2f(2.f, 4.f));
            EXPECT_EQ(file->mTransparency, 0.75f);
            EXPECT_TRUE(file->mAlphaBlend);
            EXPECT_EQ(file->mSourceBlendMode, 5u);
            EXPECT_EQ(file->mDestinationBlendMode, 6u);
            EXPECT_EQ(file->mAlphaTestThreshold, 128);
            EXPECT_TRUE(file->mAlphaTest);
            EXPECT_TRUE(file->mDepthWrite);
            EXPECT_FALSE(file->mDepthTest);
            EXPECT_TRUE(file->mSSR);
            EXPECT_FALSE(file->mWetnessControlSSR);
            EXPECT_TRUE(file->mDecal);
            EXPECT_FALSE(file->mTwoSided);
            EXPECT_TRUE(file->mDecalNoFade);
            EXPECT_FALSE(file->mNonOccluder);
            EXPECT_TRUE(file->mRefraction);
            EXPECT_FALSE(file->mRefractionFalloff);
            EXPECT_EQ(file->mRefractionPower, 1.5f);
            EXPECT_TRUE(file->mDepthBias);
            EXPECT_TRUE(file->mGrayscaleToPaletteColor);
            EXPECT_EQ(file->mMaskWrites, 7);

            const BGSMFile& bgsm = as<BGSMFile>(file);
            EXPECT_EQ(bgsm.mDiffuseMap, "textures/diffuse.dds");
            EXPECT_EQ(bgsm.mNormalMap, "textures/normal.dds");
            EXPECT_EQ(bgsm.mSmoothSpecMap, "textures/smooth.dds");
            EXPECT_EQ(bgsm.mGrayscaleMap, "textures/gray.dds");
            EXPECT_EQ(bgsm.mGlowMap, "textures/glow.dds");
            EXPECT_EQ(bgsm.mWrinkleMap, "textures/wrinkle.dds");
            EXPECT_EQ(bgsm.mSpecularMap, "textures/spec.dds");
            EXPECT_EQ(bgsm.mLightingMap, "textures/lighting.dds");
            EXPECT_EQ(bgsm.mFlowMap, "textures/flow.dds");
            EXPECT_EQ(bgsm.mDistanceFieldAlphaMap, "textures/field.dds");
            EXPECT_TRUE(bgsm.mEnableEditorAlphaThreshold);
            EXPECT_TRUE(bgsm.mTranslucency);
            EXPECT_FALSE(bgsm.mTranslucencyThickObject);
            EXPECT_TRUE(bgsm.mTranslucencyMixAlbedoWithSubsurfaceColor);
            EXPECT_EQ(bgsm.mTranslucencySubsurfaceColor, osg::Vec3f(0.1f, 0.2f, 0.3f));
            EXPECT_EQ(bgsm.mTranslucencyTransmissiveScale, 0.4f);
            EXPECT_EQ(bgsm.mTranslucencyTurbulence, 0.5f);
            EXPECT_TRUE(bgsm.mSpecularEnabled);
            EXPECT_EQ(bgsm.mSpecularColor, osg::Vec3f(0.5f, 0.6f, 0.7f));
            EXPECT_EQ(bgsm.mSpecularMult, 8.f);
            EXPECT_EQ(bgsm.mSmoothness, 9.f);
            EXPECT_EQ(bgsm.mFresnelPower, 10.f);
            EXPECT_EQ(bgsm.mWetnessControlSpecScale, 11.f);
            EXPECT_EQ(bgsm.mWetnessControlSpecPowerScale, 12.f);
            EXPECT_EQ(bgsm.mWetnessControlSpecMinvar, 13.f);
            EXPECT_EQ(bgsm.mWetnessControlFresnelPower, 15.f);
            EXPECT_EQ(bgsm.mWetnessControlMetalness, 16.f);
            EXPECT_TRUE(bgsm.mPBR);
            EXPECT_TRUE(bgsm.mCustomPorosity);
            EXPECT_EQ(bgsm.mPorosityValue, 17.f);
            EXPECT_EQ(bgsm.mRootMaterialPath, "materials/root.bgsm");
            EXPECT_TRUE(bgsm.mAnisoLighting);
            EXPECT_TRUE(bgsm.mEmitEnabled);
            EXPECT_EQ(bgsm.mEmittanceColor, osg::Vec3f(0.3f, 0.4f, 0.5f));
            EXPECT_EQ(bgsm.mEmittanceMult, 18.f);
            EXPECT_TRUE(bgsm.mModelSpaceNormals);
            EXPECT_FALSE(bgsm.mExternalEmittance);
            EXPECT_EQ(bgsm.mLumEmittance, 19.f);
            EXPECT_TRUE(bgsm.mUseAdaptiveEmissive);
            EXPECT_EQ(bgsm.mAdaptiveEmissiveExposureParams, osg::Vec3f(1.f, 2.f, 3.f));
            EXPECT_TRUE(bgsm.mReceiveShadows);
            EXPECT_FALSE(bgsm.mHideSecret);
            EXPECT_TRUE(bgsm.mCastShadows);
            EXPECT_FALSE(bgsm.mDissolveFade);
            EXPECT_TRUE(bgsm.mAssumeShadowmask);
            EXPECT_FALSE(bgsm.mGlowMapEnabled);
            EXPECT_TRUE(bgsm.mHair);
            EXPECT_EQ(bgsm.mHairTintColor, osg::Vec3f(0.9f, 0.8f, 0.7f));
            EXPECT_TRUE(bgsm.mTree);
            EXPECT_FALSE(bgsm.mFacegen);
            EXPECT_TRUE(bgsm.mSkinTint);
            EXPECT_FALSE(bgsm.mTessellate);
            EXPECT_EQ(bgsm.mGrayscaleToPaletteScale, 20.f);
            EXPECT_TRUE(bgsm.mSkewSpecularAlpha);
            EXPECT_TRUE(bgsm.mTerrain);
            EXPECT_EQ(bgsm.mTerrainParams, osg::Vec3f(21.f, 22.f, 23.f));
        }

        TEST(BgsmFileTest, the_oldest_lighting_materials_have_the_fields_that_were_taken_out_of_the_newer_ones)
        {
            const MaterialFilePtr file = parse(stream(lighting(2).bytes()));

            EXPECT_EQ(file->mVersion, 2u);
            // The environment map is part of the common fields up to version 9
            EXPECT_TRUE(file->mEnvMapEnabled);
            EXPECT_EQ(file->mEnvMapMaskScale, 0.25f);
            const BGSMFile& bgsm = as<BGSMFile>(file);
            EXPECT_EQ(bgsm.mEnvMap, "textures/env.dds");
            EXPECT_EQ(bgsm.mInnerLayerMap, "textures/inner.dds");
            EXPECT_EQ(bgsm.mDisplacementMap, "textures/displacement.dds");
            EXPECT_EQ(bgsm.mGlowMap, "textures/glow.dds");
            EXPECT_TRUE(bgsm.mRimLighting);
            EXPECT_EQ(bgsm.mRimPower, 2.f);
            EXPECT_EQ(bgsm.mBackLightPower, 3.f);
            EXPECT_TRUE(bgsm.mSubsurfaceLighting);
            EXPECT_EQ(bgsm.mSubsurfaceLightingRolloff, 4.f);
            EXPECT_EQ(bgsm.mWetnessControlEnvMapScale, 14.f);
            EXPECT_TRUE(bgsm.mBackLighting);
            EXPECT_TRUE(bgsm.mEnvMapWindow);
            EXPECT_TRUE(bgsm.mEnvMapEye);
            EXPECT_EQ(bgsm.mDisplacementMapParams, osg::Vec2f(5.f, 6.f));
            EXPECT_EQ(bgsm.mTessellationParams, osg::Vec3f(7.f, 8.f, 9.f));
            EXPECT_EQ(bgsm.mGrayscaleToPaletteScale, 20.f);
            EXPECT_TRUE(bgsm.mSkewSpecularAlpha);
            EXPECT_EQ(bgsm.mRootMaterialPath, "materials/root.bgsm");
        }

        TEST(BgsmFileTest, every_version_of_a_lighting_material_is_read_to_its_end)
        {
            // The last field has to be where the version says: the terrain is the last field that a file may have
            for (std::uint32_t version : { 0u, 1u, 2u, 3u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 16u, 17u, 20u, 22u })
            {
                SCOPED_TRACE(version);
                const MaterialFilePtr file = parse(stream(lighting(version).bytes()));
                const BGSMFile& bgsm = as<BGSMFile>(file);
                EXPECT_EQ(bgsm.mGrayscaleToPaletteScale, 20.f);
                EXPECT_EQ(bgsm.mMaskWrites, version >= 6 ? 7 : bgsm.mMaskWrites);
                EXPECT_EQ(bgsm.mDiffuseMap, "textures/diffuse.dds");
                EXPECT_EQ(bgsm.mRootMaterialPath, "materials/root.bgsm");
                if (version >= 3)
                {
                    EXPECT_TRUE(bgsm.mTerrain);
                    EXPECT_EQ(bgsm.mTerrainParams, osg::Vec3f(21.f, 22.f, 23.f));
                }
                if (version >= 1)
                    EXPECT_TRUE(bgsm.mSkewSpecularAlpha);
                if (version >= 17)
                    EXPECT_EQ(bgsm.mDistanceFieldAlphaMap, "textures/field.dds");
                if (version < 10)
                    EXPECT_EQ(bgsm.mWetnessControlEnvMapScale, 14.f);
            }
        }

        TEST(BgsmFileTest, a_lighting_material_without_emittance_or_terrain_does_not_have_their_fields)
        {
            const MaterialFilePtr file = parse(stream(lighting(13, false, false).bytes()));

            const BGSMFile& bgsm = as<BGSMFile>(file);
            EXPECT_FALSE(bgsm.mEmitEnabled);
            EXPECT_FALSE(bgsm.mTerrain);
            EXPECT_EQ(bgsm.mEmittanceMult, 18.f);
            EXPECT_EQ(bgsm.mLumEmittance, 19.f);
            EXPECT_EQ(bgsm.mGrayscaleToPaletteScale, 20.f);
        }

        TEST(BgsmFileTest, the_third_version_of_a_lighting_material_has_four_bytes_before_the_terrain)
        {
            const MaterialFilePtr file = parse(stream(lighting(3).bytes()));

            EXPECT_EQ(as<BGSMFile>(file).mTerrainParams, osg::Vec3f(21.f, 22.f, 23.f));
        }

        TEST(BgsmFileTest, an_effect_material_has_the_common_fields_and_the_texture_paths)
        {
            const MaterialFilePtr file = parse(stream(effect(22).bytes()));

            ASSERT_NE(file, nullptr);
            EXPECT_EQ(file->mShaderType, ShaderType::Effect);
            EXPECT_EQ(file->mVersion, 22u);
            EXPECT_EQ(file->mUVScale, osg::Vec2f(2.f, 4.f));

            const BGEMFile& bgem = as<BGEMFile>(file);
            EXPECT_EQ(bgem.mBaseMap, "textures/base.dds");
            EXPECT_EQ(bgem.mGrayscaleMap, "textures/gray.dds");
            EXPECT_EQ(bgem.mEnvMap, "textures/env.dds");
            EXPECT_EQ(bgem.mNormalMap, "textures/normal.dds");
            EXPECT_EQ(bgem.mEnvMapMask, "textures/mask.dds");
            EXPECT_EQ(bgem.mSpecularMap, "textures/spec.dds");
            EXPECT_EQ(bgem.mLightingMap, "textures/lighting.dds");
            EXPECT_EQ(bgem.mGlowMap, "textures/glow.dds");
            EXPECT_EQ(bgem.mGlassRoughnessScratchMap, "textures/scratch.dds");
            EXPECT_EQ(bgem.mGlassDirtOverlayMap, "textures/dirt.dds");
            EXPECT_TRUE(bgem.mGlassEnabled);
            EXPECT_EQ(bgem.mGlassFresnelColor, osg::Vec3f(0.1f, 0.2f, 0.3f));
            EXPECT_EQ(bgem.mGlassBlurScaleBase, 0.4f);
            EXPECT_EQ(bgem.mGlassBlurScaleFactor, 0.5f);
            EXPECT_EQ(bgem.mGlassRefractionScaleBase, 0.6f);
            EXPECT_TRUE(bgem.mEnvMapEnabled);
            EXPECT_EQ(bgem.mEnvMapMaskScale, 0.7f);
            EXPECT_TRUE(bgem.mBlood);
            EXPECT_FALSE(bgem.mEffectLighting);
            EXPECT_TRUE(bgem.mFalloff);
            EXPECT_FALSE(bgem.mFalloffColor);
            EXPECT_TRUE(bgem.mGrayscaleToPaletteAlpha);
            EXPECT_FALSE(bgem.mSoft);
            EXPECT_EQ(bgem.mBaseColor, osg::Vec3f(0.4f, 0.5f, 0.6f));
            EXPECT_EQ(bgem.mBaseColorScale, 0.8f);
            EXPECT_EQ(bgem.mFalloffParams, osg::Vec4f(1.f, 2.f, 3.f, 4.f));
            EXPECT_EQ(bgem.mLightingInfluence, 0.9f);
            EXPECT_EQ(bgem.mEnvmapMinLOD, 9);
            EXPECT_EQ(bgem.mSoftDepth, 1.25f);
            EXPECT_EQ(bgem.mEmittanceColor, osg::Vec3f(2.f, 3.f, 4.f));
            EXPECT_EQ(bgem.mAdaptiveEmissiveExposureParams, osg::Vec3f(5.f, 6.f, 7.f));
            EXPECT_TRUE(bgem.mGlowMapEnabled);
            EXPECT_TRUE(bgem.mEffectPbrSpecular);
        }

        TEST(BgsmFileTest, every_version_of_an_effect_material_is_read_to_its_end)
        {
            for (std::uint32_t version : { 1u, 2u, 6u, 9u, 10u, 11u, 15u, 16u, 20u, 21u, 22u })
            {
                SCOPED_TRACE(version);
                const MaterialFilePtr file = parse(stream(effect(version).bytes()));
                const BGEMFile& bgem = as<BGEMFile>(file);
                EXPECT_EQ(bgem.mBaseMap, "textures/base.dds");
                EXPECT_EQ(bgem.mEnvMapMask, "textures/mask.dds");
                EXPECT_EQ(bgem.mSoftDepth, 1.25f);
                if (version >= 11)
                    EXPECT_EQ(bgem.mEmittanceColor, osg::Vec3f(2.f, 3.f, 4.f));
                if (version >= 15)
                    EXPECT_EQ(bgem.mAdaptiveEmissiveExposureParams, osg::Vec3f(5.f, 6.f, 7.f));
                if (version >= 16)
                    EXPECT_TRUE(bgem.mGlowMapEnabled);
                if (version >= 20)
                    EXPECT_TRUE(bgem.mEffectPbrSpecular);
            }
        }

        TEST(BgsmFileTest, an_effect_material_of_the_first_glass_version_has_no_blur_factor)
        {
            const MaterialFilePtr file = parse(stream(effect(21).bytes()));

            const BGEMFile& bgem = as<BGEMFile>(file);
            EXPECT_TRUE(bgem.mGlassEnabled);
            EXPECT_EQ(bgem.mGlassBlurScaleBase, 0.4f);
            EXPECT_EQ(bgem.mGlassBlurScaleFactor, 1.f);
            EXPECT_EQ(bgem.mGlassRefractionScaleBase, 0.6f);
        }

        TEST(BgsmFileTest, an_effect_material_without_glass_has_none_of_its_values)
        {
            const MaterialFilePtr file = parse(stream(effect(22, false).bytes()));

            const BGEMFile& bgem = as<BGEMFile>(file);
            EXPECT_FALSE(bgem.mGlassEnabled);
            EXPECT_EQ(bgem.mGlassBlurScaleBase, 0.f);
            EXPECT_EQ(bgem.mGlassBlurScaleFactor, 1.f);
            EXPECT_EQ(bgem.mSoftDepth, 1.25f);
            EXPECT_TRUE(bgem.mEffectPbrSpecular);
        }

        // The version 20 lighting material with another diffuse map in the file: the length that the file says, and
        // what follows it
        std::string withDiffuseMap(std::uint32_t length, const std::string& content)
        {
            const std::string original = lighting(20).bytes();
            Writer common;
            common.raw("BGSM");
            writeCommon(common, 20);
            const std::size_t offset = common.bytes().size();
            const std::size_t oldSize = 4 + std::strlen("textures/diffuse.dds") + 1;
            Writer replacement;
            replacement.u32(length).raw(content);
            return original.substr(0, offset) + replacement.bytes() + original.substr(offset + oldSize);
        }

        TEST(BgsmFileTest, a_string_ends_at_the_first_zero_and_may_have_no_zero_at_all)
        {
            const std::string path = "textures/other.dds";
            // The characters after a zero are not part of it, whatever the length says
            const std::string bytes
                = withDiffuseMap(static_cast<std::uint32_t>(path.size() + 4), path + std::string("\0xyz", 4));
            const MaterialFilePtr file = parse(stream(bytes));
            EXPECT_EQ(as<BGSMFile>(file).mDiffuseMap, path);
            EXPECT_EQ(as<BGSMFile>(file).mNormalMap, "textures/normal.dds");

            // One that has none is what the length says
            const MaterialFilePtr unterminated
                = parse(stream(withDiffuseMap(static_cast<std::uint32_t>(path.size()), path)));
            EXPECT_EQ(as<BGSMFile>(unterminated).mDiffuseMap, path);
            EXPECT_EQ(as<BGSMFile>(unterminated).mNormalMap, "textures/normal.dds");

            // An empty one is empty
            EXPECT_TRUE(as<BGSMFile>(parse(stream(withDiffuseMap(0, "")))).mDiffuseMap.empty());
        }

        TEST(BgsmFileTest, a_file_that_is_not_a_material_is_refused)
        {
            EXPECT_THROW(parse(stream("NOPE0000")), std::runtime_error);
            EXPECT_THROW(parse(stream("bgsm0000")), std::runtime_error);
            try
            {
                parse(stream("JUNK0000"));
                FAIL() << "No exception";
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_STREQ(e.what(), "Invalid material file");
            }
        }

        TEST(BgsmFileTest, a_file_that_is_cut_short_is_refused)
        {
            EXPECT_THROW(parse(stream("")), std::runtime_error);
            EXPECT_THROW(parse(stream("BG")), std::runtime_error);
            EXPECT_THROW(parse(stream("BGSM")), std::runtime_error);
            const std::string full = lighting(20).bytes();
            for (std::size_t length : { std::size_t{ 6 }, std::size_t{ 30 }, std::size_t{ 100 }, full.size() - 1 })
            {
                SCOPED_TRACE(length);
                EXPECT_THROW(parse(stream(full.substr(0, length))), std::runtime_error);
            }
            const std::string effectFull = effect(22).bytes();
            EXPECT_THROW(parse(stream(effectFull.substr(0, effectFull.size() - 2))), std::runtime_error);
        }

        TEST(BgsmFileTest, a_string_that_is_longer_than_a_path_can_be_is_refused)
        {
            try
            {
                parse(stream(withDiffuseMap(1025, std::string(1025, 'a'))));
                FAIL() << "No exception";
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_NE(std::string(e.what()).find("too large"), std::string::npos);
            }

            // The longest that is allowed is read
            const MaterialFilePtr file = parse(stream(withDiffuseMap(1024, std::string(1024, 'b'))));
            EXPECT_EQ(as<BGSMFile>(file).mDiffuseMap, std::string(1024, 'b'));

            // A string that is cut short is not read at all
            Writer cut;
            cut.raw("BGSM");
            writeCommon(cut, 20);
            cut.u32(100).raw("abc");
            EXPECT_THROW(parse(stream(cut.bytes())), std::runtime_error);
        }
    }
}
