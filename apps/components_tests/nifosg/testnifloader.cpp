#include "../nif/node.hpp"
#include "../nif/sequence.hpp"

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

#include <osg/Geometry>
#include <osgDB/Registry>

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
}
