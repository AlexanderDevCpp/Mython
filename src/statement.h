#pragma once

#include "runtime.h"

#include <functional>

namespace ast {

using Statement = runtime::Executable;

// Выражение, возвращающее значение типа T,
// используется как основа для создания констант
template <typename T>
class ValueStatement : public Statement {
public:
    explicit ValueStatement(T v)
        : value_(std::move(v)) {
    }

    runtime::ObjectHolder Execute(runtime::Closure& /*closure*/,
                                  runtime::Context& /*context*/) override {
        return runtime::ObjectHolder::Share(value_);
    }

private:
    T value_;
};

using NumericConst = ValueStatement<runtime::Number>;
using StringConst = ValueStatement<runtime::String>;
using BoolConst = ValueStatement<runtime::Bool>;

/*
Вычисляет значение переменной либо цепочки вызовов полей объектов id1.id2.id3.
Например, выражение circle.center.x - цепочка вызовов полей объектов в инструкции:
x = circle.center.x
*/

class VariableValue : public Statement {
public:
    explicit VariableValue(const std::string& var_name);
    explicit VariableValue(std::vector<std::string> dotted_ids);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
private:
    std::vector<std::string> var_names_;
};

class Assignment : public Statement {
public:
    Assignment(std::string var, std::unique_ptr<Statement> rv);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::string var_name_;
    std::unique_ptr<Statement> var_value_;
};

class FieldAssignment : public Statement {
public:
    FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    VariableValue object_;
    std::string field_name_;
    std::unique_ptr<Statement> field_value_;
};

class None : public Statement {
public:
    runtime::ObjectHolder Execute([[maybe_unused]] runtime::Closure& closure,
                                  [[maybe_unused]] runtime::Context& context) override {
        return {};
    }
};

class Print : public Statement {
public:
    explicit Print(const std::string& name);
    // Инициализирует команду print для вывода значения выражения argument
    explicit Print(std::unique_ptr<Statement> argument);
    // Инициализирует команду print для вывода списка значений args
    explicit Print(std::vector<std::unique_ptr<Statement>> args);

    static std::unique_ptr<Print> Variable(const std::string& name);

    // Во время выполнения команды print вывод должен осуществляться в поток, возвращаемый из
    // context.GetOutputStream()
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::vector<std::unique_ptr<Statement>> args_;
    std::string name_;
};

class MethodCall : public Statement {
public:
    MethodCall(std::unique_ptr<Statement> object, std::string method,
               std::vector<std::unique_ptr<Statement>> args);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> object_;
    std::string method_;
    std::vector<std::unique_ptr<Statement>> args_;
};

/*
Создаёт новый экземпляр класса class_, передавая его конструктору набор параметров args.
Если в классе отсутствует метод __init__ с заданным количеством аргументов,
то экземпляр класса создаётся без вызова конструктора (поля объекта не будут проинициализированы):

class Person:
  def set_name(name):
    self.name = name

p = Person()
# Поле name будет иметь значение только после вызова метода set_name
p.set_name("Ivan")
*/
class NewInstance : public Statement {
public:
    explicit NewInstance(const runtime::Class& class_);
    NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;


private:
    runtime::ClassInstance instance;
    std::vector<std::unique_ptr<Statement>> args_;
};

// Базовый класс для унарных операций
class UnaryOperation : public Statement {
public:
    explicit UnaryOperation(std::unique_ptr<Statement> argument) 
        :argument_(std::move(argument))
    {
    }

    Statement* GetArgument() {
        return argument_.get();
    }

private:
    std::unique_ptr<Statement> argument_;
};

class Stringify : public UnaryOperation {
public:
    using UnaryOperation::UnaryOperation;
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// Родительский класс Бинарная операция с аргументами lhs и rhs
class BinaryOperation : public Statement {
public:
    BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs) 
        :lhs_(std::move(lhs)), rhs_(std::move(rhs))
    {
    }

    Statement* GetLeftArgument() {
        return lhs_.get();
    }

    Statement* GetRightArgument() {
        return rhs_.get();
    }
private:
    std::unique_ptr<Statement> lhs_;
    std::unique_ptr<Statement> rhs_;
};

class Add : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается сложение:
    //  число + число
    //  строка + строка
    //  объект1 + объект2, если у объект1 - пользовательский класс с методом _add__(rhs)
    // В противном случае при вычислении выбрасывается runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class Sub : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается вычитание:
    //  число - число
    // Если lhs и rhs - не числа, выбрасывается исключение runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class Mult : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается умножение:
    //  число * число
    // Если lhs и rhs - не числа, выбрасывается исключение runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class Div : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается деление:
    //  число / число
    // Если lhs и rhs - не числа, выбрасывается исключение runtime_error
    // Если rhs равен 0, выбрасывается исключение runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class Or : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class And : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class Not : public UnaryOperation {
public:
    using UnaryOperation::UnaryOperation;
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

class Compound : public Statement {
public:

    template <typename... Args>
    explicit Compound(Args&&... args) 
    {
        std::vector<Statement*> args_{ReleasePointer(std::move(args))...};
        for (auto arg : args_) {
            instructions_.push_back(std::unique_ptr<Statement>(arg));
        }
    }

    template <typename T>
    Statement* ReleasePointer(std::unique_ptr<T>&& ptr = nullptr) {
        return ptr.release();
    }

    void AddStatement(std::unique_ptr<Statement> stmt) {
        instructions_.push_back(std::move(stmt));
    }

    // Последовательно выполняет добавленные инструкции. Возвращает None
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::vector<std::unique_ptr<Statement>> instructions_;
};

class MethodBody : public Statement {
public:
    explicit MethodBody(std::unique_ptr<Statement>&& body);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> body_;
};

class Return : public Statement {
public:
    explicit Return(std::unique_ptr<Statement> statement) 
        :statement_(std::move(statement))
    {
    }

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> statement_;
};

class ClassDefinition : public Statement {
public:
    explicit ClassDefinition(runtime::ObjectHolder cls);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    runtime::ObjectHolder cls_;
};

class IfElse : public Statement {
public:
    IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
           std::unique_ptr<Statement> else_body);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
private:
    std::unique_ptr<Statement> condition_;
    std::unique_ptr<Statement> if_body_;
    std::unique_ptr<Statement> else_body_;
};

// Операция сравнения
class Comparison : public BinaryOperation {
public:
    using Comparator = std::function<bool(const runtime::ObjectHolder&,
                                          const runtime::ObjectHolder&, runtime::Context&)>;

    Comparison(Comparator cmp, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs);

    // Вычисляет значение выражений lhs и rhs и возвращает результат работы comparator,
    // приведённый к типу runtime::Bool
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    Comparator cmp_;

};

}  // namespace ast
