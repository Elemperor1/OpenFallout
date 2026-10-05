#include "guiextensions.hpp"

#include <components/compiler/opcodes.hpp>

#include <components/interpreter/context.hpp>
#include <components/interpreter/interpreter.hpp>
#include <components/interpreter/opcodes.hpp>
#include <components/interpreter/runtime.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "ref.hpp"

namespace OFScript
{
    namespace Gui
    {
        class OpEnableWindow : public Interpreter::Opcode0
        {
            OFGui::GuiWindow mWindow;

        public:
            OpEnableWindow(OFGui::GuiWindow window)
                : mWindow(window)
            {
            }

            void execute(Interpreter::Runtime& runtime) override
            {
                OFBase::Environment::get().getWindowManager()->allow(mWindow);
            }
        };

        class OpEnableRest : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                OFBase::Environment::get().getWindowManager()->enableRest();
            }
        };

        template <class R>
        class OpShowRestMenu : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                OFWorld::Ptr bed = R()(runtime, false);

                if (bed.isEmpty()
                    || !OFBase::Environment::get().getMechanicsManager()->sleepInBed(OFMechanics::getPlayer(), bed))
                    OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Rest, bed);
            }
        };

        class OpShowDialogue : public Interpreter::Opcode0
        {
            OFGui::GuiMode mDialogue;

        public:
            OpShowDialogue(OFGui::GuiMode dialogue)
                : mDialogue(dialogue)
            {
            }

            void execute(Interpreter::Runtime& runtime) override
            {
                OFBase::Environment::get().getWindowManager()->pushGuiMode(mDialogue);
            }
        };

        class OpGetButtonPressed : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                runtime.push(OFBase::Environment::get().getWindowManager()->readPressedButton());
            }
        };

        class OpToggleFogOfWar : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                runtime.getContext().report(OFBase::Environment::get().getWindowManager()->toggleFogOfWar()
                        ? "Fog of war -> On"
                        : "Fog of war -> Off");
            }
        };

        class OpToggleFullHelp : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                runtime.getContext().report(OFBase::Environment::get().getWindowManager()->toggleFullHelp()
                        ? "Full help -> On"
                        : "Full help -> Off");
            }
        };

        class OpShowMap : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                std::string_view cell = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                // In Morrowind, using an empty string either errors out (e.g. console) or kills the game
                // so it should be reasonable to interrupt the script
                if (cell.empty())
                    throw std::runtime_error("ShowMap substring must not be empty");

                // "Will match complete or partial cells, so ShowMap, "Vivec" will show cells Vivec and Vivec, Fred's
                // House as well." http://www.uesp.net/wiki/Tes3Mod:ShowMap

                const OFWorld::Store<ESM::Cell>& cells = OFBase::Environment::get().getESMStore()->get<ESM::Cell>();

                OFBase::WindowManager* winMgr = OFBase::Environment::get().getWindowManager();

                for (auto it = cells.extBegin(); it != cells.extEnd(); ++it)
                {
                    const auto& cellName = it->mName;
                    if (Misc::StringUtils::ciStartsWith(cellName, cell))
                        winMgr->addVisitedLocation(cellName, it->getGridX(), it->getGridY());
                }
            }
        };

        class OpFillMap : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                const OFWorld::Store<ESM::Cell>& cells = OFBase::Environment::get().getESMStore()->get<ESM::Cell>();

                for (auto it = cells.extBegin(); it != cells.extEnd(); ++it)
                {
                    const std::string& name = it->mName;
                    if (!name.empty())
                        OFBase::Environment::get().getWindowManager()->addVisitedLocation(
                            name, it->getGridX(), it->getGridY());
                }
            }
        };

        class OpMenuTest : public Interpreter::Opcode1
        {
        public:
            void execute(Interpreter::Runtime& runtime, unsigned int arg0) override
            {
                int arg = 0;
                if (arg0 > 0)
                {
                    arg = runtime[0].mInteger;
                    runtime.pop();
                }

                if (arg == 0)
                {
                    OFGui::GuiMode modes[] = { OFGui::GM_Inventory, OFGui::GM_Container };

                    for (int i = 0; i < 2; ++i)
                    {
                        if (OFBase::Environment::get().getWindowManager()->containsMode(modes[i]))
                            OFBase::Environment::get().getWindowManager()->removeGuiMode(modes[i]);
                    }
                }
                else
                {
                    OFGui::GuiWindow gw = OFGui::GW_None;
                    if (arg == 3)
                        gw = OFGui::GW_Stats;
                    if (arg == 4)
                        gw = OFGui::GW_Inventory;
                    if (arg == 5)
                        gw = OFGui::GW_Magic;
                    if (arg == 6)
                        gw = OFGui::GW_Map;

                    OFBase::Environment::get().getWindowManager()->pinWindow(gw);
                }
            }
        };

        class OpToggleMenus : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                bool state = OFBase::Environment::get().getWindowManager()->setHudVisibility(
                    !OFBase::Environment::get().getWindowManager()->isHudVisible());
                runtime.getContext().report(state ? "GUI -> On" : "GUI -> Off");

                if (!state)
                {
                    while (OFBase::Environment::get().getWindowManager()->getMode()
                        != OFGui::GM_None) // don't use isGuiMode, or we get an infinite loop for modal message boxes!
                        OFBase::Environment::get().getWindowManager()->popGuiMode();
                }
            }
        };

        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment5<OpShowDialogue>(Compiler::Gui::opcodeEnableBirthMenu, OFGui::GM_Birth);
            interpreter.installSegment5<OpShowDialogue>(Compiler::Gui::opcodeEnableClassMenu, OFGui::GM_Class);
            interpreter.installSegment5<OpShowDialogue>(Compiler::Gui::opcodeEnableNameMenu, OFGui::GM_Name);
            interpreter.installSegment5<OpShowDialogue>(Compiler::Gui::opcodeEnableRaceMenu, OFGui::GM_Race);
            interpreter.installSegment5<OpShowDialogue>(Compiler::Gui::opcodeEnableStatsReviewMenu, OFGui::GM_Review);
            interpreter.installSegment5<OpShowDialogue>(Compiler::Gui::opcodeEnableLevelupMenu, OFGui::GM_Levelup);

            interpreter.installSegment5<OpEnableWindow>(Compiler::Gui::opcodeEnableInventoryMenu, OFGui::GW_Inventory);
            interpreter.installSegment5<OpEnableWindow>(Compiler::Gui::opcodeEnableMagicMenu, OFGui::GW_Magic);
            interpreter.installSegment5<OpEnableWindow>(Compiler::Gui::opcodeEnableMapMenu, OFGui::GW_Map);
            interpreter.installSegment5<OpEnableWindow>(Compiler::Gui::opcodeEnableStatsMenu, OFGui::GW_Stats);

            interpreter.installSegment5<OpEnableRest>(Compiler::Gui::opcodeEnableRest);

            interpreter.installSegment5<OpShowRestMenu<ImplicitRef>>(Compiler::Gui::opcodeShowRestMenu);
            interpreter.installSegment5<OpShowRestMenu<ExplicitRef>>(Compiler::Gui::opcodeShowRestMenuExplicit);

            interpreter.installSegment5<OpGetButtonPressed>(Compiler::Gui::opcodeGetButtonPressed);

            interpreter.installSegment5<OpToggleFogOfWar>(Compiler::Gui::opcodeToggleFogOfWar);

            interpreter.installSegment5<OpToggleFullHelp>(Compiler::Gui::opcodeToggleFullHelp);

            interpreter.installSegment5<OpShowMap>(Compiler::Gui::opcodeShowMap);
            interpreter.installSegment5<OpFillMap>(Compiler::Gui::opcodeFillMap);
            interpreter.installSegment3<OpMenuTest>(Compiler::Gui::opcodeMenuTest);
            interpreter.installSegment5<OpToggleMenus>(Compiler::Gui::opcodeToggleMenus);
        }
    }
}
