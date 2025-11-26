//---------------------------------------------------------------------------
// Lua serialization/deserialization based on he::Symbols type description
#ifndef OpcUA_SerializerH
#define OpcUA_SerializerH
//---------------------------------------------------------------------------
#include <lua.hpp>
#include "Symbols.h"

namespace he {
namespace lua {

class Serializer
{
public:
	// Serialize the table on top of the stack to the binary representation according to the type node description
	static int Serialize(lua_State* L, const he::Symbols::TypeDB& db, const he::Symbols::TypeNode& node, uint8_t* pDstBuf, size_t iDstLen);

	// Deserialize the given binary buffer into a lua table according to the given type node description
	static int Deserialize(lua_State* L, const he::Symbols::TypeDB& db, const he::Symbols::TypeNode& node, const uint8_t* pSrcBuf, size_t iSrcLen);

	// Get the type definition as LUA table
	static int GetTypeDef(lua_State* L, const he::Symbols::TypeDB& db, const he::Symbols::TypeNode& node);

	// Dump the type node definition
	static int Dump(const he::Symbols::TypeDB& db, const he::Symbols::TypeNode& node);

	// Dump the full data structure (type+value)
	static int Dump(const he::Symbols::TypeDB& db, const he::Symbols::TypeNode& node, const uint8_t* pBuf);

	// Dump the LUA table at the given stack index
	static void DumpTable(lua_State* L, int index);

};
} // namespace lua
} // namespace he


//---------------------------------------------------------------------------
#endif
