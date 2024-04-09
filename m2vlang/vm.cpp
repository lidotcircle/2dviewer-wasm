#include "vm.h"
#include "vm_object.h"
using namespace M2V;


VirtualMachine::VirtualMachine():
    m_nextFreeId(1),
    m_status(VMStatus::Uninit),
    m_gcGeneration(0),
    m_nullVal(std::make_unique<VMNullObject>(m_nextFreeId++)),
    m_trueVal(std::make_unique<VMBooleanObject>(m_nextFreeId++, true)),
    m_falseVal(std::make_unique<VMBooleanObject>(m_nextFreeId++, false))
{
    m_status = VMStatus::Initialized;
}

void VirtualMachine::ExecuteModule(const ExecutionModule& module, const std::string& funcname)
{
    MASSERT(m_status == VMStatus::Initialized);
    const auto initializer = LoadModule(module);
    m_status = VMStatus::Running;
    if (initializer) {
        m_callstacks.emplace_back(std::make_unique<CallStack>(initializer, std::vector<VMObjectPtr>()));
        this->MainLoop();
        if (m_status != VMStatus::Exited || (m_exitStatus.has_value() && m_exitStatus.value() != 0)) {
            VMPanic("fail to load executable module");
        }
    }
}

static auto VMGetInt(VMObjectPtr obj)
{
    MASSERT(obj->type() == VMObjectType::Integer);
    return static_cast<VMIntegerObject*>(&*obj)->GetValue();
}
static auto VMGetFloat(VMObjectPtr obj)
{
    MASSERT(obj->type() == VMObjectType::Float);
    return static_cast<VMFloatObject*>(&*obj)->GetValue();
}
static auto VMGetBool(VMObjectPtr obj)
{
    MASSERT(obj->type() == VMObjectType::Boolean);
    return static_cast<VMBooleanObject*>(&*obj)->GetValue();
}
static auto& VMGetString(VMObjectPtr obj)
{
    MASSERT(obj->type() == VMObjectType::String);
    return static_cast<VMStringObject*>(&*obj)->GetValue();
}
static bool VMConvertToBool(VMObjectPtr obj)
{
    switch (obj->type()) {
    case VMObjectType::Null:
        return false;
    case VMObjectType::Integer:
        return VMGetInt(obj) != 0;
    case VMObjectType::Boolean:
        return VMGetBool(obj);
    case VMObjectType::Float:
        return VMGetFloat(obj) != 0;
    case VMObjectType::String:
    case VMObjectType::Array:
    case VMObjectType::Object:
    case VMObjectType::Function:
    case VMObjectType::Module:
        break;
    }
    return true;
}

void VirtualMachine::ExecuteInstruction(const VMInstruction& instruction)
{
    auto callstack = GetActiveCallstack();
    switch (instruction.m_opcode) {
    case VMOpcode::NOP:
        break;
    case VMOpcode::POPN:
        callstack->Pop(instruction.m_operand1);
        break;
    case VMOpcode::ADD:
    case VMOpcode::SUB:
    case VMOpcode::MUL:
    case VMOpcode::DIV:
    case VMOpcode::MOD:
    case VMOpcode::LOGICAL_AND:
    case VMOpcode::LOGICAL_OR:
    case VMOpcode::EQUAL:
    case VMOpcode::INEQUAL:
    case VMOpcode::GREATER:
    case VMOpcode::GREATER_EQ:
    case VMOpcode::LESS:
    case VMOpcode::LESS_EQ:
    {
        const auto op1 = callstack->Get(instruction.m_operand1);
        const auto op2 = callstack->Get(instruction.m_operand2);
        const auto result = this->ExecuteBinaryOperator(instruction.m_opcode, op1, op2);
        callstack->Push(result);
        break;
    }
    case VMOpcode::CALL:
    {
        auto op1 = callstack->Get(instruction.m_operand1);
        if (op1->type() != VMObjectType::Function) {
            VMPanic("call to non-funciton object");
            break;
        }
        auto func = static_cast<VMFunctionObject*>(&*op1);
        if (func->isInternal()) {
            func->invokeInternal(*this, *this->GetActiveCallstack());
        } else {
            MASSERT(instruction.m_operand2 >= 0);
            const auto args = callstack->GetTopN(instruction.m_operand2);
            if (func->isVarArgs()) {
                auto array = CreateArray();
                for (auto& a: args) {
                    static_cast<VMArrayObject*>(&*array)->push(a);
                }
                m_callstacks.emplace_back(std::make_unique<CallStack>(func, std::vector<VMObjectPtr>{array}));
            } else {
                m_callstacks.emplace_back(std::make_unique<CallStack>(func, args));
            }
            return;
        }
        break;
    }
    case VMOpcode::CALL_MODULEFUNC:
    {
        auto func = callstack->GetModule()->GetNthFunction(instruction.m_operand1);;
        auto idx = callstack->StackSize();
        callstack->Push(VMObjectPtr(func));
        return ExecuteInstruction(VMInstruction(VMOpcode::CALL, idx, instruction.m_operand2));
    }
    case VMOpcode::DUP:
        callstack->Dup(instruction.m_operand1);;
        break;
    case VMOpcode::RET:
    {
        auto val = callstack->Get(instruction.m_operand1);
        m_callstacks.pop_back();
        if (m_callstacks.empty()) {
            m_status = VMStatus::Exited;
            if (val->type() == VMObjectType::Integer) {
                m_exitStatus = VMGetInt(val);
            }
        } else {
            GetActiveCallstack()->Push(val);
        }
        break;
    }
    case VMOpcode::RETNULL:
        m_callstacks.pop_back();
        if (m_callstacks.empty()) {
            m_status = VMStatus::Exited;
        } else {
            GetActiveCallstack()->Push(GetNull());
        }
        break;
    case VMOpcode::PUSHSTR:
        callstack->Push(CreateString(GetActiveModule().GetNthString(instruction.m_operand1)));
        break;
    case VMOpcode::PUSHINT:
        callstack->Push(CreateInteger(GetActiveModule().GetNthInteger(instruction.m_operand1)));
        break;
    case VMOpcode::PUSHFLT:
        callstack->Push(CreateFloat(GetActiveModule().GetNthFloat(instruction.m_operand1)));
        break;
    case M2V::VMOpcode::PUSHNULL:
        callstack->Push(GetNull());
        break;
    case M2V::VMOpcode::PUSHTRUE:
        callstack->Push(GetTrue());
        break;
    case M2V::VMOpcode::PUSHFALSE:
        callstack->Push(GetFalse());
        break;
    case M2V::VMOpcode::PUSHARRAY:
        callstack->Push(CreateArray());
        break;
    case M2V::VMOpcode::PUSHOBJECT:
        callstack->Push(CreateObject());
        break;
    case M2V::VMOpcode::CREATE_CLOSURE:
        break;
    case VMOpcode::GLOBAL_GETVAR:
    {
        auto s = callstack->Get(instruction.m_operand1);
        if (s->type() != VMObjectType::String) {
            VMPanic("invalid key");
            break;
        }
        const auto key = VMGetString(s);
        if (m_globalObjects.count(key)) {
            callstack->Push(m_globalObjects.at(key));
        } else {
            VMPanic("undefine variable '" + key + "'");
        }
        break;
    }
    case VMOpcode::GLOBAL_SETVAR:
    {
        auto s = callstack->Get(instruction.m_operand1);
        if (s->type() != VMObjectType::String) {
            VMPanic("invalid key");
            break;
        }
        const auto key = VMGetString(s);
        if (m_globalObjects.count(key)) {
            m_globalObjects.at(key) = callstack->Get(instruction.m_operand2);
        } else {
            m_globalObjects.insert({key, callstack->Get(instruction.m_operand2)});
        }
        break;
    }
    case VMOpcode::MODULE_GETVAR:
    {
        auto s = callstack->Get(instruction.m_operand1);
        if (s->type() != VMObjectType::String) {
            VMPanic("invalid key");
            break;
        }
        const auto key = VMGetString(s);
        auto varOpt = callstack->GetModule()->GetModuleVariable(key);
        if (varOpt.has_value()) {
            callstack->Push(varOpt.value());
        } else {
            VMPanic("undefine variable '" + key + "'");
        }
        break;
    }
    case VMOpcode::MODULE_SETVAR:
    {
        auto s = callstack->Get(instruction.m_operand1);
        if (s->type() != VMObjectType::String) {
            VMPanic("invalid key");
            break;
        }
        const auto key = VMGetString(s);
        callstack->GetModule()->SetModuleVariable(key, callstack->Get(instruction.m_operand2));
        break;
    }
    case VMOpcode::LOAD_MODULE:
    {
        auto v1 = callstack->Get(instruction.m_operand1);
        if (v1->type() != VMObjectType::String) {
            VMPanic("fail to load module");
        }
        const auto s = VMGetString(v1);
        if (m_modules.count(s)) {
            callstack->Push(VMObjectPtr(m_modules.at(s)));
            callstack->Push(GetNull());
            callstack->Push(GetNull());
        } else {
            auto func = this->LoadModuleFromFile(s);
            MASSERT(m_modules.count(s));
            callstack->Push(VMObjectPtr(m_modules.at(s)));
            if (m_status == VMStatus::Running && func) {
                const uint16_t idx = callstack->StackSize();
                callstack->Push(VMObjectPtr(func));
                this->ExecuteInstruction(VMInstruction(VMOpcode::CALL, idx, 0));
            }
        }
        break;
    }

    case VMOpcode::BEGIN_FUNCTION:
    case VMOpcode::END_FUNCTION:
        break;

    case VMOpcode::JMP_TRUE:
    case VMOpcode::JMP_FLASE:
    {
        const bool isTrue = VMConvertToBool(callstack->Get(instruction.m_operand1));
        if (isTrue == (VMOpcode::JMP_TRUE == instruction.m_opcode)) {
            GetActiveCallstack()->Jmp(instruction.m_operand2);
        }
        break;
    }
    default:
        MUnreachable();
    }
    GetActiveCallstack()->MoveNext();
}

template<typename T>
static T number_operation(VMOpcode opcode, T v1, T v2)
{
    if (opcode == VMOpcode::ADD) {
        return v1 + v2;
    } else if (opcode == VMOpcode::SUB) {
        return v1 - v2;
    } else if (opcode == VMOpcode::MUL) {
        return v1 * v2;
    } else if (opcode == VMOpcode::DIV) {
        return v1 / v2;
    } else if (opcode == VMOpcode::MOD) {
        return static_cast<IntegerValueType>(v1) % static_cast<IntegerValueType>(v2);
    }
    MUnreachable();
    return T(0);
}

template<typename T1, typename T2>
static bool val_compare(VMOpcode opcode, T1 v1, T2 v2)
{
    if (opcode == VMOpcode::GREATER_EQ) {
        return v1 >= v2;
    } else if (opcode == VMOpcode::LESS_EQ) {
        return v1 <= v2;
    } else if (opcode == VMOpcode::GREATER) {
        return v1 > v2;
    } else if (opcode == VMOpcode::LESS) {
        return v1 < v2;
    }
    MUnreachable();
    return false;
}

VMObjectPtr
VirtualMachine::ExecuteBinaryOperator(VMOpcode opcode, VMObjectPtr op1, VMObjectPtr op2)
{
    switch (opcode) {
    case VMOpcode::ADD:
    case VMOpcode::SUB:
    case VMOpcode::MUL:
    case VMOpcode::DIV:
    case VMOpcode::MOD:
    {
        if (op1->type() != VMObjectType::Integer && op1->type() != VMObjectType::Float) {
            VMPanic("inproper type");
            return GetNull();
        }
        if (op2->type() != VMObjectType::Integer && op2->type() != VMObjectType::Float) {
            VMPanic("inproper type");
            return GetNull();
        }
        if ((op1->type() == VMObjectType::Float || op2->type() == VMObjectType::Float) && opcode == VMOpcode::MOD) {
            VMPanic("inproper type");
            return GetNull();
        }

        if (op1->type() == VMObjectType::Integer) {
            if (op2->type() == VMObjectType::Integer) {
                const auto val = number_operation(opcode, VMGetInt(op1), VMGetInt(op2)); 
                return CreateInteger(val);
            } else {
                const auto val = number_operation(opcode, static_cast<FloatValueType>(VMGetInt(op1)), VMGetFloat(op2)); 
                return CreateFloat(val);
            }
        } else {
            if (op2->type() == VMObjectType::Integer) {
                const auto val = number_operation(opcode, VMGetFloat(op1), static_cast<FloatValueType>(VMGetInt(op2))); 
                return CreateFloat(val);
            } else {
                const auto val = number_operation(opcode, VMGetFloat(op1), VMGetFloat(op2)); 
                return CreateFloat(val);
            }
        }
        break;
    }
    case VMOpcode::LOGICAL_AND:
    {
        const bool b1 = VMConvertToBool(op1);
        const bool b2 = VMConvertToBool(op2);
        return b1 && b2 ? GetTrue() : GetFalse();
    }
    case VMOpcode::LOGICAL_OR:
    {
        const bool b1 = VMConvertToBool(op1);
        const bool b2 = VMConvertToBool(op2);
        return b1 || b2 ? GetTrue() : GetFalse();
    }
    case VMOpcode::EQUAL:
        if (op1->type() != op2->type()) {
            return GetFalse();
        } else {
            if (op1->type() == VMObjectType::String) {
                auto s1 = static_cast<VMStringObject*>(&*op1);
                auto s2 = static_cast<VMStringObject*>(&*op2);
                if (s1->GetId() == s2->GetId() || s1->GetValue() == s2->GetValue()) {
                    return GetTrue();
                } else {
                    return GetFalse();
                }
            } else if (op1->type() == VMObjectType::Boolean) {
                return op1->GetId() == op1->GetId() ? GetTrue() : GetFalse();
            } else if (op1->type() == VMObjectType::Integer) {
                const auto s1 = static_cast<VMIntegerObject*>(&*op1);
                const auto s2 = static_cast<VMIntegerObject*>(&*op2);
                return s1->GetValue() == s2->GetValue() ? GetTrue() : GetFalse();
            } else if (op1->type() == VMObjectType::Float) {
                const auto s1 = static_cast<VMFloatObject*>(&*op1);
                const auto s2 = static_cast<VMFloatObject*>(&*op2);
                return s1->GetValue() == s2->GetValue() ? GetTrue() : GetFalse();
            } else {
                return op1->GetId() == op1->GetId() ? GetTrue() : GetFalse();
            }
        }
    case VMOpcode::INEQUAL:
        return ExecuteBinaryOperator(VMOpcode::EQUAL, op1, op2)->GetId() == GetTrue()->GetId() 
            ? GetFalse() : GetTrue();
    case VMOpcode::GREATER:
    case VMOpcode::GREATER_EQ:
    case VMOpcode::LESS:
    case VMOpcode::LESS_EQ:
        if (op1->type() != VMObjectType::Integer && op1->type() != VMObjectType::Float) {
            VMPanic("inproper comparison type");
            return GetNull();
        }
        if (op2->type() != VMObjectType::Integer && op2->type() != VMObjectType::Float) {
            VMPanic("inproper comparison type");
            return GetNull();
        }
        if (op1->type() == VMObjectType::Integer) {
            if (op2->type() == VMObjectType::Integer) {
                return val_compare(opcode, VMGetInt(op1), VMGetInt(op2)) ? GetTrue() : GetFalse(); 
            } else {
                return val_compare(opcode, VMGetInt(op1), VMGetFloat(op2)) ? GetTrue() : GetFalse(); 
            }
        } else {
            if (op2->type() == VMObjectType::Integer) {
                return val_compare(opcode, VMGetFloat(op1), VMGetInt(op2)) ? GetTrue() : GetFalse(); 
            } else {
                return val_compare(opcode, VMGetFloat(op1), VMGetFloat(op2)) ? GetTrue() : GetFalse(); 
            }
        }
    default:
        MUnreachable();
    }
    return VMObjectPtr(nullptr);
}

void VirtualMachine::MainLoop()
{
    size_t instructionCount = 0;
    while (m_status == VMStatus::Running) {
        const auto instruction = GetActiveCallstack()->FetchInstruction();
        ExecuteInstruction(instruction);
        instructionCount++;

        if (instructionCount % 10000000 == 0 && m_status == VMStatus::Running) {
            m_status = VMStatus::GC;
            RunGarbageColletion();
            m_status = VMStatus::Running;
        }
    }
}

VMFunctionObject* VirtualMachine::LoadModule(const ExecutionModule& module)
{
    auto mod = CreateModule(module.GetModuleName(), module);
    auto m = static_cast<VMModuleObject*>(&*mod);
    return m->GetInitializer();
}

VMFunctionObject* VirtualMachine::LoadModuleFromFile(const std::string& moduleName)
{
    return nullptr;
}

void VirtualMachine::RunGarbageColletion()
{
    MDEBUG_LOG("run garbage collection");
    m_gcGeneration++;

    for (auto& [_, v]: m_globalObjects) {
        v->MarkGeneration(m_gcGeneration);
    }
    for (auto& [_, m]: m_modules) {
        m->MarkGeneration(m_gcGeneration);
    }
    for (auto& stack: m_callstacks) {
        stack->MarkObjects(m_gcGeneration);
    }

    std::vector<VMObjectId> removedObjects;
    for (auto& [id, obj]: m_objects) {
        if (obj->GetGeneration() != m_gcGeneration) {
            MASSERT(obj->GetGeneration() < m_gcGeneration);
            removedObjects.push_back(id);
        }
    }

    for (auto& id: removedObjects) {
        m_objects.erase(id);
    }
}

