#pragma once
#include "Pch.h"

namespace json {

class Value;
using Object = std::vector<std::pair<std::string, Value>>;
using Array  = std::vector<Value>;

class Value
{
public:
    enum Type { Null, Bool, Number, String, ObjectT, ArrayT };

    Value() : type_(Null) {}
    Value(bool b) : type_(Bool), b_(b) {}
    Value(int v) : type_(Number), d_(v) {}
    Value(double v) : type_(Number), d_(v) {}
    Value(const char* s) : type_(String), s_(s) {}
    Value(const std::string& s) : type_(String), s_(s) {}

    static Value makeObject() { Value v; v.type_ = ObjectT; return v; }
    static Value makeArray()  { Value v; v.type_ = ArrayT;  return v; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool isString() const { return type_ == String; }
    bool isObject() const { return type_ == ObjectT; }
    bool isArray() const { return type_ == ArrayT; }

    const std::string& asString() const { return s_; }
    double asNumber() const { return d_; }
    bool   asBool() const { return b_; }

    void set(const std::string& key, Value v)
    {
        for (auto& kv : obj_) if (kv.first == key) { kv.second = std::move(v); return; }
        obj_.emplace_back(key, std::move(v));
    }
    void push(Value v) { arr_.push_back(std::move(v)); }

    const Value* find(const std::string& key) const
    {
        for (auto& kv : obj_) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    const Value* at(size_t i) const
    {
        return (type_ == ArrayT && i < arr_.size()) ? &arr_[i] : nullptr;
    }
    size_t size() const { return type_ == ArrayT ? arr_.size() : obj_.size(); }
    const Array& array() const { return arr_; }
    const Object& object() const { return obj_; }

private:
    Type type_ = Null;
    bool b_ = false;
    double d_ = 0;
    std::string s_;
    Object obj_;
    Array  arr_;

    friend bool parse(const std::string& s, Value& out);
    friend std::string write(const Value& v, bool pretty);
};

bool parse(const std::string& s, Value& out);
std::string write(const Value& v, bool pretty = false);

}
