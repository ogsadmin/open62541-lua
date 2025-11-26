//---------------------------------------------------------------------------

#pragma hdrstop

#include "OpcUA_Serializer_Lua.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

const char* TypeNode_Proxy::GetItemName() {
	return Node.item.ItemName.c_str();
}

sol::variadic_results TypeNode_Proxy::serialize(sol::table newValue, sol::this_state L)
{
	sol::variadic_results result;

	// encode from lua structure
	const he::Symbols::TypeNode& symDef = Node;
	if (!symDef.item.isValid()) {
		// We don't have a valid symbol definition (likely the PLC uses some unknown
		// data types), so this function cannot be used.
		// Use raw I/O instead!
		result.push_back({ L, sol::lua_nil });
		result.push_back({ L, sol::in_place, "Symbol definition is not valid, cannot serialize. Use raw I/O functions to exchange data instead!" });
		return result;
	}

	// get the table on the TOS
	int  ref = newValue.registry_index();
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

	// serialize the table at TOS into a binary data stream
	UA_ByteString bs;
	UA_ByteString_init(&bs);
	bs.data = new UA_Byte[8192];
	bs.length = 8192;
	bs.length = he::lua::Serializer::Serialize(L, _db, symDef, bs.data, bs.length);

	lua_pop(L, 1);
	if (bs.length == 0) {
		// This is *NOT* normal - most likely the symDef is not available, as
		// the PLC uses some unknown data types...
		delete[] bs.data;
		// return an error.
		result.push_back({ L, sol::lua_nil });
		result.push_back({ L, sol::in_place, "Failed to serialize, possibly data or data type definition is invalid!" });
		return result;
	}

	result.push_back({ L, sol::in_place_type<std::string>, std::string((const char*)bs.data, bs.length)});
	delete[] bs.data;
	return result;
}


sol::variadic_results TypeNode_Proxy::deserialize(const std::string& sData, sol::this_state L)
{
	sol::variadic_results result;

	UA_ByteString bs;
	UA_ByteString_init(&bs);
	bs.data = (UA_Byte*)sData.c_str();
	bs.length = sData.size();

	const he::Symbols::TypeNode& symDef = Node;
	if (symDef.item.isValid) {
		if (bs.data) {
			// deserialize the results into a new table at TOS
			he::lua::Serializer::Deserialize(L, _db, symDef, bs.data, bs.length);
			// wrap the native LUA table in a sol::table to return it through the variadic_result vector
			sol::table table(L, -1);
			result.push_back(table);
			lua_pop(L,1);                       // remove the table created earlier from the LUA stack
		}
		else {
			result.push_back({ L, sol::lua_nil });
		}
	}
	else {
		// fallback to raw I/O only
		result.push_back({ L, sol::lua_nil });
	}

	result.push_back({ L, sol::in_place_type<std::string>, std::string((const char*)bs.data, bs.length)});
	UA_ByteString_clear(&bs);
	return result;
}


// Get an internal type definition as lua table
// Returns <table> or nil,errormessage
sol::variadic_results TypeNode_Proxy::asTable(sol::this_state L)
{
	sol::variadic_results result;

	const he::Symbols::TypeNode& symDef = Node;
	if (!symDef.item.isValid()) {
		// We don't have a valid symbol definition (likely the PLC uses some unknown
		// data types), so this function cannot be used.
		// Use raw I/O instead!
		result.push_back({ L, sol::lua_nil });
		result.push_back({ L, sol::in_place, "Symbol definition is not valid!" });
		return result;
	}

	// deserialize the results into a new table at TOS
	int ret = he::lua::Serializer::GetTypeDef(L, _db, symDef);
	if (ret != 1) {
		// some error occurred
		result.push_back({ L, sol::lua_nil });
		result.push_back({ L, sol::in_place, "Failed to decode type definition!" });
	}
	else {
		// we have the table on TOS
		// wrap the native LUA table in a sol::table to return it through the variadic_result vector
		sol::table table(L, -1);
		result.push_back(table);
		lua_pop(L,1);                       // remove the table created earlier from the LUA stack
	}
	return result;
}
