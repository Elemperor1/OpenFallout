#include "cellextensions.hpp"

#include <limits>

#include <components/compiler/opcodes.hpp>

#include <components/interpreter/context.hpp>
#include <components/interpreter/interpreter.hpp>
#include <components/interpreter/opcodes.hpp>
#include <components/interpreter/runtime.hpp>

#include <components/misc/strings/algorithm.hpp>

#include <components/esm/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/actionteleport.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/scene.hpp"

#include "../mwmechanics/actorutil.hpp"

namespace OFScript
{
    namespace Cell
    {
        class OpCellChanged : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                runtime.push(OFBase::Environment::get().getWorldScene()->hasCellChanged() ? 1 : 0);
            }
        };

        class OpTestCells : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                if (OFBase::Environment::get().getStateManager()->getState() != OFBase::StateManager::State_NoGame)
                {
                    runtime.getContext().report(
                        "Use TestCells from the main menu, when there is no active game session.");
                    return;
                }

                bool wasConsole = OFBase::Environment::get().getWindowManager()->isConsoleMode();
                if (wasConsole)
                    OFBase::Environment::get().getWindowManager()->toggleConsole();

                OFBase::Environment::get().getWorldScene()->testExteriorCells();

                if (wasConsole)
                    OFBase::Environment::get().getWindowManager()->toggleConsole();
            }
        };

        class OpTestInteriorCells : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                if (OFBase::Environment::get().getStateManager()->getState() != OFBase::StateManager::State_NoGame)
                {
                    runtime.getContext().report(
                        "Use TestInteriorCells from the main menu, when there is no active game session.");
                    return;
                }

                bool wasConsole = OFBase::Environment::get().getWindowManager()->isConsoleMode();
                if (wasConsole)
                    OFBase::Environment::get().getWindowManager()->toggleConsole();

                OFBase::Environment::get().getWorldScene()->testInteriorCells();

                if (wasConsole)
                    OFBase::Environment::get().getWindowManager()->toggleConsole();
            }
        };

        class OpCOC : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                std::string_view cell = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                ESM::Position pos;
                OFBase::World* world = OFBase::Environment::get().getWorld();
                OFWorld::Ptr playerPtr = world->getPlayerPtr();

                if (const ESM::RefId refId = world->findExteriorPosition(cell, pos); !refId.empty())
                {
                    OFWorld::ActionTeleport(refId, pos, false).execute(playerPtr);
                    playerPtr = world->getPlayerPtr(); // could be changed by ActionTeleport
                    world->adjustPosition(playerPtr, false);
                    return;
                }
                if (const ESM::RefId refId = world->findInteriorPosition(cell, pos); !refId.empty())
                {
                    OFWorld::ActionTeleport(refId, pos, false).execute(playerPtr);
                    return;
                }
                throw std::runtime_error("Cell " + std::string(cell) + " is not found");
            }
        };

        class OpCOE : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                Interpreter::Type_Integer x = runtime[0].mInteger;
                runtime.pop();

                Interpreter::Type_Integer y = runtime[0].mInteger;
                runtime.pop();

                ESM::Position pos;
                OFBase::World* world = OFBase::Environment::get().getWorld();
                OFWorld::Ptr playerPtr = world->getPlayerPtr();

                const osg::Vec2f posFromIndex
                    = ESM::indexToPosition(ESM::ExteriorCellLocation(x, y, ESM::Cell::sDefaultWorldspaceId), true);
                pos.pos[0] = posFromIndex.x();
                pos.pos[1] = posFromIndex.y();
                pos.pos[2] = 0;

                pos.rot[0] = pos.rot[1] = pos.rot[2] = 0;

                OFWorld::ActionTeleport(ESM::RefId::esm3ExteriorCell(x, y), pos, false).execute(playerPtr);
                playerPtr = world->getPlayerPtr(); // could be changed by ActionTeleport
                world->adjustPosition(playerPtr, false);
            }
        };

        class OpGetInterior : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                if (!OFMechanics::getPlayer().isInCell())
                {
                    runtime.push(0);
                    return;
                }

                bool interior = !OFMechanics::getPlayer().getCell()->getCell()->isExterior();

                runtime.push(interior ? 1 : 0);
            }
        };

        class OpGetPCCell : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                std::string_view name = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                if (!OFMechanics::getPlayer().isInCell())
                {
                    runtime.push(0);
                    return;
                }
                const OFWorld::CellStore* cell = OFMechanics::getPlayer().getCell();

                std::string_view current = OFBase::Environment::get().getWorld()->getCellName(cell);
                bool match = Misc::StringUtils::ciCompareLen(name, current, name.length()) == 0;

                runtime.push(match ? 1 : 0);
            }
        };

        class OpGetWaterLevel : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                if (!OFMechanics::getPlayer().isInCell())
                {
                    runtime.push(0.f);
                    return;
                }
                OFWorld::CellStore* cell = OFMechanics::getPlayer().getCell();
                if (cell->isExterior())
                    runtime.push(0.f); // vanilla oddity, return 0 even though water is actually at -1
                else if (cell->getCell()->hasWater())
                    runtime.push(cell->getWaterLevel());
                else
                    runtime.push(-std::numeric_limits<float>::max());
            }
        };

        class OpSetWaterLevel : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                Interpreter::Type_Float level = runtime[0].mFloat;
                runtime.pop();

                if (!OFMechanics::getPlayer().isInCell())
                {
                    return;
                }

                OFWorld::CellStore* cell = OFMechanics::getPlayer().getCell();

                if (cell->getCell()->isExterior())
                    throw std::runtime_error("Can't set water level in exterior cell");

                cell->setWaterLevel(level);
                OFBase::Environment::get().getWorld()->setWaterHeight(cell->getWaterLevel());
            }
        };

        class OpModWaterLevel : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                Interpreter::Type_Float level = runtime[0].mFloat;
                runtime.pop();

                if (!OFMechanics::getPlayer().isInCell())
                {
                    return;
                }

                OFWorld::CellStore* cell = OFMechanics::getPlayer().getCell();

                if (cell->getCell()->isExterior())
                    throw std::runtime_error("Can't set water level in exterior cell");

                cell->setWaterLevel(cell->getWaterLevel() + level);
                OFBase::Environment::get().getWorld()->setWaterHeight(cell->getWaterLevel());
            }
        };

        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment5<OpCellChanged>(Compiler::Cell::opcodeCellChanged);
            interpreter.installSegment5<OpTestCells>(Compiler::Cell::opcodeTestCells);
            interpreter.installSegment5<OpTestInteriorCells>(Compiler::Cell::opcodeTestInteriorCells);
            interpreter.installSegment5<OpCOC>(Compiler::Cell::opcodeCOC);
            interpreter.installSegment5<OpCOE>(Compiler::Cell::opcodeCOE);
            interpreter.installSegment5<OpGetInterior>(Compiler::Cell::opcodeGetInterior);
            interpreter.installSegment5<OpGetPCCell>(Compiler::Cell::opcodeGetPCCell);
            interpreter.installSegment5<OpGetWaterLevel>(Compiler::Cell::opcodeGetWaterLevel);
            interpreter.installSegment5<OpSetWaterLevel>(Compiler::Cell::opcodeSetWaterLevel);
            interpreter.installSegment5<OpModWaterLevel>(Compiler::Cell::opcodeModWaterLevel);
        }
    }
}
