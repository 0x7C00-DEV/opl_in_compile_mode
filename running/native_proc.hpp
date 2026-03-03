#ifndef COPL_NATIVE_PROC_H
#define COPL_NATIVE_PROC_H
#include "value.hpp"
#include "fstream"
#include "val_conv.hpp"
#define VM_NUL STACK_VALUE::make_null();

std::string get_string(OPL_VALUE::OPL_BasicValue* tmp) {
    std::string res;
    switch (tmp->kind) {
    case OPL_VALUE::BV_INT:
        res = std::to_string(((OPL_VALUE::OPL_Integer*)tmp)->i);
        break;
    case OPL_VALUE::BV_FLOAT:
        res = std::to_string(((OPL_VALUE::OPL_Float*)tmp)->f);
        break;
    case OPL_VALUE::BV_STRING:
        res = ((OPL_VALUE::OPL_String*)tmp)->str;
        break;
    case OPL_VALUE::BV_BOOL:
        res = (((OPL_VALUE::OPL_Bool*)tmp)->b) ? "true" : "false";
        break;
    case OPL_VALUE::BV_ARRAY:
        res += "[";
        for (auto i : ((OPL_VALUE::OPL_Array*)tmp)->elements)
            res += get_string(i) + ", ";
        res += "]";
        break;
    case OPL_VALUE::BV_OBJ:
        res = "<object>";
        break;
    case OPL_VALUE::BV_NULL:
        res = "null";
        break;
    case OPL_VALUE::BV_RAW_POINT: {
        char buf[32];
        sprintf(buf, "%p", ((OPL_VALUE::OPL_Point*)tmp)->pointer);
        res = "<pointer: ";
        res += buf;
        res += ">";
    }
        break;
    }
    return res;
}


std::string get_string(OPL_VALUE::STACK_VALUE* arg) {
    if (arg->is_heap_ref) {
        return get_string(arg->obj);
    } else {
        std::string res;
        switch (arg->kind) {
        case OPL_VALUE::STACK_VALUE::S_INT:
            res = std::to_string(arg->i_val);
            break;
        case OPL_VALUE::STACK_VALUE::S_BOOL:
            res = (arg->b_val)? "true" : "false";
            break;
        case OPL_VALUE::STACK_VALUE::S_DOUBLE:
            res = std::to_string(arg->d_val);
            break;
        case OPL_VALUE::STACK_VALUE::S_RAW:
            res = "VOID";
            break;
        case OPL_VALUE::STACK_VALUE::S_NULL:
            res = "null";
            break;
        case OPL_VALUE::STACK_VALUE::S_STR:
            res = arg->str_value;
            break;
        case OPL_VALUE::STACK_VALUE::S_FUC:
            throw std::exception();
            break;
        }
        return res;
    }
}

OPL_VALUE::STACK_VALUE* print(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    for (auto i : args) printf("%s", get_string(i).c_str());
    return OPL_VALUE::VM_NUL;
}

OPL_VALUE::STACK_VALUE* println(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    print(args);
    printf("\n");
    return OPL_VALUE::VM_NUL;
}

OPL_VALUE::STACK_VALUE* get_id_info(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    auto t = args[0];
    printf("- the info of args[0]:\n");
    printf("- type: %d\n", args[0]->kind);
    printf("- is heap ref: %d\n", args[0]->is_heap_ref);
    if (args[0]->is_heap_ref) {
        printf("    - Address: %lld\n", (long long)args[0]->obj);
        if (args[0]->obj) printf("    - RefType: %d\n", args[0]->obj->kind);
        else printf(" - RefTarget is a null pointer\n");
    }
    return OPL_VALUE::VM_NUL;
}


OPL_VALUE::STACK_VALUE* input(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    print(args);
    std::string res;
    std::getline(std::cin, res);
    return OPL_VALUE::STACK_VALUE::make_str(res);
}

OPL_VALUE::STACK_VALUE* read_file(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    std::ifstream ifs(get_string(args[0]));
    std::string buffer, res;
    while (std::getline(ifs, buffer))
        res += buffer + '\n';
    return OPL_VALUE::STACK_VALUE::make_str(res);
}

OPL_VALUE::STACK_VALUE* str2int(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    std::string tmp = get_string(args[0]);
    return OPL_VALUE::STACK_VALUE::make_int(std::stoi(tmp));
}

OPL_VALUE::STACK_VALUE* int2str(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    return OPL_VALUE::STACK_VALUE::make_str(get_integer(args[0]));
}

OPL_VALUE::STACK_VALUE* append(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    auto target = args[1];
    auto value = args[0];
    if (!target->is_heap_ref) {
        if (target->kind != OPL_VALUE::STACK_VALUE::S_STR) {
            std::cout << "is not a heap ref\n";
            exit(-1);
        } else {
            target->str_value += get_string(val_conv(value));
            return OPL_VALUE::VM_NUL;
        }
    }
    if (target->obj->kind == OPL_VALUE::BV_ARRAY) {
        ((OPL_VALUE::OPL_Array*) target->obj)->elements.push_back(val_conv(value));
    } else if (target->obj->kind == OPL_VALUE::BV_STRING) {
        ((OPL_VALUE::OPL_String*) target)->str += get_string(val_conv(value));
    } else {
        printf("unknown type %d\n", target->obj->kind);
        exit(-1);
    }
    return OPL_VALUE::VM_NUL;
}

OPL_VALUE::STACK_VALUE* pop_back(std::vector<OPL_VALUE::STACK_VALUE*> args) {
	auto tmp = args[0];
	if (tmp->is_heap_ref && tmp->obj) {
		if (tmp->obj->kind == OPL_VALUE::BV_ARRAY) {
			((OPL_VALUE::OPL_Array*)tmp->obj)->elements.pop_back();
		} else if (tmp->obj->kind == OPL_VALUE::BV_STRING) {
			((OPL_VALUE::OPL_String*)tmp->obj)->str.pop_back();
		} else {
			printf("Error: want a string or array\n");
			exit(-1);
		}
	} else {
		if (tmp->kind != OPL_VALUE::STACK_VALUE::S_STR) {
			printf("Error: want a string\n");
			exit(-1);
		}
		tmp->str_value.pop_back();
	}
    return OPL_VALUE::VM_NUL;
}

OPL_VALUE::STACK_VALUE* not_null(std::vector<OPL_VALUE::STACK_VALUE*> args) {
	OPL_VALUE::STACK_VALUE* v = args[0];
    bool is_null;
    if (v->is_heap_ref) {
        is_null = (v->obj == nullptr || v->obj->kind == OPL_VALUE::BV_NULL);
    } else {
        is_null = (v->kind == OPL_VALUE::STACK_VALUE::S_NULL);
    }
    return OPL_VALUE::STACK_VALUE::make_bool(!is_null);
}

OPL_VALUE::STACK_VALUE* length(std::vector<OPL_VALUE::STACK_VALUE*> args) {
    auto _this = args[0];
    if (_this->is_heap_ref) {
        if (_this->obj && _this->obj->kind == OPL_VALUE::BV_STRING)
            return OPL_VALUE::STACK_VALUE::make_int(((OPL_VALUE::OPL_String*)_this->obj)->str.size());
        else if (_this->obj && _this->obj->kind == OPL_VALUE::BV_ARRAY)
            return OPL_VALUE::STACK_VALUE::make_int(((OPL_VALUE::OPL_Array*)_this->obj)->elements.size());
        else {
            printf("Warning: length() called on non-string/array heap object, returning 0\n");
            return OPL_VALUE::STACK_VALUE::make_int(0);
        }
    } else {
        if (_this->kind == OPL_VALUE::STACK_VALUE::S_STR)
            return OPL_VALUE::STACK_VALUE::make_int(_this->str_value.size());
        else {
            printf("Warning: length() called on non-string stack value, returning 0\n");
            return OPL_VALUE::STACK_VALUE::make_int(0);
        }
    }
}

const std::unordered_map<std::string, OPL_VALUE::BUILD_IN_PROC*> builtins = {
    {"print", print},
    {"println", println},
    {"input", input},
    {"debug", get_id_info},
    {"append", append},
    {"size", length},
    {"str2int", str2int},
    {"not_null", not_null},
    {"read_file", read_file},
    {"int2str", int2str},
    {"pop_back", pop_back}
};

#endif