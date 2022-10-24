#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    return closure[var_name_] = var_value_.get()->Execute(closure, context);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) 
    :var_name_(var), var_value_(std::move(rv))
{
}

VariableValue::VariableValue(const std::string& var_name) {
    var_names_.push_back(var_name);
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids) {
    var_names_ = dotted_ids;
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
    runtime::ObjectHolder obj;

    try {
        obj = closure.at(var_names_[0]);
    }
    catch (const std::exception&) {
        throw std::runtime_error("execute error");
    }

    for (size_t i = 1; i < var_names_.size(); i++) {
        obj = obj.TryAs<runtime::ClassInstance>()->Fields().at(var_names_[i]);
    }

    return runtime::ObjectHolder::Share(*obj.Get());
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return std::make_unique<Print>(name);
}

Print::Print(const std::string& name)
    :name_(name)
{
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args) 
    :args_(std::move(args))
{
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    std::ostream& output = context.GetOutputStream();

    if (!name_.empty()) {
        closure.at(name_).Get()->Print(output, context);
        output << std::endl;
    }
    else if(args_.size()) {
        for (size_t i = 0; i < args_.size(); i++) {
            auto ptr = args_[i].get()->Execute(closure, context);
            if (ptr) {
                ptr->Print(output, context);
            }
            else {
                output << "None";
            }

            if (i < (args_.size() - 1)) {
                output << ' ';
            }
            else {
                output << std::endl;
            }
        }
    }
    else {
        output << std::endl;
    }

    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args) 
    :object_(std::move(object)),method_(std::move(method)), args_(std::move(args))
{
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    vector<ObjectHolder> args;

    for (auto& arg : args_) {
        args.push_back(arg.get()->Execute(closure, context));
    }

    return object_.get()->Execute(closure,context).TryAs<runtime::ClassInstance>()->Call(method_, args, context);

    return {};
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    std::stringstream str;

    auto ptr = GetArgument()->Execute(closure, context).Get();

    if (ptr) {
        ptr->Print(str, context);
    }
    else {
        str << "None";
    }

    if (!str.str().empty()) {
        return runtime::ObjectHolder::Own<runtime::String>(str.str());
    }
    else {
        str << ptr;
    }

    return runtime::ObjectHolder::Own<runtime::String>(str.str());
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    auto lhs = GetLeftArgument()->Execute(closure, context);
    auto rhs = GetRightArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
        auto lhs_value = lhs.TryAs<runtime::Number>()->GetValue();
        auto rhs_value = rhs.TryAs<runtime::Number>()->GetValue();

        return runtime::ObjectHolder::Own<runtime::Number>(lhs_value + rhs_value);
    }
    if (lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()) {
        auto lhs_value = lhs.TryAs<runtime::String>()->GetValue();
        auto rhs_value = rhs.TryAs<runtime::String>()->GetValue();

        return runtime::ObjectHolder::Own<runtime::String>(lhs_value + rhs_value);
    }
    if (lhs.TryAs<runtime::ClassInstance>()) {
        if (lhs.TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1)) {
            return lhs.TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, { rhs }, context);
        }
    }

    throw std::runtime_error("Add Error");
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    auto lhs = GetLeftArgument()->Execute(closure, context);
    auto rhs = GetRightArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
        auto lhs_value = lhs.TryAs<runtime::Number>()->GetValue();
        auto rhs_value = rhs.TryAs<runtime::Number>()->GetValue();

        return runtime::ObjectHolder::Own<runtime::Number>(lhs_value - rhs_value);
    }

    throw std::runtime_error("Sub Error");
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    auto lhs = GetLeftArgument()->Execute(closure, context);
    auto rhs = GetRightArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
        auto lhs_value = lhs.TryAs<runtime::Number>()->GetValue();
        auto rhs_value = rhs.TryAs<runtime::Number>()->GetValue();

        return runtime::ObjectHolder::Own<runtime::Number>(lhs_value * rhs_value);
    }

    throw std::runtime_error("Mult Error");
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    auto lhs = GetLeftArgument()->Execute(closure, context);
    auto rhs = GetRightArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Number>() && rhs.TryAs<runtime::Number>()) {
        if (rhs.TryAs<runtime::Number>()->GetValue() == 0) {
            throw std::runtime_error("Div by 0");
        }
        else {
            auto lhs_value = lhs.TryAs<runtime::Number>()->GetValue();
            auto rhs_value = rhs.TryAs<runtime::Number>()->GetValue();

            return runtime::ObjectHolder::Own<runtime::Number>(lhs_value / rhs_value);
        }
    }

    throw std::runtime_error("Div Error");
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (auto& instruction : instructions_) {
        instruction.get()->Execute(closure, context);
    }

    return runtime::ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    auto result = statement_.get()->Execute(closure, context);

    std::stringstream ss;
    result.Get()->Print(ss, context);

    throw std::runtime_error(ss.str());

    return {};
}

ClassDefinition::ClassDefinition(ObjectHolder cls) 
    :cls_(cls)
{
}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
    return closure[cls_.TryAs<runtime::Class>()->GetName()] = runtime::ObjectHolder::Share(*cls_.TryAs<runtime::Class>());
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
    :object_(object), field_name_(field_name), field_value_(std::move(rv))
{

}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    return object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields()[field_name_] = field_value_.get()->Execute(closure, context);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body)
    :condition_(std::move(condition)),if_body_(std::move(if_body)),else_body_(std::move(else_body))
{
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    auto result = condition_.get()->Execute(closure, context).TryAs<runtime::Bool>()->GetValue();

    if (result) {
        return if_body_.get()->Execute(closure, context);
    }
    if (else_body_ != nullptr) {
        return else_body_.get()->Execute(closure, context);
    }

    return {};
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    auto lhs = GetLeftArgument()->Execute(closure, context);
    auto rhs = GetRightArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {
        if (lhs.TryAs<runtime::Bool>()->GetValue() == false) {
            auto lhs_value = lhs.TryAs<runtime::Bool>()->GetValue();
            auto rhs_value = rhs.TryAs<runtime::Bool>()->GetValue();

            return runtime::ObjectHolder::Own<runtime::Bool>(lhs_value || rhs_value);
        }

        return runtime::ObjectHolder::Own<runtime::Bool>(true);
    }

    throw std::runtime_error("Or Error");
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    auto lhs = GetLeftArgument()->Execute(closure, context);
    auto rhs = GetRightArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Bool>() && rhs.TryAs<runtime::Bool>()) {
       
        if (lhs.TryAs<runtime::Bool>()->GetValue() == true) {
            auto lhs_value = lhs.TryAs<runtime::Bool>()->GetValue();
            auto rhs_value = rhs.TryAs<runtime::Bool>()->GetValue();

            return runtime::ObjectHolder::Own<runtime::Bool>(lhs_value && rhs_value);
        }

        return runtime::ObjectHolder::Own<runtime::Bool>(false);
    }

    throw std::runtime_error("And Error");
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    auto lhs = GetArgument()->Execute(closure, context);

    if (lhs.TryAs<runtime::Bool>()) {
        if (lhs.TryAs<runtime::Bool>()->GetValue() == true) {
            return runtime::ObjectHolder::Own<runtime::Bool>(false);
        }

        return runtime::ObjectHolder::Own<runtime::Bool>(true);
    }

    throw std::runtime_error("Not Error");
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    return runtime::ObjectHolder::Own<runtime::Bool>(cmp_(this->GetLeftArgument()->Execute(closure, context), this->GetRightArgument()->Execute(closure, context), context));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    :instance(class_), args_(std::move(args))
{
}

NewInstance::NewInstance(const runtime::Class& class_)
    :instance(class_)
{
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    std::vector<ObjectHolder> args;

    for (auto& arg : args_) {
        args.push_back(arg.get()->Execute(closure, context));
    }

    if (instance.HasMethod(INIT_METHOD, args.size())) {
        instance.Call(INIT_METHOD, args, context);
    }

    return runtime::ObjectHolder::Share(instance);
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body) 
    :body_(std::move(body))
{
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {

    try {
        auto result = body_.get()->Execute(closure, context);
    }
    catch (const std::exception& ex) {
        std::string result  = ex.what();
        return runtime::ObjectHolder::Own<runtime::String>(result);
    }
    return runtime::ObjectHolder::None();
}

}  // namespace ast