#include "../nif/node.hpp"
#include "../nif/sequence.hpp"

#include <components/misc/osguservalues.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/node.hpp>
#include <components/nif/property.hpp>
#include <components/nifosg/nifloader.hpp>
#include <components/resource/bgsmfilemanager.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/sceneutil/keyframe.hpp>
#include <components/sceneutil/serialize.hpp>
#include <components/sceneutil/userdata.hpp>
#include <components/vfs/manager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <osg/AlphaFunc>
#include <osg/Geometry>
#include <osg/Image>
#include <osgDB/Registry>

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    using namespace testing;
    using namespace NifOsg;
    using namespace Nif::Testing;

    constexpr VFS::Path::NormalizedView testNif("test.nif");

    struct BaseNifOsgLoaderTest
    {
        VFS::Manager mVfs;
        Resource::ImageManager mImageManager{ &mVfs, 0 };
        Resource::BgsmFileManager mMaterialManager{ &mVfs, 0 };
        const osgDB::ReaderWriter* mReaderWriter = osgDB::Registry::instance()->getReaderWriterForExtension("osgt");
        osg::ref_ptr<osgDB::Options> mOptions = new osgDB::Options;

        BaseNifOsgLoaderTest()
        {
            SceneUtil::registerSerializers();

            if (mReaderWriter == nullptr)
                throw std::runtime_error("osgt reader writer is not found");

            mOptions->setPluginStringData("fileType", "Ascii");
            mOptions->setPluginStringData("WriteImageHint", "UseExternal");
        }

        std::string serialize(const osg::Node& node) const
        {
            std::stringstream stream;
            mReaderWriter->writeNode(node, stream, mOptions);
            std::string result;
            for (std::string line; std::getline(stream, line);)
            {
                if (line.starts_with('#'))
                    continue;
                line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
                result += line;
                result += '\n';
            }
            return result;
        }
    };

    struct NifOsgLoaderTest : Test, BaseNifOsgLoaderTest
    {
    };

    TEST_F(NifOsgLoaderTest, shouldLoadFileWithDefaultNode)
    {
        Nif::NiAVObject node;
        init(node);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        EXPECT_EQ(serialize(*result), R"(
osg::Group {
  UniqueID 1
  DataVariance STATIC
  UserDataContainer TRUE {
    osg::DefaultUserDataContainer {
      UniqueID 2
      UDC_UserObjects 1 {
        osg::StringValueObject {
          UniqueID 3
          Name "fileHash"
        }
      }
    }
  }
  Children 1 {
    osg::Group {
      UniqueID 4
      DataVariance STATIC
      UserDataContainer TRUE {
        osg::DefaultUserDataContainer {
          UniqueID 5
          UDC_UserObjects 1 {
            osg::UIntValueObject {
              UniqueID 6
              Name "recordIndex"
              Value 4294967295
            }
          }
        }
      }
    }
  }
}
)");
    }

    // The hair, eyes and hats of the characters of the games are not skinned: the file names the bone they are put on
    TEST_F(NifOsgLoaderTest, keepsTheNameOfTheNodeThatAModelAsksToBeAttachedTo)
    {
        Nif::NiStringExtraData extra;
        init(extra);
        extra.mName = "Prn";
        extra.mData = "Bip01 Head";
        extra.mRecordType = Nif::RC_NiStringExtraData;
        Nif::NiNode node;
        init(node);
        node.mExtra = Nif::ExtraPtr(&extra);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        const osg::Group* root = result->asGroup();
        ASSERT_NE(root, nullptr);
        ASSERT_EQ(root->getNumChildren(), 1u);
        std::string attachTo;
        EXPECT_TRUE(root->getChild(0)->getUserValue(Misc::OsgUserValues::sAttachTo, attachTo));
        EXPECT_EQ(attachTo, "Bip01 Head");
    }

    // The turn of that node is folded into the meshes when the model is optimized: a part is put on its bone by it
    TEST_F(NifOsgLoaderTest, keepsTheTurnOfTheNodeThatAModelAsksToBeAttachedTo)
    {
        Nif::NiStringExtraData extra;
        init(extra);
        extra.mName = "Prn";
        extra.mData = "Bip01 Head";
        extra.mRecordType = Nif::RC_NiStringExtraData;
        Nif::NiNode node;
        init(node);
        node.mExtra = Nif::ExtraPtr(&extra);
        // A quarter of a turn about z, as a file writes it (acting on column vectors): x goes to y
        node.mTransform.mRotation.mValues[0][0] = 0.f;
        node.mTransform.mRotation.mValues[0][1] = -1.f;
        node.mTransform.mRotation.mValues[1][0] = 1.f;
        node.mTransform.mRotation.mValues[1][1] = 0.f;
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        const osg::Group* root = result->asGroup();
        ASSERT_NE(root, nullptr);
        ASSERT_EQ(root->getNumChildren(), 1u);
        osg::Quat turn;
        ASSERT_TRUE(root->getChild(0)->getUserValue(Misc::OsgUserValues::sAttachRotation, turn));
        const osg::Vec3d moved = turn * osg::Vec3d(1.0, 0.0, 0.0);
        EXPECT_NEAR(moved.x(), 0.0, 1e-5);
        EXPECT_NEAR(moved.y(), 1.0, 1e-5);
        EXPECT_NEAR(moved.z(), 0.0, 1e-5);
    }

    // The female hair files turn their root and their shapes by the same half turn: the two cancel, and the part is
    // put on its bone by what is left
    struct AttachedShapeFile
    {
        AttachedShapeFile(float rootTurn, float shapeTurn)
        {
            mData.mRecordType = Nif::RC_NiTriShapeData;
            mData.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(1, 1, 0) };
            mData.mNumTriangles = 1;
            mData.mTriangles = { 0, 1, 2 };
            Nif::Testing::init(mShape);
            mShape.mRecordType = Nif::RC_BSSegmentedTriShape;
            mShape.mData = Nif::NiGeometryDataPtr(&mData);
            mShape.mShaderProperty = Nif::BSShaderPropertyPtr(nullptr);
            mShape.mAlphaProperty = Nif::NiAlphaPropertyPtr(nullptr);
            turnAboutZ(mShape, shapeTurn);
            Nif::Testing::init(mExtra);
            mExtra.mName = "Prn";
            mExtra.mData = "Bip01 Head";
            mExtra.mRecordType = Nif::RC_NiStringExtraData;
            Nif::Testing::init(mRoot);
            mRoot.mExtra = Nif::ExtraPtr(&mExtra);
            mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&mShape) };
            turnAboutZ(mRoot, rootTurn);
        }

        // By a number of quarter turns about z, as a file writes it (acting on column vectors): x goes to y for one
        static void turnAboutZ(Nif::NiAVObject& object, float quarterTurns)
        {
            object.mTransform.mRotation.mValues[0][0] = 0.f;
            object.mTransform.mRotation.mValues[0][1] = -quarterTurns;
            object.mTransform.mRotation.mValues[1][0] = quarterTurns;
            object.mTransform.mRotation.mValues[1][1] = 0.f;
        }

        Nif::NiTriShapeData mData;
        Nif::BSSegmentedTriShape mShape;
        Nif::NiStringExtraData mExtra;
        Nif::NiNode mRoot;
    };

    TEST_F(NifOsgLoaderTest, keepsTheTurnOfTheShapeUnderTheNodeThatAModelAsksToBeAttachedToAsWell)
    {
        // A quarter turn of the root and the same one of the shape: a half turn, x goes to -x
        AttachedShapeFile same(1.f, 1.f);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&same.mRoot);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        const osg::Group* root = result->asGroup();
        ASSERT_NE(root, nullptr);
        ASSERT_EQ(root->getNumChildren(), 1u);
        osg::Quat turn;
        ASSERT_TRUE(root->getChild(0)->getUserValue(Misc::OsgUserValues::sAttachRotation, turn));
        const osg::Vec3d moved = turn * osg::Vec3d(1.0, 0.0, 0.0);
        EXPECT_NEAR(moved.x(), -1.0, 1e-5);
        EXPECT_NEAR(moved.y(), 0.0, 1e-5);
        EXPECT_NEAR(moved.z(), 0.0, 1e-5);
    }

    TEST_F(NifOsgLoaderTest, keepsNoTurnWhenTheRootAndTheShapeOfAModelTurnOppositeWays)
    {
        // The two cancel: the vertices are used as they are stored
        AttachedShapeFile opposite(1.f, -1.f);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&opposite.mRoot);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        const osg::Group* root = result->asGroup();
        ASSERT_NE(root, nullptr);
        ASSERT_EQ(root->getNumChildren(), 1u);
        osg::Quat turn;
        ASSERT_TRUE(root->getChild(0)->getUserValue(Misc::OsgUserValues::sAttachRotation, turn));
        const osg::Vec3d moved = turn * osg::Vec3d(1.0, 0.0, 0.0);
        EXPECT_NEAR(moved.x(), 1.0, 1e-5);
        EXPECT_NEAR(moved.y(), 0.0, 1e-5);
        EXPECT_NEAR(moved.z(), 0.0, 1e-5);
    }

    TEST_F(NifOsgLoaderTest, doesNotNameANodeToAttachToForOtherExtraData)
    {
        Nif::NiStringExtraData extra;
        init(extra);
        extra.mName = "UPB";
        extra.mData = "BSBoneLOD#Bone#0#";
        extra.mRecordType = Nif::RC_NiStringExtraData;
        Nif::NiNode node;
        init(node);
        node.mExtra = Nif::ExtraPtr(&extra);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        const osg::Group* root = result->asGroup();
        ASSERT_NE(root, nullptr);
        ASSERT_EQ(root->getNumChildren(), 1u);
        std::string attachTo;
        EXPECT_FALSE(root->getChild(0)->getUserValue(Misc::OsgUserValues::sAttachTo, attachTo));
    }

    // The shape of the blocks of distant objects of the games: a triangle shape with a list of segments
    TEST_F(NifOsgLoaderTest, drawsTheTrianglesOfASegmentedTriShape)
    {
        Nif::NiTriShapeData data;
        data.mRecordType = Nif::RC_NiTriShapeData;
        data.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(1, 1, 0) };
        data.mNumTriangles = 1;
        data.mTriangles = { 0, 1, 2 };
        Nif::BSSegmentedTriShape shape;
        init(shape);
        shape.mRecordType = Nif::RC_BSSegmentedTriShape;
        shape.mData = Nif::NiGeometryDataPtr(&data);
        shape.mShaderProperty = Nif::BSShaderPropertyPtr(nullptr);
        shape.mAlphaProperty = Nif::NiAlphaPropertyPtr(nullptr);

        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&shape);
        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        struct FindGeometry : osg::NodeVisitor
        {
            FindGeometry()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }
            void apply(osg::Geometry& geometry) override { mGeometry = &geometry; }
            const osg::Geometry* mGeometry = nullptr;
        } find;
        result->accept(find);
        ASSERT_NE(find.mGeometry, nullptr);
        ASSERT_EQ(find.mGeometry->getNumPrimitiveSets(), 1u);
        EXPECT_EQ(find.mGeometry->getPrimitiveSet(0)->getNumIndices(), 3u);
    }

    // The trees of a block of distant objects are pictures with see-through parts in the atlas, and the file asks for
    // no alpha test: the flag of the shader says what the games test
    struct LodShape
    {
        Nif::NiTriShapeData mData;
        Nif::BSSegmentedTriShape mShape;
        Nif::BSShaderPPLightingProperty mProperty;

        explicit LodShape(std::uint32_t flags2)
        {
            mData.mRecordType = Nif::RC_NiTriShapeData;
            mData.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(1, 1, 0) };
            mData.mNumTriangles = 1;
            mData.mTriangles = { 0, 1, 2 };
            init(mShape);
            mShape.mRecordType = Nif::RC_BSSegmentedTriShape;
            mShape.mData = Nif::NiGeometryDataPtr(&mData);
            mShape.mShaderProperty = Nif::BSShaderPropertyPtr(&mProperty);
            mShape.mAlphaProperty = Nif::NiAlphaPropertyPtr(nullptr);
            mProperty.mRecordType = Nif::RC_BSShaderPPLightingProperty;
            mProperty.mTextureSet = nullptr;
            mProperty.mController = nullptr;
            mProperty.mType = static_cast<std::uint32_t>(Nif::BSShaderType::ShaderType_Default);
            mProperty.mShaderFlags2 = flags2;
        }
    };

    const osg::AlphaFunc* alphaTestOf(const osg::Node& root)
    {
        // Whichever node of the shape holds the state
        struct FindAlphaFunc : osg::NodeVisitor
        {
            FindAlphaFunc()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }
            void apply(osg::Node& node) override
            {
                if (mFound == nullptr && node.getStateSet() != nullptr)
                    mFound = dynamic_cast<const osg::AlphaFunc*>(
                        node.getStateSet()->getAttribute(osg::StateAttribute::ALPHAFUNC));
                traverse(node);
            }
            const osg::AlphaFunc* mFound = nullptr;
        } find;
        const_cast<osg::Node&>(root).accept(find);
        return find.mFound;
    }

    TEST_F(NifOsgLoaderTest, testsTheAlphaOfTheShapeOfABlockOfDistantObjects)
    {
        LodShape lod(Nif::BSSFlag2_LODBuilding | Nif::BSSFlag2_DepthWrite);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&lod.mShape);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        const osg::AlphaFunc* test = alphaTestOf(*result);
        ASSERT_NE(test, nullptr);
        EXPECT_EQ(test->getFunction(), osg::AlphaFunc::GREATER);
        EXPECT_NEAR(test->getReferenceValue(), 0.5f, 0.01f);
    }

    TEST_F(NifOsgLoaderTest, testsNoAlphaOfAShapeThatIsNotABlockOfDistantObjects)
    {
        LodShape other(Nif::BSSFlag2_DepthWrite);
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&other.mShape);

        const osg::ref_ptr<osg::Node> result = Loader::load(file, &mImageManager, &mMaterialManager);

        EXPECT_EQ(alphaTestOf(*result), nullptr);
    }

    // One 4 by 4 block of DXT1 in which every texel is see-through: the first colour is not above the second and every
    // index is 3
    constexpr std::array<unsigned char, 8> seeThroughBlock = { 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff };

    osg::ref_ptr<osg::Image> makeDxt1Image(GLenum format)
    {
        osg::ref_ptr<osg::Image> image(new osg::Image);
        unsigned char* data = new unsigned char[seeThroughBlock.size()];
        std::copy(seeThroughBlock.begin(), seeThroughBlock.end(), data);
        image->setImage(4, 4, 1, format, format, GL_UNSIGNED_BYTE, data, osg::Image::USE_NEW_DELETE);
        return image;
    }

    TEST(NifOsgDxt1Test, givesADxt1PictureThatHasNoAlphaTheFormatThatHasIt)
    {
        const osg::ref_ptr<osg::Image> image = makeDxt1Image(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
        EXPECT_FALSE(image->isImageTranslucent());

        EXPECT_TRUE(allowTransparencyOfDxt1(*image));

        EXPECT_EQ(image->getPixelFormat(), static_cast<GLenum>(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
        EXPECT_EQ(image->getInternalTextureFormat(), static_cast<GLint>(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
        EXPECT_TRUE(image->isImageTranslucent());
        // the data is as it was
        EXPECT_EQ(static_cast<const unsigned char*>(image->getDataPointer())[4], 0xff);
        EXPECT_EQ(image->s(), 4);
        EXPECT_EQ(image->t(), 4);
    }

    TEST(NifOsgDxt1Test, leavesAPictureThatHasAlphaOrIsOfAnotherFormatAsItIs)
    {
        const osg::ref_ptr<osg::Image> withAlpha = makeDxt1Image(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
        EXPECT_FALSE(allowTransparencyOfDxt1(*withAlpha));
        EXPECT_EQ(withAlpha->getPixelFormat(), static_cast<GLenum>(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));

        osg::ref_ptr<osg::Image> plain(new osg::Image);
        plain->allocateImage(2, 2, 1, GL_RGB, GL_UNSIGNED_BYTE);
        EXPECT_FALSE(allowTransparencyOfDxt1(*plain));
        EXPECT_EQ(plain->getPixelFormat(), static_cast<GLenum>(GL_RGB));

        // asked twice, the second time there is nothing to change
        const osg::ref_ptr<osg::Image> once = makeDxt1Image(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
        EXPECT_TRUE(allowTransparencyOfDxt1(*once));
        EXPECT_FALSE(allowTransparencyOfDxt1(*once));
    }

    std::string formatOsgNodeForBSShaderProperty(std::string_view shaderPrefix)
    {
        std::ostringstream oss;
        oss << R"(
osg::Group {
  UniqueID 1
  DataVariance STATIC
  UserDataContainer TRUE {
    osg::DefaultUserDataContainer {
      UniqueID 2
      UDC_UserObjects 1 {
        osg::StringValueObject {
          UniqueID 3
          Name "fileHash"
        }
      }
    }
  }
  Children 1 {
    osg::Group {
      UniqueID 4
      DataVariance STATIC
      UserDataContainer TRUE {
        osg::DefaultUserDataContainer {
          UniqueID 5
          UDC_UserObjects 2 {
            osg::UIntValueObject {
              UniqueID 6
              Name "recordIndex"
              Value 4294967295
            }
            osg::StringValueObject {
              UniqueID 7
              Name "shaderPrefix"
              Value ")"
            << shaderPrefix << R"("
            }
          }
        }
      }
      StateSet TRUE {
        osg::StateSet {
          UniqueID 8
        }
      }
    }
  }
}
)";
        return oss.str();
    }

    std::string formatOsgNodeForBSLightingShaderProperty(std::string_view shaderPrefix)
    {
        std::ostringstream oss;
        oss << R"(
osg::Group {
  UniqueID 1
  DataVariance STATIC
  UserDataContainer TRUE {
    osg::DefaultUserDataContainer {
      UniqueID 2
      UDC_UserObjects 1 {
        osg::StringValueObject {
          UniqueID 3
          Name "fileHash"
        }
      }
    }
  }
  Children 1 {
    osg::Group {
      UniqueID 4
      DataVariance STATIC
      UserDataContainer TRUE {
        osg::DefaultUserDataContainer {
          UniqueID 5
          UDC_UserObjects 2 {
            osg::UIntValueObject {
              UniqueID 6
              Name "recordIndex"
              Value 4294967295
            }
            osg::StringValueObject {
              UniqueID 7
              Name "shaderPrefix"
              Value ")"
            << shaderPrefix << R"("
            }
          }
        }
      }
      StateSet TRUE {
        osg::StateSet {
          UniqueID 8
          ModeList 1 {
            GL_DEPTH_TEST ON
          }
          AttributeList 1 {
            osg::Depth {
              UniqueID 9
              Function LEQUAL
            }
            Value OFF
          }
        }
      }
    }
  }
}
)";
        return oss.str();
    }

    struct ShaderPrefixParams
    {
        unsigned int mShaderType;
        std::string_view mExpectedShaderPrefix;
    };

    struct NifOsgLoaderBSShaderPrefixTest : TestWithParam<ShaderPrefixParams>, BaseNifOsgLoaderTest
    {
        static constexpr std::array sParams = {
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSShaderType::ShaderType_Default), "bs/default" },
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSShaderType::ShaderType_NoLighting), "bs/nolighting" },
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSShaderType::ShaderType_Tile), "bs/default" },
            ShaderPrefixParams{ std::numeric_limits<unsigned int>::max(), "bs/default" },
        };
    };

    TEST_P(NifOsgLoaderBSShaderPrefixTest, shouldAddShaderPrefix)
    {
        Nif::NiAVObject node;
        init(node);
        Nif::BSShaderPPLightingProperty property;
        property.mRecordType = Nif::RC_BSShaderPPLightingProperty;
        property.mTextureSet = nullptr;
        property.mController = nullptr;
        property.mType = GetParam().mShaderType;
        node.mProperties.push_back(Nif::RecordPtrT<Nif::NiProperty>(&property));
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        EXPECT_EQ(serialize(*result), formatOsgNodeForBSShaderProperty(GetParam().mExpectedShaderPrefix));
    }

    INSTANTIATE_TEST_SUITE_P(Params, NifOsgLoaderBSShaderPrefixTest, ValuesIn(NifOsgLoaderBSShaderPrefixTest::sParams));

    struct NifOsgLoaderBSLightingShaderPrefixTest : TestWithParam<ShaderPrefixParams>, BaseNifOsgLoaderTest
    {
        static constexpr std::array sParams = {
            ShaderPrefixParams{
                static_cast<unsigned int>(Nif::BSLightingShaderType::ShaderType_Default), "bs/default" },
            ShaderPrefixParams{ static_cast<unsigned int>(Nif::BSLightingShaderType::ShaderType_Cloud), "bs/default" },
            ShaderPrefixParams{ std::numeric_limits<unsigned int>::max(), "bs/default" },
        };
    };

    TEST_P(NifOsgLoaderBSLightingShaderPrefixTest, shouldAddShaderPrefix)
    {
        Nif::NiAVObject node;
        init(node);
        Nif::BSLightingShaderProperty property;
        property.mRecordType = Nif::RC_BSLightingShaderProperty;
        property.mTextureSet = nullptr;
        property.mController = nullptr;
        property.mType = GetParam().mShaderType;
        property.mShaderFlags1 |= Nif::BSShaderFlags1::BSSFlag1_DepthTest;
        property.mShaderFlags2 |= Nif::BSShaderFlags2::BSSFlag2_DepthWrite;
        node.mProperties.push_back(Nif::RecordPtrT<Nif::NiProperty>(&property));
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&node);
        auto result = Loader::load(file, &mImageManager, &mMaterialManager);
        EXPECT_EQ(serialize(*result), formatOsgNodeForBSLightingShaderProperty(GetParam().mExpectedShaderPrefix));
    }

    INSTANTIATE_TEST_SUITE_P(
        Params, NifOsgLoaderBSLightingShaderPrefixTest, ValuesIn(NifOsgLoaderBSLightingShaderPrefixTest::sParams));
    // A door or a switch holds its animations in the file and a script plays them by name (Open, Close).
    struct NifOsgLoaderEmbeddedSequencesTest : Test, BaseNifOsgLoaderTest
    {
        Nif::NiNode mRoot;
        Nif::NiNode mGear;
        Nif::NiControllerManager mManager;
        Nif::NiKeyframeController mGearController;
        Nif::NIFFile mFile{ testNif };

        NifOsgLoaderEmbeddedSequencesTest()
        {
            init(mRoot);
            mRoot.mRecordType = Nif::RC_NiNode;
            mRoot.mName = "Door";
            init(mGear);
            mGear.mRecordType = Nif::RC_NiNode;
            mGear.mName = "Gear";
            mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&mGear) };
            mGear.mParents.push_back(&mRoot);

            init(mManager);
            mManager.mRecordType = Nif::RC_NiControllerManager;
            mManager.mCumulative = false;
            mManager.mObjectPalette = Nif::NiDefaultAVObjectPalettePtr(nullptr);
            mRoot.mController = Nif::NiTimeControllerPtr(&mManager);

            init(mGearController);
            mGearController.mRecordType = Nif::RC_NiKeyframeController;
            mGearController.mManagerControlled = true;
            mGearController.mData = Nif::NiKeyframeDataPtr(nullptr);
            mGearController.mInterpolator = Nif::NiInterpolatorPtr(nullptr);
            mGear.mController = Nif::NiTimeControllerPtr(&mGearController);

            mFile.mRoots.push_back(&mRoot);
        }

        void addSequenceToManager(const std::string& name, float stop)
        {
            auto& sequence
                = Nif::Testing::addSequence(mFile, name, 0.f, stop, Nif::NiTimeController::ExtrapolationMode::Constant,
                    { { "Gear", { { 0.f, osg::Vec3f(0, 0, 0) }, { stop, osg::Vec3f(0, 0, 10) } } } });
            mManager.mSequences.push_back(Nif::RecordPtrT<Nif::NiControllerSequence>(&sequence));
        }

        const SceneUtil::EmbeddedSequences* sequencesOf(osg::Node& root)
        {
            return SceneUtil::findUserData<SceneUtil::EmbeddedSequences>(root);
        }

        static const osg::Node* find(const osg::Node& node, const std::string& name)
        {
            if (node.getName() == name)
                return &node;
            if (const osg::Group* group = node.asGroup())
                for (unsigned int i = 0; i < group->getNumChildren(); ++i)
                    if (const osg::Node* found = find(*group->getChild(i), name))
                        return found;
            return nullptr;
        }
    };

    TEST_F(NifOsgLoaderEmbeddedSequencesTest, keepsTheSequencesOfTheManagerAsGroupsOfTheirNames)
    {
        addSequenceToManager("Open", 5.f);
        addSequenceToManager("Close", 4.f);

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        const SceneUtil::EmbeddedSequences* sequences = sequencesOf(*result);
        ASSERT_NE(sequences, nullptr);
        ASSERT_EQ(sequences->size(), 2u);
        EXPECT_THAT((*sequences)[0]->mTextKeys.getGroups(), ElementsAre("open"));
        EXPECT_THAT((*sequences)[1]->mTextKeys.getGroups(), ElementsAre("close"));
        EXPECT_EQ((*sequences)[0]->mTextKeys.begin()->first, 0.f);
        EXPECT_EQ((*sequences)[0]->mTextKeys.rbegin()->first, 5.f);
        EXPECT_EQ((*sequences)[1]->mTextKeys.rbegin()->first, 4.f);
        EXPECT_EQ((*sequences)[0]->mKeyframeControllers.count("Gear"), 1u);
    }

    TEST_F(NifOsgLoaderEmbeddedSequencesTest, keepsOnlyTheFirstOfSequencesOfOneName)
    {
        addSequenceToManager("Open", 5.f);
        addSequenceToManager("open", 9.f);

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        const SceneUtil::EmbeddedSequences* sequences = sequencesOf(*result);
        ASSERT_NE(sequences, nullptr);
        ASSERT_EQ(sequences->size(), 1u);
        EXPECT_EQ((*sequences)[0]->mTextKeys.rbegin()->first, 5.f);
    }

    TEST_F(NifOsgLoaderEmbeddedSequencesTest, takesTheSequencesOfAManagerThatIsNotActive)
    {
        addSequenceToManager("Open", 5.f);
        mManager.mFlags = 0;

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        const SceneUtil::EmbeddedSequences* sequences = sequencesOf(*result);
        ASSERT_NE(sequences, nullptr);
        EXPECT_EQ(sequences->size(), 1u);
    }

    TEST_F(NifOsgLoaderEmbeddedSequencesTest, addsNothingForAFileWithoutAManager)
    {
        mRoot.mController = Nif::NiTimeControllerPtr(nullptr);

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        EXPECT_EQ(sequencesOf(*result), nullptr);
    }

    // The nodes that a sequence moves must stay nodes of their own and not be merged into their parent.
    TEST_F(NifOsgLoaderEmbeddedSequencesTest, keepsTheNodeThatTheManagerMoves)
    {
        addSequenceToManager("Open", 5.f);

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        const osg::Node* gear = find(*result, "Gear");
        ASSERT_NE(gear, nullptr);
        EXPECT_EQ(gear->getDataVariance(), osg::Object::DYNAMIC);
    }

    // The lock of the lockpicking screen: the node carries no controller of its own (a NiMultiTargetTransformController
    // on the root lists it, and the engine does not run that one), only the sequence names it.
    TEST_F(NifOsgLoaderEmbeddedSequencesTest, keepsTheNodeThatOnlyTheSequenceNames)
    {
        addSequenceToManager("Open", 5.f);
        mGear.mController = Nif::NiTimeControllerPtr(nullptr);

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        const osg::Node* gear = find(*result, "Gear");
        ASSERT_NE(gear, nullptr);
        EXPECT_EQ(gear->getDataVariance(), osg::Object::DYNAMIC);
    }

    TEST_F(NifOsgLoaderEmbeddedSequencesTest, leavesANodeThatNoSequenceNamesToTheOptimizer)
    {
        addSequenceToManager("Open", 5.f);
        mGear.mController = Nif::NiTimeControllerPtr(nullptr);
        mGear.mName = "Other";

        const osg::ref_ptr<osg::Node> result = Loader::load(mFile, &mImageManager, &mMaterialManager);

        const osg::Node* other = find(*result, "Other");
        ASSERT_NE(other, nullptr);
        EXPECT_NE(other->getDataVariance(), osg::Object::DYNAMIC);
    }
}
