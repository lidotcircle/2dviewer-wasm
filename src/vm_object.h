#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include "common.h"


namespace M2V {

class VMInstruction;
class VMModuleObject;
class ExecutionModule;


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

class VMFunctionObject: public VMObject {
public:
    VMFunctionObject(VMObjectId id):
        VMObject(VMObjectType::Function, id) {}

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Function; }

    VMInstruction* GetInstruction(size_t instructionPointer);
    auto InstructionSize() const { return m_instructionSize; }

    void MarkGeneration(size_t gen) override;

private:
    size_t m_baseOffset;
    size_t m_instructionSize;
    VMModuleObject* m_module;
};

class VMModuleObject: public VMObject {
public:
    VMModuleObject(VMObjectId id):
        VMObject(VMObjectType::Module, id) {}

    static bool ClassOf(VMObjectPtr obj) { return obj->type() == VMObjectType::Module; }

    VMInstruction* GetInstruction(size_t instructioinPointer);
    std::string& GetNthString(size_t idx);
    IntegerValueType GetNthInteger(size_t idx);
    FloatValueType GetNthFloat(size_t idx);

private:
    std::unique_ptr<ExecutionModule> m_module;
};

}
