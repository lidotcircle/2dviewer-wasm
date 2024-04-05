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
    auto& callstack = GetActiveCallstack();
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
    case VMOpcode::CALL_VARGS:
    case VMOpcode::DUP:
        callstack->Dup(instruction.m_operand1);;
        break;
    case VMOpcode::RET:
    case VMOpcode::RETNULL:
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
    case VMOpcode::GLOBAL_GETVAR:
    case VMOpcode::GLOBAL_SETVAR:
    case VMOpcode::MODULE_GETVAR:
    case VMOpcode::MODULE_SETVAR:
    case VMOpcode::LOAD_MODULE:
    {
        auto v1 = callstack->Get(instruction.m_operand1);
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

        if (instructionCount % 10000000 == 0) {
            m_status = VMStatus::GC;
            RunGarbageColletion();
            m_status = VMStatus::Running;
        }
    }
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

