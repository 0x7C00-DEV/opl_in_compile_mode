#ifndef COPL_VM_HPP
#define COPL_VM_HPP

#include "program_loader.hpp"
#include "value.hpp"
#include "../pathproc.hpp"
#include <cmath>
#include "libary.hpp"
#include "native_proc.hpp"
#include "asm.hpp"
#include <unordered_map>

struct Module {
	std::vector<OPL_VALUE::Frame*> funcs;
	std::string name;
	HMODULE __module___;
	bool is_dyn_lib;
	std::unordered_map<std::string, OPL_VALUE::STACK_VALUE*> globals;
	
	Module(std::vector<OPL_VALUE::Frame*> funcs, std::string name) {
		this->name = name;
		this->funcs = funcs;
		this->is_dyn_lib = false;
	}
	
	Module(HMODULE mo, std::string name) {
		this->name = name;
		this->__module___ = mo;
		this->is_dyn_lib = true;
	}
	
	~Module() {
		if (!is_dyn_lib) {
			for (auto f : funcs) delete f;
		}
		for (auto& p : globals) delete p.second;
	}
	
	bool is_exist(std::string fname) {
		for (auto i : funcs)
			if (i->func_name == fname)
				return true;
		return false;
	}
	
	OPL_VALUE::Frame* load_func(std::string fname) {
		if (is_dyn_lib) {
			auto tmp = (OPL_VALUE::BUILD_IN_PROC*) GetProcAddress(__module___, fname.c_str());
			return new OPL_VALUE::Frame(tmp, fname);
		}
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
		heap_head = new OPL_VALUE::OPL_BasicValue(OPL_VALUE::BV_NULL);
		calls.push_back(find_function_by_name("main", "at file init"));
		calls.push_back(find_function_by_name("#init__", "at file init"));
		global_stack.push_back(&globals);
		execute();
	}
	
	VM(OPL_VALUE::Frame* f, VM* vm, bool is_debug) {
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
		calls.push_back(find_function_by_name("#init__", "at module init"));
		global_stack.push_back(&globals);
		execute();
	}
	
	VM(std::vector<OPL_VALUE::Frame*> frames, bool is_debug = false, bool run_init_only = false) : is_debug(is_debug) {
		this->frames = frames;
		heap_head = new OPL_VALUE::OPL_BasicValue(OPL_VALUE::BV_NULL);
		if (!run_init_only) {
			calls.push_back(find_function_by_name("main", "at program init"));
		}
		calls.push_back(find_function_by_name("#init__", "at program init"));
		global_stack.push_back(&globals);
		execute();
	}
	
	~VM() {
		for (auto f : frames) delete f;
		for (auto m : modules) delete m;
		OPL_VALUE::OPL_BasicValue* cur = heap_head;
		while (cur) {
			OPL_VALUE::OPL_BasicValue* next = cur->next;
			delete cur;
			cur = next;
		}
		for (auto& p : globals) delete p.second;
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
					get_current()->push(OPL_VALUE::STACK_VALUE::make_null());
					debug();
					break;
				}
				
				case OP_LOAD_TRUE: {
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(true));
					debug();
					break;
				}
				
				case OP_LOAD_FALSE: {
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(false));
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
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("ErrorTypeIs: %d\n", v->obj->kind);
								printf("RuntimeError: cannot add non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					bool is_double = (left->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE || right->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE) ||
					                 (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_FLOAT) ||
					                 (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_FLOAT);
					if (is_double)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(l + r));
					else
						get_current()->push(OPL_VALUE::STACK_VALUE::make_int((int32_t)(l + r)));
					debug();
					break;
				}
				
				case OP_SUB: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [  ](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot subtract non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					bool is_double = (left->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE || right->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE) ||
					                 (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_FLOAT) ||
					                 (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_FLOAT);
					if (is_double)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(l - r));
					else
						get_current()->push(OPL_VALUE::STACK_VALUE::make_int((int32_t)(l - r)));
					debug();
					break;
				}
				
				case OP_MUL: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot multiply non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					bool is_double = (left->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE || right->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE) ||
					                 (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_FLOAT) ||
					                 (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_FLOAT);
					if (is_double)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(l * r));
					else
						get_current()->push(OPL_VALUE::STACK_VALUE::make_int((int32_t)(l * r)));
					debug();
					break;
				}
				
				case OP_DIV: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot divide non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					bool is_double = (left->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE || right->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE) ||
					                 (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_FLOAT) ||
					                 (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_FLOAT);
					if (is_double)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(l / r));
					else
						get_current()->push(OPL_VALUE::STACK_VALUE::make_int((int32_t)(l / r)));
					debug();
					break;
				}
				
				case OP_MOD: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot modulo non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					bool is_double = (left->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE || right->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE) ||
					                 (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_FLOAT) ||
					                 (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_FLOAT);
					if (is_double)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(fmod(l, r)));
					else
						get_current()->push(OPL_VALUE::STACK_VALUE::make_int((int32_t)l % (int32_t)r));
					debug();
					break;
				}
				
				case OP_LEFT: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_int = [](OPL_VALUE::STACK_VALUE* v) -> int {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else {
								printf("RuntimeError: left shift requires integer operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_INT) {
								printf("RuntimeError: left shift requires integer operand\n");
								exit(-1);
							}
							return v->i_val;
						}
					};
					int l = to_int(left);
					int r = to_int(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_int(l << r));
					debug();
					break;
				}
				
				case OP_RIGHT: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_int = [](OPL_VALUE::STACK_VALUE* v) -> int {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else {
								printf("RuntimeError: right shift requires integer operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_INT) {
								printf("RuntimeError: right shift requires integer operand\n");
								exit(-1);
							}
							return v->i_val;
						}
					};
					int l = to_int(left);
					int r = to_int(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_int(l >> r));
					debug();
					break;
				}
				
				case OP_NEG: {
					OPL_VALUE::STACK_VALUE *a = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot negate non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double val = to_number(a);
					if (a->is_heap_ref && a->obj->kind == OPL_VALUE::BV_FLOAT)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(-val));
					else if (!a->is_heap_ref && a->kind == OPL_VALUE::STACK_VALUE::S_DOUBLE)
						get_current()->push(OPL_VALUE::STACK_VALUE::make_double(-val));
					else
						get_current()->push(OPL_VALUE::STACK_VALUE::make_int((int32_t)-val));
					debug();
					break;
				}
				
				case OP_EQ: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					std::string ltmp, rtmp;
					if ((!right->is_heap_ref && right->kind == OPL_VALUE::STACK_VALUE::S_STR) || (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_STRING))
						rtmp = ((OPL_VALUE::OPL_String*)val_conv(right))->str;
					if ((!left->is_heap_ref && left->kind == OPL_VALUE::STACK_VALUE::S_STR) || (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_STRING))
						ltmp = ((OPL_VALUE::OPL_String*)val_conv(left))->str;
					if (!ltmp.empty() || !rtmp.empty()) {
						get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(ltmp == rtmp));
						break;
					}
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot compare non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l == r));
					debug();
					break;
				}
				
				case OP_NE: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					std::string ltmp, rtmp;
					if ((!right->is_heap_ref && right->kind == OPL_VALUE::STACK_VALUE::S_STR) || (right->is_heap_ref && right->obj->kind == OPL_VALUE::BV_STRING))
						rtmp = ((OPL_VALUE::OPL_String*)val_conv(right))->str;
					if ((!left->is_heap_ref && left->kind == OPL_VALUE::STACK_VALUE::S_STR) || (left->is_heap_ref && left->obj->kind == OPL_VALUE::BV_STRING))
						ltmp = ((OPL_VALUE::OPL_String*)val_conv(left))->str;
					if (!ltmp.empty() || !rtmp.empty()) {
						get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(ltmp != rtmp));
						break;
					}
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot compare non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l != r));
					debug();
					break;
				}
				
				case OP_LT: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot compare non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l < r));
					debug();
					break;
				}
				
				case OP_LE: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot compare non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l <= r));
					debug();
					break;
				}
				
				case OP_GT: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot compare non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l > r));
					debug();
					break;
				}
				
				case OP_GE: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_number = [](OPL_VALUE::STACK_VALUE* v) -> double {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else if (v->obj->kind == OPL_VALUE::BV_FLOAT)
								return ((OPL_VALUE::OPL_Float*)v->obj)->f;
							else {
								printf("RuntimeError: cannot compare non-numeric heap type\n");
								exit(-1);
							}
						} else {
							return (v->kind == OPL_VALUE::STACK_VALUE::S_INT) ? v->i_val : v->d_val;
						}
					};
					double l = to_number(left);
					double r = to_number(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l >= r));
					debug();
					break;
				}
				
				case OP_BIT_AND: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_int = [](OPL_VALUE::STACK_VALUE* v) -> int {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else {
								printf("RuntimeError: bitwise AND requires integer operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_INT) {
								printf("RuntimeError: bitwise AND requires integer operand\n");
								exit(-1);
							}
							return v->i_val;
						}
					};
					int l = to_int(left);
					int r = to_int(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_int(l & r));
					debug();
					break;
				}
				
				case OP_BIT_OR: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_int = [](OPL_VALUE::STACK_VALUE* v) -> int {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else {
								printf("RuntimeError: bitwise OR requires integer operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_INT) {
								printf("RuntimeError: bitwise OR requires integer operand\n");
								exit(-1);
							}
							return v->i_val;
						}
					};
					int l = to_int(left);
					int r = to_int(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_int(l | r));
					debug();
					break;
				}
				
				case OP_BIT_NOT: {
					OPL_VALUE::STACK_VALUE *a = get_current()->pop();
					auto to_int = [](OPL_VALUE::STACK_VALUE* v) -> int {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_INT)
								return ((OPL_VALUE::OPL_Integer*)v->obj)->i;
							else {
								printf("RuntimeError: bitwise NOT requires integer operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_INT) {
								printf("RuntimeError: bitwise NOT requires integer operand\n");
								exit(-1);
							}
							return v->i_val;
						}
					};
					int val = to_int(a);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_int(~val));
					debug();
					break;
				}
				
				case OP_NOT: {
					auto t = get_current()->pop();
					bool b;
					if (t->is_heap_ref) {
						if (t->obj->kind == OPL_VALUE::BV_BOOL)
							b = ((OPL_VALUE::OPL_Bool*)t->obj)->b;
						else {
							printf("RuntimeError: logical NOT requires boolean operand\n");
							exit(-1);
						}
					} else {
						if (t->kind != OPL_VALUE::STACK_VALUE::S_BOOL) {
							printf("RuntimeError: logical NOT requires boolean operand\n");
							exit(-1);
						}
						b = t->b_val;
					}
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(!b));
					debug();
					break;
				}
				
				case OP_AND: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_bool = [](OPL_VALUE::STACK_VALUE* v) -> bool {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_BOOL)
								return ((OPL_VALUE::OPL_Bool*)v->obj)->b;
							else {
								printf("RuntimeError: logical AND requires boolean operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_BOOL) {
								printf("RuntimeError: logical AND requires boolean operand\n");
								exit(-1);
							}
							return v->b_val;
						}
					};
					bool l = to_bool(left);
					bool r = to_bool(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l && r));
					debug();
					break;
				}
				
				case OP_OR: {
					OPL_VALUE::STACK_VALUE *right = get_current()->pop();
					OPL_VALUE::STACK_VALUE *left = get_current()->pop();
					auto to_bool = [](OPL_VALUE::STACK_VALUE* v) -> bool {
						if (v->is_heap_ref) {
							if (v->obj->kind == OPL_VALUE::BV_BOOL)
								return ((OPL_VALUE::OPL_Bool*)v->obj)->b;
							else {
								printf("RuntimeError: logical OR requires boolean operand\n");
								exit(-1);
							}
						} else {
							if (v->kind != OPL_VALUE::STACK_VALUE::S_BOOL) {
								printf("RuntimeError: logical OR requires boolean operand\n");
								exit(-1);
							}
							return v->b_val;
						}
					};
					bool l = to_bool(left);
					bool r = to_bool(right);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_bool(l || r));
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
						if (cond->obj->kind == OPL_VALUE::BV_BOOL)
							b = ((OPL_VALUE::OPL_Bool*)cond->obj)->b;
						else {
							printf("RuntimeError: condition must be boolean\n");
							exit(-1);
						}
					} else {
						if (cond->kind != OPL_VALUE::STACK_VALUE::S_BOOL) {
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
						if (cond->obj->kind == OPL_VALUE::BV_BOOL)
							b = ((OPL_VALUE::OPL_Bool*)cond->obj)->b;
						else {
							printf("RuntimeError: condition must be boolean\n");
							exit(-1);
						}
					} else {
						if (cond->kind != OPL_VALUE::STACK_VALUE::S_BOOL) {
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
					auto it = (*global_stack.back()).find(name);
					if (it != (*global_stack.back()).end())
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
					OPL_VALUE::STACK_VALUE* retval = get_current()->pop();
					OPL_VALUE::Frame* caller = get_current()->caller;
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
						OPL_VALUE::OPL_BasicValue* copy = val->obj->__copy__();
						get_tail()->next = copy;
						copy->next = nullptr;
						get_current()->push(OPL_VALUE::STACK_VALUE::make_heap(copy));
					} else {
						get_current()->push(val);
					}
					debug();
					break;
				}
				
				case OP_LOAD_IMMEDIATLY: {
					get_current()->push(OPL_VALUE::STACK_VALUE::make_int(GET));
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
				
				case OP_LOAD_MODULE_METHOD: {
					std::string mod_name = ((OPL_VALUE::OPL_String*)val_conv(get_current()->pop()))->str;
					current_module = mod_name;
					std::string method_name = ((OPL_VALUE::OPL_String*)val_conv(get_current()->load_const(GET)))->str;
					Module* mod = find_module(mod_name);
					OPL_VALUE::Frame* method = mod->load_func(method_name);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_func((void*)method));
					pending_module = mod;
					break;
				}
				
				case OP_LOAD_MODULE: {
					std::string path = ((OPL_VALUE::OPL_String*)val_conv(get_current()->load_const(GET)))->str;
					std::string name = ((OPL_VALUE::OPL_String*)val_conv(get_current()->load_const(GET)))->str;
					Module* mod = nullptr;
					if (!end_with(path, "dll")) {
						auto frames = load_bytecode(path, builtins);
						mod = new Module(frames, name);
						VM temp_vm(mod->funcs, is_debug, true);
						mod->globals = temp_vm.globals;
						for (auto dep_mod : temp_vm.modules) {
							bool exists = false;
							for (auto existing : modules) {
								if (existing->name == dep_mod->name) {
									exists = true;
									break;
								}
							}
							if (!exists) {
								modules.push_back(dep_mod);
							}
						}
						temp_vm.modules.clear();
					} else {
						mod = new Module(load_native_libary(path), name);
					}
					modules.push_back(mod);
					break;
				}
				
				case OP_LOAD_FUNC_ADDR: {
					auto id = GET;
					auto lf = find_function_by_id(id)->clone();
					get_current()->push(OPL_VALUE::STACK_VALUE::make_func((void*)lf));
					break;
				}
				
				case OP_SPECIAL_CALL: {
					auto _v = get_current()->pop();
					OPL_VALUE::Frame* callee = nullptr;
					if (_v->is_heap_ref) callee = (OPL_VALUE::Frame*)(((OPL_VALUE::OPL_Point*)_v->obj)->pointer);
					else callee = (OPL_VALUE::Frame*)_v->raw_ptr;
					OPL_VALUE::Frame* caller = get_current();
					callee->caller = caller;
					if (!callee->is_build_in) {
						callee->pc = callee->get_start();
					}
					for (int g = 0; g < callee->args_len; ++g)
						callee->push(caller->pop());
					if (pending_module != nullptr) {
						global_stack.push_back(&pending_module->globals);
						pending_module = nullptr;
					}
					calls.push_back(callee);
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
					if (v->is_heap_ref && v->obj->kind == OPL_VALUE::BV_STRING) {
						std::cout << ((OPL_VALUE::OPL_String*)v->obj)->str;
					} else if (!v->is_heap_ref && v->kind == OPL_VALUE::STACK_VALUE::S_INT) {
						std::cout << v->i_val;
					} else if (v->is_heap_ref && v->obj->kind == OPL_VALUE::BV_INT) {
						std::cout << ((OPL_VALUE::OPL_Integer*)v->obj)->i;
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
					expect_heap_val(obj, OPL_VALUE::BV_OBJ);
					int index = GET;
					auto member = ((OPL_VALUE::OPL_Object*)obj->obj)->__memberget__(index);
					get_current()->push(OPL_VALUE::STACK_VALUE::make_heap(member));
					debug();
					break;
				}
				
				case OP_MEMBER_SET: {
					auto val = get_current()->pop();
					auto obj = get_current()->pop();
					expect_heap_val(obj, OPL_VALUE::BV_OBJ);
					int index = GET;
					((OPL_VALUE::OPL_Object*)obj->obj)->__memberset__(index, val_conv(val));
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
	std::vector<OPL_VALUE::Frame*> frames;
	std::vector<OPL_VALUE::Frame*> calls;
	std::vector<Module*> modules;
	OPL_VALUE::OPL_BasicValue* heap_head;
	std::unordered_map<std::string, OPL_VALUE::STACK_VALUE*> globals;
	std::vector<std::unordered_map<std::string, OPL_VALUE::STACK_VALUE*>*> global_stack;
	friend struct Frame;
	std::string current_module;
	Module* pending_module = nullptr;
	
	OPL_VALUE::Frame* find_method_proc(std::string mod_name, std::string method_name) {
		for (auto _i : modules) {
			if (_i->name == mod_name) {
				if (_i->is_dyn_lib) {
					return _i->load_func(method_name);
				} else if (_i->is_exist(method_name)) {
					return _i->load_func(method_name);
				}
			}
		}
		std::cout << "Module '" << mod_name << "' not found or function missing\n";
		exit(-1);
	}
	
	Module* find_module(std::string mod_name) {
		for (auto m : modules)
			if (m->name == mod_name)
				return m;
		std::cout << "Module '" << mod_name << "' not found\n";
		exit(-1);
	}
	
	OPL_VALUE::OPL_BasicValue* val_conv(OPL_VALUE::STACK_VALUE* value) {
		if (value->is_heap_ref) return value->obj;
		OPL_VALUE::OPL_BasicValue* new_obj = nullptr;
		switch (value->kind) {
			case OPL_VALUE::STACK_VALUE::S_INT:
				new_obj = new OPL_VALUE::OPL_Integer(value->i_val);
				break;
			case OPL_VALUE::STACK_VALUE::S_BOOL:
				new_obj = new OPL_VALUE::OPL_Bool(value->b_val);
				break;
			case OPL_VALUE::STACK_VALUE::S_DOUBLE:
				new_obj = new OPL_VALUE::OPL_Float(value->d_val);
				break;
			case OPL_VALUE::STACK_VALUE::S_RAW:
				new_obj = new OPL_VALUE::OPL_Point(value->raw_ptr);
				break;
			case OPL_VALUE::STACK_VALUE::S_NULL:
				new_obj = new OPL_VALUE::OPL_Null;
				break;
			case OPL_VALUE::STACK_VALUE::S_STR:
				new_obj = new OPL_VALUE::OPL_String(value->str_value);
				break;
			case OPL_VALUE::STACK_VALUE::S_FUC:
				new_obj = new OPL_VALUE::OPL_Point(value->raw_ptr);
				break;
			default:
				return nullptr;
		}
		get_tail()->next = new_obj;
		new_obj->next = nullptr;
		return new_obj;
	}
	
	OPL_VALUE::Frame* get_current() { if (calls.empty()) { return nullptr; } return calls.back(); }
	
	void create_task_by_id(int id) {
		OPL_VALUE::Frame* callee = find_function_by_id(id)->clone();
		OPL_VALUE::Frame* caller = get_current();
		callee->caller = caller;
		if (!callee->is_build_in) callee->pc = callee->get_start();
		for (int w = 0; w < callee->args_len; ++w)
			callee->push(caller->pop());
		calls.push_back(callee);
	}
	
	void create_task_by_name(std::string id) {
		OPL_VALUE::Frame* callee = find_function_by_name(id, " at sub proc caller, create task ny name")->clone();
		OPL_VALUE::Frame* caller = get_current();
		callee->caller = caller;
		if (!callee->is_build_in) callee->pc = callee->get_start();
		for (int w = 0; w < callee->args_len; ++w)
			callee->push(caller->pop());
		calls.push_back(callee);
	}
	
	void expect_val(OPL_VALUE::STACK_VALUE* value, OPL_VALUE::STACK_VALUE::ValueType valueType) {
		if (value->kind != valueType) {
			printf("RuntimeError: expected type %d, got %d\n", valueType, value->kind);
			exit(-1);
		}
	}
	
	void expect_heap_val(OPL_VALUE::STACK_VALUE* value, OPL_VALUE::BV_Kind valueType) {
		if (!value->is_heap_ref) {
			printf("RuntimeError: expected heap reference\n");
			exit(-1);
		}
		if (value->obj->kind != valueType) {
			printf("RuntimeError: expected heap type %d, got %d\n", valueType, value->obj->kind);
			exit(-1);
		}
	}
	
	void set_global(std::string name, OPL_VALUE::STACK_VALUE* value) {
		(*global_stack.back())[name] = value;
	}
	
	std::string load_string(OPL_VALUE::STACK_VALUE* value) {
		return ((OPL_VALUE::OPL_String*)value->obj)->str;
	}
	
	OPL_VALUE::OPL_BasicValue* get_tail() {
		OPL_VALUE::OPL_BasicValue* head_ = heap_head;
		while (head_->next) head_ = head_->next;
		return head_;
	}
	
	void element_set(OPL_VALUE::STACK_VALUE* object, OPL_VALUE::STACK_VALUE* pos, OPL_VALUE::STACK_VALUE* value) {
		if (object->is_heap_ref) {
			auto arr_obj = object->obj;
			int position = ((OPL_VALUE::OPL_Integer*)val_conv(pos))->i;
			auto op = new OPL_VALUE::OPL_Integer(position);
			auto val_obj = val_conv(value);
			arr_obj->__elementset__(op, val_obj);
			delete op;
		} else if (object->kind == OPL_VALUE::STACK_VALUE::S_STR) {
			object->str_value[pos->i_val] = ((OPL_VALUE::OPL_String*)val_conv(value))->str[0];
		}
	}
	
	OPL_VALUE::STACK_VALUE* element_get(OPL_VALUE::STACK_VALUE* object, OPL_VALUE::STACK_VALUE* pos) {
		if (!object->is_heap_ref) {
			char tmp = object->str_value[get_int(pos)];
			std::string tm;
			tm += tmp;
			OPL_VALUE::STACK_VALUE* v = new OPL_VALUE::STACK_VALUE;
			v->is_heap_ref = false;
			v->str_value = tm;
			v->kind = OPL_VALUE::STACK_VALUE::S_STR;
			return v;
		}
		auto obj = object->obj;
		auto pos_obj = (pos->is_heap_ref)? pos->obj : new OPL_VALUE::OPL_Integer(pos->i_val);
		auto result = obj->__elementget__(pos_obj);
		delete pos_obj;
		return OPL_VALUE::STACK_VALUE::make_heap(result);
	}
	
	OPL_VALUE::STACK_VALUE* new_array(int size) {
		OPL_VALUE::OPL_Array* new_array = new OPL_VALUE::OPL_Array(size);
		get_tail()->next = new_array;
		new_array->next = nullptr;
		OPL_VALUE::STACK_VALUE* value = new OPL_VALUE::STACK_VALUE;
		value->is_heap_ref = true;
		value->obj = new_array;
		return value;
	}
	
	OPL_VALUE::STACK_VALUE* new_object(int size) {
		OPL_VALUE::OPL_Object* obj = new OPL_VALUE::OPL_Object(size);
		get_tail()->next = obj;
		obj->next = nullptr;
		OPL_VALUE::STACK_VALUE* value = new OPL_VALUE::STACK_VALUE;
		value->is_heap_ref = true;
		value->obj = obj;
		return value;
	}
	
	OPL_VALUE::Frame* find_function_by_name(std::string name, std::string info) {
		for (auto ti : frames) {
			if (ti->func_name == name)
				return ti;
		}
		printf("Function '%s' not found, %s\n", name.c_str(), info.c_str());
		exit(-1);
	}
	
	OPL_VALUE::Frame* find_function_by_id(int id) {
		for (auto ti : frames)
			if (ti->func_id == id)
				return ti;
		printf("Function '%d' not found\n", id);
		exit(-1);
	}
	
};

#endif