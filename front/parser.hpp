#ifndef COPL_PARSER_HPP
#define COPL_PARSER_HPP
#include <iostream>
#include <vector>
#include "ast.hpp"
#include "lexer.hpp"
#include <vector>
#include <unordered_map>

class Parser {
public:
    Parser(std::vector<Token> tokens) {
        this->tokens = tokens;
        pos = -1;
        current = nullptr;
        advance();
        make_all();
    }
    std::vector<AST*> ast;

    const std::vector<std::string> self_operator = { "+=", "-=", "*=", "/=", "%=", ">>=", "<<=", "|=", "&=", "=" };

    void make_all() {
        while (current) {
            if (match("if")) ast.push_back(make_if());
            else if (match("def")) ast.push_back(make_function_define());
            else if (match("while")) ast.push_back(make_while());
            else if (match("for")) ast.push_back(make_for());
            else if (match("class")) ast.push_back(make_class());
            else if (match("continue")) ast.push_back(make_continue());
            else if (match("import")) ast.push_back(make_import());
            else if (match("break")) ast.push_back(make_break());
            else if (match("return")) ast.push_back(make_return());
            else if (match("$")) ast.push_back(make_lambda());
            else if (match("new")) ast.push_back(make_malloc());
            else if (match("let")) {
                auto a = make_var_define_group();
                for (auto i : a) ast.push_back(i);
            }
            else {
                auto tmp = make_expression();
                if (std::count(self_operator.begin(), self_operator.end(), current->data) == 1 &&
                    (tmp->kind == AST::A_ID || tmp->kind == AST::A_MEMBER_ACCESS || tmp->kind == AST::A_ELEMENT_GET)) {
                    std::string op = current->data;
                    advance();
                    auto val = make_expression();
                    tmp = new SelfOperator(op, tmp, val);
                }
                ast.push_back(tmp);
                expect_data(";", get_pos());
            }
        }
    }

private:
    std::vector<Token> tokens;
    int pos;
    Token* current;

    bool match(Token::TokenKind kind) { return current != nullptr && current->kind == kind; }

    bool match(std::string data) { return current != nullptr && current->data == data; }

    std::string expect_get(Token::TokenKind kind) {
        if (!match(kind))
            make_error("SyntaxError", "want '" + std::to_string(kind) + "' get '" + current->data + "'");
        auto tmp = current->data;
        advance();
        return tmp;
    }

    void expect_data(std::string name, Position _pos) {
        if (!current || !match(name))
            make_error("SyntaxError", "want '" + name + "', meet '" + ((current)? current->data : "None"), _pos);
        advance();
    }

    Position get_pos() { if (!current) { make_error("SyntaxError", "meet eof"); } return current->start_pos; }

    ImportNode* make_import() {
        expect_data("import", get_pos());
        auto path = expect_get(Token::TT_STRING);
        expect_data("as", get_pos());
        auto re_name = expect_get(Token::TT_ID);
        expect_data(";", get_pos());
        return new ImportNode(path, re_name);
    }

    LambdaNode* make_lambda() {
        expect_data("$", get_pos());
        auto args = make_area("(", ")", ",", &Parser::make_var_define);
        expect_data("->", get_pos());
        TypeNode* ret_t = make_type();
        auto body = make_block();
        return new LambdaNode(args, body, new TypeNode("lambda", ret_t, args.size()));
    }

    ContinueNode* make_continue() {
        advance();
        expect_data(";", get_pos());
        return new ContinueNode();
    }

    ReturnNode* make_return() {
        expect_data("return", get_pos());
        if (match(";")) {
            advance();
            return new ReturnNode(nullptr);
        }
        auto val = make_expression();
        expect_data(";", get_pos());
        return new ReturnNode(val);
    }

    BreakNode* make_break() {
        advance();
        expect_data(";", get_pos());
        return new BreakNode();
    }

    IfNode* make_if() {
        expect_data("if", get_pos());
        expect_data("(", get_pos());
        auto cond = make_expression();
        expect_data(")", get_pos());
        Block* body = make_block();
        Block* el = nullptr;
        if (match("else")) {
            advance();
            el = make_block();
        }
        return new IfNode(cond, body, el);
    }

    FunctionNode* make_function_define() {
        if (match("def")) advance();
        std::string name = expect_get(Token::TT_ID);
        std::vector<AST*> vals = make_area("(", ")", ",", &Parser::make_var_define);
        int w = 0;
        auto body = make_block();
        return new FunctionNode(name, vals, body);
    }

    TypeNode* make_type() {
        std::string root_type;
        TypeNode* child_type = nullptr;
        if (match("lambda")) {
            advance();
            expect_data("->", get_pos());
            root_type = "lambda";
            child_type = make_type();
            return new TypeNode(root_type, child_type);
        }
        if (match("[")) {
            root_type = "Array";
            advance();
            child_type = make_type();
            expect_data("]", get_pos());
        } else {
            root_type = expect_get(Token::TT_ID);
        }
        return new TypeNode(root_type, child_type);
    }

    AST* make_var_define() {
        std::string name;
        TypeNode* type;
        AST* init_value = nullptr;
        name = expect_get(Token::TT_ID);
        expect_data(":", get_pos());
        type = make_type();
        if (match("=")) {
            advance();
            init_value = make_expression();
        }
        return new VarDefineNode(name, type, init_value);
    }

    MemoryMallocNode* make_malloc() {
        expect_data("new", get_pos());
        std::string name = expect_get(Token::TT_ID);
        std::vector<AST*> arg;
        bool is_call_constructor = false;
        if (match("(")) is_call_constructor = true, arg = make_area("(", ")", ",", &Parser::make_expression);
        return new MemoryMallocNode(name, arg, is_call_constructor);
    }

    WhileNode* make_while() {
        expect_data("while", get_pos());
        expect_data("(", get_pos());
        auto condition = make_expression();
        expect_data(")", get_pos());
        auto block = make_block();
        return new WhileNode(condition, block);
    }

    std::vector<AST*> make_var_define_group() {
        return make_area("let", ";", ",", &Parser::make_var_define);
    }

    NullNode* make_null () {
        expect_data("null", get_pos());
        return new NullNode();
    }

    ForNode* make_for() {
        expect_data("for", get_pos());
        expect_data("(", get_pos());
        AST *init, *is_continue, *change;
        Block *body;
        init = make_var_define();
        expect_data(";", get_pos());
        is_continue = make_expression();
        expect_data(";", get_pos());
        change = make_for_change();
        expect_data(")", get_pos());
        body = make_block();
        return new ForNode(init, is_continue, change, body);
    }

    ObjectNode* make_class() {
        expect_data("class", get_pos());
        std::unordered_map<std::string, AST*> members;
        std::unordered_map<std::string, ObjectNode::AccessState> as;
        std::string name = expect_get(Token::TT_ID);
        expect_data("{", get_pos());
        ObjectNode::AccessState _as = ObjectNode::PRIVATE;
        while (current && !match("}")) {
            if (match("public")) advance(), _as = ObjectNode::PUBLIC;
            else if (match("private")) advance(),_as = ObjectNode::PRIVATE;
            if (match("def") || match("constructor")) {
                std::string __name = current->data;
                if (__name == "constructor") _as = ObjectNode::PUBLIC;
                auto tmp = make_function_define();
                members[tmp->name] = tmp;
                as[__name] = _as;
                _as = ObjectNode::PRIVATE;
            } else if (match("let")) {
                auto vars = make_var_define_group();
                for (auto i : vars) members[((VarDefineNode*)i)->name] = i, as[((VarDefineNode*)i)->name] = _as;
            } else {
                auto v = (VarDefineNode*)make_var_define();
                members[v->name] = v;
                as[v->name] = _as;
                expect_data(";", get_pos());
            }
        }
        expect_data("}", get_pos());
        return new ObjectNode(name, members, as);
    }

    Block* make_block() {
        if (!match("{")) {
            std::vector<AST*> codes;
            if (match("if")) codes.push_back(make_if());
            else if (match("while")) codes.push_back(make_while());
            else if (match("import")) codes.push_back(make_import());
            else if (match("for")) codes.push_back(make_for());
            else if (match("continue")) codes.push_back(make_continue());
            else if (match("new")) codes.push_back(make_malloc());
            else if (match("break")) codes.push_back(make_break());
            else if (match("return")) codes.push_back(make_return());
            else if (match("let")) {
                auto a = make_var_define_group();
                for (auto i : a) codes.push_back(i);
            }
            else {
                auto tmp = make_expression();
                if (std::count(self_operator.begin(), self_operator.end(), current->data) == 1 &&
                    (tmp->kind == AST::A_ID || tmp->kind == AST::A_MEMBER_ACCESS || tmp->kind == AST::A_ELEMENT_GET)) {
                    std::string op = current->data;
                    advance();
                    auto val = make_expression();
                    tmp = new SelfOperator(op, tmp, val);
                }
                expect_data(";", get_pos());
                codes.push_back(tmp);
            }
            return new Block(codes);
        }
        expect_data("{", get_pos());
        std::vector<AST*> codes;
        while (current && !match("}")) {
            if (match("if")) codes.push_back(make_if());
            else if (match("while")) codes.push_back(make_while());
            else if (match("import")) codes.push_back(make_import());
            else if (match("for")) codes.push_back(make_for());
            else if (match("continue")) codes.push_back(make_continue());
            else if (match("new")) codes.push_back(make_malloc());
            else if (match("break")) codes.push_back(make_break());
            else if (match("return")) codes.push_back(make_return());
            else if (match("let")) {
                auto a = make_var_define_group();
                for (auto i : a) codes.push_back(i);
            }
            else {
                auto tmp = make_expression();
                if (std::count(self_operator.begin(), self_operator.end(), current->data) == 1 &&
                    (tmp->kind == AST::A_ID || tmp->kind == AST::A_MEMBER_ACCESS || tmp->kind == AST::A_ELEMENT_GET)) {
                    std::string op = current->data;
                    advance();
                    auto val = make_expression();
                    tmp = new SelfOperator(op, tmp, val);
                }
                expect_data(";", get_pos());
                codes.push_back(tmp);
            }
        }
        expect_data("}", get_pos());
        return new Block(codes);
    }

    void advance() {
        ++pos;
        current = nullptr;
        if (pos < tokens.size())
            current = &tokens[pos];
    }

    AST* make_bin_op_node(CALLBACK_FUNCTION left_process, std::vector<std::string> opers, CALLBACK_FUNCTION right_process = nullptr) {
        if (right_process == nullptr)
            right_process = left_process;
        AST* left = (this->*left_process)();
        while (current && std::count(opers.begin(), opers.end(), current->data) > 0) {
            std::string op = current->data;
            advance();
            AST* right = (this->*right_process)();
            left = new BinOpNode(op, left, right);
        }
        return left;
    }

    std::vector<AST*> make_area(std::string begin, std::string end, std::string split, CALLBACK_FUNCTION proc, int cnt = -1) {
        std::vector<AST*> res;
        expect_data(begin, get_pos());
        int count = 0;
        while (current && !match(end) && (cnt == -1 || ++count <= cnt)) {
            res.push_back((this->*proc)());
            if (match(end)) break;
            expect_data(split, get_pos());
        }
        expect_data(end, get_pos());
        return res;
    }

    AST* make_call_node() {
        return _make_call_node(make_member_access());
    }

    AST* _make_call_node(AST* name) {
        while (current && (match("(") || match("[") || match("."))) {
            if (match("(")) {
                auto args = make_area("(", ")", ",", &Parser::make_expression);
                name = new CallNode(name, args);
            } else if (match("[")) {
                advance();
                name = new ElementGetNode(name, make_expression());
                expect_data("]", get_pos());
            } else if (match(".")) {
                name = _make_member_access(name);
            }
        }
        return name;
    }

    AST* make_element_get_node() {
        return _make_element_get_node(make_member_access());
    }

    AST* _make_element_get_node(AST* name) {
        while (current && (match("["))) {
            advance();
            name = new ElementGetNode(name, make_expression());
            expect_data("]", get_pos());
        }
        return name;
    }

    AST* make_member_access() {
        AST* left = new IdNode(current->data);
        advance();
        return _make_member_access(left);
    }

    AST* _make_member_access(AST* left) {
        while (current && match(".")) {
            advance();
            left = new MemberAccessNode(left, current->data);
            advance();
        }
        return left;
    }

    AST* make_for_change() {
        if (match("++") || match("--")) {
            return make_expression();
        } else if (match(Token::TT_ID)) {
            auto tmp = make_value();
            if (tmp->kind == AST::A_SELF_DEC || tmp->kind == AST::A_SELF_INC) {
                return tmp;
            }
            std::string op = expect_get(Token::TT_OP);
            auto value = make_expression();
            return new SelfOperator(op, tmp, value);
        } else {
            std::cout << "unknown token: ";
            current->debug();
            std::cout << std::endl;
            exit(-1);
        }
    }

    AST* make_expression() { return make_bin_op_node(&Parser::make_expression5, {"||"}); }

    AST* make_expression5() { return make_bin_op_node(&Parser::make_expression4, {"&&"}); }

    AST* make_expression4() { return make_bin_op_node(&Parser::make_expression3, {">", "<", ">=", "<=", "==", "!="}); }

    AST* make_expression3() { return make_bin_op_node(&Parser::make_expression2, {"+", "-"}); }

    AST* make_expression2() { return make_bin_op_node(&Parser::make_expression1, {"*", "/", "<<", ">>", "%", "^", "|", "&"}); }

    AST* make_array() { return new ArrayNode(make_area("[", "]", ",", &Parser::make_expression)); }

    AST* make_expression1() { return make_bin_op_node(&Parser::make_value, {"**"}); }

    AST* make_value() {
        if (match(Token::TT_INTEGER)) {
            auto tmp = new IntegerNode(current->data);
            advance();
            return tmp;
        } else if (match("$")) {
            return make_lambda();
        } else if (match("null")) {
            return make_null();
        } else if (match("++")) {
            advance();
            return new SelfIncNode(make_value(), pre);
        } else if (match("--")) {
            advance();
            return new SelfDecNode(make_value(), pre);
        } else if (match("new")) {
            return make_malloc();
        } else if (match(Token::TT_FLOAT)) {
            auto tmp = new FloatNode(current->data);
            advance();
            return tmp;
        } else if (match("[")) {
            auto tmp = make_array();
            while (match(".") || match("(") || match("[")) {
                if (match("."))
                    tmp = _make_member_access(tmp);
                if (match("("))
                    tmp = _make_call_node(tmp);
                if (match("["))
                    tmp = _make_element_get_node(tmp);
            }
            return tmp;
        } else if (match(Token::TT_STRING)) {
            auto t = new StringNode(current->data);
            advance();
            return t;
        } else if (match("~")) {
            advance();
            return new BitNotNode(make_value());
        } else if (match("!")) {
            advance();
            return new NotNode(make_value());
        } else if (match("-")) {
            advance();
            return new BinOpNode("*", new FloatNode("-1.0"), make_value());
        } else if (match("true")) {
            advance();
            return new TrueNode();
        } else if (match("false")) {
            advance();
            return new FalseNode();
        } else if (match(Token::TT_ID)) {
            auto tmp = make_call_node();
            while (match("++") || match("--")) {
                if (match("++")) {
                    advance();
                    tmp = new SelfIncNode(tmp, npre);
                }
                if (match("--")) {
                    advance();
                    tmp = new SelfDecNode(tmp, npre);
                }
            }
            return tmp;
        } else {
            expect_data("(", get_pos());
            auto tmp = make_expression();
            expect_data(")", get_pos());
            if (match("."))
                tmp = _make_member_access(tmp);
            if (match("("))
                tmp = _make_call_node(tmp);
            return tmp;
        }
    }
};
#endif
