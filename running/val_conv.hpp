//
// Created by User on 2026/3/2.
//

#ifndef COPL_VAL_CONV_HPP
#define COPL_VAL_CONV_HPP
#include "value.hpp"

inline OPL_VALUE::OPL_BasicValue* val_conv(OPL_VALUE::STACK_VALUE* value) {
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
	new_obj->next = nullptr;
	return new_obj;
}


inline std::string get_integer(OPL_VALUE::STACK_VALUE* arg)  {
	return std::to_string(((OPL_VALUE::OPL_Integer*)val_conv(arg))->i);
}

#endif //COPL_VAL_CONV_HPP
