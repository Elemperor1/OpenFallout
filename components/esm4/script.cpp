#include "script.hpp"

#include <algorithm>
#include <stdexcept>

#include <components/esm/common.hpp>

#include "reader.hpp"

namespace ESM4
{
    /// Consume a recognized script subrecord and return true; leave unknown subrecords unread.
    /// Skip SCHR headers of unexpected size and throw std::runtime_error for truncated SCDA data,
    /// or SCDA data that is declared longer than the rest of its record.
    bool ScriptDefinition::loadSubRecord(Reader& reader)
    {
        const SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM::fourCC("SCHR"):
                if (subHdr.dataSize == sizeof(ScriptHeader))
                    reader.get(scriptHeader);
                else
                    reader.skipSubRecordData();
                return true;
            case ESM::fourCC("SCDA"):
                if (!reader.subRecordFitsRecord())
                    throw std::runtime_error("ESM4::ScriptDefinition::loadSubRecord - SCDA is longer than its record");
                compiledScript.resize(subHdr.dataSize);
                if (!reader.get(compiledScript.data(), compiledScript.size()))
                    throw std::runtime_error("ESM4::ScriptDefinition::loadSubRecord - SCDA is shorter than its size");
                return true;
            case ESM::fourCC("SCTX"):
                reader.getString(scriptSource);
                return true;
            case ESM::fourCC("SLSD"):
            {
                ScriptLocalVariableData localVar;
                reader.get(localVar.index);
                reader.get(localVar.unknown1);
                reader.get(localVar.unknown2);
                reader.get(localVar.unknown3);
                reader.get(localVar.type);
                reader.get(localVar.unknown4);
                localVarData.push_back(std::move(localVar));
                // WARN: assumes SCVR will follow immediately
                return true;
            }
            case ESM::fourCC("SCVR"): // assumed always pair with SLSD
                if (!localVarData.empty())
                    reader.getZString(localVarData.back().variableName);
                else
                    reader.skipSubRecordData();
                return true;
            case ESM::fourCC("SCRO"):
            {
                ScriptReference reference;
                reader.getFormId(reference.formId);
                references.push_back(reference);
                return true;
            }
            case ESM::fourCC("SCRV"):
            {
                ScriptReference reference;
                reference.isVariable = true;
                reader.get(reference.variableIndex);
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
