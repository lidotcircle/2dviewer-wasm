#include "vm_object.h"
#include "vm.h"

using namespace M2V;


void VMFunctionObject::MarkGeneration(size_t gen)
{
    for (auto& v: m_capturedVariable) {
        v->MarkGeneration(gen);
    }
    m_module->MarkGeneration(gen);
}

VMModuleObject::VMModuleObject(VMObjectId id, const ExecutionModule& module, VirtualMachine& vm):
    VMObject(VMObjectType::Module, id), m_module(std::make_unique<ExecutionModule>(module))
{
    for (auto& func: m_module->GetFunctionTable()) {
        auto kfunc = vm.CreateFunction(this, func.m_begin, func.m_size, 
                                       std::vector<VMObjectPtr>(), func.m_varadic);
        m_functions.push_back(static_cast<VMFunctionObject*>(&*kfunc));
    }
}

const VMInstruction& VMModuleObject::GetInstruction(size_t instructioinPointer) const
{
    return m_module->GetInstruction(instructioinPointer);
}

const std::string& VMModuleObject::GetNthString(size_t idx) const
{
    return m_module->GetNthString(idx);
}
IntegerValueType VMModuleObject::GetNthInteger(size_t idx) const
{
    return m_module->GetNthInt(idx);
}
FloatValueType VMModuleObject::GetNthFloat(size_t idx) const
{
    return m_module->GetNthFloat(idx);
}

VMFunctionObject* VMModuleObject::GetInitializer()
{
    auto idxOpt = m_module->ModuleIntializer();
    if (idxOpt.has_value()) {
        return GetNthFunction(idxOpt.value());
    } else {
        return nullptr;
    }
}
