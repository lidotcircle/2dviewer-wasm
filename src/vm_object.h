#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "common.h"


namespace M2V {

class VMInstruction;
class VMModuleObject;
class ExecutionModule;
class VirtualMachine;
class CallStack;


enum class VMObjectType {
    Integer = 1, Boolean, Float, String,
    Array, Object, Null,
    Function, Module,
};
using VMObjectId = std::size_t;

class VMObject {
public:
    VMObject(VMObjectType type, VMObjectId id): m_type(type), m_id(id), m_gen(0) {}

    auto GetId() const { return m_id; }
    auto type() const { return m_type; }
    virtual ~VMObject() = default;

    virtual void MarkGeneration(size_t gen)
    {
        MASSERT(gen > m_gen);
        m_gen = gen;
    }

    size_t GetGeneration() const { return m_gen; }

private:
    VMObjectType m_type;
    VMObjectId m_id;
    size_t m_gen;
};
using VMObjectPtr = QPtr<VMObject>;;

using IntegerValueType = int64_t;
class VMIntegerObject: public VMObject {
public:
    VMIntegerObject(VMObjectId id, IntegerValueType val):
        VMObject(VMObjectType::Integer, id), m_val(val) {}

    auto GetValue() const { return m_val; }

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Integer; }

private:
    IntegerValueType m_val;
};

class VMBooleanObject: public VMObject {
public:
    VMBooleanObject(VMObjectId id, bool val):
        VMObject(VMObjectType::Boolean, id), m_val(val) {}

    bool GetValue() const { return m_val; }

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Boolean; }

private:
    bool m_val;
};

using FloatValueType = double;
class VMFloatObject: public VMObject {
public:
    VMFloatObject(VMObjectId id, FloatValueType val):
        VMObject(VMObjectType::Float, id), m_val(val) {}

    auto GetValue() const { return m_val; }

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Float; }

private:
    FloatValueType m_val;
};

using StringValueType = std::string;
class VMStringObject: public VMObject {
public:
    VMStringObject(VMObjectId id, StringValueType val):
        VMObject(VMObjectType::String, id), m_val(val) {}

    auto& GetValue() const { return m_val; }

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::String; }

private:
    StringValueType m_val;
};

class VMArrayObject: public VMObject {
public:
    VMArrayObject(VMObjectId id):
        VMObject(VMObjectType::Array, id) {}

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Array; }

    void MarkGeneration(size_t gen) override {
        if (gen != GetGeneration()) {
            VMObject::MarkGeneration(gen);
            for(auto& o: m_objects) {
                o->MarkGeneration(gen);
            }
        }
    }

    auto size() const { return m_objects.size(); }
    void clear() { m_objects.clear(); }
    void push(VMObjectPtr obj) { m_objects.push_back(obj); }
    void insert(size_t idx, VMObjectPtr obj) { m_objects.insert(m_objects.begin() + idx, obj); }
    auto get(size_t idx) const { return m_objects.at(idx); }

private:
    std::vector<VMObjectPtr> m_objects;
};

class VMMapObject: public VMObject {
public:
    VMMapObject(VMObjectId id):
        VMObject(VMObjectType::Object, id) {}

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Object; }

    void MarkGeneration(size_t gen) override {
        if (gen != GetGeneration()) {
            VMObject::MarkGeneration(gen);
            for(auto& [_, o]: m_map) {
                o->MarkGeneration(gen);
            }
        }
    }

    auto size() const { return m_map.size(); }
    void clear() { m_map.clear(); }
    void insert(const std::string& key, VMObjectPtr obj) { m_map.insert({key, obj}); }
    bool has(const std::string& key) const { return m_map.count(key); }
    void erase(const std::string& key) { m_map.erase(key); }
    auto get(const std::string& key) const { return m_map.at(key); }

private:
    std::unordered_map<std::string,VMObjectPtr> m_map;
};

class VMNullObject: public VMObject {
public:
    VMNullObject(VMObjectId id):
        VMObject(VMObjectType::Null, id) {}

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Null; }

private:
    std::unordered_map<std::string,VMObjectPtr> m_map;
};

using InternalFunctionType = std::function<int(const VirtualMachine&, const CallStack&)>;
class VMFunctionObject: public VMObject {
public:
    VMFunctionObject(VMObjectId id, VMModuleObject* module, size_t baseOffset, 
                     size_t instructionSize, std::vector<VMObjectPtr> capturedVariables, bool varArgs):
        VMObject(VMObjectType::Function, id), m_baseOffset(baseOffset), m_instructionSize(instructionSize),
        m_module(module), m_capturedVariable(capturedVariables), m_varArgs(varArgs), m_internalFunction() {}

    VMFunctionObject(VMObjectId id, InternalFunctionType func):
        VMObject(VMObjectType::Function, id), m_baseOffset(0), m_instructionSize(0),
        m_module(nullptr), m_capturedVariable(), m_varArgs(false), m_internalFunction(func) {}

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Function; }

    VMInstruction* GetInstruction(size_t instructionPointer);
    auto InstructionSize() const { return m_instructionSize; }

    bool isClosure() const { return m_capturedVariable.size() > 0; }
    bool isInternal() const { return m_internalFunction != nullptr; }
    bool isVarArgs() const { return m_varArgs; }

    auto GetModule() { return m_module; }

    int invokeInternal(const VirtualMachine& vm, const CallStack& stack)
    {
        MASSERT(m_internalFunction);
        return m_internalFunction(vm, stack);
    }

    auto& GetCaptured() const { return m_capturedVariable; }

    void MarkGeneration(size_t gen) override;

private:
    size_t m_baseOffset;
    size_t m_instructionSize;
    std::vector<VMObjectPtr> m_capturedVariable;
    VMModuleObject* m_module;
    bool m_varArgs;
    InternalFunctionType m_internalFunction;
};

class VMModuleObject: public VMObject {
public:
    VMModuleObject(VMObjectId id, const ExecutionModule& module, VirtualMachine& vm);

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Module; }

    const VMInstruction& GetInstruction(size_t instructioinPointer) const;
    const std::string& GetNthString(size_t idx) const;
    IntegerValueType GetNthInteger(size_t idx) const;
    FloatValueType GetNthFloat(size_t idx) const;
    VMFunctionObject* GetNthFunction(size_t idx)
    {
        MASSERT(m_functions.size() > idx);
        return m_functions.at(idx);
    }

     const std::string& GetModuleName() const;

     std::optional<VMObjectPtr> GetModuleVariable(const std::string& name);
     void SetModuleVariable(const std::string& name, VMObjectPtr obj);

     VMFunctionObject* GetInitializer();

private:
    std::unique_ptr<ExecutionModule> m_module;
    std::unordered_map<std::string, VMObjectPtr> m_moduleVariable;
    std::vector<VMFunctionObject*> m_functions;
};

}
