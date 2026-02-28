#ifndef COPL_PROGRAM_LOADER_HPP
#define COPL_PROGRAM_LOADER_HPP

#include "value.hpp"
#include "..\resfile_types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>

std::vector<Frame*> load_bytecode(const std::string& filename,
                                   const std::unordered_map<std::string, BUILD_IN_PROC*>& builtins) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        printf("Cannot open bytecode file: %s\n", filename.c_str());
        exit(-1);
    }

    uint32_t magic;
    fread(&magic, sizeof(magic), 1, file);
    if (magic != 0xC0001) {
        printf("Invalid bytecode file (magic mismatch)\n");
        fclose(file);
        exit(-1);
    }

    uint32_t func_cnt;
    fread(&func_cnt, sizeof(func_cnt), 1, file);

    std::vector<Frame*> frames;
    frames.reserve(func_cnt);

    for (uint32_t i = 0; i < func_cnt; ++i) {
        uint32_t name_len;
        fread(&name_len, sizeof(name_len), 1, file);
        std::string func_name(name_len, '\0');
        fread(&func_name[0], 1, name_len, file);

        int32_t func_id, args_len;
        fread(&func_id, sizeof(func_id), 1, file);
        fread(&args_len, sizeof(args_len), 1, file);
     
        uint32_t code_size;
        fread(&code_size, sizeof(code_size), 1, file);

        auto it = builtins.find(func_name);
        bool is_builtin = (it != builtins.end());

        std::vector<int> op_codes;
        if (!is_builtin && code_size > 0) {
            op_codes.resize(code_size);
            for (uint32_t j = 0; j < code_size; ++j) {
                int32_t op;
                fread(&op, sizeof(op), 1, file);
                op_codes[j] = op;
            }
        } else {
            fseek(file, code_size * sizeof(int32_t), SEEK_CUR);
        }

        uint32_t name_count;
        fread(&name_count, sizeof(name_count), 1, file);
        std::vector<std::string> names;
        if (!is_builtin && name_count > 0) {
            names.resize(name_count);
            for (uint32_t j = 0; j < name_count; ++j) {
                uint32_t len;
                fread(&len, sizeof(len), 1, file);
                std::string str(len, '\0');
                fread(&str[0], 1, len, file);
                names[j] = std::move(str);
            }
        } else {
            for (uint32_t j = 0; j < name_count; ++j) {
                uint32_t len;
                fread(&len, sizeof(len), 1, file);
                fseek(file, len, SEEK_CUR);
            }
        }

        uint32_t const_count;
        fread(&const_count, sizeof(const_count), 1, file);
        std::vector<STACK_VALUE*> const_pool;
        if (!is_builtin && const_count > 0) {
            const_pool.resize(const_count);
            for (uint32_t j = 0; j < const_count; ++j) {
                uint8_t type;
                fread(&type, 1, 1, file);
                STACK_VALUE* val = nullptr;
                switch (type) {
                    case BINT: {
                        int32_t i;
                        fread(&i, sizeof(i), 1, file);
                        val = STACK_VALUE::make_int(i);
                        break;
                    }
                    case BFLOAT: {
                        double d;
                        fread(&d, sizeof(d), 1, file);
                        val = STACK_VALUE::make_double(d);
                        break;
                    }
                    case BSTRING: {
                        uint32_t len;
                        fread(&len, sizeof(len), 1, file);
                        std::string s(len, '\0');
                        fread(&s[0], 1, len, file);
                        val = STACK_VALUE::make_str(s);
                        break;
                    }
                    case BBOOL: {
                        uint8_t b;
                        fread(&b, 1, 1, file);
                        val = STACK_VALUE::make_bool(b != 0);
                        break;
                    }
                    case BNULL:
                    default:
                        val = STACK_VALUE::make_null();
                        break;
                }
                const_pool[j] = val;
            }
        } else {
            for (uint32_t j = 0; j < const_count; ++j) {
                uint8_t type;
                fread(&type, 1, 1, file);
                switch (type) {
                    case BINT:    fseek(file, sizeof(int32_t), SEEK_CUR); break;
                    case BFLOAT:  fseek(file, sizeof(double), SEEK_CUR); break;
                    case BSTRING: {
                        uint32_t len;
                        fread(&len, sizeof(len), 1, file);
                        fseek(file, len, SEEK_CUR);
                        break;
                    }
                    case BBOOL:   fseek(file, 1, SEEK_CUR); break;
                    case BNULL:   break;
                    default:       break;
                }
            }
        }

        Frame* frame = nullptr;
        if (is_builtin) {
            frame = new Frame(it->second, func_name);
            frame->func_id = func_id;
            frame->args_len = args_len;
        } else {
            Chunk* chunk = new Chunk;
            chunk->op_codes = std::move(op_codes);
            chunk->names = std::move(names);
            chunk->const_pool = std::move(const_pool);
            frame = new Frame(chunk);
            frame->func_name = func_name;
            frame->func_id = func_id;
            frame->args_len = args_len;
        }

        frames.push_back(frame);
    }

    fclose(file);
    return frames;
}

#endif 