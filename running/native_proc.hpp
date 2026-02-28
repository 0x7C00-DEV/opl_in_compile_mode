#ifndef COPL_NATIVE_PROC_H
#define COPL_NATIVE_PROC_H
#include "value.hpp"
#include "fstream"
#define VM_NUL STACK_VALUE::make_null();

std::string get_string(OPL_BasicValue* tmp) {
    std::string res;
    switch (tmp->kind) {
    case BV_INT:
        res = std::to_string(((OPL_Integer*)tmp)->i);
        break;
    case BV_FLOAT:
        res = std::to_string(((OPL_Float*)tmp)->f);
        break;
    case BV_STRING:
        res = ((OPL_String*)tmp)->str;
        break;
    case BV_BOOL:
        res = (((OPL_Bool*)tmp)->b) ? "true" : "false";
        break;
    case BV_ARRAY:
        res += "[";
        for (auto i : ((OPL_Array*)tmp)->elements)
            res += get_string(i) + ", ";
        res += "]";
        break;
    case BV_OBJ:
        res = "<object>";
        break;
    case BV_NULL:
        res = "null";
        break;
    case BV_RAW_POINT: {
        char buf[32];
        sprintf(buf, "%p", ((OPL_Point*)tmp)->pointer);
        res = "<pointer: ";
        res += buf;
        res += ">";
    }
        break;
    }
    return res;
}

std::string get_string(STACK_VALUE* arg) {
    if (arg->is_heap_ref) {
        return get_string(arg->obj);
    } else {
        std::string res;
        switch (arg->kind) {
        case STACK_VALUE::S_INT:
            res = std::to_string(arg->i_val);
            break;
        case STACK_VALUE::S_BOOL:
            res = (arg->b_val)? "true" : "false";
            break;
        case STACK_VALUE::S_DOUBLE:
            res = std::to_string(arg->d_val);
            break;
        case STACK_VALUE::S_RAW:
            res = "VOID";
            break;
        case STACK_VALUE::S_NULL:
            res = "null";
            break;
        case STACK_VALUE::S_STR:
            res = arg->str_value;
            break;
        case STACK_VALUE::S_FUC:
            throw std::exception();
            break;
        }
        return res;
    }
}

STACK_VALUE* print(std::vector<STACK_VALUE*> args) {
    for (auto i : args) printf("%s", get_string(i).c_str());
    return VM_NUL;
}

STACK_VALUE* println(std::vector<STACK_VALUE*> args) {
    print(args);
    printf("\n");
    return VM_NUL;
}

STACK_VALUE* get_id_info(std::vector<STACK_VALUE*> args) {
    auto t = args[0];
    printf("- the info of args[0]:\n");
    printf("- type: %d\n", args[0]->kind);
    printf("- is heap ref: %d\n", args[0]->is_heap_ref);
    if (args[0]->is_heap_ref) {
        printf("    - Address: %lld\n", (long long)args[0]->obj);
        if (args[0]->obj) printf("    - RefType: %d\n", args[0]->obj->kind);
        else printf(" - RefTarget is a null pointer\n");
    }
    return VM_NUL;
}

STACK_VALUE* input(std::vector<STACK_VALUE*> args) {
    print(args);
    std::string res;
    std::getline(std::cin, res);
    return STACK_VALUE::make_str(res);
}

STACK_VALUE* read_file(std::vector<STACK_VALUE*> args) {
    std::ifstream ifs(get_string(args[0]));
    std::string buffer, res;
    while (std::getline(ifs, buffer))
        res += buffer + '\n';
    return STACK_VALUE::make_str(res);
}

STACK_VALUE* str2int(std::vector<STACK_VALUE*> args) {
    std::string tmp = get_string(args[0]);
    return STACK_VALUE::make_int(std::stoi(tmp));
}

STACK_VALUE* int2str(std::vector<STACK_VALUE*> args) {
    return VM_NUL;
}

OPL_BasicValue* val_conv(STACK_VALUE* value) {
    if (value->is_heap_ref) return value->obj;
    OPL_BasicValue* new_obj = nullptr;
    switch (value->kind) {
    case STACK_VALUE::S_INT:
        new_obj = new OPL_Integer(value->i_val);
        break;
    case STACK_VALUE::S_BOOL:
        new_obj = new OPL_Bool(value->b_val);
        break;
    case STACK_VALUE::S_DOUBLE:
        new_obj = new OPL_Float(value->d_val);
        break;
    case STACK_VALUE::S_RAW:
        new_obj = new OPL_Point(value->raw_ptr);
        break;
    case STACK_VALUE::S_NULL:
        new_obj = new OPL_Null;
        break;
    case STACK_VALUE::S_STR:
        new_obj = new OPL_String(value->str_value);
        break;
    case STACK_VALUE::S_FUC:
        new_obj = new OPL_Point(value->raw_ptr);
        break;
    default:
        return nullptr;
    }
    new_obj->next = nullptr;
    return new_obj;
}

STACK_VALUE* append(std::vector<STACK_VALUE*> args) {
    auto target = args[1];
    auto value = args[0];
    if (!target->is_heap_ref) {
        if (target->kind != STACK_VALUE::S_STR) {
            std::cout << "is not a heap ref\n";
            exit(-1);
        } else {
            target->str_value += get_string(val_conv(value));
            return VM_NUL;
        }
    }
    if (target->obj->kind == BV_ARRAY) {
        ((OPL_Array*) target->obj)->elements.push_back(val_conv(value));
    } else if (target->obj->kind == BV_STRING) {
        ((OPL_String*) target)->str += get_string(val_conv(value));
    } else {
        printf("unknown type %d\n", target->obj->kind);
        exit(-1);
    }
    return VM_NUL;
}

STACK_VALUE* pop_back(std::vector<STACK_VALUE*> args) {
    return VM_NUL;
}

STACK_VALUE* not_null(std::vector<STACK_VALUE*> args) {
    STACK_VALUE* v = args[0];
    bool is_null = false;
    if (v->is_heap_ref) {
        is_null = (v->obj == nullptr || v->obj->kind == BV_NULL);
    } else {
        is_null = (v->kind == STACK_VALUE::S_NULL);
    }
    return STACK_VALUE::make_bool(!is_null);
}

STACK_VALUE* length(std::vector<STACK_VALUE*> args) {
    auto _this = args[0];
    if (_this->is_heap_ref) {
        if (_this->obj && _this->obj->kind == BV_STRING)
            return STACK_VALUE::make_int(((OPL_String*)_this->obj)->str.size());
        else if (_this->obj && _this->obj->kind == BV_ARRAY)
            return STACK_VALUE::make_int(((OPL_Array*)_this->obj)->elements.size());
        else {
            printf("Warning: length() called on non-string/array heap object, returning 0\n");
            return STACK_VALUE::make_int(0);
        }
    } else {
        if (_this->kind == STACK_VALUE::S_STR)
            return STACK_VALUE::make_int(_this->str_value.size());
        else {
            printf("Warning: length() called on non-string stack value, returning 0\n");
            return STACK_VALUE::make_int(0);
        }
    }
}

const std::unordered_map<std::string, BUILD_IN_PROC*> builtins = {
    {"print", print},
    {"println", println},
    {"input", input},
    {"debug", get_id_info},
    {"append", append},
    {"size", length},
    {"str2int", str2int},
    {"not_null", not_null}
};

#endif