//---------------------------------------------------------------------------

#ifndef OpcUA_Serializer_LuaH
#define OpcUA_Serializer_LuaH
//---------------------------------------------------------------------------
#ifdef _WIN32
#include <rpc.h>
#else
#include <uuid/uuid.h>
#endif

#include "open62541.h"

#include <lua.hpp>
#include "Symbols.h"
#include "OpcUA_Serializer.h"

#define SOL_CHECK_ARGUMENTS 1
#include "sol/sol.hpp"

// Proxy class to create a bridge between sol and serializer/symbols
class TypeNode_Proxy {
protected:
	//TypeNode_Proxy(TypeNode_Proxy& prox);
	//UA_Client* _client;
	//ClientNodeMgr* _mgr;
	//typedef std::function<void(UA_UInt32 monId, UA_DataValue value, UA_UInt32 subId, void *monContext)> SubscribeCallback;

public:
	TypeNode_Proxy() {};
	TypeNode_Proxy(const he::Symbols::TypeNode& tn) : Node(tn) {};
	he::Symbols::TypeNode   Node;

	const char* GetItemName();
	sol::variadic_results serialize(sol::table newValue, sol::this_state L);
	sol::variadic_results deserialize(const std::string& sData, sol::this_state L);
	sol::variadic_results asTable(sol::this_state L);
};

#endif
