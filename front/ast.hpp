#ifndef COPL_AST_HPP
#define COPL_AST_HPP
#include <iostream>
#include <vector>
#include "lexer.hpp"
#include <vector>
#include <unordered_map>

class AST {
public:
    enum AKind {
        A_IF, A_BLOCK, A_STRING, A_INT, A_FLO, A_TRUE, A_FALSE, A_WHILE,
        A_FOR, A_CLASS, A_RETURN, A_BREAK, A_CONTINUE, A_BIN_OP, A_BIT_NOT,
        A_MEMBER_ACCESS, A_ID, A_ELEMENT_GET, A_CALL, A_NOT, A_ARRAY, A_SELF_INC,
        A_SELF_DEC, A_VAR_DEF, A_FUNC_DEFINE, A_SELF_OPERA, A_MEM_MALLOC, A_LAMBDA,
        A_NULL, A_IMPORT, A_TYPE, A_SW_UNIT, A_SW
    } kind;

    AST(AKind kind) { this->kind = kind; }

    ~AST() { }
};

class TrueNode : public AST {
public:
    TrueNode() : AST(AST::A_TRUE) { }
};

class ImportNode : public AST {
public:
    std::string path, re_name;
    ImportNode(std::string path, std::string re_name) : AST(A_IMPORT) {
        this->path = path;
        this->re_name = re_name;
    }
};

class FalseNode : public AST {
public:
    FalseNode() : AST(AST::A_FALSE) { }
};

const int pre = 1; // ++a;

const int npre = 0; // a++;

class SelfIncNode : public AST {
public:
    int ipre = 0;
    AST* id;
    SelfIncNode(AST* id, int i) : AST(A_SELF_INC) {
        this->id = id;
        this->ipre = i;
    }

};

class MemoryMallocNode : public AST {
public:
    std::string name;
    std::vector<AST*> args;
    bool is_call_c;
    MemoryMallocNode(std::string name, std::vector<AST*> args, bool is_call_constructor) : AST(A_MEM_MALLOC) {
        this->name = name;
        this->args = args;
        this->is_call_c = is_call_constructor;
    }
};

class SelfDecNode : public AST {
public:
    int ipre = 0;
    AST* id;
    SelfDecNode(AST* id, int i) : AST(A_SELF_DEC) {
        this->id = id;
        this->ipre = i;
    }

    ~SelfDecNode() {
        delete id;
    }
};

class BinOpNode : public AST {
public:
    std::string op;
    AST *left, *right;
    BinOpNode(std::string op, AST* left, AST* right) : AST(AST::A_BIN_OP) {
        this->op = op;
        this->right = right;
        this->left = left;
    }

    ~BinOpNode() {
        delete left;
        delete right;
    }
};

class MemberAccessNode : public AST {
public:
    AST* parent;
    std::string member;
    MemberAccessNode(AST* left, std::string member) : AST(AST::A_MEMBER_ACCESS) {
        this->parent = left;
        this->member = member;
    }

    ~MemberAccessNode() {
        delete parent;
    }
};

class StringNode : public AST {
public:
    std::string str;
    StringNode(std::string str) : AST(AST::A_STRING) {
        this->str = str;
    }
};

class IntegerNode : public AST {
public:
    std::string number;
    IntegerNode(std::string number) : AST(AST::A_INT) {
        this->number = number;
    }
};


class BitNotNode : public AST {
public:
    AST* expr;
    BitNotNode(AST* expr) : AST(AST::A_BIT_NOT) {
        this->expr = expr;
    }
};

class IdNode : public AST {
public:
    std::string id;
    IdNode(std::string id) : AST(AST::A_ID) {
        this->id = id;
    }
};

class CallNode : public AST {
public:
    AST* func_name;
    std::vector<AST*> args;
    CallNode(AST* func_name, std::vector<AST*> args) : AST(AST::A_CALL) {
        this->func_name = func_name;
        this->args = args;
    }

    ~CallNode() {
        delete func_name;
    }
};

class ElementGetNode : public AST {
public:
    AST* array_name;
    AST* position;
    ElementGetNode(AST* array_name, AST* position) : AST(AST::A_ELEMENT_GET) {
        this->array_name = array_name;
        this->position = position;
    }

    ~ElementGetNode() {
        delete array_name;
        delete position;
    }
};

class NotNode : public AST {
public:
    AST* expr;
    NotNode(AST* expr) : AST(AST::A_NOT) {
        this->expr = expr;
    }

    ~NotNode() {
        delete expr;
    }
};

class Block : public AST {
public:
    std::vector<AST*> codes;
    Block(std::vector<AST*> codes) : AST(A_BLOCK) {
        this->codes = codes;
    }
};

class IfNode : public AST {
public:
    AST* condition;
    Block *if_true;
    Block *if_false;
    IfNode(AST* condition, Block* if_true, Block* if_false) : AST(A_IF) {
        this->condition = condition;
        this->if_false = if_false;
        this->if_true = if_true;
    }

    ~IfNode() {
        delete condition;
        delete if_false;
        delete if_true;
    }
};

class WhileNode : public AST {
public:
    AST* condition;
    Block* body;
    WhileNode(AST* condition, Block* body) : AST(A_WHILE) {
        this->condition = condition;
        this->body = body;
    }

    ~WhileNode() {
        delete condition;
        delete body;
    }
};

class TypeNode : public AST {
public:
    std::string root_type;
    std::string module_path, re_name;
    TypeNode* child_type;
    int args_size = 0;
	
	enum TKind { TK_NORMAL, TK_MODULE, TK_FUNCTION } __kind;
	
    TypeNode(std::string root_type) : AST(A_TYPE) {
        this->root_type = root_type;
		__kind = TK_NORMAL;
    }

    TypeNode(std::string module_path, std::string re_name) : AST(A_TYPE) {
        this->module_path = module_path;
        this->re_name = re_name;
		__kind = TK_MODULE;
    }

    TypeNode(std::string root_type, TypeNode* child_type, int size = 0) : AST(A_TYPE) {
        this->root_type = root_type;
        this->child_type = child_type;
        this->args_size = size;
		__kind = TK_FUNCTION;
    }
};

class LambdaNode : public AST {
public:
    std::vector<AST*> args;
    Block* body;
    TypeNode* type__;
    LambdaNode(std::vector<AST*> args, Block* body, TypeNode* type__): AST(AST::A_LAMBDA) {
        this->args = args;
        this->body = body;
        this->type__ = type__;
    }

    ~LambdaNode() {
        delete body;
    }
};

class VarDefineNode : public AST {
public:
    std::string name;
    TypeNode* type;
    AST* init_value;
    VarDefineNode(std::string name, TypeNode* type,  AST* init_value = nullptr) : AST(A_VAR_DEF) {
        this->name = name;
        this->type = type;
        this->init_value = init_value;
    }

    ~VarDefineNode() {
        delete init_value;
    }
};

class FunctionNode : public AST {
public:
    Block* body;
    std::string name;
    std::vector<AST*> args;
    FunctionNode(std::string name, std::vector<AST*> args, Block* body) : AST(A_FUNC_DEFINE) {
        this->body = body;
        this->name = name;
        this->args = args;
    }

    ~FunctionNode() {
        delete body;
    }
};

class SelfOperator : public AST {
public:
    std::string op;
    AST* target;
    AST* value;
    SelfOperator(std::string op, AST* target, AST* value) : AST(A_SELF_OPERA) {
        this->op = op;
        this->target = target;
        this->value = value;
    }

    ~SelfOperator() {
        delete target;
        delete value;
    }
};

class ForNode : public AST {
public:
    AST* init;
    AST* is_continue;
    AST* change;
    Block* body;
    ForNode(AST* init, AST* is_continue, AST* change, Block* body) : AST(A_FOR) {
        this->init = init;
        this->is_continue = is_continue;
        this->change = change;
        this->body = body;
    }

    ~ForNode() {
        delete init;
        delete is_continue;
        delete change;
        delete body;
    }
};

class ContinueNode : public AST {
public:
    ContinueNode() : AST(A_CONTINUE) {}
};

class SwitchUnit : public AST {
public:
    AST* value;
    Block* stmt;
    bool is_default = false;
    SwitchUnit(AST* value, Block* stmt) : AST(A_SW_UNIT) {
        this->value = value;
        this->stmt = stmt;
        is_default = false;
    }

    SwitchUnit(Block* stmt) : AST(A_SW_UNIT) {
        this->value = nullptr;
        this->stmt = stmt;
        is_default = true;
    }
};

class SwitchNode : public AST {
public:
    std::vector<SwitchUnit*> units;
    AST* target_value;
    SwitchNode(AST* target_value, std::vector<SwitchUnit*> units) : AST(A_SW) {
        this->units = units;
        this->target_value = target_value;
    }
};


class BreakNode : public AST {
public:
    BreakNode() : AST(A_BREAK) {}
};

class ReturnNode : public AST {
public:
    AST* value;
    ReturnNode(AST* value) : AST(A_RETURN) {
        this->value = value;
    }

    ~ReturnNode() { delete value; }
};

class ObjectNode : public AST {
public:
    std::string name;
	
	std::vector<std::string> extern_class;

    enum AccessState { PUBLIC, PRIVATE };

    std::unordered_map<std::string, AST*> members;
    std::unordered_map<std::string, AccessState> as;
    ObjectNode(std::string name, std::unordered_map<std::string, AST*> members, std::unordered_map<std::string, AccessState> as, std::vector<std::string> ex) : AST(A_CLASS) {
        this->members = members;
        this->name = name;
        this->as = as;
		this->extern_class = ex;
    }
};

class NullNode : public AST {
public:
    NullNode() : AST(A_NULL) {}
};

class ArrayNode : public AST {
public:
    std::vector<AST*> elements;
    ArrayNode(std::vector<AST*> elements) : AST(AST::A_ARRAY){
        this->elements = elements;
    }
};

class FloatNode : public AST {
public:
    std::string number;
    FloatNode(std::string number) : AST(AST::A_FLO) {
        this->number = number;
    }
};

class Parser;

typedef AST*(Parser::*CALLBACK_FUNCTION)();

void make_error(std::string error_type, std::string error_info, Position error_pos) {
    std::cout << error_type << ": " << error_info << " at lin " << error_pos.lin << ", col " << error_pos.col << std::endl;
    exit(-1);
}

void make_error(std::string error_type, std::string error_info) {
    std::cout << error_type << ": " << error_info << " at EOF" << std::endl;
    exit(-1);
}

std::string print_indent(int indent, std::string inde = "    ") {
    std::string res;
    while (indent--) res += inde;
    return res;
}

void decompiler(AST* a, int indent = 0, std::string fo = "") {
    switch (a->kind) {
        case AST::A_ID: {
            std::cout << print_indent(indent) << fo << "Id<" << ((IdNode*)a)->id << ">\n";
            break;
        }
        case AST::A_IF: {
            auto n = (IfNode*) a;
            std::cout << print_indent(indent) << fo << "IfNode {\n";
            decompiler(n->condition, indent + 1, "Condition: ");
            decompiler(n->if_true, indent + 1, "IfConditionIsTrue: ");
            if (n->if_false) decompiler(n->if_false, indent + 1, "IfNotTrue: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_BLOCK: {
            auto al = (Block*) a;
            std::cout << print_indent(indent) << fo << "Block {\n";
            for (int i = 0; i < al->codes.size(); ++i)
                decompiler(al->codes[i], indent + 1, std::to_string(i) + ": ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_STRING: {
            std::cout << print_indent(indent) << fo << "String('" << ((StringNode*)a)->str << "')\n";
            break;
        }
        case AST::A_INT: {
            std::cout << print_indent(indent) << fo << "Int('" << ((IntegerNode*)a)->number << "')\n";
            break;
        }
        case AST::A_FLO: {
            std::cout << print_indent(indent) << fo << "Float('" << ((FloatNode*)a)->number << "')\n";
            break;
        }
        case AST::A_ARRAY: {
            auto node = (ArrayNode*) a;
            std::cout << print_indent(indent) << fo << "Array: [\n";
            for (int i = 0; i < node->elements.size(); ++i)
                decompiler(node->elements[i], indent + 1, "E[" + std::to_string(i) + "] = ");
            std::cout << print_indent(indent) << "]\n";
            break;
        }
        case AST::A_TRUE: {
            std::cout << print_indent(indent) << fo << "Bool<true>\n";
            break;
        }
        case AST::A_FALSE: {
            std::cout << print_indent(indent) << fo << "Bool<false>\n";
            break;
        }
        case AST::A_WHILE: {
            std::cout << print_indent(indent) << fo << "WhileNode {\n";
            auto w = (WhileNode*) a;
            decompiler(w->condition, indent + 1, "Condition: ");
            decompiler(w->body, indent + 1, "Body: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_FOR: {
            std::cout << print_indent(indent) << fo << "ForLoopNode {\n";
            auto n = (ForNode*)a;
            decompiler(n->init, indent + 1, "ForLoopInit: ");
            decompiler(n->is_continue, indent + 1, "LoopCondition: ");
            decompiler(n->change, indent + 1, "ForLoopChange: ");
            decompiler(n->body, indent + 1, "ForLoopBody: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_CLASS: {
            auto node = (ObjectNode*) a;
            std::cout << print_indent(indent) << fo << "Class {\n";
            std::cout << print_indent(indent + 1) << "Name: " << node->name << std::endl;
            if (!node->members.empty()) {
                std::cout << print_indent(indent + 1) << "Members [\n";
                for (auto i : node->members)
                    decompiler(i.second, indent + 2, i.first + ": ");
                std::cout << print_indent(indent + 1) << "]\n";
            }
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_RETURN: {
            std::cout << print_indent(indent) << fo << "ReturnSignal(\n";
            decompiler(((ReturnNode*)a)->value, indent + 1, "ReturnValue: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_BREAK: {
            std::cout << print_indent(indent) << fo << "<BreakSignal>\n";
            break;
        }
        case AST::A_CONTINUE: {
            std::cout << print_indent(indent) << fo << "<ContinueSignal>\n";
            break;
        }
        case AST::A_NOT: {
            std::cout << print_indent(indent) << fo << "Not(\n";
            decompiler(((NotNode*)a)->expr, indent + 1);
            std::cout << print_indent(indent) << ")\n";
            break;
        }
        case AST::A_BIN_OP: {
            std::cout << print_indent(indent) << fo << "BinOpNode: <'" << ((BinOpNode*)a)->op << "'>{\n";
            decompiler(((BinOpNode*)a)->left, indent + 1, "Left: ");
            decompiler(((BinOpNode*)a)->right, indent + 1, "Right: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_BIT_NOT: {
            std::cout << print_indent(indent) << fo << "BitNode: ";
            decompiler(((BitNotNode*)a)->expr, indent + 1);
            break;
        }
        case AST::A_MEMBER_ACCESS: {
            std::cout << print_indent(indent) << fo << "MemberAccessNode {\n";
            decompiler(((MemberAccessNode*)a)->parent, indent + 1, "ParentClass: ");
            std::cout << print_indent(indent + 1) << "Member: " << ((MemberAccessNode*)a)->member << std::endl;
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_ELEMENT_GET: {
            auto node = (ElementGetNode*)a;
            std::cout << print_indent(indent) << fo << "ElementGetNode {\n";
            decompiler(node->array_name, indent + 1, "ArrayAddress: ");
            decompiler(node->position, indent + 1, "Position: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_CALL: {
            std::cout << print_indent(indent) << fo << "CallNode { \n";
            decompiler(((CallNode*)a)->func_name, indent + 1, "FuncAddress: ");
            std::cout << print_indent(indent + 1) << "Args: [" ;
            std::string ind;
            if (!((CallNode*)a)->args.empty()) {
                printf("\n");
                ind = print_indent(indent + 1);
                for (int i = 0; i < ((CallNode *) a)->args.size(); ++i)
                    decompiler(((CallNode *) a)->args[i], indent + 2, "Args[" + std::to_string(i) + "]: ");
            }
            std::cout << ind << "]\n";
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_SELF_INC: {
            auto node = (SelfIncNode*) a;
            std::cout << print_indent(indent) << fo << "SelfInc {\n";
            std::cout << print_indent(indent + 1) << "PRE: " << node->ipre << std::endl;
            decompiler(node->id, indent + 1, "Address: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_SELF_DEC: {
            auto node = (SelfDecNode*) a;
            std::cout << print_indent(indent) << fo << "SelfDec {\n";
            std::cout << print_indent(indent + 1) << "PRE: " << node->ipre << std::endl;
            decompiler(node->id, indent + 1, "Address: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_VAR_DEF: {
            auto v = (VarDefineNode*) a;
            std::cout << print_indent(indent) << fo << "VarDefine {\n";
            std::cout << print_indent(indent + 1) << "VarName: " << v->name << "\n";
            if (v->init_value) decompiler(v->init_value, indent + 1, "InitValue: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_FUNC_DEFINE: {
            auto fn = (FunctionNode*) a;
            auto fn_arg = fn->args;
            std::cout << print_indent(indent) << fo << "FunctionDefine<'" << fn->name << "'> {\n";
            std::cout << print_indent(indent + 1) << "FunctionArgs: ";
            if (fn_arg.empty()) { std::cout << "[ ]\n"; }
            else {
                std::cout << "[\n";
                for (int i = 0; i < fn_arg.size(); ++i)
                    decompiler(fn_arg[i], indent + 2, std::to_string(i) + ": ");
                std::cout << print_indent(indent + 1) << "]\n";
            }
            decompiler(fn->body, indent + 1, "FuncBody: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_SELF_OPERA: {
            auto sa = (SelfOperator*) a;
            std::cout << print_indent(indent) << fo << "SelfOperator<'" + sa->op << "'> {\n";
            decompiler(sa->target, indent + 1, "Target: ");
            decompiler(sa->value, indent + 1, "Value: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_LAMBDA: {
            std::cout << print_indent(indent) << fo << "Lambda {\n";
            auto ln = (LambdaNode*) a;
            std::cout << print_indent(indent + 1) << "Args: (\n";
            for (int i = 0; i < ln->args.size(); ++i)
                decompiler(ln->args[i], indent + 2, std::to_string(i) + ": ");
            std::cout << print_indent(indent + 1) << ")\n";
            decompiler(ln->body, indent + 1, "Body: ");
            std::cout << print_indent(indent) << "}\n";
            break;
        }
        case AST::A_MEM_MALLOC: {
            std::cout << print_indent(indent) << fo << "MemoryMalloc (\n";
            auto n = (MemoryMallocNode*) a;
            for (int i = 0; i < n->args.size(); ++i)
                decompiler(n->args[i], indent + 1, std::to_string(i) + ": ");
            std::cout << print_indent(indent) << ")\n";
            break;
        }
        case AST::A_NULL: {
            std::cout << print_indent(indent) << fo << "Null\n";
            break;
        }
        case AST::A_IMPORT: {
            std::cout << print_indent(indent) << "Import(" << ((ImportNode*)a)->path << ")\n";
            break;
        }
    }
}

#endif