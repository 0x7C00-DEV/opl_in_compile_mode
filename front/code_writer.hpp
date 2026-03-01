#ifndef COPL_CODE_WRITER_HPP
#define COPL_CODE_WRITER_HPP

#include "compiler.hpp"
#include "..\resfile_types.hpp"
#include <cstdint>
#include <string>

std::string get_file_name(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return name;
    return name.substr(0, dot);
}

void write_value(FILE* code_file, STACK_VALUE* value) {
    if (!value) {
        uint8_t type = BNULL;
        fwrite(&type, 1, 1, code_file);
        return;
    }

    if (value->is_heap_ref) {
        OPL_BasicValue* obj = value->obj;
        if (!obj) {
            uint8_t type = BNULL;
            fwrite(&type, 1, 1, code_file);
            return;
        }

        switch (obj->kind) {
            case BV_INT: {
                uint8_t type = BINT;
                fwrite(&type, 1, 1, code_file);
                int32_t val = ((OPL_Integer*)obj)->i;
                fwrite(&val, sizeof(val), 1, code_file);
                break;
            }
            case BV_FLOAT: {
                uint8_t type = BFLOAT;
                fwrite(&type, 1, 1, code_file);
                double val = ((OPL_Float*)obj)->f;
                fwrite(&val, sizeof(val), 1, code_file);
                break;
            }
            case BV_STRING: {
                uint8_t type = BSTRING;
                fwrite(&type, 1, 1, code_file);
                std::string& str = ((OPL_String*)obj)->str;
                uint32_t len = str.size();
                fwrite(&len, sizeof(len), 1, code_file);
                fwrite(str.c_str(), 1, len, code_file);
                break;
            }
            case BV_BOOL: {
                uint8_t type = BBOOL;
                fwrite(&type, 1, 1, code_file);
                uint8_t b = ((OPL_Bool*)obj)->b ? 1 : 0;
                fwrite(&b, 1, 1, code_file);
                break;
            }
            case BV_NULL: {
                uint8_t type = BNULL;
                fwrite(&type, 1, 1, code_file);
                break;
            }
            case BV_ARRAY: case BV_OBJ:
            case BV_RAW_POINT: {
                uint8_t type = BNULL;
                fwrite(&type, 1, 1, code_file);
            }
            break;
        }
    } else {
        switch (value->kind) {
            case STACK_VALUE::S_INT: {
                uint8_t type = BINT;
                fwrite(&type, 1, 1, code_file);
                int32_t val = value->i_val;
                fwrite(&val, sizeof(val), 1, code_file);
                break;
            }
            case STACK_VALUE::S_DOUBLE: {
                uint8_t type = BFLOAT;
                fwrite(&type, 1, 1, code_file);
                double val = value->d_val;
                fwrite(&val, sizeof(val), 1, code_file);
                break;
            }
            case STACK_VALUE::S_STR: {
                uint8_t type = BSTRING;
                fwrite(&type, 1, 1, code_file);
                uint32_t len = value->str_value.size();
                fwrite(&len, sizeof(len), 1, code_file);
                fwrite(value->str_value.c_str(), 1, len, code_file);
                break;
            }
            case STACK_VALUE::S_BOOL: {
                uint8_t type = BBOOL;
                fwrite(&type, 1, 1, code_file);
                uint8_t b = value->b_val ? 1 : 0;
                fwrite(&b, 1, 1, code_file);
                break;
            }
            case STACK_VALUE::S_NULL: {
                uint8_t type = BNULL;
                fwrite(&type, 1, 1, code_file);
                break;
            }
            case STACK_VALUE::S_RAW:
            case STACK_VALUE::S_FUC:{
                uint8_t type = BNULL;
                fwrite(&type, 1, 1, code_file);
            }
            break;
        }
    }
}

void write_code(FILE* code_file, Frame* func) {
    uint32_t name_len = func->func_name.size();
    fwrite(&name_len, sizeof(name_len), 1, code_file);
    fwrite(func->func_name.c_str(), 1, name_len, code_file);
    int32_t id = func->func_id;
    int32_t arg_len = func->args_len;
    fwrite(&id, sizeof(id), 1, code_file);
    fwrite(&arg_len, sizeof(arg_len), 1, code_file);
    uint32_t code_size = (!func->is_build_in)? func->codes->op_codes.size() : 0;
    fwrite(&code_size, sizeof(code_size), 1, code_file);
    if (!func->is_build_in)
        for (int op : func->codes->op_codes) {
            int32_t op32 = op;
            fwrite(&op32, sizeof(op32), 1, code_file);
        }
    uint32_t name_count = (!func->is_build_in)? func->codes->names.size() : 0;
    fwrite(&name_count, sizeof(name_count), 1, code_file);
    if (!func->is_build_in)
        for (const auto& n : func->codes->names) {
            uint32_t len = n.size();
            fwrite(&len, sizeof(len), 1, code_file);
            fwrite(n.c_str(), 1, len, code_file);
        }
    uint32_t const_count = (!func->is_build_in)? func->codes->const_pool.size() : 0;
    fwrite(&const_count, sizeof(const_count), 1, code_file);
    if (!func->is_build_in)
        for (auto val : func->codes->const_pool) {
            write_value(code_file, val);
        }
}

void save_code(const std::string& filename, CompileOutput* output) {
    FILE* code_file = fopen(filename.c_str(), "wb");
    uint32_t magic = 0xC0001;
    fwrite(&magic, sizeof(magic), 1, code_file);
    uint32_t func_cnt = output->funcs.size();
    fwrite(&func_cnt, sizeof(func_cnt), 1, code_file);
    for (auto frame : output->funcs) {
        write_code(code_file, frame);
    }
    fclose(code_file);
}

#endif