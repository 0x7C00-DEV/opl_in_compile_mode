#ifndef COPL_VM_HPP
#define COPL_VM_HPP
#include "program_loader.hpp"
#include "value.hpp"
#include <cmath>
#include "native_proc.hpp"
#include "asm.hpp"
#include <unordered_map>

struct Module {
    std::vector<Frame*> funcs;
    std::string name;

    bool is_exist(std::string fname) {
        for (auto i : funcs)
            if (i->func_name == fname)
                return true;
        return false;
    }

    Frame* load_func(std::string fname) {
        for (auto i : funcs)
            if (i->func_name == fname)
                return i;
        std::cout << "Function '" << fname << "' not found in module '" << name << "'\n";
        exit(-1);
    }
};

class VM {
public:

    VM(std::string path, bool is_debug = false) : is_debug(is_debug) {
        this->frames = load_bytecode(path, builtins);
        heap_head = new OPL_BasicValue(BV_NULL);
        calls.push_back(find_function_by_name("main"));
        execute();
    }
	
	VM(Frame* f, VM* vm, bool is_debug) {
		this->is_debug = is_debug;
		std::string name = vm->current_module;
		for (auto l : vm->modules) {
			if (l->name == name)
				this->frames = l->funcs;
		}
		for (auto l : vm->modules)
			if (l->name != name)
				modules.push_back(l);
		calls.push_back(f);
		execute();
	}

    VM(std::vector<Frame*> frames, bool is_debug = false) : is_debug(is_debug) {
        this->frames = frames;
        heap_head = new OPL_BasicValue(BV_NULL);
        calls.push_back(find_function_by_name("main"));
        execute();
    }

    int i;
    bool is_debug = false;

    void debug() {
        if (is_debug)
            printf("CurrentOperator = %d, StackSize = %zu\n", i, (!calls.empty())? get_current()->stack.size() : 0);
    }

    bool execute() {
        while (!calls.empty()) {
#define GET *(get_current()->pc++)
            if (get_current()->is_build_in) {
                get_current()->__build_in_call__();
                calls.pop_back();
                debug();
                continue;
            }
            i = GET;
            switch (i) {
                case OP_LOAD_CONST: {
                    auto tmp = get_current()->load_const(GET);
                    get_current()->push(tmp);
                    debug();
                    break;
                }

                case OP_LOAD_NULL: {
                    get_current()->push(STACK_VALUE::make_null());
                    debug();
                    break;
                }

                case OP_LOAD_TRUE: {
                    get_current()->push(STACK_VALUE::make_bool(true));
                    debug();
                    break;
                }

                case OP_LOAD_FALSE: {
                    get_current()->push(STACK_VALUE::make_bool(false));
                    debug();
                    break;
                }

                case OP_LOAD_NAME: {
                    int id = GET;
                    get_current()->push(get_current()->load_name(id));
                    debug();
                    break;
                }

                case OP_SET_NAME: {
                    int id = GET;
                    get_current()->set_name(id, get_current()->pop());
                    debug();
                    break;
                }

                case OP_POP: {
                    get_current()->pop();
                    debug();
                    break;
                }

                case OP_DUP: {
                    get_current()->push(get_current()->top());
                    debug();
                    break;
                }

                case OP_PUSH: {
                    int id = GET;
                    get_current()->push(get_current()->load_const(id));
                    debug();
                    break;
                }

                case OP_ADD: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("ErrorTypeIs: %d\n", v->obj->kind);
                                printf("RuntimeError: cannot add non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    bool is_double = (left->kind == STACK_VALUE::S_DOUBLE || right->kind == STACK_VALUE::S_DOUBLE) ||
                                     (left->is_heap_ref && left->obj->kind == BV_FLOAT) ||
                                     (right->is_heap_ref && right->obj->kind == BV_FLOAT);
                    if (is_double)
                        get_current()->push(STACK_VALUE::make_double(l + r));
                    else
                        get_current()->push(STACK_VALUE::make_int((int32_t)(l + r)));
                    debug();
                    break;
                }

                case OP_SUB: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [  ](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot subtract non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    bool is_double = (left->kind == STACK_VALUE::S_DOUBLE || right->kind == STACK_VALUE::S_DOUBLE) ||
                                     (left->is_heap_ref && left->obj->kind == BV_FLOAT) ||
                                     (right->is_heap_ref && right->obj->kind == BV_FLOAT);
                    if (is_double)
                        get_current()->push(STACK_VALUE::make_double(l - r));
                    else
                        get_current()->push(STACK_VALUE::make_int((int32_t)(l - r)));
                    debug();
                    break;
                }

                case OP_MUL: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot multiply non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    bool is_double = (left->kind == STACK_VALUE::S_DOUBLE || right->kind == STACK_VALUE::S_DOUBLE) ||
                                     (left->is_heap_ref && left->obj->kind == BV_FLOAT) ||
                                     (right->is_heap_ref && right->obj->kind == BV_FLOAT);
                    if (is_double)
                        get_current()->push(STACK_VALUE::make_double(l * r));
                    else
                        get_current()->push(STACK_VALUE::make_int((int32_t)(l * r)));
                    debug();
                    break;
                }

                case OP_DIV: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot divide non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    bool is_double = (left->kind == STACK_VALUE::S_DOUBLE || right->kind == STACK_VALUE::S_DOUBLE) ||
                                     (left->is_heap_ref && left->obj->kind == BV_FLOAT) ||
                                     (right->is_heap_ref && right->obj->kind == BV_FLOAT);
                    if (is_double)
                        get_current()->push(STACK_VALUE::make_double(l / r));
                    else
                        get_current()->push(STACK_VALUE::make_int((int32_t)(l / r)));
                    debug();
                    break;
                }

                case OP_MOD: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot modulo non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    bool is_double = (left->kind == STACK_VALUE::S_DOUBLE || right->kind == STACK_VALUE::S_DOUBLE) ||
                                     (left->is_heap_ref && left->obj->kind == BV_FLOAT) ||
                                     (right->is_heap_ref && right->obj->kind == BV_FLOAT);
                    if (is_double)
                        get_current()->push(STACK_VALUE::make_double(fmod(l, r)));
                    else
                        get_current()->push(STACK_VALUE::make_int((int32_t)l % (int32_t)r));
                    debug();
                    break;
                }

                case OP_LEFT: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_int = [](STACK_VALUE* v) -> int {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else {
                                printf("RuntimeError: left shift requires integer operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_INT) {
                                printf("RuntimeError: left shift requires integer operand\n");
                                exit(-1);
                            }
                            return v->i_val;
                        }
                    };
                    int l = to_int(left);
                    int r = to_int(right);
                    get_current()->push(STACK_VALUE::make_int(l << r));
                    debug();
                    break;
                }

                case OP_RIGHT: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_int = [](STACK_VALUE* v) -> int {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else {
                                printf("RuntimeError: right shift requires integer operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_INT) {
                                printf("RuntimeError: right shift requires integer operand\n");
                                exit(-1);
                            }
                            return v->i_val;
                        }
                    };
                    int l = to_int(left);
                    int r = to_int(right);
                    get_current()->push(STACK_VALUE::make_int(l >> r));
                    debug();
                    break;
                }

                case OP_NEG: {
                    STACK_VALUE *a = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot negate non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double val = to_number(a);
                    if (a->is_heap_ref && a->obj->kind == BV_FLOAT)
                        get_current()->push(STACK_VALUE::make_double(-val));
                    else if (!a->is_heap_ref && a->kind == STACK_VALUE::S_DOUBLE)
                        get_current()->push(STACK_VALUE::make_double(-val));
                    else
                        get_current()->push(STACK_VALUE::make_int((int32_t)-val));
                    debug();
                    break;
                }

                case OP_EQ: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    std::string ltmp, rtmp;
                    if ((!right->is_heap_ref && right->kind == STACK_VALUE::S_STR) || (right->is_heap_ref && right->obj->kind == BV_STRING))
                        rtmp = ((OPL_String*)val_conv(right))->str;
                    if ((!left->is_heap_ref && left->kind == STACK_VALUE::S_STR) || (left->is_heap_ref && left->obj->kind == BV_STRING))
                        ltmp = ((OPL_String*)val_conv(left))->str;
                    if (!ltmp.empty() || !rtmp.empty()) {
                        get_current()->push(STACK_VALUE::make_bool(ltmp == rtmp));
                        break;
                    }
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot compare non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    get_current()->push(STACK_VALUE::make_bool(l == r));
                    debug();
                    break;
                }

                case OP_NE: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    std::string ltmp, rtmp;
                    if ((!right->is_heap_ref && right->kind == STACK_VALUE::S_STR) || (right->is_heap_ref && right->obj->kind == BV_STRING))
                        rtmp = ((OPL_String*)val_conv(right))->str;
                    if ((!left->is_heap_ref && left->kind == STACK_VALUE::S_STR) || (left->is_heap_ref && left->obj->kind == BV_STRING))
                        ltmp = ((OPL_String*)val_conv(left))->str;
                    if (!ltmp.empty() || !rtmp.empty()) {
                        get_current()->push(STACK_VALUE::make_bool(ltmp != rtmp));
                        break;
                    }
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot compare non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    get_current()->push(STACK_VALUE::make_bool(l != r));
                    debug();
                    break;
                }

                case OP_LT: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot compare non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    get_current()->push(STACK_VALUE::make_bool(l < r));
                    debug();
                    break;
                }

                case OP_LE: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot compare non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    get_current()->push(STACK_VALUE::make_bool(l <= r));
                    debug();
                    break;
                }

                case OP_GT: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot compare non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    get_current()->push(STACK_VALUE::make_bool(l > r));
                    debug();
                    break;
                }

                case OP_GE: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_number = [](STACK_VALUE* v) -> double {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else if (v->obj->kind == BV_FLOAT)
                                return ((OPL_Float*)v->obj)->f;
                            else {
                                printf("RuntimeError: cannot compare non-numeric heap type\n");
                                exit(-1);
                            }
                        } else {
                            return (v->kind == STACK_VALUE::S_INT) ? v->i_val : v->d_val;
                        }
                    };
                    double l = to_number(left);
                    double r = to_number(right);
                    get_current()->push(STACK_VALUE::make_bool(l >= r));
                    debug();
                    break;
                }

                case OP_BIT_AND: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_int = [](STACK_VALUE* v) -> int {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else {
                                printf("RuntimeError: bitwise AND requires integer operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_INT) {
                                printf("RuntimeError: bitwise AND requires integer operand\n");
                                exit(-1);
                            }
                            return v->i_val;
                        }
                    };
                    int l = to_int(left);
                    int r = to_int(right);
                    get_current()->push(STACK_VALUE::make_int(l & r));
                    debug();
                    break;
                }

                case OP_BIT_OR: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_int = [](STACK_VALUE* v) -> int {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else {
                                printf("RuntimeError: bitwise OR requires integer operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_INT) {
                                printf("RuntimeError: bitwise OR requires integer operand\n");
                                exit(-1);
                            }
                            return v->i_val;
                        }
                    };
                    int l = to_int(left);
                    int r = to_int(right);
                    get_current()->push(STACK_VALUE::make_int(l | r));
                    debug();
                    break;
                }

                case OP_BIT_NOT: {
                    STACK_VALUE *a = get_current()->pop();
                    auto to_int = [](STACK_VALUE* v) -> int {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_INT)
                                return ((OPL_Integer*)v->obj)->i;
                            else {
                                printf("RuntimeError: bitwise NOT requires integer operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_INT) {
                                printf("RuntimeError: bitwise NOT requires integer operand\n");
                                exit(-1);
                            }
                            return v->i_val;
                        }
                    };
                    int val = to_int(a);
                    get_current()->push(STACK_VALUE::make_int(~val));
                    debug();
                    break;
                }

                case OP_NOT: {
                    auto t = get_current()->pop();
                    bool b;
                    if (t->is_heap_ref) {
                        if (t->obj->kind == BV_BOOL)
                            b = ((OPL_Bool*)t->obj)->b;
                        else {
                            printf("RuntimeError: logical NOT requires boolean operand\n");
                            exit(-1);
                        }
                    } else {
                        if (t->kind != STACK_VALUE::S_BOOL) {
                            printf("RuntimeError: logical NOT requires boolean operand\n");
                            exit(-1);
                        }
                        b = t->b_val;
                    }
                    get_current()->push(STACK_VALUE::make_bool(!b));
                    debug();
                    break;
                }

                case OP_AND: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_bool = [](STACK_VALUE* v) -> bool {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_BOOL)
                                return ((OPL_Bool*)v->obj)->b;
                            else {
                                printf("RuntimeError: logical AND requires boolean operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_BOOL) {
                                printf("RuntimeError: logical AND requires boolean operand\n");
                                exit(-1);
                            }
                            return v->b_val;
                        }
                    };
                    bool l = to_bool(left);
                    bool r = to_bool(right);
                    get_current()->push(STACK_VALUE::make_bool(l && r));
                    debug();
                    break;
                }

                case OP_OR: {
                    STACK_VALUE *right = get_current()->pop();
                    STACK_VALUE *left = get_current()->pop();
                    auto to_bool = [](STACK_VALUE* v) -> bool {
                        if (v->is_heap_ref) {
                            if (v->obj->kind == BV_BOOL)
                                return ((OPL_Bool*)v->obj)->b;
                            else {
                                printf("RuntimeError: logical OR requires boolean operand\n");
                                exit(-1);
                            }
                        } else {
                            if (v->kind != STACK_VALUE::S_BOOL) {
                                printf("RuntimeError: logical OR requires boolean operand\n");
                                exit(-1);
                            }
                            return v->b_val;
                        }
                    };
                    bool l = to_bool(left);
                    bool r = to_bool(right);
                    get_current()->push(STACK_VALUE::make_bool(l || r));
                    debug();
                    break;
                }

                case OP_JUMP: {
                    auto addr = GET;
                    get_current()->pc = get_current()->get_start() + addr;
                    debug();
                    break;
                }

                case OP_JUMP_IF_FALSE: {
                    int addr = GET;
                    auto cond = get_current()->pop();
                    bool b;
                    if (cond->is_heap_ref) {
                        if (cond->obj->kind == BV_BOOL)
                            b = ((OPL_Bool*)cond->obj)->b;
                        else {
                            printf("RuntimeError: condition must be boolean\n");
                            exit(-1);
                        }
                    } else {
                        if (cond->kind != STACK_VALUE::S_BOOL) {
                            printf("RuntimeError: condition must be boolean\n");
                            exit(-1);
                        }
                        b = cond->b_val;
                    }
                    if (!b)
                        get_current()->pc = get_current()->get_start() + addr;
                    debug();
                    break;
                }

                case OP_JUMP_IF_TRUE: {
                    int addr = GET;
                    auto cond = get_current()->pop();
                    bool b;
                    if (cond->is_heap_ref) {
                        if (cond->obj->kind == BV_BOOL)
                            b = ((OPL_Bool*)cond->obj)->b;
                        else {
                            printf("RuntimeError: condition must be boolean\n");
                            exit(-1);
                        }
                    } else {
                        if (cond->kind != STACK_VALUE::S_BOOL) {
                            printf("RuntimeError: condition must be boolean\n");
                            exit(-1);
                        }
                        b = cond->b_val;
                    }
                    if (b)
                        get_current()->pc = get_current()->get_start() + addr;
                    debug();
                    break;
                }

                case OP_GET_GLOBAL: {
                    std::string name = get_current()->get_name_by_id(GET);
                    auto it = globals.find(name);
                    if (it != globals.end())
                        get_current()->push(it->second);
                    else {
                        printf("Undefined global '%s'\n", name.c_str());
                        exit(-1);
                    }
                    debug();
                    break;
                }

                case OP_SET_GLOBAL: {
                    auto val = get_current()->pop();
                    set_global(get_current()->get_name_by_id(GET), val);
                    debug();
                    break;
                }

                case OP_CALL: {
                    create_task_by_id(GET);
                    debug();
                    break;
                }

                case OP_LEAVE: {
                    calls.pop_back();
                    debug();
                    break;
                }

                case OP_RETURN: {
                    STACK_VALUE* retval = get_current()->pop();
                    Frame* caller = get_current()->caller;
                    calls.pop_back();
                    if (caller) caller->push(retval);
					else {
						std::cout << "is null!\n";
	                    exit(-1);
					}
                    debug();
                    break;
                }

                case OP_NEW_ARRAY: {
                    get_current()->push(new_array(GET));
                    debug();
                    break;
                }

                case OP_GET_ELEMENT: {
                    auto _pos = get_current()->pop();
                    auto _obj = get_current()->pop();
                    get_current()->push(element_get(_obj, _pos));
                    debug();
                    break;
                }

                case OP_COPY: {
                    auto val = get_current()->pop();
                    if (val->is_heap_ref) {
                        OPL_BasicValue* copy = val->obj->__copy__();
                        get_tail()->next = copy;
                        copy->next = nullptr;
                        get_current()->push(STACK_VALUE::make_heap(copy));
                    } else {
                        get_current()->push(val);
                    }
                    debug();
                    break;
                }

                case OP_LOAD_IMMEDIATLY: {
                    get_current()->push(STACK_VALUE::make_int(GET));
                    debug();
                    break;
                }

                case OP_SWAP: {
                    auto a = get_current()->pop();
                    auto b = get_current()->pop();
                    get_current()->push(a);
                    get_current()->push(b);
                    debug();
                    break;
                }

                case OP_ROT: {
                    auto a = get_current()->pop();
                    auto b = get_current()->pop();
                    auto c = get_current()->pop();
                    get_current()->push(b);
                    get_current()->push(a);
                    get_current()->push(c);
                    debug();
                    break;
                }

                // OP_LOAD_MODULE_METHOD <mod_name>(on stack top) <method_name>
                case OP_LOAD_MODULE_METHOD: {
					is_p_modile = true;
                    std::string mod_name = ((OPL_String*)val_conv(get_current()->pop()))->str;
					current_module = mod_name;
                    std::string method_name = ((OPL_String*)val_conv(get_current()->load_const(GET)))->str;
                    get_current()->push(STACK_VALUE::make_func((void*) (find_method_proc(mod_name, method_name))->clone()));
                    break;
                }

                // OP_LOAD_MODULE <path> <name>
                case OP_LOAD_MODULE: {
                    std::string path = ((OPL_String*)val_conv(get_current()->load_const(GET)))->str;
                    std::string name = ((OPL_String*)val_conv(get_current()->load_const(GET)))->str;
                    Module* m = new Module;
                    m->name = name;
                    m->funcs = load_bytecode(path, builtins);
                    modules.push_back(m);
                    break;
                }

                case OP_LOAD_FUNC_ADDR: {
                    auto id = GET;
                    auto lf = find_function_by_id(id)->clone();
                    get_current()->push(STACK_VALUE::make_func((void*)lf));
                    break;
                }
	            
	            case OP_SPECIAL_CALL: {
		            auto _v = get_current()->pop();
		            Frame* callee = nullptr;
		            if (_v->is_heap_ref) callee = (Frame*)(((OPL_Point*)_v->obj)->pointer);
		            else callee = (Frame*)_v->raw_ptr;
		            Frame* caller = get_current();
		            callee->caller = caller;
		            if (!callee->is_build_in) {
			            callee->pc = callee->get_start();
		            }
		            for (int g = 0; g < callee->args_len; ++g)
			            callee->push(caller->pop());
					if (!is_p_modile) calls.push_back(callee);
					else {
						VM sub_proc(callee, this, is_debug);
						is_p_modile = false;
						current_module = "";
					}
		            break;
	            }

                case OP_SET_ELEMENT: {
                    auto _val = get_current()->pop();
                    auto _pos = get_current()->pop();
                    auto _obj = get_current()->pop();
                    if (!_obj) {
                        std::cout << "Element get error: object is null pointer\n";
                        exit(-1);
                    }
                    element_set(_obj, _pos, _val);
                    debug();
                    break;
                }

                case OP_NEW_OBJECT: {
                    get_current()->push(new_object(GET));
                    debug();
                    break;
                }

                case OP_PRINT: {
                    auto v = get_current()->pop();
                    if (v->is_heap_ref && v->obj->kind == BV_STRING) {
                        std::cout << ((OPL_String*)v->obj)->str;
                    } else if (!v->is_heap_ref && v->kind == STACK_VALUE::S_INT) {
                        std::cout << v->i_val;
                    } else if (v->is_heap_ref && v->obj->kind == BV_INT) {
                        std::cout << ((OPL_Integer*)v->obj)->i;
                    } else {
                        printf("Unsupported type for PRINT\n");
                        exit(-1);
                    }
                    debug();
                    break;
                }

                case OP_MEMBER_GET: {
                    auto obj = get_current()->pop();
                    if (!obj) {
                        std::cout << "in OP_MEMBER_GET, obj is null pointer\n";
                        exit(-1);
                    }
                    if (!obj->is_heap_ref || !obj->obj) {
                        std::cout << "object is not a heap ref or value is null\n";
                        exit(-1);
                    }
                    expect_heap_val(obj, BV_OBJ);
                    int index = GET;
                    auto member = ((OPL_Object*)obj->obj)->__memberget__(index);
                    get_current()->push(STACK_VALUE::make_heap(member));
                    debug();
                    break;
                }

                case OP_MEMBER_SET: {
                    auto val = get_current()->pop();
                    auto obj = get_current()->pop();
                    expect_heap_val(obj, BV_OBJ);
                    int index = GET;
                    ((OPL_Object*)obj->obj)->__memberset__(index, val_conv(val));
                    debug();
                    break;
                }

                case OP_HALT: { exit(get_current()->pop()->i_val); debug(); }

                case OP_NOP: { debug(); break; }

                default: {
                    printf("unknown operator: %d\n", i);
                    exit(-1);
                }
            }
        }
        return true;
    }

private:
    std::vector<Frame*> frames;
    std::vector<Frame*> calls;
    std::vector<Module*> modules;
    OPL_BasicValue* heap_head;
    std::unordered_map<std::string, STACK_VALUE*> globals;
    friend struct Frame;

    Frame* find_method_proc(std::string mod_name, std::string method_name) {
        for (auto _i : modules)
            if (_i->name == mod_name && _i->is_exist(method_name))
                return _i->load_func(method_name);
        std::cout << "module '" << mod_name << "' not found\n";
        exit(-1);
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
        get_tail()->next = new_obj;
        new_obj->next = nullptr;
        return new_obj;
    }
	
	std::string current_module;
	bool is_p_modile = false;
	
	Frame* get_current() { if (calls.empty()) { return nullptr; } return calls.back(); }

    void create_task_by_id(int id) {
        Frame* callee = find_function_by_id(id)->clone();
        Frame* caller = get_current();
        callee->caller = caller;
        if (!callee->is_build_in) callee->pc = callee->get_start();
        for (int w = 0; w < callee->args_len; ++w)
            callee->push(caller->pop());
        calls.push_back(callee);
    }

    void create_task_by_name(std::string id) {
        Frame* callee = find_function_by_name(id)->clone();
        Frame* caller = get_current();
        callee->caller = caller;
        if (!callee->is_build_in) callee->pc = callee->get_start();
        for (int w = 0; w < callee->args_len; ++w)
            callee->push(caller->pop());
        calls.push_back(callee);
    }

    void expect_val(STACK_VALUE* value, STACK_VALUE::ValueType valueType) {
        if (value->kind != valueType) {
            printf("RuntimeError: expected type %d, got %d\n", valueType, value->kind);
            exit(-1);
        }
    }

    void expect_heap_val(STACK_VALUE* value, BV_Kind valueType) {
        if (!value->is_heap_ref) {
            printf("RuntimeError: expected heap reference\n");
            exit(-1);
        }
        if (value->obj->kind != valueType) {
            printf("RuntimeError: expected heap type %d, got %d\n", valueType, value->obj->kind);
            exit(-1);
        }
    }

    void set_global(std::string name, STACK_VALUE* value) {
        globals[name] = value;
    }

    std::string load_string(STACK_VALUE* value) {
        return ((OPL_String*)value->obj)->str;
    }

    OPL_BasicValue* get_tail() {
        OPL_BasicValue* head_ = heap_head;
        while (head_->next) head_ = head_->next;
        return head_;
    }

    void element_set(STACK_VALUE* object, STACK_VALUE* pos, STACK_VALUE* value) {
        if (object->is_heap_ref) {
            auto arr_obj = object->obj;
            int position = ((OPL_Integer*)val_conv(pos))->i;
            auto op = new OPL_Integer(position);
            auto val_obj = val_conv(value);
            arr_obj->__elementset__(op, val_obj);
            delete op;
        } else if (object->kind == STACK_VALUE::S_STR) {
            object->str_value[pos->i_val] = ((OPL_String*)val_conv(value))->str[0];
        }
    }

    STACK_VALUE* element_get(STACK_VALUE* object, STACK_VALUE* pos) {
        if (!object->is_heap_ref) {
            char tmp = object->str_value[get_int(pos)];
            std::string tm;
            tm += tmp;
            STACK_VALUE* v = new STACK_VALUE;
            v->is_heap_ref = false;
            v->str_value = tm;
            v->kind = STACK_VALUE::S_STR;
            return v;
        }
        auto obj = object->obj;
        auto pos_obj = (pos->is_heap_ref)? pos->obj : new OPL_Integer(pos->i_val);
        auto result = obj->__elementget__(pos_obj);
        delete pos_obj;
        return STACK_VALUE::make_heap(result);
    }

    STACK_VALUE* new_array(int size) {
        OPL_Array* new_array = new OPL_Array(size);
        get_tail()->next = new_array;
        new_array->next = nullptr;
        STACK_VALUE* value = new STACK_VALUE;
        value->is_heap_ref = true;
        value->obj = new_array;
        return value;
    }

    STACK_VALUE* new_object(int size) {
        OPL_Object* obj = new OPL_Object(size);
        get_tail()->next = obj;
        obj->next = nullptr;
        STACK_VALUE* value = new STACK_VALUE;
        value->is_heap_ref = true;
        value->obj = obj;
        return value;
    }

    Frame* find_function_by_name(std::string name) {
        for (auto ti : frames)
            if (ti->func_name == name)
                return ti;
        printf("Function '%s' not found\n", name.c_str());
        exit(-1);
    }

    Frame* find_function_by_id(int id) {
        for (auto ti : frames)
            if (ti->func_id == id)
                return ti;
        printf("Function '%d' not found\n", id);
        exit(-1);
    }

};

#endif