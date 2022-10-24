#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <set>

namespace parse {

namespace token_type {
struct Number {  // Лексема «число»
    int value;   // число
};

struct Id {             // Лексема «идентификатор»
    std::string value;  // Имя идентификатора
};

struct Char {    // Лексема «символ»
    char value;  // код символа
};

struct String {  // Лексема «строковая константа»
    std::string value;
};

struct Class {};    // Лексема «class»
struct Return {};   // Лексема «return»
struct If {};       // Лексема «if»
struct Else {};     // Лексема «else»
struct Def {};      // Лексема «def»
struct Newline {};  // Лексема «конец строки»
struct Print {};    // Лексема «print»
struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
struct Dedent {};  // Лексема «уменьшение отступа»
struct Eof {};     // Лексема «конец файла»
struct And {};     // Лексема «and»
struct Or {};      // Лексема «or»
struct Not {};     // Лексема «not»
struct Eq {};      // Лексема «==»
struct NotEq {};   // Лексема «!=»
struct LessOrEq {};     // Лексема «<=»
struct GreaterOrEq {};  // Лексема «>=»
struct None {};         // Лексема «None»
struct True {};         // Лексема «True»
struct False {};        // Лексема «False»
}  // namespace token_type

using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                   token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                   token_type::None, token_type::True, token_type::False, token_type::Eof>;

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
public:
    explicit Lexer(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] const Token& CurrentToken() const;

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Expect() const {
        using namespace std::literals;
        if (tokens_[current_token_].Is<T>()) {
            return tokens_[current_token_].As<T>();
        }
        throw LexerError("Expect error"s);
    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Expect(const U& value) const {
        using namespace std::literals;
        if (tokens_[current_token_].Is<T>()) {
            if (tokens_[current_token_].As<T>().value != value) {
                throw LexerError("Expect error");
            }
        }
        else {
            throw LexerError("Expect error");
        }
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& ExpectNext() {
        this->NextToken();
        return this->Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void ExpectNext(const U& value) {
        this->NextToken();
        this->Expect<T,U>(value);
    }

private:

    void ReadNumber(std::istream& input);
    void ReadString(std::istream& input);
    void ReadId(std::istream& input);
    void ReadIndents(std::istream& input, int& old_indent);
    void ReadChar(std::istream& input);
    void ReadOperation(std::istream& input);

    std::map<std::string, Token> keywords_ { 
        {"class",token_type::Class()}, {"return",token_type::Return()} , {"if", token_type::If()}, 
        {"else" ,token_type::Else()}, {"def" ,token_type::Def()}, {"print" ,token_type::Print()}, 
        {"or" ,token_type::Or()}, {"None" ,token_type::None()},  {"and" ,token_type::And()}, 
        {"not" ,token_type::Not()}, {"True" ,token_type::True()}, {"False" ,token_type::False()}};

    std::map<char, Token> operations_char_{
        {'+',token_type::Char{'+'}}, {'-',token_type::Char{'-'}}, {'*',token_type::Char{'*'}},
        {'/',token_type::Char{'/'}}, {'=',token_type::Char{'='}}, {'>',token_type::Char{'>'}},
        {'<',token_type::Char{'<'}}, {'!',token_type::None()} };

    std::map<std::string, Token> operations_ {
        {"!=",token_type::NotEq{}},  {"==",token_type::Eq{}},
        {"<=",token_type::LessOrEq{}},  {">=",token_type::GreaterOrEq{}} };

    std::map<char, token_type::Char> chars_ { 
        {'(', token_type::Char{'('}}, {')', token_type::Char{')'}},
        {':', token_type::Char{':'}}, {',', token_type::Char{','}}, {'.', token_type::Char{'.'}} };

    std::set<char> numbers_{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    std::vector<Token> tokens_;
    int current_token_ = 0;

};

}  // namespace parse