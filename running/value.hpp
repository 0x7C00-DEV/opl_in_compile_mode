#ifndef COPL_VALUE_HPP
#define COPL_VALUE_HPP
#include <unordered_map>
#include <iostream>
#include <exception>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>
#include "asm.hpp"

enum BV_Kind { BV_INT, BV_FLOAT, BV_STRING, BV_BOOL, BV_ARRAY, BV_OBJ, BV_NULL, BV_RAW_POINT };

struct OPL_BasicValue {
    OPL_BasicValue* next;
    bool marked;
    BV_Kind kind;
    OPL_BasicValue(BV_Kind k) : next(nullptr), marked(false), kind(k) {}

    virtual void operator_not_impl_error(std::string info, std::string symbol) {
        printf("Operator '%s' is not impl in object '%d', %s\n", symbol.c_str(), kind, info.c_str());
        throw std::exception();
    }

    virtual void __set__(OPL_BasicValue*) { operator_not_impl_error("", "__set__"); }

    virtual void __elementset__(OPL_BasicValue*, OPL_BasicValue*) {  operator_not_impl_error("", "__elementset__"); }

    virtual OPL_BasicValue* __elementget__(OPL_BasicValue*) { operator_not_impl_error("", "__elementget__"); return nullptr; }

    virtual OPL_BasicValue* __memberget__(int) { operator_not_impl_error("", "__memeberget__"); return nullptr; }

    virtual void __memberset__(int, OPL_BasicValue*) { operator_not_impl_error("", "__memberset__"); }

    virtual OPL_BasicValue* __copy__() { operator_not_impl_error("", "__copy__"); return nullptr; }

    void expect_(OPL_BasicValue* other, std::vector<BV_Kind> kind_) {
        if (std::count(kind_.begin(), kind_.end(), other->kind) <= 0) {
            printf("RunningTimeError in OPL_BasicValue: want %d, get %d\n", kind_[0], other->kind);
            exit(-1);
        }
    }
};

struct OPL_Point : public OPL_BasicValue {
    void* pointer;
    OPL_Point(void* pointer) : OPL_BasicValue(BV_RAW_POINT) {
        this->pointer = pointer;
    }
};

struct STACK_VALUE {
    bool is_heap_ref;
    enum ValueType { S_INT, S_BOOL, S_DOUBLE, S_RAW, S_NULL, S_STR , S_FUC} kind;

    STACK_VALUE* copy() {
        if (is_heap_ref) {
            return make_heap(obj->__copy__());
        }
        switch (kind) {
        case S_INT:
            return make_int(i_val);
        case S_BOOL:
            return make_bool(b_val);
        case S_DOUBLE:
            return make_double(d_val);
        case S_NULL:
            return make_null();
        case S_STR:
            return make_str(str_value);
        case S_RAW: {
            STACK_VALUE* sv = new STACK_VALUE;
            sv->is_heap_ref = false;
            sv->kind = S_RAW;
            sv->raw_ptr = raw_ptr;
            return sv;
        }
        default:
            throw std::exception();
        }
    }

    std::string str_value;
    union {
        OPL_BasicValue* obj;
        int32_t i_val;
        double d_val;
        bool b_val;
        void* raw_ptr;
    };

    static STACK_VALUE* make_func(void* pointer) {
        STACK_VALUE* sv = new STACK_VALUE;
        sv->is_heap_ref = false;
        sv->kind = S_FUC;
        sv->raw_ptr = pointer;
        return sv;
    }

    static STACK_VALUE* make_str(std::string value) {
        STACK_VALUE* sv = new STACK_VALUE;
        sv->kind = S_STR;
        sv->is_heap_ref = false;
        sv->str_value = value;
        return sv;
    }

    static STACK_VALUE* make_null() {
        STACK_VALUE* sv = new STACK_VALUE;
        sv->kind = S_NULL;
        sv->is_heap_ref = false;
        sv->i_val = 0;
        return sv;
    }

    static STACK_VALUE* make_int(int32_t v) {
        STACK_VALUE *sv = new STACK_VALUE;
        sv->is_heap_ref = false;
        sv->kind = S_INT;
        sv->i_val = v;
        return sv;
    }

    static STACK_VALUE* make_double(double v) {
        STACK_VALUE *sv = new STACK_VALUE;
        sv->is_heap_ref = false;
        sv->kind = S_DOUBLE;
        sv->d_val = v;
        return sv;
    }

    static STACK_VALUE* make_bool(bool v) {
        STACK_VALUE *sv = new STACK_VALUE;
        sv->is_heap_ref = false;
        sv->b_val = v;
        sv->kind = S_BOOL;
        return sv;
    }

    static STACK_VALUE* make_heap(OPL_BasicValue* v) {
        STACK_VALUE *sv = new STACK_VALUE;
        sv->is_heap_ref = true;
        sv->obj = v;
        return sv;
    }

    STACK_VALUE() : is_heap_ref(false), obj(nullptr) {}
};



struct OPL_Null : public OPL_BasicValue {
    OPL_Null() : OPL_BasicValue(BV_NULL) { }
};

struct OPL_Object : public OPL_BasicValue {
    std::vector<OPL_BasicValue*> members;
    OPL_Object(std::vector<OPL_BasicValue*> m) : OPL_BasicValue(BV_OBJ), members(std::move(m)) {}

    OPL_BasicValue* __copy__() override {
        std::vector<OPL_BasicValue*> tmp;
        for (auto i : members)
            if (i) tmp.push_back(i->__copy__());
        return new OPL_Object(tmp);
    }

    OPL_Object(int size) : OPL_BasicValue(BV_OBJ) {
        members.resize(size);
        for (auto& m : members)
            m = new OPL_Null();
    }

    OPL_BasicValue* __memberget__(int offset) override {
        return members[offset];
    }

    void __memberset__(int offset, OPL_BasicValue* value) override {
        auto& target = members[offset];
        if (target == nullptr) {
            target = value;
            return;
        }
        switch (target->kind) {
            case BV_INT: case BV_FLOAT: case BV_STRING: case BV_BOOL: case BV_ARRAY:
                target->__set__(value);
                break;
            case BV_OBJ: case BV_NULL: case BV_RAW_POINT:
                target = value;
                break;
        }
    }
};

inline int get_int(void*);

struct OPL_Float : public OPL_BasicValue {
    double f;
    OPL_Float(double _f) : OPL_BasicValue(BV_FLOAT), f(_f) {}

    OPL_BasicValue* __copy__() override {
        return new OPL_Float(f);
    }

    void __set__(OPL_BasicValue* other) override {
        expect_(other, {BV_INT, BV_FLOAT});
        if (other->kind == BV_INT) f = get_int((void*)other);
        else if (other->kind == BV_FLOAT) f = ((OPL_Float*)other)->f;
    }
};

struct OPL_Integer : public OPL_BasicValue {
    int i;
    OPL_Integer(int _i) : OPL_BasicValue(BV_INT), i(_i) {}

    OPL_BasicValue* __copy__() override {
        return new OPL_Integer(i);
    }

    void __set__(OPL_BasicValue* other) override {
        expect_(other, {BV_INT, BV_FLOAT});
        if (other->kind == BV_INT) i = ((OPL_Integer*)other)->i;
        else if (other->kind == BV_FLOAT) i = ((OPL_Float*)other)->f;
    }
};




int get_int(STACK_VALUE* arg) {
    if (arg->is_heap_ref) {
        return ((OPL_Integer*) arg)->i;
    }
    return arg->i_val;
}

inline int get_int(void* p) { return ((OPL_Integer*)p)->i; }

struct OPL_Bool : public OPL_BasicValue {
    bool b;
    OPL_Bool(bool _b) : OPL_BasicValue(BV_BOOL), b(_b) {}

    OPL_BasicValue* __copy__() override {
        return new OPL_Bool(b);
    }

    void __set__(OPL_BasicValue* other) override {
        expect_(other, {BV_BOOL});
        b = ((OPL_Bool*)other)->b;
    }
};

struct OPL_String : public OPL_BasicValue {
    std::string str;
    OPL_String(const std::string& st) : OPL_BasicValue(BV_STRING), str(st) {}

    OPL_BasicValue* __copy__() override {
        return new OPL_String(str);
    }

    void __set__(OPL_BasicValue* other) override {
        expect_(other, {BV_STRING});
        str = ((OPL_String*)other)->str;
    }

    OPL_BasicValue* __elementget__(OPL_BasicValue* other) override {
        expect_(other, {BV_INT});
        return new OPL_String(std::string (1, str[((OPL_Integer*)other)->i]));
    }

    void __elementset__(OPL_BasicValue* pos_, OPL_BasicValue* val_) override {
        expect_(pos_, {BV_INT});
        expect_(val_, {BV_STRING});
        str[((OPL_Integer*)pos_)->i] = ((OPL_String*)val_)->str[0];
    }
};

struct OPL_Array : public OPL_BasicValue {
    std::vector<OPL_BasicValue*> elements;
    OPL_Array(std::vector<OPL_BasicValue*> el)
    : OPL_BasicValue(BV_ARRAY), elements(std::move(el)) {}

    OPL_BasicValue* __copy__() override {
        std::vector<OPL_BasicValue*> tmp;
        for (auto i : elements) {
            if (i) tmp.push_back(i->__copy__());
        }
        return new OPL_Array(tmp);
    }

    OPL_Array(int size) : OPL_BasicValue(BV_ARRAY) { elements.resize(size); }

    void __set__(OPL_BasicValue* other) override {
        expect_(other, {BV_ARRAY});
        elements = ((OPL_Array*)other)->elements;
    }

    OPL_BasicValue* __elementget__(OPL_BasicValue* other) override {
        expect_(other, {BV_INT});
        return elements[((OPL_Integer*)other)->i];
    }

    void __elementset__(OPL_BasicValue* pos_, OPL_BasicValue* val_) override {
        auto temp = ((OPL_Integer*)pos_)->i;
        expect_(pos_, {BV_INT});
        auto tmp = elements[((OPL_Integer*)pos_)->i];
        if (tmp == nullptr) {
            elements[((OPL_Integer*)pos_)->i] = val_;
            return;
        }
        switch (tmp->kind) {
            case BV_INT:case BV_FLOAT:case BV_STRING:case BV_BOOL:case BV_ARRAY:
                tmp->__set__(val_);
                break;
            case BV_OBJ:case BV_NULL:case BV_RAW_POINT:
                elements[((OPL_Integer*)pos_)->i] = val_;
                break;
        }
    }
};

struct Chunk {
    std::vector<int> op_codes;
    std::vector<STACK_VALUE*> const_pool;
    std::vector<OPL_BasicValue> cons;
    std::vector<std::string> names;

    int get_name(std::string name) {
        for (int i = 0; i < names.size(); ++i)
            if (names[i] == name)
                return i;
        return -1;
    }

    int add_const(STACK_VALUE* value) {
        const_pool.push_back(value);
        return const_pool.size() - 1;
    }

    int add_name(std::string name) {
        names.push_back(name);
        return names.size() - 1;
    }
};

class VM;

typedef STACK_VALUE*(BUILD_IN_PROC)(std::vector<STACK_VALUE*>);

struct Frame {
	
    Frame* clone() {
        Frame* new_frame = nullptr;
        if (this->is_build_in) {
            new_frame = new Frame(this->proc, this->func_name);
            new_frame->func_id = this->func_id;
            new_frame->args_len = this->args_len;
            new_frame->is_lambda = this->is_lambda;
        } else {
            new_frame = new Frame(this->codes);
            new_frame->func_name = this->func_name;
            new_frame->func_id = this->func_id;
            new_frame->args_len = this->args_len;
            new_frame->is_lambda = this->is_lambda;
            new_frame->pc = nullptr;
        }
        new_frame->caller = nullptr;
        new_frame->stack.clear();
        new_frame->names.clear();
        return new_frame;
    }

    bool is_build_in = false;

    Frame(BUILD_IN_PROC proc, std::string name) {
        this->proc = proc;
        this->func_name = name;
        this->is_build_in = true;
    }

    void __build_in_call__() {
        caller->push((proc)(stack));
        stack.clear();
    }

    BUILD_IN_PROC* proc;

    int* pc, func_id, args_len = 0;
    Chunk *codes;
    std::string func_name;
    Frame* caller;
    std::vector<STACK_VALUE*> stack;
    std::unordered_map<std::string, STACK_VALUE*> names;

    void clear() {
        caller = nullptr;
        stack.clear();
        names.clear();
    }

    inline std::string get_name_by_id(int id) { return codes->names[id]; }

    inline int* get_start() {
        if (codes->op_codes.empty() || is_build_in) {
            std::cout << "In SubProc <Frame.get_start>, want get start, but " << ((is_build_in)? "the function is a build-in function" : "code is empty") << std::endl;
            exit(-1);
        }
        return &codes->op_codes[0];
    }

    inline STACK_VALUE* top() { return stack[stack.size() - 1]; }

    STACK_VALUE* pop() {
        if (stack.empty()) {
			std::cout << "Name = " << func_name << std::endl;
            printf("RuntimeError in pop: pop from empty stack\n");
            exit(-1);
        }
        auto tmep = stack.back();
        stack.pop_back();
        return tmep;
    }

    inline void push(STACK_VALUE* value) { stack.push_back(value); }

    inline void set_name(int id, STACK_VALUE* value) { names[codes->names[id]] = value; }

    inline STACK_VALUE* load_name(int id) { return names[codes->names[id]]; }

    inline void store_name(int id, STACK_VALUE* value) { names[codes->names[id]] = value; }

    inline STACK_VALUE* load_const(int id) { return codes->const_pool[id]; }

    Chunk tmp;

    Frame(Chunk codes) {
        caller = nullptr;
        this->tmp = codes;
        this->codes->op_codes = this->tmp.op_codes;
        pc = &this->tmp.op_codes[0];
    }

    bool is_lambda = false;

    Frame(Chunk *codes) { caller = nullptr; this->codes = codes; pc = &this->codes->op_codes[0]; }

    Frame(Frame* caller) { caller = nullptr;this->caller = caller; pc = &codes->op_codes[0]; }

    void debug() {
        printf("Function['%s', %zu]\n", func_name.c_str(), codes->op_codes.size());
        int i = 0;
        while (i < codes->op_codes.size()) {
            if (!(i % 16)) printf("\n\t");
            printf("%d ", codes->op_codes[i]);
            ++i;
        }
        printf("\n");
    }
};

static const struct {
    const char* name;
    int arg_count;
} instruction_info[] = {
     {"OP_LOAD_CONST", 1},
     {"OP_LOAD_NULL", 0},
     {"OP_LOAD_TRUE", 0},
     {"OP_LOAD_FALSE", 0},
     {"OP_LOAD_NAME", 1},
     {"OP_SET_NAME", 1},
     {"OP_LOAD_IMMEDIATLY", 1},
     {"OP_COPY", 0},
     {"OP_LOAD_FUNC_ADDR", 0},
     {"OP_LOAD_MODULE_METHOD", 1},
     {"OP_LOAD_MODULE", 2},
     {"OP_POP", 0},
     {"OP_DUP", 0},
     {"OP_PUSH", 1},
     {"OP_ADD", 0},
     {"OP_SUB", 0},
     {"OP_MUL", 0},
     {"OP_DIV", 0},
     {"OP_MOD", 0},
     {"OP_NEG", 0},
     {"OP_LEFT", 0},
     {"OP_RIGHT", 0},
     {"OP_BIT_AND", 0},
     {"OP_BIT_OR", 0},
     {"OP_BIT_NOT", 0},
     {"OP_EQ", 0},
     {"OP_NE", 0},
     {"OP_LT", 0},
     {"OP_LE", 0},
     {"OP_GT", 0},
     {"OP_GE", 0},
     {"OP_NOT", 0},
     {"OP_AND", 0},
     {"OP_OR", 0},
     {"OP_JUMP", 1},
     {"OP_JUMP_IF_FALSE", 1},
     {"OP_JUMP_IF_TRUE", 1},
     {"OP_GET_GLOBAL", 1},
     {"OP_SET_GLOBAL", 1},
     {"OP_CALL", 1},
     {"OP_RETURN", 0},
     {"OP_LEAVE", 0},
     {"OP_SPECIAL_CALL", 0},
     {"OP_NEW_ARRAY", 1},
     {"OP_GET_ELEMENT", 0},
     {"OP_SET_ELEMENT", 0},
     {"OP_NEW_OBJECT", 1},
     {"OP_MEMBER_GET", 1},
     {"OP_MEMBER_SET", 1},
     {"OP_PRINT", 0},
     {"OP_HALT", 0},
     {"OP_NOP", 0},
     {"OP_SWAP", 0},
     {"OP_ROT", 0}
};

static const int instruction_count = sizeof(instruction_info) / sizeof(instruction_info[0]);

void disassemble_instruction(Chunk* chunk, int* offset) {
    int op = chunk->op_codes[*offset];
    const auto& info = instruction_info[op];
    printf("%s", info.name);
    if (info.arg_count == 1) {
        int arg = chunk->op_codes[*offset + 1];
        printf("\t\t%d", arg);

        if (op == OP_LOAD_CONST) {
            STACK_VALUE* val = chunk->const_pool[arg];
            printf(" (");
            switch (val->kind) {
                case STACK_VALUE::S_INT:    printf("%d", val->i_val); break;
                case STACK_VALUE::S_DOUBLE: printf("%f", val->d_val); break;
                case STACK_VALUE::S_BOOL:   printf("%s", val->b_val ? "true" : "false"); break;
                case STACK_VALUE::S_STR:    printf("\"%s\"", val->str_value.c_str()); break;
                case STACK_VALUE::S_NULL:   printf("null"); break;
                default:                     printf("?"); break;
            }
            printf(")");
        }
        else if (op == OP_LOAD_NAME || op == OP_SET_NAME ||
                 op == OP_GET_GLOBAL || op == OP_SET_GLOBAL) {
            if (arg >= 0 && arg < chunk->names.size())
                printf(" (%s)", chunk->names[arg].c_str());
        }
        else if (op == OP_JUMP || op == OP_JUMP_IF_FALSE || op == OP_JUMP_IF_TRUE) {
            printf(" -> %d", arg);
        }
        else if (op == OP_CALL) {
        }
        else if (op == OP_NEW_ARRAY || op == OP_NEW_OBJECT) {
            printf(" size=%d", arg);
        }
        else if (op == OP_MEMBER_GET || op == OP_MEMBER_SET) {
            printf(" offset=%d", arg);
        }
        printf("\n");
        *offset += 2;
    } else {
        printf("\n");
        *offset += 1;
    }
}

void disassemble_chunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);
    int offset = 0;
    while (offset < chunk->op_codes.size()) {
        printf("%04d  ", offset);
        disassemble_instruction(chunk, &offset);
    }
}

#endif