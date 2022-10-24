#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

void Lexer::ReadNumber(std::istream& input) {
    char c = -1;
    std::string number;
    bool is_minus = false;

    input >> c;

    if (c == '-') {
        is_minus = true;
    }
    else {
        input.putback(c);
    }

    while (input >> c) {
        if (numbers_.count(c) != 0) {
            number.push_back(c);
        }
        else {
            input.putback(c);
            break;
        }
    }

    if (is_minus) {
        tokens_.push_back(token_type::Char{ '-' });
    }

    tokens_.push_back(token_type::Number{ std::stoi(number) });
}
void Lexer::ReadString(std::istream& input) {
    char c = -1;
    char quote = '\"';
    std::string result;

    input >> c;

    if (c == '\'') {
        quote = '\'';
    }

    while (input >> c) {
        if (c == '\\') {
            input >> c;
            if (c == 't') {
                result.push_back('\t');
            }
            else if (c == 'n') {
                result.push_back('\n');
            }
            else {
                result.push_back(c);
            }

        }
        else if (c == quote) {
            break;
        }
        else {
            result.push_back(c);
        }
    }

    tokens_.push_back(token_type::String{ std::move(result) });
}
void Lexer::ReadId(std::istream& input) {
    char c = -1;
    std::string id;

    while (input >> c) {
        if (c == '#') {
            input.clear(std::istream::eofbit | std::istream::failbit);

            if (!id.empty()) {
                tokens_.push_back(token_type::Id{ std::move(id) });
            }

            return;
        }

        if (c == ' ' || chars_.count(c) || operations_char_.count(c)) {
            input.putback(c);
            break;
        }
        else {
            id.push_back(c);
        }
    }

    if (keywords_.count(id)) {
        tokens_.push_back(keywords_.at(id));
    }
    else {
        tokens_.push_back(token_type::Id{ std::move(id) });
    }
}

void Lexer::ReadIndents(std::istream& input, int& old_indent) {
    char c = -1;
    int indent = 0;
    while (true) {
        input >> c;
        if (c == ' ') {
            if (input.peek() == ' ') {
                input.get();
                indent++;
            }
            else {
                return;
            }
        }
        else {
            input.putback(c);
            break;
        }
    }

    if (indent > old_indent) {
        // я не понимаю, почему иногда оно убирает пробелы, стабильно ~одна такая штука в проекте :/
        for (int i = 0; i < indent - old_indent; i++) {
            tokens_.push_back(token_type::Indent());
        }
        old_indent = indent;
    }
    else if (indent < old_indent) {
        for (int i = 0; i < old_indent - indent; i++) {
            tokens_.push_back(token_type::Dedent());
        }
        old_indent = indent;
    }
}

void Lexer::ReadChar(std::istream& input) {
    tokens_.push_back(chars_.at(input.get()));
}

void Lexer::ReadOperation(std::istream& input) {
    char c = -1;
    std::string operation;

    input >> c;
    operation.push_back(c);

    if (c == '=' || c == '<' || c == '>' || c == '!') {
        if (input.peek() == '=') {
            input >> c;
            operation.push_back(c);
        }
    }

    if (operation.size() == 1) {
        tokens_.push_back(operations_char_.at(operation[0]));
    }
    else {
        tokens_.push_back(operations_.at(operation));
    }
}

Lexer::Lexer(std::istream& input) {
    std::string temp_string;
    int indent_count = 0;

    while (!input.eof()) {
        std::getline(input, temp_string);
        std::istringstream line(temp_string);

        char c = -1;

        std::noskipws(line);

        if (line.str().size() == 0 || line.str()[0] == '#') {
            continue;
        }

        ReadIndents(line, indent_count);

        while (line >> c) {
            if (c == ' ') {
                continue;
            }
            if (c == '\'' || c == '\"') {
                line.putback(c);
                ReadString(line);
            }
            else if (operations_char_.count(c)) {
                line.putback(c);
                ReadOperation(line);
            }
            else if (chars_.count(c)) {
                line.putback(c);
                ReadChar(line);
            }
            else if (numbers_.count(c) != 0) {
                line.putback(c);
                ReadNumber(line);
            }
            else {
                line.putback(c);
                ReadId(line);
            }

        }

        std::skipws(line);

        if (!tokens_.empty()) {
            tokens_.push_back(token_type::Newline());
        }
    }

    if (indent_count != 0) {
        for (int i = 0; i < indent_count; i++) {
            tokens_.push_back(token_type::Dedent());
        }
    }

    tokens_.push_back(token_type::Eof());
}

const Token& Lexer::CurrentToken() const {
    return tokens_[current_token_];
}

Token Lexer::NextToken() {
    if (!(current_token_ == static_cast<int>((tokens_.size() - 1)))) {
        current_token_++;
    }
    return CurrentToken();
}

}  // namespace parse