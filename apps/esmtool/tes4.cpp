#include "tes4.hpp"
#include "arguments.hpp"
#include "labels.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include <components/debug/writeflags.hpp>
#include <components/esm/esmcommon.hpp>
#include <components/esm/format.hpp>
#include <components/esm/path.hpp>
#include <components/esm/refid.hpp>
#include <components/esm/typetraits.hpp>
#include <components/esm4/census.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/esm4/records.hpp>
#include <components/esm4/referencecensus.hpp>
#include <components/esm4/survey.hpp>
#include <components/esm4/typetraits.hpp>
#include <components/files/conversion.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/toutf8/toutf8.hpp>

namespace EsmTool
{
    namespace
    {
        struct Params
        {
            const bool mQuite;
            // Where the census collects what it knows about the records that are read, if this is a census.
            ESM4::Census* const mCensus;

            /// Derive quiet mode from the command and retain an optional, non-owning script census pointer.
            explicit Params(const Arguments& info, ESM4::Census* census = nullptr)
                : mQuite(info.quiet_given || info.mode == "clone" || info.mode == "census" || info.mode == "survey")
                , mCensus(census)
            {
            }
        };

        std::string toString(ESM4::GroupType type)
        {
            switch (type)
            {
                case ESM4::Grp_RecordType:
                    return "RecordType";
                case ESM4::Grp_WorldChild:
                    return "WorldChild";
                case ESM4::Grp_InteriorCell:
                    return "InteriorCell";
                case ESM4::Grp_InteriorSubCell:
                    return "InteriorSubCell";
                case ESM4::Grp_ExteriorCell:
                    return "ExteriorCell";
                case ESM4::Grp_ExteriorSubCell:
                    return "ExteriorSubCell";
                case ESM4::Grp_CellChild:
                    return "CellChild";
                case ESM4::Grp_TopicChild:
                    return "TopicChild";
                case ESM4::Grp_CellPersistentChild:
                    return "CellPersistentChild";
                case ESM4::Grp_CellTemporaryChild:
                    return "CellTemporaryChild";
                case ESM4::Grp_CellVisibleDistChild:
                    return "CellVisibleDistChild";
            }

            return "Unknown (" + std::to_string(type) + ")";
        }

        template <class T>
        struct WriteArray
        {
            std::string_view mPrefix;
            const T& mValue;

            explicit WriteArray(std::string_view prefix, const T& value)
                : mPrefix(prefix)
                , mValue(value)
            {
            }
        };

        template <class T>
        struct WriteData
        {
            const T& mValue;

            explicit WriteData(const T& value)
                : mValue(value)
            {
            }
        };

        template <class T>
        std::ostream& operator<<(std::ostream& stream, const WriteArray<T>& write)
        {
            for (const auto& value : write.mValue)
                stream << write.mPrefix << value;
            return stream;
        }

        template <class T>
        std::ostream& operator<<(std::ostream& stream, const WriteData<T>& /*write*/)
        {
            return stream << " ?";
        }

        std::ostream& operator<<(std::ostream& stream, const std::monostate&)
        {
            return stream << "[none]";
        }

        std::ostream& operator<<(std::ostream& stream, const WriteData<ESM4::GameSetting::Data>& write)
        {
            std::visit([&](const auto& v) { stream << v; }, write.mValue);
            return stream;
        }

        struct WriteCellFlags
        {
            std::uint16_t mValue;
        };

        using CellFlagString = Debug::FlagString<std::uint16_t>;

        constexpr std::array cellFlags{
            CellFlagString{ ESM4::CELL_Interior, "Interior" },
            CellFlagString{ ESM4::CELL_HasWater, "HasWater" },
            CellFlagString{ ESM4::CELL_NoTravel, "NoTravel" },
            CellFlagString{ ESM4::CELL_HideLand, "HideLand" },
            CellFlagString{ ESM4::CELL_Public, "Public" },
            CellFlagString{ ESM4::CELL_HandChgd, "HandChgd" },
            CellFlagString{ ESM4::CELL_QuasiExt, "QuasiExt" },
            CellFlagString{ ESM4::CELL_SkyLight, "SkyLight" },
        };

        std::ostream& operator<<(std::ostream& stream, const WriteCellFlags& write)
        {
            return Debug::writeFlags(stream, write.mValue, cellFlags);
        }

        /// Load the current record as T, collect its scripts when supported, and print it unless quiet.
        /// Reader and loader errors propagate to the caller.
        template <class T>
        void readTypedRecord(const Params& params, ESM4::Reader& reader)
        {
            reader.getRecordData();

            T value;
            value.load(reader);

            if constexpr (requires(ESM4::Census& census) { census.addScripts(value); })
            {
                if (params.mCensus != nullptr)
                    params.mCensus->addScripts(value);
            }

            if (params.mQuite)
                return;

            std::cout << "\n  Record: " << ESM::NAME(reader.hdr().record.typeId).toStringView();
            if constexpr (ESM::HasId<T>)
                std::cout << "\n  Id: " << value.mId;
            if constexpr (ESM4::HasFlags<T>)
                std::cout << "\n  Record flags: " << recordFlags(value.mFlags);
            if constexpr (ESM4::HasParent<T>)
                std::cout << "\n  Parent: " << value.mParent;
            if constexpr (ESM4::HasEditorId<T>)
                std::cout << "\n  EditorId: " << value.mEditorId;
            if constexpr (ESM4::HasFullName<T>)
                std::cout << "\n  FullName: " << value.mFullName;
            if constexpr (ESM4::HasCellFlags<T>)
                std::cout << "\n  CellFlags: " << WriteCellFlags{ value.mCellFlags };
            if constexpr (ESM4::HasX<T>)
                std::cout << "\n  X: " << value.mX;
            if constexpr (ESM4::HasY<T>)
                std::cout << "\n  Y: " << value.mY;
            if constexpr (ESM::HasModel<T>)
                std::cout << "\n  Model: " << value.mModel.getOriginal();
            if constexpr (ESM4::HasModelMale<T>)
                std::cout << "\n  ModelMale: " << value.mModelMale.getOriginal();
            if constexpr (ESM4::HasModelMaleWorld<T>)
                std::cout << "\n  ModelMaleWorld: " << value.mModelMaleWorld.getOriginal();
            if constexpr (ESM4::HasModelFemale<T>)
                std::cout << "\n  ModelFemale: " << value.mModelFemale.getOriginal();
            if constexpr (ESM4::HasModelFemaleWorld<T>)
                std::cout << "\n  ModelFemaleWorld: " << value.mModelFemaleWorld.getOriginal();
            if constexpr (ESM4::HasNif<T>)
                std::cout << "\n  Nif:" << WriteArray("\n  - ", value.mNif);
            if constexpr (ESM4::HasKf<T>)
                std::cout << "\n  Kf:" << WriteArray("\n  - ", value.mKf);
            if constexpr (ESM4::HasType<T>)
                std::cout << "\n  Type: " << value.mType;
            if constexpr (ESM4::HasValue<T>)
                std::cout << "\n  Value: " << value.mValue;
            if constexpr (ESM4::HasData<T>)
                std::cout << "\n  Data: " << WriteData(value.mData);
            std::cout << '\n';
        }

        /// Load the current record as T if the file is one of Fallout 3 or New Vegas, as readTypedRecord does. The
        /// loaders that only know the layouts of those games are not used for the records of other games. Return false
        /// and read nothing for those.
        template <class T>
        bool readFalloutRecord(const Params& params, ESM4::Reader& reader)
        {
            if (!reader.isFalloutFile())
                return false;
            readTypedRecord<T>(params, reader);
            return true;
        }

        bool readRecord(const Params& params, ESM4::Reader& reader)
        {
            switch (static_cast<ESM4::RecordTypes>(reader.hdr().record.typeId))
            {
                case ESM4::REC_AACT:
                    break;
                case ESM4::REC_ACHR:
                    readTypedRecord<ESM4::ActorCharacter>(params, reader);
                    return true;
                case ESM4::REC_ACRE:
                    readTypedRecord<ESM4::ActorCreature>(params, reader);
                    return true;
                case ESM4::REC_ACTI:
                    readTypedRecord<ESM4::Activator>(params, reader);
                    return true;
                case ESM4::REC_ADDN:
                    if (readFalloutRecord<ESM4::AddonNode>(params, reader))
                        return true;
                    break;
                case ESM4::REC_ALCH:
                    readTypedRecord<ESM4::Potion>(params, reader);
                    return true;
                case ESM4::REC_ALOC:
                    readTypedRecord<ESM4::MediaLocationController>(params, reader);
                    return true;
                case ESM4::REC_AMEF:
                    if (readFalloutRecord<ESM4::AmmoEffect>(params, reader))
                        return true;
                    break;
                case ESM4::REC_AMMO:
                    readTypedRecord<ESM4::Ammunition>(params, reader);
                    return true;
                case ESM4::REC_ANIO:
                    readTypedRecord<ESM4::AnimObject>(params, reader);
                    return true;
                case ESM4::REC_APPA:
                    readTypedRecord<ESM4::Apparatus>(params, reader);
                    return true;
                case ESM4::REC_ARMA:
                    readTypedRecord<ESM4::ArmorAddon>(params, reader);
                    return true;
                case ESM4::REC_ARMO:
                    readTypedRecord<ESM4::Armor>(params, reader);
                    return true;
                case ESM4::REC_ARTO:
                    break;
                case ESM4::REC_ASPC:
                    readTypedRecord<ESM4::AcousticSpace>(params, reader);
                    return true;
                case ESM4::REC_ASTP:
                    break;
                case ESM4::REC_AVIF:
                    if (readFalloutRecord<ESM4::ActorValueInfo>(params, reader))
                        return true;
                    break;
                case ESM4::REC_BOOK:
                    readTypedRecord<ESM4::Book>(params, reader);
                    return true;
                case ESM4::REC_BPTD:
                    readTypedRecord<ESM4::BodyPartData>(params, reader);
                    return true;
                case ESM4::REC_CAMS:
                    if (readFalloutRecord<ESM4::CameraShot>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CCRD:
                    if (readFalloutRecord<ESM4::CaravanCard>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CDCK:
                    if (readFalloutRecord<ESM4::CaravanDeck>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CELL:
                    readTypedRecord<ESM4::Cell>(params, reader);
                    return true;
                case ESM4::REC_CHAL:
                    if (readFalloutRecord<ESM4::Challenge>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CHIP:
                    if (readFalloutRecord<ESM4::PokerChip>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CLAS:
                    readTypedRecord<ESM4::Class>(params, reader);
                    return true;
                case ESM4::REC_CLFM:
                    readTypedRecord<ESM4::Colour>(params, reader);
                    return true;
                case ESM4::REC_CLMT:
                    if (readFalloutRecord<ESM4::Climate>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CLOT:
                    readTypedRecord<ESM4::Clothing>(params, reader);
                    return true;
                case ESM4::REC_CMNY:
                    if (readFalloutRecord<ESM4::CaravanMoney>(params, reader))
                        return true;
                    break;
                case ESM4::REC_COBJ:
                    break;
                case ESM4::REC_COLL:
                    break;
                case ESM4::REC_CONT:
                    readTypedRecord<ESM4::Container>(params, reader);
                    return true;
                case ESM4::REC_CPTH:
                    if (readFalloutRecord<ESM4::CameraPath>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CREA:
                    readTypedRecord<ESM4::Creature>(params, reader);
                    return true;
                case ESM4::REC_CSNO:
                    if (readFalloutRecord<ESM4::Casino>(params, reader))
                        return true;
                    break;
                case ESM4::REC_CSTY:
                    if (readFalloutRecord<ESM4::CombatStyle>(params, reader))
                        return true;
                    break;
                case ESM4::REC_DEBR:
                    if (readFalloutRecord<ESM4::Debris>(params, reader))
                        return true;
                    break;
                case ESM4::REC_DEHY:
                    if (readFalloutRecord<ESM4::DehydrationStage>(params, reader))
                        return true;
                    break;
                case ESM4::REC_DIAL:
                    readTypedRecord<ESM4::Dialogue>(params, reader);
                    return true;
                case ESM4::REC_DLBR:
                    break;
                case ESM4::REC_DLVW:
                    break;
                case ESM4::REC_DOBJ:
                    readTypedRecord<ESM4::DefaultObj>(params, reader);
                    return true;
                case ESM4::REC_DOOR:
                    readTypedRecord<ESM4::Door>(params, reader);
                    return true;
                case ESM4::REC_DUAL:
                    break;
                case ESM4::REC_ECZN:
                    if (readFalloutRecord<ESM4::EncounterZone>(params, reader))
                        return true;
                    break;
                case ESM4::REC_EFSH:
                    if (readFalloutRecord<ESM4::EffectShader>(params, reader))
                        return true;
                    break;
                case ESM4::REC_ENCH:
                    if (readFalloutRecord<ESM4::Enchantment>(params, reader))
                        return true;
                    break;
                case ESM4::REC_EQUP:
                    break;
                case ESM4::REC_EXPL:
                    if (readFalloutRecord<ESM4::Explosion>(params, reader))
                        return true;
                    break;
                case ESM4::REC_EYES:
                    readTypedRecord<ESM4::Eyes>(params, reader);
                    return true;
                case ESM4::REC_FACT:
                    readTypedRecord<ESM4::Faction>(params, reader);
                    return true;
                case ESM4::REC_FLOR:
                    readTypedRecord<ESM4::Flora>(params, reader);
                    return true;
                case ESM4::REC_FLST:
                    readTypedRecord<ESM4::FormIdList>(params, reader);
                    return true;
                case ESM4::REC_FSTP:
                    break;
                case ESM4::REC_FSTS:
                    break;
                case ESM4::REC_FURN:
                    readTypedRecord<ESM4::Furniture>(params, reader);
                    return true;
                case ESM4::REC_GLOB:
                    readTypedRecord<ESM4::GlobalVariable>(params, reader);
                    return true;
                case ESM4::REC_GMST:
                    readTypedRecord<ESM4::GameSetting>(params, reader);
                    return true;
                case ESM4::REC_GRAS:
                    readTypedRecord<ESM4::Grass>(params, reader);
                    return true;
                case ESM4::REC_GRUP:
                    break;
                case ESM4::REC_HAIR:
                    readTypedRecord<ESM4::Hair>(params, reader);
                    return true;
                case ESM4::REC_HAZD:
                    break;
                case ESM4::REC_HDPT:
                    readTypedRecord<ESM4::HeadPart>(params, reader);
                    return true;
                case ESM4::REC_HUNG:
                    if (readFalloutRecord<ESM4::HungerStage>(params, reader))
                        return true;
                    break;
                case ESM4::REC_IDLE:
                    readTypedRecord<ESM4::IdleAnimation>(params, reader);
                    return true;
                    break;
                case ESM4::REC_IDLM:
                    readTypedRecord<ESM4::IdleMarker>(params, reader);
                    return true;
                case ESM4::REC_IMAD:
                    if (readFalloutRecord<ESM4::ImageSpaceModifier>(params, reader))
                        return true;
                    break;
                case ESM4::REC_IMGS:
                    if (readFalloutRecord<ESM4::ImageSpace>(params, reader))
                        return true;
                    break;
                case ESM4::REC_IMOD:
                    readTypedRecord<ESM4::ItemMod>(params, reader);
                    return true;
                case ESM4::REC_INFO:
                    readTypedRecord<ESM4::DialogInfo>(params, reader);
                    return true;
                case ESM4::REC_INGR:
                    readTypedRecord<ESM4::Ingredient>(params, reader);
                    return true;
                case ESM4::REC_IPCT:
                    if (readFalloutRecord<ESM4::ImpactData>(params, reader))
                        return true;
                    break;
                case ESM4::REC_IPDS:
                    if (readFalloutRecord<ESM4::ImpactDataSet>(params, reader))
                        return true;
                    break;
                case ESM4::REC_KEYM:
                    readTypedRecord<ESM4::Key>(params, reader);
                    return true;
                case ESM4::REC_KYWD:
                    break;
                case ESM4::REC_LAND:
                    readTypedRecord<ESM4::Land>(params, reader);
                    return true;
                case ESM4::REC_LCRT:
                    break;
                case ESM4::REC_LCTN:
                    break;
                case ESM4::REC_LGTM:
                    readTypedRecord<ESM4::LightingTemplate>(params, reader);
                    return true;
                case ESM4::REC_LIGH:
                    readTypedRecord<ESM4::Light>(params, reader);
                    return true;
                case ESM4::REC_LSCR:
                    if (readFalloutRecord<ESM4::LoadScreen>(params, reader))
                        return true;
                    break;
                case ESM4::REC_LSCT:
                    if (readFalloutRecord<ESM4::LoadScreenType>(params, reader))
                        return true;
                    break;
                case ESM4::REC_LTEX:
                    readTypedRecord<ESM4::LandTexture>(params, reader);
                    return true;
                case ESM4::REC_LVLC:
                    readTypedRecord<ESM4::LevelledCreature>(params, reader);
                    return true;
                case ESM4::REC_LVLI:
                    readTypedRecord<ESM4::LevelledItem>(params, reader);
                    return true;
                case ESM4::REC_LVLN:
                    readTypedRecord<ESM4::LevelledNpc>(params, reader);
                    return true;
                case ESM4::REC_LVSP:
                    break;
                case ESM4::REC_MATO:
                    readTypedRecord<ESM4::Material>(params, reader);
                    return true;
                case ESM4::REC_MATT:
                    break;
                case ESM4::REC_MESG:
                    if (readFalloutRecord<ESM4::Message>(params, reader))
                        return true;
                    break;
                case ESM4::REC_MGEF:
                    if (readFalloutRecord<ESM4::MagicEffect>(params, reader))
                        return true;
                    break;
                case ESM4::REC_MICN:
                    if (readFalloutRecord<ESM4::MenuIcon>(params, reader))
                        return true;
                    break;
                case ESM4::REC_MISC:
                    readTypedRecord<ESM4::MiscItem>(params, reader);
                    return true;
                case ESM4::REC_MOVT:
                    break;
                case ESM4::REC_MSET:
                    readTypedRecord<ESM4::MediaSet>(params, reader);
                    return true;
                case ESM4::REC_MSTT:
                    readTypedRecord<ESM4::MovableStatic>(params, reader);
                    return true;
                case ESM4::REC_MUSC:
                    readTypedRecord<ESM4::Music>(params, reader);
                    return true;
                case ESM4::REC_MUST:
                    break;
                case ESM4::REC_NAVI:
                    readTypedRecord<ESM4::Navigation>(params, reader);
                    return true;
                case ESM4::REC_NAVM:
                    readTypedRecord<ESM4::NavMesh>(params, reader);
                    return true;
                case ESM4::REC_NOTE:
                    readTypedRecord<ESM4::Note>(params, reader);
                    return true;
                case ESM4::REC_NPC_:
                    readTypedRecord<ESM4::Npc>(params, reader);
                    return true;
                case ESM4::REC_OTFT:
                    readTypedRecord<ESM4::Outfit>(params, reader);
                    return true;
                case ESM4::REC_PACK:
                    readTypedRecord<ESM4::AIPackage>(params, reader);
                    return true;
                case ESM4::REC_PERK:
                    if (readFalloutRecord<ESM4::Perk>(params, reader))
                        return true;
                    break;
                case ESM4::REC_PGRD:
                    readTypedRecord<ESM4::Pathgrid>(params, reader);
                    return true;
                case ESM4::REC_PGRE:
                    readTypedRecord<ESM4::PlacedGrenade>(params, reader);
                    return true;
                case ESM4::REC_PHZD:
                    break;
                case ESM4::REC_PROJ:
                    if (readFalloutRecord<ESM4::Projectile>(params, reader))
                        return true;
                    break;
                case ESM4::REC_PWAT:
                    readTypedRecord<ESM4::PlaceableWater>(params, reader);
                    return true;
                case ESM4::REC_QUST:
                    readTypedRecord<ESM4::Quest>(params, reader);
                    return true;
                case ESM4::REC_RACE:
                    readTypedRecord<ESM4::Race>(params, reader);
                    return true;
                case ESM4::REC_RADS:
                    if (readFalloutRecord<ESM4::RadiationStage>(params, reader))
                        return true;
                    break;
                case ESM4::REC_RCCT:
                    if (readFalloutRecord<ESM4::RecipeCategory>(params, reader))
                        return true;
                    break;
                case ESM4::REC_RCPE:
                    if (readFalloutRecord<ESM4::Recipe>(params, reader))
                        return true;
                    break;
                case ESM4::REC_REFR:
                    readTypedRecord<ESM4::Reference>(params, reader);
                    return true;
                case ESM4::REC_REGN:
                    readTypedRecord<ESM4::Region>(params, reader);
                    return true;
                case ESM4::REC_RELA:
                    break;
                case ESM4::REC_REPU:
                    if (readFalloutRecord<ESM4::Reputation>(params, reader))
                        return true;
                    break;
                case ESM4::REC_REVB:
                    break;
                case ESM4::REC_RFCT:
                    break;
                case ESM4::REC_RGDL:
                    if (readFalloutRecord<ESM4::Ragdoll>(params, reader))
                        return true;
                    break;
                case ESM4::REC_ROAD:
                    readTypedRecord<ESM4::Road>(params, reader);
                    return true;
                case ESM4::REC_SBSP:
                    readTypedRecord<ESM4::SubSpace>(params, reader);
                    return true;
                case ESM4::REC_SCEN:
                    break;
                case ESM4::REC_SCOL:
                    readTypedRecord<ESM4::StaticCollection>(params, reader);
                    return true;
                case ESM4::REC_SCPT:
                    readTypedRecord<ESM4::Script>(params, reader);
                    return true;
                case ESM4::REC_SCRL:
                    readTypedRecord<ESM4::Scroll>(params, reader);
                    return true;
                case ESM4::REC_SGST:
                    readTypedRecord<ESM4::SigilStone>(params, reader);
                    return true;
                case ESM4::REC_SHOU:
                    break;
                case ESM4::REC_SLGM:
                    readTypedRecord<ESM4::SoulGem>(params, reader);
                    return true;
                case ESM4::REC_SLPD:
                    if (readFalloutRecord<ESM4::SleepDeprivationStage>(params, reader))
                        return true;
                    break;
                case ESM4::REC_SMBN:
                    break;
                case ESM4::REC_SMEN:
                    break;
                case ESM4::REC_SMQN:
                    break;
                case ESM4::REC_SNCT:
                    break;
                case ESM4::REC_SNDR:
                    readTypedRecord<ESM4::SoundReference>(params, reader);
                    return true;
                case ESM4::REC_SOPM:
                    break;
                case ESM4::REC_SOUN:
                    readTypedRecord<ESM4::Sound>(params, reader);
                    return true;
                case ESM4::REC_SPEL:
                    if (readFalloutRecord<ESM4::Spell>(params, reader))
                        return true;
                    break;
                case ESM4::REC_SPGD:
                    break;
                case ESM4::REC_STAT:
                    readTypedRecord<ESM4::Static>(params, reader);
                    return true;
                case ESM4::REC_TACT:
                    readTypedRecord<ESM4::TalkingActivator>(params, reader);
                    return true;
                case ESM4::REC_TERM:
                    readTypedRecord<ESM4::Terminal>(params, reader);
                    return true;
                case ESM4::REC_TES4:
                    readTypedRecord<ESM4::Header>(params, reader);
                    return true;
                case ESM4::REC_TREE:
                    readTypedRecord<ESM4::Tree>(params, reader);
                    return true;
                case ESM4::REC_TXST:
                    readTypedRecord<ESM4::TextureSet>(params, reader);
                    return true;
                case ESM4::REC_VTYP:
                    if (readFalloutRecord<ESM4::VoiceType>(params, reader))
                        return true;
                    break;
                case ESM4::REC_WATR:
                    if (readFalloutRecord<ESM4::Water>(params, reader))
                        return true;
                    break;
                case ESM4::REC_WEAP:
                    readTypedRecord<ESM4::Weapon>(params, reader);
                    return true;
                case ESM4::REC_WOOP:
                    break;
                case ESM4::REC_WRLD:
                    readTypedRecord<ESM4::World>(params, reader);
                    return true;
                case ESM4::REC_WTHR:
                    if (readFalloutRecord<ESM4::Weather>(params, reader))
                        return true;
                    break;
            }

            if (!params.mQuite)
                std::cout << "\n  Unsupported record: " << ESM::NAME(reader.hdr().record.typeId).toStringView() << '\n';
            return false;
        }

    }

    int loadTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream)
    {
        std::cout << "Loading TES4 file: " << info.filename << '\n';

        try
        {
            const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::calculateEncoding(info.encoding));
            ESM4::Reader reader(std::move(stream), info.filename, nullptr, &encoder, true);
            const Params params(info);

            if (!params.mQuite)
            {
                std::cout << "Author: " << reader.getAuthor() << '\n'
                          << "Description: " << reader.getDesc() << '\n'
                          << "File format version: " << reader.esmVersionF() << '\n';

                if (const std::vector<ESM::MasterData>& masterData = reader.getGameFiles(); !masterData.empty())
                {
                    std::cout << "Masters:" << '\n';
                    for (const auto& master : masterData)
                        std::cout << "  " << master.name << ", " << master.size << " bytes\n";
                }
            }

            auto visitorRec = [&params](ESM4::Reader& r) { return readRecord(params, r); };
            auto visitorGroup = [&params](ESM4::Reader& r) {
                if (params.mQuite)
                    return;
                auto groupType = static_cast<ESM4::GroupType>(r.hdr().group.type);
                std::cout << "\nGroup: " << toString(groupType) << " " << ESM::NAME(r.hdr().group.typeId).toStringView()
                          << '\n';
            };
            ESM4::ReaderUtils::readAll(reader, visitorRec, visitorGroup);
        }
        catch (const std::exception& e)
        {
            std::cout << "\nERROR:\n\n  " << e.what() << std::endl;
            return -1;
        }

        return 0;
    }

    /// Read a TES4 plugin and print record and script census tables to standard output.
    /// Return 0 after a complete scan, or -1 on a fatal read error or exception.
    int censusTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream)
    {
        std::cout << "Census of TES4 file: " << info.filename << '\n';

        try
        {
            const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::calculateEncoding(info.encoding));
            ESM4::Reader reader(std::move(stream), info.filename, nullptr, &encoder, true);
            ESM4::Census census;
            const Params params(info, &census);

            std::cout << "File format version: " << reader.esmVersionF() << '\n';
            if (const std::vector<ESM::MasterData>& masterData = reader.getGameFiles(); !masterData.empty())
            {
                std::cout << "Masters:\n";
                for (const auto& master : masterData)
                    std::cout << "  " << master.name << '\n';
            }
            std::cout << '\n';

            census.collect(reader, [&params](ESM4::Reader& r) { return readRecord(params, r); });
            census.write(std::cout);
            return census.getFatalError().empty() ? 0 : -1;
        }
        catch (const std::exception& e)
        {
            std::cout << "\nERROR:\n\n  " << e.what() << std::endl;
            return -1;
        }
    }
    /// Survey every file named on the command line, as one list of sub-records, and print it to standard output.
    /// Return 0 when every file was read to its end, or -1 if one could not be opened or read.
    int surveyTes4(const Arguments& info)
    {
        ESM4::Survey survey(std::set<std::string>(info.types.begin(), info.types.end()), info.failed_given);
        const Params params(info);
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::calculateEncoding(info.encoding));
        int result = 0;

        for (const std::filesystem::path& path : info.inputFiles)
        {
            const std::string name = Files::pathToUnicodeString(path.filename());
            try
            {
                auto stream = Files::openBinaryInputFileStream(path);
                if (!stream->is_open())
                {
                    std::cout << "Failed to open file " << name << ": " << std::generic_category().message(errno)
                              << '\n';
                    result = -1;
                    continue;
                }
                if (ESM::readFormat(*stream) != ESM::Format::Tes4)
                {
                    std::cout << "Survey mode only supports TES4-format files: " << name << '\n';
                    result = -1;
                    continue;
                }
                stream->seekg(0);

                ESM4::Reader reader(std::move(stream), path, nullptr, &encoder, true);
                survey.collect(reader, [&params](ESM4::Reader& r) { return readRecord(params, r); });
            }
            catch (const std::exception& e)
            {
                std::cout << "\nERROR in " << name << ":\n\n  " << e.what() << std::endl;
                result = -1;
            }
        }

        std::cout << "Surveyed " << info.inputFiles.size() << " files\n\n";
        survey.write(std::cout);
        return survey.getFatalErrors().empty() ? result : -1;
    }

    /// Count the references that every file named on the command line places, read in the order given, and print the
    /// counts to standard output. Return 0 when every file was read to its end, or -1 if one could not be opened or
    /// read.
    int referencesTes4(const Arguments& info)
    {
        ESM4::ReferenceCensus census;
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::calculateEncoding(info.encoding));
        std::map<std::string, int> nameToIndex;
        int result = 0;

        std::uint32_t index = 0;
        for (const std::filesystem::path& path : info.inputFiles)
        {
            const std::string name = Files::pathToUnicodeString(path.filename());
            const std::uint32_t modIndex = index++;
            try
            {
                auto stream = Files::openBinaryInputFileStream(path);
                if (!stream->is_open())
                {
                    std::cout << "Failed to open file " << name << ": " << std::generic_category().message(errno)
                              << '\n';
                    result = -1;
                    continue;
                }
                if (ESM::readFormat(*stream) != ESM::Format::Tes4)
                {
                    std::cout << "References mode only supports TES4-format files: " << name << '\n';
                    result = -1;
                    continue;
                }
                stream->seekg(0);

                ESM4::Reader reader(std::move(stream), path, nullptr, &encoder, true);
                reader.setModIndex(modIndex);
                reader.updateModIndices(nameToIndex);
                nameToIndex[Misc::StringUtils::lowerCase(name)] = static_cast<int>(modIndex);
                census.collect(reader);
            }
            catch (const std::exception& e)
            {
                std::cout << "\nERROR in " << name << ":\n\n  " << e.what() << std::endl;
                result = -1;
            }
        }

        std::cout << "Counted the references of " << info.inputFiles.size() << " files\n\n";
        census.write(std::cout);
        return census.getFatalErrors().empty() ? result : -1;
    }
}
