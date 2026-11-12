#ifndef GAME_RENDER_ESM4NPCANIMATION_H
#define GAME_RENDER_ESM4NPCANIMATION_H

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <osg/Image>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include "falloutactoranimation.hpp"

namespace ESM4
{
    struct Npc;
}

namespace OFClass
{
    struct FalloutNpcPart;
}

namespace OFRender
{
    class ESM4NpcAnimation : public FalloutActorAnimation
    {
    public:
        /// With `firstPerson` the character is the arms that the player sees through their own eyes: the skeleton of the
        /// first person with the hands (and the Pip-Boy glove) of the character on it, and the animations that sit beside
        /// that skeleton; the rest of the body is left out.
        ESM4NpcAnimation(const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode,
            Resource::ResourceSystem* resourceSystem, bool firstPerson = false);
        ~ESM4NpcAnimation() override;

    private:
        /// The pictures that the look of a Fallout character is made of, which are the same for every part of it
        struct FalloutLookImages
        {
            /// The texture of the face that the editor wrote for the character: mid grey, the colour of its skin on top
            /// of the one of its race (empty when it has none)
            osg::ref_ptr<osg::Image> mFaceTint;
            /// The same for the body, which the hands take the average of
            osg::ref_ptr<osg::Image> mBodyTint;
            /// The coefficients of FGTS of the face (those of the race for the sex, and the character's on top): the
            /// colour of a face that has no texture written by the editor is made from them. Empty when the
            /// character has none.
            std::vector<float> mTone;
            /// The record of the character (for the log only)
            std::uint32_t mFormId = 0;
            /// The picture of the eyes that the character has
            osg::ref_ptr<osg::Image> mEyes;
            /// Whether the hair is the one that fits under a hat
            bool mHat = false;
            /// The colour of the hair, each channel from 0 to 1; not set when the character has none
            std::optional<osg::Vec3f> mHairColour;
        };

        /// Gives a part of a Fallout character its look: the shape of the morph file beside the model, moved by the
        /// coefficients (FGGS and FGGA of the race and of the character together), and the picture of its skin, hair or
        /// eyes
        struct FalloutPartLook
        {
            /// The meshes of the part that the morph moved
            std::size_t mMorphed = 0;
            /// Where the picture of the part comes from, empty when it keeps that of its model
            std::string mTexture;
        };
        FalloutPartLook shapeFalloutPart(osg::Node& part, const OFClass::FalloutNpcPart& npcPart,
            const std::vector<float>& symmetric, const std::vector<float>& asymmetric, const FalloutLookImages& images);

        /// The picture in the files of the game (a path under textures); null when there is none or it is not there
        osg::ref_ptr<osg::Image> loadFalloutImage(const std::string& path);

        // Works for FO3/FONV/TES5
        void insertHeadParts(const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes);

        void updateParts();
        void updatePartsTES4(const ESM4::Npc& traits);
        void updatePartsFallout(const ESM4::Npc& traits);
        void updatePartsTES5(const ESM4::Npc& traits);
    };
}

#endif // GAME_RENDER_ESM4NPCANIMATION_H
