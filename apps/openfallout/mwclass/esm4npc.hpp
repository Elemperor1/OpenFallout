#ifndef GAME_MWCLASS_ESM4ACTOR_H
#define GAME_MWCLASS_ESM4ACTOR_H

#include <functional>
#include <vector>

#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/registeredclass.hpp"

#include "esm4base.hpp"

namespace ESM4
{
    struct Armor;
    struct Hair;
    struct HeadPart;
    struct Race;
}

namespace OFClass
{
    /// The half extents of the body that a person is solid as, in game units: a human is 40 units wide and 128 tall
    /// (the box of the placeholder skeleton of the player), and a race that is raceHeight times as tall is that much
    /// taller and wider. A height that is not a positive number (a record without data has none) counts as 1.
    osg::Vec3f npcBodyHalfExtents(float raceHeight);

    /// The head parts of a character: those it names and, after each, the extra parts that it names in turn (a head
    /// part can be only a name for others, as a beard of several pieces is). A part is listed once whatever names it,
    /// so records that name each other end the walk. A part that `find` does not have is left out, and so are its extra
    /// parts.
    std::vector<const ESM4::HeadPart*> expandHeadParts(
        const std::vector<ESM::FormId>& ids, const std::function<const ESM4::HeadPart*(ESM::FormId)>& find);

    /// The models that make up a character of Fallout 3 or New Vegas, as the records name them (paths under meshes), in
    /// the order body, head, hair, worn pieces. The race has the body (upper body, left hand, right hand) and the head
    /// (head, ears, mouth, teeth, tongue, eyes) of each sex; the character has a hair and may have more parts of the
    /// head. A piece of armour or clothing is the model of the sex of the character (the male one when there is no
    /// female one) and takes the place of the parts of the race at the biped slots that it covers: the upper body, a
    /// hand, the head (which takes the face parts too), the hair. A piece is worn when no piece before it in the list
    /// covers a slot that it covers; one that covers no slot or has no model is not. Null pointers are left out.
    std::vector<std::string> falloutNpcModels(const ESM4::Race& race, bool isFemale, const ESM4::Hair* hair,
        const std::vector<const ESM4::HeadPart*>& headParts, const std::vector<const ESM4::Armor*>& armor);

    class ESM4Npc final : public OFWorld::RegisteredClass<ESM4Npc>
    {
    public:
        ESM4Npc()
            : OFWorld::RegisteredClass<ESM4Npc>(ESM4::Npc::sRecordId)
        {
        }

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override
        {
            const OFWorld::LiveCellRef<ESM4::Npc>* ref = ptr.get<ESM4::Npc>();
            return OFWorld::Ptr(cell.insert(ref), &cell);
        }

        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override
        {
            renderingInterface.getObjects().insertNPC(ptr);
        }

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override
        {
            insertObjectPhysics(ptr, model, rotation, physics);
        }

        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override { return true; }
        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override
        {
            return ESM4Impl::getToolTipInfo(getName(ptr), count);
        }

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;
        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;

        static const ESM4::Npc* getTraitsRecord(const OFWorld::Ptr& ptr);
        static const ESM4::Race* getRace(const OFWorld::Ptr& ptr);
        static bool isFemale(const OFWorld::Ptr& ptr);
        static const std::vector<const ESM4::Armor*>& getEquippedArmor(const OFWorld::Ptr& ptr);
        static const std::vector<const ESM4::Clothing*>& getEquippedClothing(const OFWorld::Ptr& ptr);

    private:
        static ESM4NpcCustomData& getCustomData(const OFWorld::ConstPtr& ptr);
    };
}

#endif // GAME_MWCLASS_ESM4ACTOR_H
