#include "script.hpp"

#include <stdexcept>

#include <components/esm/common.hpp>

#include "reader.hpp"

namespace ESM4
{
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

    bool ScriptDefinition::isConsistent() const
    {
        return scriptHeader.compiledSize == compiledScript.size() && scriptHeader.refCount == references.size()
            && scriptHeader.variableCount == localVarData.size();
    }
}
