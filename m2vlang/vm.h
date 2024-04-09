#pragma once
#include "vm_object.h"
#include <memory>


namespace M2V {

enum class VMOpcode: uint16_t {
    NOP = 0,         // NOP,
    POPN,            // popn n
    ADD,             // add idx1, idx2
    SUB,             // sub idx1, idx2
    MUL,             // MUL idx1, idx2
    DIV,             // DIV idx1, idx2
    MOD,             // MOD idx1, idx2
    EQUAL,           // EQUAL idx1, idx2
    INEQUAL,         // INEQUAL idx1, idx2
    GREATER,         // GREATER idx1, idx2
    LESS,            // LESS idx1, idx2
    GREATER_EQ,      // GREATER_EQ idx1, idx2
    LESS_EQ,         // LESS idx1, idx2
    LOGICAL_AND,     // LAND idx1, idx2
    LOGICAL_OR,      // LOR idx1, idx2
    CALL,            // call funcidx, nargs
    CALL_MODULEFUNC, // call modfuncIdx, nargs
    DUP,             // dup idx
    RET,             // RET idx
    RETNULL,         // RETNULL
    PUSHSTR,         // PUSHSTR strLiteralIdx
    PUSHINT,         // PUSHINT intLiteralIdx
    PUSHFLT,         // PUSHFLT fltLiteralIdx
    PUSHNULL,
    PUSHTRUE,
    PUSHFALSE,
    PUSHARRAY,
    PUSHOBJECT,
    CREATE_CLOSURE,  // CREATE_CLOSURE, ncaptured
    GLOBAL_GETVAR,   // GGet nameidx1
    GLOBAL_SETVAR,   // GSet nameidx1, idx2
    MODULE_GETVAR,   // MGet nameidx1
    MODULE_SETVAR,   // GSet nameidx1, idx2
    LOAD_MODULE,     // Load namestr
    BEGIN_FUNCTION,  // MARK Begin of a function
    END_FUNCTION,    // MARK end of a function

    JMP_TRUE,        // JMP_TRUE idx, offset
    JMP_FLASE,       // JMP_FALSE idx, offset
};

struct VMInstruction {
    VMOpcode m_opcode;
    int16_t m_operand1;
    int16_t m_operand2;

    VMInstruction():
        m_opcode(VMOpcode::NOP), m_operand1(0), m_operand2(0) {}

    VMInstruction(VMOpcode opcode, int16_t op1, int16_t op2):
        m_opcode(opcode), m_operand1(op1), m_operand2(op2) {}
};

struct StringLiteralPool {
    std::vector<std::string> m_strings;
};
struct IntegerLiteralPool {
    std::vector<IntegerValueType> m_integers;
};
struct FloatLiteralPool {
    std::vector<FloatValueType> m_floatValues;
};
struct FunctionInfo {
    std::string m_name;
    size_t m_begin;
    size_t m_size;
    bool   m_varadic;
};

class ExecutionModule {
public:
    const std::string& GetModuleName() const { return m_moduleName; }
    const std::string& GetNthString(size_t idx) const
    {
        MASSERT(idx < m_stringPool.m_strings.size());
        return m_stringPool.m_strings.at(idx);
    }
    IntegerValueType GetNthInt(size_t idx) const
    {
        MASSERT(idx < m_integerPool.m_integers.size());
        return m_integerPool.m_integers.at(idx);
    }
    FloatValueType GetNthFloat(size_t idx) const
    {
        MASSERT(idx < m_floatPool.m_floatValues.size());
        return m_floatPool.m_floatValues.at(idx);
    }
    VMInstruction& GetInstruction(size_t idx)
    {
        MASSERT(idx < m_instructions.size());
        return m_instructions.at(idx);
    }

    const auto& GetFunctionTable() const { return m_functionTable; }
    const std::optional<size_t> ModuleIntializer() const { return m_initializer; }

private:
    std::string m_moduleName;
    StringLiteralPool m_stringPool;
    IntegerLiteralPool m_integerPool;
    FloatLiteralPool m_floatPool;
    std::vector<FunctionInfo> m_functionTable;
    std::optional<size_t> m_initializer;
    std::vector<VMInstruction> m_instructions;
};

class CallStack {
public:
    VMObjectPtr Get(int index)
    {
        if (index > 0) {
            MASSERT(index < m_stackValues.size());
            return m_stackValues.at(index);
        } else {
            size_t n = -index;
            MASSERT(n < m_argsAndCaptured.size());
            return m_argsAndCaptured.at(n);
        }
    }

    std::vector<VMObjectPtr> GetTopN(size_t n) const {
        MASSERT(m_stackValues.size() > n);
        std::vector<VMObjectPtr> ans(m_stackValues.end() - n, m_stackValues.end());
        return ans;
    }

    void Pop(size_t n) {
        MASSERT(n < m_stackValues.size());
        m_stackValues.erase(m_stackValues.end() - n, m_stackValues.end());
    }

    void Push(VMObjectPtr obj) {
        m_stackValues.push_back(obj);
    }

    void Dup(int idx) {
        m_stackValues.push_back(Get(idx));
    }

    size_t StackSize() const { return m_stackValues.size(); }

    VMInstruction FetchInstruction()
    {
        return *m_function->GetInstruction(m_instructionPtr);
    }

    void MoveNext() {
        m_instructionPtr++;
        MASSERT(m_instructionPtr < m_function->InstructionSize());
    }

    void Jmp(int offset) {
        if (offset > 0) {
            m_instructionPtr += offset;
            MASSERT(m_instructionPtr < m_function->InstructionSize());
        } else {
            MASSERT(m_instructionPtr >= -offset);
            m_instructionPtr -= (-offset);
        }
    }

    void MarkObjects(size_t gen) {
        for (auto& v: m_stackValues) {
            v->MarkGeneration(gen);
        }
        for (auto& v: m_argsAndCaptured) {
            v->MarkGeneration(gen);
        }
        m_function->MarkGeneration(gen);
    }

    VMModuleObject* GetModule()
    {
        return m_function->GetModule();
    }

    CallStack(VMFunctionObject* function, const std::vector<VMObjectPtr>& args):
        m_function(function), m_argsAndCaptured(function->GetCaptured())
    {
        m_argsAndCaptured.insert(m_argsAndCaptured.end(), args.begin(), args.end());
    }

private:
    std::vector<VMObjectPtr> m_stackValues;
    std::vector<VMObjectPtr> m_argsAndCaptured;
    VMFunctionObject* m_function;
    size_t m_instructionPtr;
};

class VirtualMachine {
public:
    VirtualMachine();

    void ExecuteModule(const ExecutionModule& module, const std::string& funcname);

protected:
    friend class VMModuleObject;

    template<typename ... Args>
    VMObjectPtr CreateFunction(Args&& ... args)
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMFunctionObject>(id, std::forward<Args>(args)...);
        VMObjectPtr ans(pt.get());
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

private:
    enum class VMStatus
    {
        Uninit, Initialized, Running, GC, Exited, Panic
    };

    VMObjectPtr GetNull() const { return VMObjectPtr(m_nullVal.get()); }
    VMObjectPtr GetTrue() const { return VMObjectPtr(m_trueVal.get()); }
    VMObjectPtr GetFalse() const { return VMObjectPtr(m_falseVal.get()); }

    void ExecuteInstruction(const VMInstruction& instruction);
    VMObjectPtr ExecuteBinaryOperator(VMOpcode opcode, VMObjectPtr op1, VMObjectPtr op2);

    void MainLoop();

    void VMPanic(const std::string&);
    void VMExit(int status);

    CallStack* GetActiveCallstack()
    {
        MASSERT(!m_callstacks.empty());
        return m_callstacks.back().get();
    }

    VMModuleObject& GetActiveModule();

    VMObjectPtr CreateInteger(IntegerValueType val)
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMIntegerObject>(id, val);
        VMObjectPtr ans(pt.get());
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

    VMObjectPtr CreateFloat(FloatValueType val)
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMFloatObject>(id, val);
        VMObjectPtr ans(pt.get());
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

    VMObjectPtr CreateString(const std::string& val)
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMStringObject>(id, val);
        VMObjectPtr ans(pt.get());
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

    VMObjectPtr CreateArray()
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMArrayObject>(id);
        VMObjectPtr ans(pt.get());
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

    VMObjectPtr CreateObject()
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMMapObject>(id);
        VMObjectPtr ans(pt.get());
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

    VMObjectPtr CreateModule(const std::string& moduleName, const ExecutionModule& module)
    {
        const auto id = m_nextFreeId++;
        auto pt = std::make_unique<VMModuleObject>(id, module, *this);
        VMObjectPtr ans(pt.get());
        m_modules.insert({moduleName, pt.get()});
        m_objects.insert({id, std::move(pt)});
        return ans;
    }

    VMFunctionObject* LoadModule(const ExecutionModule& module);
    VMFunctionObject* LoadModuleFromFile(const std::string& moduleName);

    void RunGarbageColletion();

    VMObjectId m_nextFreeId;
    VMStatus m_status;
    size_t m_gcGeneration;
    std::unordered_map<VMObjectId, std::unique_ptr<VMObject>> m_objects;
    std::unordered_map<std::string, VMObjectPtr> m_globalObjects;
    std::unique_ptr<VMNullObject> m_nullVal;
    std::unique_ptr<VMBooleanObject> m_trueVal, m_falseVal;
    std::vector<std::unique_ptr<CallStack>> m_callstacks;
    std::unordered_map<std::string,VMModuleObject*> m_modules;

    std::optional<int> m_exitStatus;
};

}
