#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {
    
    namespace {
        const string STR_METHOD = "__str__"s;
        const string EQ_METHOD = "__eq__"s;
        const string LESS_METHOD = "__lt__"s;
    }  // namespace


ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (object.TryAs<Bool>()) {
        return object.TryAs<Bool>()->GetValue();
    }
    if (object.TryAs<String>()) {
        return object.TryAs<String>()->GetValue().size();
    }
    if (object.TryAs<Number>()) {
        return object.TryAs<Number>()->GetValue();
    }

    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (auto ptr = class_.GetMethod(STR_METHOD)) {
        ptr->body.get()->Execute(fields_, context).Get()->Print(os, context);
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    if (auto* ptr = class_.GetMethod(method)) {
        if (ptr->formal_params.size() == argument_count) {
            return true;
        }
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return fields_;
}

const Closure& ClassInstance::Fields() const {
    return fields_;
}

ClassInstance::ClassInstance(const Class& cls) 
    :class_(cls)
{
    fields_["self"] = ObjectHolder().Share(*this);
}

ObjectHolder ClassInstance::Call(const std::string& method, const std::vector<ObjectHolder>& actual_args, Context& context) {

    const Method* method_ptr = class_.GetMethod(method);

    if (method_ptr == nullptr || actual_args.size() != method_ptr->formal_params.size()) {
        throw std::runtime_error("Not implemented"s);
    }

    Closure closure;
    closure["self"] = ObjectHolder::Share(*this);

    for (size_t i = 0; i < actual_args.size(); i++) {
        closure[method_ptr->formal_params[i]] = actual_args[i];
    }

    return method_ptr->body->Execute(closure, context);

}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent) {
    name_ = name;
    parent_ = parent;

    for (auto& method : methods) {
        methods_.insert({ method.name, std::move(method) });
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    if (methods_.count(name) != 0) {
        return &methods_.at(name);
    }
    if (parent_ != nullptr) {
        return parent_->GetMethod(name);
    }

    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, Context& /*context*/) {
    os << "Class " << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

template <typename T, typename Pred>
bool Compare(const ObjectHolder& lhs, const ObjectHolder& rhs, Pred predicate) {
    return predicate(lhs.TryAs<T>()->GetValue(), rhs.TryAs<T>()->GetValue());
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return Compare<Number>(lhs, rhs, std::equal_to());
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return Compare<String>(lhs, rhs, std::equal_to());
    }
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return Compare<Bool>(lhs, rhs, std::equal_to());
    }
    if (!lhs.operator bool() && !rhs.operator bool() ) {
        return true;
    }
    if (lhs.TryAs<ClassInstance>()) {
        return IsTrue(lhs.TryAs<ClassInstance>()->Call(EQ_METHOD, { rhs }, context));
    }
    if (lhs.Get() == rhs.Get()) {
        return true;
    }

    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return Compare<Number>(lhs, rhs, std::less());
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return Compare<String>(lhs, rhs, std::less());
    }
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return Compare<Bool>(lhs, rhs, std::less());
    }
    if (lhs.TryAs<ClassInstance>()) {
        return IsTrue(lhs.TryAs<ClassInstance>()->Call(LESS_METHOD, { rhs }, context));
    }

    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }
    catch (const std::exception&) {
        throw std::runtime_error("Cannot compare objects for greater"s);
    }
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (Less(lhs, rhs, context)) {
        return true;
    }

    return Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!Less(lhs, rhs, context)) {
        return true;
    }

    return false;
}

}  // namespace runtime
