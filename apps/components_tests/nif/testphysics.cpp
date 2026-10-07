#include <components/nif/physics.hpp>

#include <gtest/gtest.h>

namespace Nif
{
    namespace
    {
        using namespace testing;

        constexpr VFS::Path::NormalizedView path("test");

        void writeUInt8(std::uint8_t value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        void writeUInt16(std::uint16_t value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        void writeUInt32(std::uint32_t value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        void writeString(std::string_view value, std::ostream& stream)
        {
            stream.write(value.data(), value.size());
        }

        void writeUInt8SizedString(std::string_view value, std::ostream& stream)
        {
            writeUInt8(static_cast<std::uint8_t>(value.size() + 1), stream);
            writeString(value, stream);
            stream.put(0);
        }

        void writeUInt32SizedString(std::string_view value, std::ostream& stream)
        {
            writeUInt32(static_cast<std::uint32_t>(value.size() + 1), stream);
            writeString(value, stream);
            stream.put(0);
        }

        void writeFloat(float value, std::ostream& stream)
        {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        struct NifBhkRigidBodyTest : Test
        {
            NifBhkRigidBodyTest() { Nif::Reader::setWriteNifDebugLog(true); }
        };

        void writeZeros(std::size_t count, std::ostream& stream)
        {
            for (std::size_t i = 0; i < count; ++i)
                stream.put(0);
        }

        TEST_F(NifBhkRigidBodyTest, shouldReadTheRotationAsAHavokQuaternion)
        {
            NIFFile file(path);
            const ToUTF8::StatelessUtf8Encoder* const encoder = nullptr;
            Reader reader(file, encoder);

            std::ostringstream stream;

            writeString("Gamebryo File Format, Version 20.2.0.7\n", stream);
            writeUInt32(NIFStream::generateVersion(20, 2, 0, 7), stream);
            writeUInt8(1, stream); // little endian
            writeUInt32(11, stream); // user version
            writeUInt32(1, stream); // records
            writeUInt32(34, stream); // Fallout 3
            writeUInt8SizedString("author", stream);
            writeUInt8SizedString("process_script", stream);
            writeUInt8SizedString("export_script", stream);
            writeUInt16(1, stream);
            writeUInt32SizedString("bhkRigidBody", stream);
            writeUInt16(0, stream); // the type of the record
            writeUInt32(0, stream); // its size, which is not used
            writeUInt32(0, stream); // no strings
            writeUInt32(0, stream);
            writeUInt32(0, stream); // no groups

            // The body: the shape (none) and the Havok filter (the layer is the first byte), the info of a world
            // object and of an entity, then the rigid body info with the transform that is read
            writeUInt32(0xFFFFFFFF, stream);
            writeUInt8(1, stream);
            writeZeros(3, stream);
            writeZeros(4 + 1 + 3 + 12, stream);
            writeZeros(4, stream);
            writeZeros(4 + 4 + 4 + 4 + 4, stream);
            for (float value : { 1.f, 2.f, 3.f, 0.f }) // translation, in Havok units
                writeFloat(value, stream);
            for (float value : { 0.f, 0.f, 0.6f, 0.8f }) // rotation: x, y, z, w
                writeFloat(value, stream);
            writeZeros(16 * 2 + 48 + 16 + 4 * 3 + 4 + 4 + 4 * 3 + 4 + 12, stream);
            writeUInt32(0, stream); // no constraints
            writeUInt32(0, stream); // body flags
            writeUInt32(0, stream); // no roots

            const std::string buffer = stream.str();

            std::unique_ptr<std::istringstream> input = std::make_unique<std::istringstream>(buffer);
            input->exceptions(std::ios::failbit | std::ios_base::badbit);

            reader.parse(std::move(input));

            const Record* const record = reader.getRecord(0);
            ASSERT_NE(record, nullptr);
            const bhkRigidBody* const body = dynamic_cast<const bhkRigidBody*>(record);
            ASSERT_NE(body, nullptr);
            EXPECT_EQ(body->mHavokFilter.mLayer, 1);
            EXPECT_EQ(body->mInfo.mTranslation, osg::Vec4f(1, 2, 3, 0));
            EXPECT_EQ(body->mInfo.mRotation.x(), 0.f);
            EXPECT_EQ(body->mInfo.mRotation.y(), 0.f);
            EXPECT_EQ(body->mInfo.mRotation.z(), 0.6f);
            EXPECT_EQ(body->mInfo.mRotation.w(), 0.8f);
        }

        TEST_F(NifBhkRigidBodyTest, shouldReadTheMatrixOfATransformShapeWithItsTranslationInTheLastRow)
        {
            NIFFile file(path);
            const ToUTF8::StatelessUtf8Encoder* const encoder = nullptr;
            Reader reader(file, encoder);

            std::ostringstream stream;

            writeString("Gamebryo File Format, Version 20.2.0.7\n", stream);
            writeUInt32(NIFStream::generateVersion(20, 2, 0, 7), stream);
            writeUInt8(1, stream); // little endian
            writeUInt32(11, stream); // user version
            writeUInt32(1, stream); // records
            writeUInt32(34, stream); // Fallout 3
            writeUInt8SizedString("author", stream);
            writeUInt8SizedString("process_script", stream);
            writeUInt8SizedString("export_script", stream);
            writeUInt16(1, stream);
            writeUInt32SizedString("bhkTransformShape", stream);
            writeUInt16(0, stream); // the type of the record
            writeUInt32(0, stream); // its size, which is not used
            writeUInt32(0, stream); // no strings
            writeUInt32(0, stream);
            writeUInt32(0, stream); // no groups

            // The shape (none), the material, the radius and unused bytes, then the 16 floats of the transform as
            // Havok stores them: the three columns of the rotation, then the translation (in Havok units)
            writeUInt32(0xFFFFFFFF, stream);
            writeUInt32(0, stream);
            writeFloat(0.f, stream);
            writeZeros(8, stream);
            for (float value : { 0.f, 1.f, 0.f, 0.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 3.f, 4.f, 5.f, 1.f })
                writeFloat(value, stream);
            writeUInt32(0, stream); // no roots

            const std::string buffer = stream.str();

            std::unique_ptr<std::istringstream> input = std::make_unique<std::istringstream>(buffer);
            input->exceptions(std::ios::failbit | std::ios_base::badbit);

            reader.parse(std::move(input));

            const Record* const record = reader.getRecord(0);
            ASSERT_NE(record, nullptr);
            const bhkConvexTransformShape* const shape = dynamic_cast<const bhkConvexTransformShape*>(record);
            ASSERT_NE(shape, nullptr);
            EXPECT_EQ(shape->mTransform.getTrans(), osg::Vec3f(3, 4, 5));
            // A quarter turn about z, then the translation: the x axis ends up along y
            EXPECT_EQ(osg::Vec3f(1, 0, 0) * shape->mTransform, osg::Vec3f(3, 5, 5));
        }

        struct NifBhkRagdollTemplateTest : Test
        {
            NifBhkRagdollTemplateTest() { Nif::Reader::setWriteNifDebugLog(true); }
        };

        TEST_F(NifBhkRagdollTemplateTest, shouldParseRecord)
        {
            NIFFile file(path);
            const ToUTF8::StatelessUtf8Encoder* const encoder = nullptr;
            Reader reader(file, encoder);

            std::ostringstream stream;

            writeString("Gamebryo File Format, Version 20.2.0.7\n", stream);
            writeUInt32(NIFStream::generateVersion(20, 2, 0, 7), stream);

            constexpr std::uint8_t endianness = 1;
            writeUInt8(endianness, stream);

            constexpr std::uint32_t userVersion = 11;
            writeUInt32(userVersion, stream);

            constexpr std::uint32_t recordsCount = 2;
            writeUInt32(recordsCount, stream);

            constexpr std::uint32_t bsStreamVersion = 34;
            writeUInt32(bsStreamVersion, stream);

            writeUInt8SizedString("author", stream);
            writeUInt8SizedString("process_script", stream);
            writeUInt8SizedString("export_script", stream);

            constexpr std::uint16_t recordTypesCount = 2;
            writeUInt16(recordTypesCount, stream);

            constexpr std::uint16_t bhkRagdollTemplateTypeIndex = 0;
            writeUInt32SizedString("bhkRagdollTemplate", stream);

            constexpr std::uint16_t bhkRagdollTemplateDataTypeIndex = 1;
            writeUInt32SizedString("bhkRagdollTemplateData", stream);

            writeUInt16(bhkRagdollTemplateTypeIndex, stream);
            writeUInt16(bhkRagdollTemplateDataTypeIndex, stream);

            for (std::uint32_t i = 0; i < recordsCount; ++i)
            {
                constexpr std::uint32_t recordSize = 0;
                writeUInt32(recordSize, stream);
            }

            constexpr std::uint32_t stringsCount = 2;
            writeUInt32(stringsCount, stream);

            constexpr std::uint32_t maxStringSize = 0;
            writeUInt32(maxStringSize, stream);

            constexpr std::uint32_t bhkRagdollTemplateNameIndex = 0;
            writeUInt32SizedString("bhkRagdollTemplateName", stream);

            constexpr std::uint32_t bhkRagdollTemplateDataNameIndex = 1;
            writeUInt32SizedString("bhkRagdollTemplateDataName", stream);

            constexpr std::uint32_t groupsCount = 0;
            writeUInt32(groupsCount, stream);

            writeUInt32(bhkRagdollTemplateNameIndex, stream);

            constexpr std::uint32_t bonesCount = 1;
            writeUInt32(bonesCount, stream);

            constexpr std::uint32_t bhkRagdollTemplateDataIndex = 1;
            writeUInt32(bhkRagdollTemplateDataIndex, stream);

            writeUInt32(bhkRagdollTemplateDataNameIndex, stream);

            constexpr float mass = 1.1f;
            writeFloat(mass, stream);

            constexpr float restitution = 2.2f;
            writeFloat(restitution, stream);

            constexpr float friction = 3.3f;
            writeFloat(friction, stream);

            constexpr float radius = 4.4f;
            writeFloat(radius, stream);

            constexpr std::uint32_t havokMaterial = 42;
            writeUInt32(havokMaterial, stream);

            constexpr std::uint32_t constraintsCount = 0;
            writeUInt32(constraintsCount, stream);

            constexpr std::uint32_t rootsCount = 0;
            writeUInt32(rootsCount, stream);

            const std::string buffer = stream.str();

            std::unique_ptr<std::istringstream> input = std::make_unique<std::istringstream>(buffer);
            input->exceptions(std::ios::failbit | std::ios_base::badbit);

            reader.parse(std::move(input));

            {
                const Record* const record = reader.getRecord(0);
                ASSERT_NE(record, nullptr);
                EXPECT_EQ(record->mRecordIndex, 0);
                EXPECT_EQ(record->mRecordName, "bhkRagdollTemplate");
                EXPECT_EQ(record->mRecordType, RC_bhkRagdollTemplate);

                const bhkRagdollTemplate* const typed = dynamic_cast<const bhkRagdollTemplate*>(record);
                ASSERT_NE(typed, nullptr);
                EXPECT_EQ(typed->mName, "bhkRagdollTemplateName");
                EXPECT_EQ(typed->mBones.size(), 1);
                EXPECT_NE(typed->mBones[0].getPtr(), nullptr);
            }

            {
                const Record* const record = reader.getRecord(1);
                ASSERT_NE(record, nullptr);
                EXPECT_EQ(record->mRecordIndex, 1);
                EXPECT_EQ(record->mRecordName, "bhkRagdollTemplateData");
                EXPECT_EQ(record->mRecordType, RC_bhkRagdollTemplateData);

                const bhkRagdollTemplateData* const typed = dynamic_cast<const bhkRagdollTemplateData*>(record);
                ASSERT_NE(typed, nullptr);
                EXPECT_EQ(typed->mName, "bhkRagdollTemplateDataName");
                EXPECT_EQ(typed->mMass, mass);
                EXPECT_EQ(typed->mRestitution, restitution);
                EXPECT_EQ(typed->mFriction, friction);
                EXPECT_EQ(typed->mRadius, radius);
                EXPECT_EQ(typed->mHavokMaterial.mMaterial, havokMaterial);
                EXPECT_EQ(typed->mConstraints.size(), 0);
            }
        }
    }
}
