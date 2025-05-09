#include "open62541.h"

#define SOL_CHECK_ARGUMENTS 1
#include "sol/sol.hpp"

/******
 * opcua module - open62541 lua wrapper
 * @module opcua
 *
 */

#define OPCUA_VERSION "1.3"

#ifdef UA_ENABLE_ENCRYPTION_OPENSSL
#pragma link "libcrypto-1_1.lib"
#pragma link "libssl-1_1.lib"
#endif

static int lua_test(lua_State *L)
{
	const char *s = lua_tostring(L, 1);
	lua_pushstring(L, s);
	return 1;
}

namespace lua_opcua {
	void reg_opcua_enums(sol::table& module);
	void reg_opcua_types(sol::table& module);
	void reg_opcua_node(sol::table& module);
	void reg_opcua_client(sol::table& module);
	void reg_opcua_server(sol::table& module);

	sol::table open_opcua(sol::this_state L) {
		sol::state_view lua(L);
		sol::table module = lua.create_table();

		reg_opcua_enums(module);
		reg_opcua_types(module);
		reg_opcua_node(module);
		reg_opcua_client(module);
		reg_opcua_server(module);

		/***
		 * Get status code name in string
		 * @function getStatusCodeName
		 * @tparam StatusCode sc Status code
		 * @treturn string status code name string
		 */
		module.set_function("getStatusCodeName", UA_StatusCode_name);

		/**
		 * Module version
		 */
		module["VERSION"] = OPCUA_VERSION;

//		int ref = module.registry_index();
//		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
//		lua_pushcfunction(L, lua_test); 	// Push the C function onto the stack
//		lua_setfield(L, -2, "lua_test"); 	// Set the function in the table with key

		return module;
	}
}

extern "C" int luaopen_opcua(lua_State *L) {
	int ret = sol::stack::call_lua(L, 1, lua_opcua::open_opcua);
	return ret;
}
