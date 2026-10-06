#include "script.hpp"

#include <algorithm>
#include <stdexcept>

#include <components/esm/common.hpp>

#include "reader.hpp"

namespace ESM4
{
    namespace
    {
        constexpr std::uint16_t localVariableSize = 6 * sizeof(std::uint32_t);
        constexpr std::uint16_t referenceSize = sizeof(std::uint32_t);

        /// Throw std::runtime_error if the current sub-record is declared longer than the rest of its record, or,
        /// when a size is given, if its data is of another size. Reading either would drift into the next record.
        void checkSubRecord(const Reader& reader, std::uint16_t size = 0)
        {
            if (!reader.subRecordFitsRecord())
                throw std::runtime_error(
                    "ESM4::ScriptDefinition::loadSubRecord - sub-record is longer than its record");
            if (size != 0 && reader.subRecordHeader().dataSize != size)
                throw std::runtime_error("ESM4::ScriptDefinition::loadSubRecord - sub-record has an unexpected size");
        }

        /// Read a fixed-size field, or throw std::runtime_error if the file ends inside it.
        template <class T>
        void readField(Reader& reader, T& value)
        {
            if (!reader.getExact(value))
                throw std::runtime_error("ESM4::ScriptDefinition::loadSubRecord - sub-record is shorter than its size");
        }
    }

    /// Consume a recognized script subrecord and return true; leave unknown subrecords unread.
    /// Skip SCHR headers of unexpected size. Throw std::runtime_error for a recognized subrecord that is declared
    /// longer than its record, for SLSD, SCRO and SCRV data of another size, and for SCHR, SCDA, SLSD, SCRO and
    /// SCRV data that the file ends inside.
    bool ScriptDefinition::loadSubRecord(Reader& reader)
    {
        const SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("SCHR"):
                checkSubRecord(reader);
                if (subHdr.dataSize == sizeof(ScriptHeader))
                    readField(reader, scriptHeader);
                else
                    reader.skipSubRecordData();
                return true;
            case ESM::fourCC("SCDA"):
                checkSubRecord(reader);
                compiledScript.resize(subHdr.dataSize);
                if (!reader.get(compiledScript.data(), compiledScript.size()))
                    throw std::runtime_error("ESM4::ScriptDefinition::loadSubRecord - SCDA is shorter than its size");
                return true;
            case ESM::fourCC("SCTX"):
                checkSubRecord(reader);
                reader.getString(scriptSource);
                return true;
            case ESM::fourCC("SLSD"):
            {
                checkSubRecord(reader, localVariableSize);
                ScriptLocalVariableData localVar;
                readField(reader, localVar.index);
                readField(reader, localVar.unknown1);
                readField(reader, localVar.unknown2);
                readField(reader, localVar.unknown3);
                readField(reader, localVar.type);
                readField(reader, localVar.unknown4);
                localVarData.push_back(std::move(localVar));
                // WARN: assumes SCVR will follow immediately
                return true;
            }
            case ESM::fourCC("SCVR"): // assumed always pair with SLSD
                checkSubRecord(reader);
                // The reader cannot read a string of no bytes at all, which is a name that is empty.
                if (!localVarData.empty() && subHdr.dataSize == 0)
                    localVarData.back().variableName.clear();
                else if (!localVarData.empty())
                    reader.getZString(localVarData.back().variableName);
                else
                    reader.skipSubRecordData();
                return true;
            case ESM::fourCC("SCRO"):
            {
                checkSubRecord(reader, referenceSize);
                ScriptReference reference;
                ESM::FormId32 id = 0;
                readField(reader, id);
                reference.formId = ESM::FormId::fromUint32(id);
                // A null reference (zero) is valid, and stays null.
                if (id != 0)
                    reader.adjustFormId(reference.formId);
                references.push_back(reference);
                return true;
            }
            case ESM::fourCC("SCRV"):
            {
                checkSubRecord(reader, referenceSize);
                ScriptReference reference;
                reference.isVariable = true;
                readField(reader, reference.variableIndex);
                references.push_back(reference);
                return true;
            }
            default:
                return false;
        }
    }

    /// Return whether the SCHR compiled size equals the number of stored SCDA bytes.
    bool ScriptDefinition::hasConsistentSize() const
    {
        return scriptHeader.compiledSize == compiledScript.size();
    }

    /// Return whether SCHR counts exactly the stored SCRO and SCRV entries together.
    bool ScriptDefinition::hasConsistentReferences() const
    {
        return scriptHeader.refCount == references.size();
    }

    /// Return whether the SCHR variable count covers every stored SLSD variable index.
    bool ScriptDefinition::hasConsistentVariables() const
    {
        return scriptHeader.variableCount >= highestVariableIndex();
    }

    /// Return whether size, reference count and variable indices all agree with SCHR.
    bool ScriptDefinition::isConsistent() const
    {
        return hasConsistentSize() && hasConsistentReferences() && hasConsistentVariables();
    }

    /// Return the greatest stored SLSD variable index, or zero when there are no locals.
    std::uint32_t ScriptDefinition::highestVariableIndex() const
    {
        std::uint32_t result = 0;
        for (const ScriptLocalVariableData& variable : localVarData)
            result = std::max(result, variable.index);
        return result;
    }
}
