#ifndef COPL_COMPILER_HPP
#define COPL_COMPILER_HPP
#include "../running/value.hpp"
#include "../running/asm.hpp"
#include "../running/program_loader.hpp"
#include "../running/native_proc.hpp"
#include "ast.hpp"

struct VarInfo {
	std::string name;
	TypeNode* type;
	VarInfo() {}
	VarInfo(std::string name, TypeNode* type) : name(name), type(type) {}
};

struct ObjectInfo {
	std::string name;
	std::vector<VarInfo> info;
	
	int get_offset(std::string vname) {
		for (int i = 0; i < info.size(); ++i)
			if (info[i].name == vname)
				return i;
		throw std::exception();
	}
	
	bool var_is_exist(std::string vname) {
		for (auto i : info)
			if (i.name == vname)
				return true;
		return false;
	}
	
	VarInfo get_var_info(std::string vname) {
		for (auto i : info)
			if (i.name == vname)
				return i;
		throw std::exception();
	}
};

struct CompileOutput {
	std::vector<Frame*> funcs;
	std::vector<VarInfo> globals;
	
	int add_global(VarInfo info) {
		globals.push_back(info);
		return globals.size() - 1;
	}
	
	TypeNode* get_var_type(std::string name) {
		for (auto i : globals)
			if (i.name == name)
				return i.type;
		throw std::exception();
	}
	
	std::unordered_map<std::string, ObjectInfo> object_size_record;
	int fn_cnt = 0;
	
	void regist_class(std::string name, std::vector<VarInfo> members) {
		ObjectInfo inf;
		inf.name = name;
		inf.info = members;
		object_size_record[name] = inf;
	}
	
	ObjectInfo get_class(std::string name) {
		return object_size_record[name];
	}
	
	int find_function_by_name(std::string name) {
		for (auto i : funcs)
			if (i->func_name == name)
				return i->func_id;
		return -1;
	}
	
	Frame* find_function(int id) {
		for (auto i : funcs)
			if (i->func_id == id)
				return i;
		return nullptr;
	}
	
	int get_cnt() { return ++fn_cnt; }
	
	int regist_function(Frame* func) {
		funcs.push_back(func);
		return fn_cnt;
	}
};

struct OperatorCommandUnit {
	std::string label;
	int op;
	OperatorCommandUnit(int op) : op(op) {}
	OperatorCommandUnit(std::string label) : label(label) {}
};

struct OperatorCommand {
	std::string addr;
	std::vector<OperatorCommandUnit> codes;
	OperatorCommand(std::string addr, std::vector<OperatorCommandUnit> codes)
		: addr(addr), codes(codes) {}
};

struct Scope {
	std::unordered_map<std::string, int> var_id;
	std::unordered_map<std::string, TypeNode*> var_type;
	
	VarInfo get_var(std::string name) {
		return {name, var_type[name]};
	}
	
	void add_var(std::string name, int id, TypeNode* type) {
		var_id[name] = id;
		var_type[name] = type;
	}
};

struct Tmp {
	std::vector<Scope> scopes;
	Chunk* current;
	std::vector<OperatorCommand> code_cache;
	int addr_cnt = 0, id = -1, arg_size = 0;
	std::string current_func_name;
	
	Tmp() {
		current = new Chunk();
		scopes.emplace_back();
	}
};

struct CompileResult {
	bool is_lambda_value = false;
	std::string name;
	CompileResult(bool ilv, std::string name) : is_lambda_value(ilv), name(name) {}
};

struct ModuleManager {
	struct Depend {
		Depend() {}
		Depend(std::string name, std::string path) {
			this->name = name;
			this->path = path;
		}
		std::string name, path;
	};
	std::unordered_map<std::string, Depend> modules;
	
	bool exist(std::string name) {
		return modules.find(name) != modules.end();
	}
	
	void add(std::string name, std::string path) {
		modules[name] = Depend(name, path);
	}
	
	std::string get_path(std::string name) {
		return modules[name].path;
	}
};

class Compiler {
public:
	Compiler(CompileOutput* t, std::vector<AST*> codes, ModuleManager* mg) {
		this->t_codes = codes;
		this->target = t;
		this->mg = mg;
		regist_native_proc();
		compile_all();
	}
	
	ModuleManager* mg;
	
	void regist_native_proc() {
		Frame* fprint = new Frame(&print, "print");
		fprint->args_len = 1;
		fprint->func_id = target->get_cnt();
		target->regist_function(fprint);
		
		Frame* fprintln = new Frame(&println, "println");
		fprintln->args_len = 1;
		fprintln->func_id = target->get_cnt();
		target->regist_function(fprintln);
		
		Frame* finput = new Frame(&input, "input");
		finput->args_len = 1;
		finput->func_id = target->get_cnt();
		target->regist_function(finput);
		
		Frame* debug = new Frame(&get_id_info, "debug");
		debug->args_len = 1;
		debug->func_id = target->get_cnt();
		target->regist_function(debug);
		
		Frame* _append = new Frame(&append, "append");
		_append->args_len = 2;
		_append->func_id = target->get_cnt();
		target->regist_function(_append);
		
		Frame* size = new Frame(&length, "size");
		size->args_len = 1;
		size->func_id = target->get_cnt();
		target->regist_function(size);
		
		Frame* str2int_ = new Frame(&str2int, "str2int");
		str2int_->args_len = 1;
		str2int_->func_id = target->get_cnt();
		target->regist_function(str2int_);
		
		Frame* notnul = new Frame(&not_null, "not_null");
		notnul->args_len = 1;
		notnul->func_id = target->get_cnt();
		target->regist_function(notnul);
	}
	
	void compile_all() {
		for (auto i : t_codes) {
			if (i->kind == AST::A_VAR_DEF) {
				VarInfo info;
				info.name = ((VarDefineNode*)i)->name;
				info.type = ((VarDefineNode*)i)->type;
				target->add_global(info);
			} else {
				visit_all(i, "", "");
			}
		}
	}
	
	std::vector<AST*> t_codes;
	CompileOutput* target;
	std::string current_class;

private:
	Tmp code_tmp;
	
	void full_back() {
		std::unordered_map<std::string, int> label_to_addr;
		int addr = 0;
		for (auto& cmd : code_tmp.code_cache) {
			label_to_addr[cmd.addr] = addr;
			addr += cmd.codes.size();
		}
		for (auto& cmd : code_tmp.code_cache) {
			for (auto& unit : cmd.codes) {
				if (unit.label.empty())
					code_tmp.current->op_codes.push_back(unit.op);
				else
					code_tmp.current->op_codes.push_back(label_to_addr[unit.label]);
			}
		}
	}
	
	inline int add_const(STACK_VALUE* value) { return code_tmp.current->add_const(value); }
	inline std::string make_addr() { return "L" + std::to_string(code_tmp.addr_cnt++); }
	void emit(std::string addr, std::vector<OperatorCommandUnit> codes) {
		code_tmp.code_cache.push_back(OperatorCommand(addr, codes));
	}
	
	int emit_function(bool is_lambda) {
		full_back();
		auto* f = new Frame(code_tmp.current);
		f->is_lambda = is_lambda;
		f->func_id = code_tmp.id;
		f->func_name = code_tmp.current_func_name;
		f->args_len = code_tmp.arg_size;
		return target->regist_function(f);
	}
	
	inline void create_scope() { code_tmp.scopes.emplace_back(); }
	inline void leave_scope() { if (!code_tmp.scopes.empty()) code_tmp.scopes.pop_back(); }
	
	VarInfo get_var(std::string name) {
		if (name == "this") {
			return {name, new TypeNode(current_class)};
		}
		for (int i = code_tmp.scopes.size() - 1; i >= 0; --i)
			if (code_tmp.scopes[i].var_type.find(name) != code_tmp.scopes[i].var_type.end())
				return code_tmp.scopes[i].get_var(name);
		return {name, target->get_var_type(name)};
	}
	
	bool var_is_exist(std::string name) {
		if (name == "this" && !current_class.empty()) return true;
		for (int i = code_tmp.scopes.size() - 1; i >= 0; --i)
			if (code_tmp.scopes[i].var_type.find(name) != code_tmp.scopes[i].var_type.end())
				return true;
		for (auto i : target->globals)
			if (i.name == name)
				return true;
		return false;
	}
	
	void add_var(std::string name, int id, TypeNode* type) {
		code_tmp.scopes.back().add_var(name, id, type);
	}
	
	int get_name_id(std::string name) {
		for (int j = code_tmp.scopes.size() - 1; j >= 0; --j)
			for (auto& p : code_tmp.scopes[j].var_id)
				if (p.first == name)
					return p.second;
		return -1;
	}
	
	void load_name(std::string name) {
		emit(make_addr(), {OP_LOAD_NAME, get_name_id(name)});
	}
	
	void set_name(std::string name) {
		emit(make_addr(), {OP_SET_NAME, get_name_id(name)});
	}
	
	TypeNode* make_type(std::string root, TypeNode* chi = nullptr) {
		return new TypeNode(root, chi);
	}
	
	TypeNode* get_expression_type(AST* expr) {
		switch (expr->kind) {
			case AST::A_ID: {
				std::string name = ((IdNode*)expr)->id;
				if (mg->exist(name))
					return new TypeNode(mg->get_path(name), name);
				for (int i = code_tmp.scopes.size() - 1; i >= 0; --i)
					if (code_tmp.scopes[i].var_type.find(name) != code_tmp.scopes[i].var_type.end())
						return code_tmp.scopes[i].var_type[name];
				return get_var(name).type;
			}
			case AST::A_MEMBER_ACCESS: {
				auto ma = (MemberAccessNode*)expr;
				TypeNode* parent_type = get_expression_type(ma->parent);
				return target->get_class(parent_type->root_type).get_var_info(ma->member).type;
			}
			case AST::A_ELEMENT_GET: {
				auto eg = (ElementGetNode*)expr;
				TypeNode* array_type = get_expression_type(eg->array_name);
				if (array_type) {
					if (array_type->root_type == "Array" && array_type->child_type) {
						return array_type->child_type;
					}
					std::string type_str = array_type->root_type;
					if (type_str.size() > 3 && type_str.substr(type_str.size() - 3) == "arr") {
						std::string elem_type = type_str.substr(0, type_str.size() - 3);
						return make_type(elem_type);
					}
				}
				std::cout << "can not derivation type\n";
				exit(-1);
			}
			case AST::A_INT:    return make_type("int");
			case AST::A_FLO:    return make_type("float");
			case AST::A_STRING: return make_type("string");
			case AST::A_TRUE:
			case AST::A_FALSE:  return make_type("bool");
			case AST::A_NULL:   return make_type("null");
			default:
				std::cout << "can not derivation type\n";
				exit(-1);
		}
	}
	
	int get_member_offset(AST* parent, const std::string& member) {
		TypeNode* type = get_expression_type(parent);
		return target->get_class(type->root_type).get_offset(member);
	}
	
	void visit_value(AST* a) {
		switch (a->kind) {
			case AST::A_FLO: {
				double f = std::stod(((FloatNode*)a)->number);
				emit(make_addr(), {OP_LOAD_CONST, add_const(STACK_VALUE::make_double(f))});
				return;
			}
			case AST::A_INT: {
				int i = std::stoi(((IntegerNode*)a)->number);
				emit(make_addr(), {OP_LOAD_CONST, add_const(STACK_VALUE::make_int(i))});
				return ;
			}
			case AST::A_STRING: {
				std::string s = ((StringNode*)a)->str;
				emit(make_addr(), {OP_LOAD_CONST, add_const(STACK_VALUE::make_str(s))});
				return;
			}
			case AST::A_TRUE:  emit(make_addr(), {OP_LOAD_TRUE}); return;
			case AST::A_FALSE: emit(make_addr(), {OP_LOAD_FALSE}); return;
			case AST::A_NULL:  emit(make_addr(), {OP_LOAD_NULL}); return;
			case AST::A_ARRAY: {
				auto elems = ((ArrayNode*)a)->elements;
				emit(make_addr(), {OP_NEW_ARRAY, (int)elems.size()});
				for (size_t i = 0; i < elems.size(); ++i) {
					emit(make_addr(), {OP_DUP});
					emit(make_addr(), {OP_LOAD_IMMEDIATLY, (int)i});
					visit_value(elems[i]);
					emit(make_addr(), {OP_SET_ELEMENT});
				}
				return ;
			}
			case AST::A_ID: {
				visit_member_access(a);
				return;
			}
			case AST::A_MEMBER_ACCESS: {
				TypeNode* t = visit_member_access(a);
				if (t->root_type == "int" || t->root_type == "float" || t->root_type == "bool")
					emit(make_addr(), {OP_COPY});
				return ;
			}
			case AST::A_ELEMENT_GET: {
				visit_element_get_node((ElementGetNode*)a);
				TypeNode* elem_type = get_expression_type(a);
				if (elem_type->root_type == "int" || elem_type->root_type == "float" || elem_type->root_type == "bool")
					emit(make_addr(), {OP_COPY});
				return ;
			}
			case AST::A_CALL:
				visit_call_node((CallNode*)a);
				return ;
			case AST::A_BIN_OP:
				visit_bin_op((BinOpNode*)a);
				return ;
			case AST::A_SELF_INC:
				visit_self_inc_node(a);
				return;
			case AST::A_SELF_DEC:
				visit_self_dec_node(a);
				return ;
			case AST::A_SELF_OPERA:
				visit_self_opera_node(a);
				break;
			case AST::A_MEM_MALLOC:
				visit_mem_malloc((MemoryMallocNode*)a);
				return;
			case AST::A_LAMBDA:
				visit_lambda_node((LambdaNode*)a);
				return;
			case AST::A_NOT:
				visit_not_node(a);
				break;
			default:
				printf("Unsupported value kind: %d\n", a->kind);
				exit(-1);
		}
	}
	
	void visit_mem_malloc(MemoryMallocNode* node) {
		std::string class_name = node->name;
		int size = target->object_size_record[class_name].info.size();
		emit(make_addr(), {OP_NEW_OBJECT, size});
		
		if (node->is_call_c) {
			std::string constructor_name = class_name + "$constructor";
			int func_id = target->find_function_by_name(constructor_name);
			emit(make_addr(), {OP_DUP});
			for (auto arg : node->args) {
				visit_value(arg);
			}
			emit(make_addr(), {OP_CALL, func_id});
		}
	}
	
	bool func_is_exist(std::string name) {
		for (auto i : target->funcs)
			if (i->func_name == name)
				return true;
		return false;
	}
	
	void visit_call_node(CallNode* node) {
		auto func = node->func_name;
		if (func->kind == AST::A_ID) {
			std::string name = ((IdNode*)func)->id;
			if (var_is_exist(name) && get_var(name).type->root_type == "lambda") {
				for (auto arg : node->args) visit_value(arg);
				load_name(name);
				emit(make_addr(), {OP_SPECIAL_CALL});
				return;
			}
			for (auto arg : node->args) visit_value(arg);
			int id = (name != code_tmp.current_func_name) ? target->find_function_by_name(name) : code_tmp.id;
			emit(make_addr(), {OP_CALL, id});
		}
		else if (func->kind == AST::A_MEMBER_ACCESS) {
			auto ma = (MemberAccessNode*)func;
			TypeNode* parent_type = get_expression_type(ma->parent);
			
			if (parent_type->__kind == TypeNode::TK_MODULE) {
				for (auto arg : node->args) visit_value(arg);
				visit_member_access(ma->parent);
				emit(make_addr(), {OP_LOAD_MODULE_METHOD, add_const(STACK_VALUE::make_str(ma->member))});
				emit(make_addr(), {OP_SPECIAL_CALL});
				return;
			}
			else if (!func_is_exist(ma->member) && get_member_class(ma)->root_type == "lambda") {
				for (auto arg : node->args) visit_value(arg);
				visit_member_access(ma);
				emit(make_addr(), {OP_SPECIAL_CALL});
				return;
			}
			else {
				visit_member_access(ma->parent);
				for (auto arg : node->args) visit_value(arg);
				int id = (code_tmp.current_func_name != ma->member) ? target->find_function_by_name(ma->member) : code_tmp.id;
				emit(make_addr(), {OP_CALL, id});
			}
		}
		else if (func->kind == AST::A_ELEMENT_GET) {
			for (auto arg : node->args) visit_value(arg);
			visit_element_get_node((ElementGetNode*)func);
			emit(make_addr(), {OP_SPECIAL_CALL});
		}
		else {
			throw std::exception();
		}
	}
	
	void visit_bit_not_node(AST* node) {
		visit_value(((BitNotNode*)node)->expr);
		emit(make_addr(), {OP_BIT_NOT});
	}
	
	TypeNode* visit_element_get_node(ElementGetNode* node) {
		visit_member_access(node->array_name);
		visit_value(node->position);
		emit(make_addr(), {OP_GET_ELEMENT});
		return get_expression_type(node);
	}
	
	void visit_not_node(AST* node) {
		visit_value(((NotNode*)node)->expr);
		emit(make_addr(), {OP_NOT});
	}
	
	void visit_array_node(AST* node) { visit_value(node); }
	
	void _visit_self_inc(AST* node) {
		auto self = (SelfIncNode*)node;
		AST* tmp_target = self->id;
		int is_pre = self->ipre;
		if (tmp_target->kind == AST::A_ID) {
			load_name(((IdNode*)tmp_target)->id);
			emit(make_addr(), {OP_LOAD_IMMEDIATLY, 1});
			emit(make_addr(), {OP_ADD});
			set_name(((IdNode*)tmp_target)->id);
		}
		else if (tmp_target->kind == AST::A_MEMBER_ACCESS) {
			visit_member_access(((MemberAccessNode*)tmp_target)->parent);
			visit_member_access(tmp_target);
			emit(make_addr(), {OP_LOAD_IMMEDIATLY, 1});
			emit(make_addr(), {OP_ADD});
			emit(make_addr(), {OP_MEMBER_SET, get_member_offset(tmp_target)});
			emit(make_addr(), {OP_POP});
		}
		else if (tmp_target->kind == AST::A_ELEMENT_GET) {
			auto a = (ElementGetNode*) tmp_target;
			auto obj = a->array_name;
			auto pos = a->position;
			visit_member_access(obj);
			visit_value(pos);
			visit_element_get_node(a);
			emit(make_addr(), {OP_LOAD_IMMEDIATLY, 1});
			emit(make_addr(), {OP_ADD});
			emit(make_addr(), {OP_SET_ELEMENT});
		}
		else {
			printf("Unsupported target for self increment\n");
			exit(-1);
		}
	}
	
	void _visit_self_dec(AST* node) {
		auto self = (SelfIncNode*)node;
		AST* tmp_target = self->id;
		int is_pre = self->ipre;
		if (tmp_target->kind == AST::A_ID) {
			load_name(((IdNode*)tmp_target)->id);
			emit(make_addr(), {OP_LOAD_IMMEDIATLY, 1});
			emit(make_addr(), {OP_SUB});
			set_name(((IdNode*)tmp_target)->id);
		}
		else if (tmp_target->kind == AST::A_MEMBER_ACCESS) {
			std::string member_name = ((MemberAccessNode*) tmp_target)->member;
			auto parent = ((MemberAccessNode*) tmp_target)->parent;
			visit_member_access(parent);
			visit_member_access(tmp_target);
			emit(make_addr(), {OP_LOAD_IMMEDIATLY, 1});
			emit(make_addr(), {OP_SUB});
			emit(make_addr(), {OP_MEMBER_SET, get_member_offset(tmp_target)});
		}
		else if (tmp_target->kind == AST::A_ELEMENT_GET) {
			auto a = (ElementGetNode*) tmp_target;
			auto obj = a->array_name;
			auto pos = a->position;
			visit_member_access(obj);
			visit_value(pos);
			visit_element_get_node(a);
			emit(make_addr(), {OP_LOAD_IMMEDIATLY, 1});
			emit(make_addr(), {OP_SUB});
			emit(make_addr(), {OP_SET_ELEMENT});
		}
		else {
			printf("Unsupported target for self increment\n");
			exit(-1);
		}
	}
	
	void visit_self_inc_node(AST* node) {
		auto self = (SelfIncNode*)node;
		AST* tmp_target = self->id;
		int is_pre = self->ipre;
		if (tmp_target->kind == AST::A_ID) {
			if (is_pre) {
				_visit_self_inc(node);
				load_name(((IdNode*)tmp_target)->id);
			} else {
				load_name(((IdNode*)tmp_target)->id);
				_visit_self_inc(node);
			}
		}
		else if (tmp_target->kind == AST::A_MEMBER_ACCESS) {
			if (is_pre) {
				_visit_self_inc(node);
				visit_member_access(tmp_target);
			} else {
				visit_member_access(tmp_target);
				_visit_self_inc(node);
			}
		}
		else if (tmp_target->kind == AST::A_ELEMENT_GET) {
			if (is_pre) {
				_visit_self_inc(node);
				visit_element_get_node((ElementGetNode*)tmp_target);
			} else {
				visit_element_get_node((ElementGetNode*)tmp_target);
				_visit_self_inc(node);
			}
		}
		else {
			printf("Unsupported target for self increment\n");
			exit(-1);
		}
	}
	
	void visit_self_dec_node(AST* node) {
		auto self = (SelfIncNode*)node;
		AST* tmp_target = self->id;
		int is_pre = self->ipre;
		if (tmp_target->kind == AST::A_ID) {
			if (is_pre) {
				_visit_self_dec(node);
				load_name(((IdNode*)tmp_target)->id);
			} else {
				load_name(((IdNode*)tmp_target)->id);
				_visit_self_dec(node);
			}
		}
		else if (tmp_target->kind == AST::A_MEMBER_ACCESS) {
			if (is_pre) {
				_visit_self_dec(node);
				visit_member_access(tmp_target);
			} else {
				visit_member_access(node);
				_visit_self_dec(tmp_target);
			}
		}
		else if (tmp_target->kind == AST::A_ELEMENT_GET) {
			if (is_pre) {
				_visit_self_dec(node);
				visit_element_get_node((ElementGetNode*)tmp_target);
			} else {
				visit_element_get_node((ElementGetNode*)tmp_target);
				_visit_self_dec(node);
			}
		}
		else {
			printf("Unsupported target for self increment\n");
			exit(-1);
		}
	}
	
	void visit_import_node(AST* node) {
		auto inode = (ImportNode*) node;
		std::string path = inode->path, name = inode->re_name;
		mg->add(name, path);
	}
	
	void visit_all(AST* node, std::string begin, std::string end) {
		switch (node->kind) {
			case AST::A_IF:        visit_if_node((IfNode*)node, begin, end); break;
			case AST::A_BLOCK:     visit_block((Block*)node, begin, end); break;
			case AST::A_WHILE:     visit_while_node((WhileNode*)node); break;
			case AST::A_FOR:       visit_for_node((ForNode*)node); break;
			case AST::A_CLASS:     visit_class_node((ObjectNode*)node); break;
			case AST::A_RETURN:    visit_return((ReturnNode*)node); break;
			case AST::A_BREAK:     visit_break(begin, end); break;
			case AST::A_CONTINUE:  visit_continue(begin, end); break;
			case AST::A_BIN_OP:    visit_bin_op((BinOpNode*)node); break;
			case AST::A_BIT_NOT:   visit_bit_not_node(node); break;
			case AST::A_MEMBER_ACCESS:
			case AST::A_ID:        visit_member_access(node); break;
			case AST::A_ELEMENT_GET: visit_element_get_node((ElementGetNode*)node); break;
			case AST::A_CALL:      visit_call_node((CallNode*)node); break;
			case AST::A_NOT:       visit_not_node(node); break;
			case AST::A_ARRAY:     visit_array_node(node); break;
			case AST::A_SELF_INC:  visit_self_inc_node(node); break;
			case AST::A_SELF_DEC:  visit_self_dec_node(node); break;
			case AST::A_VAR_DEF:   visit_var_define((VarDefineNode*)node); break;
			case AST::A_FUNC_DEFINE: visit_func_node((FunctionNode*)node); break;
			case AST::A_SELF_OPERA: visit_self_opera_node(node); break;
			case AST::A_MEM_MALLOC: visit_mem_malloc((MemoryMallocNode*)node); break;
			case AST::A_LAMBDA:    visit_lambda_node((LambdaNode*)node); break;
			case AST::A_NULL:      emit(make_addr(), {OP_LOAD_NULL}); break;
			case AST::A_IMPORT:    visit_import_node(node); break;
			default:
				printf("Unknown AST kind: %d\n", node->kind);
				exit(-1);
		}
	}
	
	void visit_bin_op(BinOpNode* node) {
		visit_value(node->left);
		visit_value(node->right);
		std::string op = node->op;
		if (op == "+")  emit(make_addr(), {OP_ADD});
		else if (op == "-") emit(make_addr(), {OP_SUB});
		else if (op == "*") emit(make_addr(), {OP_MUL});
		else if (op == "/") emit(make_addr(), {OP_DIV});
		else if (op == "%") emit(make_addr(), {OP_MOD});
		else if (op == "<<") emit(make_addr(), {OP_LEFT});
		else if (op == ">>") emit(make_addr(), {OP_RIGHT});
		else if (op == "&") emit(make_addr(), {OP_BIT_AND});
		else if (op == "|") emit(make_addr(), {OP_BIT_OR});
		else if (op == "&&") emit(make_addr(), {OP_AND});
		else if (op == "||") emit(make_addr(), {OP_OR});
		else if (op == ">") emit(make_addr(), {OP_GT});
		else if (op == ">=") emit(make_addr(), {OP_GE});
		else if (op == "<") emit(make_addr(), {OP_LT});
		else if (op == "<=") emit(make_addr(), {OP_LE});
		else if (op == "==") emit(make_addr(), {OP_EQ});
		else if (op == "!=") emit(make_addr(), {OP_NE});
		else {
			std::cout << "unknown operator: " << op << std::endl;
			exit(-1);
		}
	}
	
	inline int get_member_offset(AST* node) {
		return target->get_class(get_member_class(((MemberAccessNode*)node)->parent)->root_type)
			.get_offset(((MemberAccessNode*) node)->member);
	}
	
	TypeNode* get_member_class(AST* node) {
		if (node->kind == AST::A_ELEMENT_GET) {
			return get_expression_type(node);
		}
		if (node->kind == AST::A_MEMBER_ACCESS) {
			auto ma = (MemberAccessNode*)node;
			TypeNode* parent_type = get_member_class(ma->parent);
			return target->get_class(parent_type->root_type).get_var_info(ma->member).type;
		} else if (node->kind == AST::A_ID) {
			return get_var(((IdNode*)node)->id).type;
		}
		throw std::exception();
	}
	
	TypeNode* visit_member_access(AST* node) {
		if (node->kind == AST::A_ELEMENT_GET) {
			visit_element_get_node((ElementGetNode*)node);
			return get_expression_type(node);
		} else if (node->kind == AST::A_MEMBER_ACCESS) {
			auto ma = (MemberAccessNode*)node;
			TypeNode* parent_type = visit_member_access(ma->parent);
			if (parent_type->__kind != TypeNode::TK_MODULE) {
				emit(make_addr(), {OP_MEMBER_GET, target->get_class(parent_type->root_type).get_offset(ma->member)});
				return target->get_class(parent_type->root_type).get_var_info(ma->member).type;
			} else {
				emit(make_addr(), {OP_LOAD_MODULE_METHOD, add_const(STACK_VALUE::make_str(ma->member))});
				TypeNode *tn = new TypeNode("func");
				tn->__kind = TypeNode::TK_FUNCTION;
				return tn;
			}
		} else if (node->kind == AST::A_ID) {
			if (mg->exist(((IdNode*)node)->id)) {
				std::string name = ((IdNode*)node)->id;
				emit(make_addr(), {OP_LOAD_CONST, add_const(STACK_VALUE::make_str(name))});
				return new TypeNode(mg->get_path(name), name);
			}
			load_name(((IdNode*)node)->id);
			return get_var(((IdNode*)node)->id).type;
		}
		throw std::exception();
	}
	
	void visit_if_node(IfNode* node, std::string begin, std::string end) {
		create_scope();
		std::string if_else = make_addr(), if_end = make_addr();
		visit_value(node->condition);
		emit(make_addr(), {OP_JUMP_IF_FALSE, if_else});
		visit_block(node->if_true, begin, end);
		emit(make_addr(), {OP_JUMP, if_end});
		emit(if_else, {OP_NOP});
		if (node->if_false) visit_block(node->if_false, begin, end);
		emit(if_end, {OP_NOP});
		leave_scope();
	}
	
	void visit_while_node(WhileNode* node) {
		create_scope();
		std::string lp_begin = make_addr(), lp_exit = make_addr();
		emit(lp_begin, {OP_NOP});
		visit_value(node->condition);
		emit(make_addr(), {OP_JUMP_IF_FALSE, lp_exit});
		visit_block(node->body, lp_begin, lp_exit);
		emit(make_addr(), {OP_JUMP, lp_begin});
		emit(lp_exit, {OP_NOP});
		leave_scope();
	}
	
	void visit_for_node(ForNode* node) {
		create_scope();
		std::string loop_start = make_addr(), loop_exit = make_addr(), cont = make_addr();
		if (node->init) {
			if (node->init->kind == AST::A_VAR_DEF)
				visit_var_define((VarDefineNode*)node->init);
			else {
				visit_value(node->init);
				emit(make_addr(), {OP_POP});
			}
		}
		emit(loop_start, {OP_NOP});
		if (node->is_continue) {
			visit_value(node->is_continue);
			emit(make_addr(), {OP_JUMP_IF_FALSE, loop_exit});
		}
		visit_block(node->body, cont, loop_exit);
		emit(cont, {OP_NOP});
		if (node->change) {
			visit_value(node->change);
			emit(make_addr(), {OP_POP});
		}
		emit(make_addr(), {OP_JUMP, loop_start});
		emit(loop_exit, {OP_NOP});
		leave_scope();
	}
	
	void visit_self_opera_node(AST* node) {
		auto sp = (SelfOperator*)node;
		auto id = sp->target;
		auto val = sp->value;
		auto op = sp->op;
		int arith_op = -1;
		if (op == "+=")      arith_op = OP_ADD;
		else if (op == "-=") arith_op = OP_SUB;
		else if (op == "*=") arith_op = OP_MUL;
		else if (op == "/=") arith_op = OP_DIV;
		else if (op == "%=") arith_op = OP_MOD;
		else if (op == "<<=") arith_op = OP_LEFT;
		else if (op == ">>=") arith_op = OP_RIGHT;
		else if (op == "|=") arith_op = OP_BIT_OR;
		else if (op == "&=") arith_op = OP_BIT_AND;
		else if (op == "=")  arith_op = -1;
		else {
			std::cout << "unknown self operator: " << op << std::endl;
			exit(-1);
		}
		if (arith_op == -1)  {
			if (id->kind == AST::A_ID) {
				visit_value(val);
				set_name(((IdNode*) id)->id);
				return;
			} else if (id->kind == AST::A_MEMBER_ACCESS) {
				visit_value(((MemberAccessNode*) id)->parent);
				visit_value(val);
				emit(make_addr(), {OP_MEMBER_SET, get_member_offset(id)});
				return;
			} else if (id->kind == AST::A_ELEMENT_GET) {
				auto a = (ElementGetNode*) id;
				visit_member_access(a->array_name);
				visit_value(a->position);
				visit_value(sp->value);
				emit(make_addr(), {OP_SET_ELEMENT});
				return;
			}
		} else {
			if (id->kind == AST::A_ID) {
				load_name(((IdNode*) id)->id);
				visit_value(val);
				emit(make_addr(), {arith_op});
				set_name(((IdNode*) id)->id);
				return;
			} else if (id->kind == AST::A_MEMBER_ACCESS) {
				visit_value(((MemberAccessNode*) id)->parent);
				visit_member_access(id);
				visit_value(val);
				emit(make_addr(), {arith_op});
				emit(make_addr(), {OP_MEMBER_SET, get_member_offset(id)});
				return;
			} else if (id->kind == AST::A_ELEMENT_GET) {
				auto a = (ElementGetNode*) id;
				visit_member_access(a->array_name);
				visit_value(a->position);
				visit_element_get_node(a);
				visit_value(sp->value);
				emit(make_addr(), {arith_op});
				emit(make_addr(), {OP_SET_ELEMENT});
				return;
			}
		}
	}
	
	void visit_func_node(FunctionNode* node) {
		code_tmp.code_cache.clear();
		create_scope();
		code_tmp.current = new Chunk;
		code_tmp.id = target->get_cnt();
		code_tmp.current_func_name = node->name;
		bool is_constructor = (node->name.find("$constructor") != std::string::npos);
		bool is_method = (!current_class.empty() && !is_constructor);
		
		if (is_constructor || is_method) {
			int this_id = code_tmp.current->add_name("this");
			add_var("this", this_id, make_type(current_class));
			emit(make_addr(), {OP_SET_NAME, this_id});
		}
		
		for (auto& arg : node->args) {
			auto vd = (VarDefineNode*)arg;
			int id = code_tmp.current->add_name(vd->name);
			add_var(vd->name, id, vd->type);
			emit(make_addr(), {OP_SET_NAME, id});
		}
		
		if (code_tmp.current_func_name == "main") {
			for (auto i : mg->modules) {
				emit(
					make_addr(), {OP_LOAD_MODULE,
					              add_const(STACK_VALUE::make_str(i.second.path)),
					              add_const(STACK_VALUE::make_str(i.first))}
				);
			}
		}
		
		code_tmp.arg_size = node->args.size() + (is_constructor || is_method ? 1 : 0);
		visit_block(node->body, "", "");
		emit(make_addr(), {OP_LEAVE});
		emit_function(false);
		leave_scope();
	}
	
	int lambda_count = 0;
	
	inline std::string make_l_name() {
		return "lambda_" + std::to_string(lambda_count++);
	}
	
	void visit_lambda_node(LambdaNode* node) {
		auto lb_node = (LambdaNode*) node;
		auto args_list = lb_node->args;
		auto body = lb_node->body;
		Tmp tmp_codet = code_tmp;
		create_scope();
		code_tmp = Tmp();
		code_tmp.id = target->get_cnt();
		std::string name = make_l_name();
		code_tmp.current_func_name = name;
		code_tmp.arg_size = node->args.size();
		for (auto& arg : node->args) {
			auto vd = (VarDefineNode*)arg;
			int id = code_tmp.current->add_name(vd->name);
			add_var(vd->name, id, vd->type);
			emit(make_addr(), {OP_SET_NAME, id});
		}
		visit_block(body, "", "");
		emit(make_addr(), { OP_LEAVE });
		int id = emit_function(true);
		leave_scope();
		code_tmp = tmp_codet;
		emit(make_addr(), {OP_LOAD_FUNC_ADDR, id});
	}
	
	void visit_var_define(VarDefineNode* node) {
		int id = code_tmp.current->add_name(node->name);
		add_var(node->name, id, node->type);
		if (node->init_value) {
			visit_value(node->init_value);
			emit(make_addr(), {OP_SET_NAME, id});
		}
	}
	
	void add_object(std::string name, std::vector<VarInfo> infos) {
		target->regist_class(name, infos);
	}
	
	void visit_class_node(ObjectNode* node) {
		std::vector<VarInfo> infos;
		current_class = node->name;
		for (auto& p : node->members) {
			if (p.second->kind == AST::A_VAR_DEF) {
				auto vd = (VarDefineNode*)p.second;
				infos.push_back({vd->name, vd->type});
			}
		}
		add_object(node->name, infos);
		for (auto& p : node->members) {
			if (p.second->kind == AST::A_FUNC_DEFINE) {
				auto fn = (FunctionNode*)p.second;
				if (fn->name == "constructor")
					fn->name = node->name + "$constructor";
				visit_func_node(fn);
			}
		}
		current_class = "";
	}
	
	void visit_continue(std::string begin, std::string) {
		emit(make_addr(), {OP_JUMP, begin});
	}
	
	void visit_break(std::string, std::string end) {
		emit(make_addr(), {OP_JUMP, end});
	}
	
	void visit_return(ReturnNode* node) {
		if (node->value) {
			visit_value(node->value);
			emit(make_addr(), {OP_RETURN});
		} else {
			emit(make_addr(), {OP_LEAVE});
		}
	}
	
	void visit_block(Block* node, std::string begin, std::string end) {
		for (auto stmt : node->codes)
			visit_all(stmt, begin, end);
	}
};

#endif