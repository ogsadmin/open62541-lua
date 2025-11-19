//---------------------------------------------------------------------------
// Serialize/deserialize binary data according to a given TypeNode tree
// data format definition.
// Used to serialize/deserialize between binary data structures and LUA,
// e.g. OPCUA, ADS, ...
// NOTE: The primary source of truth is the TypeNode data definition, this must
//       match the actual binary data. The serializer/deserializer tries to be
//       "nice" about additional and missing LUA table fields when serializing.
#pragma hdrstop

#include "OpcUA_Serializer.h"
#include <assert.h>
#include <map>
#include <assert.h>
#include "logger.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)

using namespace he::lua;
using namespace he::Symbols;

// Define a deserializer function for a "primitive" type (no struct, no array)
typedef int (*fnPrimitiveDeserialize)(lua_State*, const he::Symbols::TypeInfo&, const uint8_t*);
#define TPARAMS(L, ti, p) lua_State* L, const he::Symbols::TypeInfo& ti, const uint8_t* p

// ARGH! Embarcadero hat keine std::function!
#if EMBARCADERO_WOULD_HAVE_STD_FUNCTION
//typedef std::map<unsigned long, std::function<int(lua_State*, const const he::Symbols::TypeInfo&, const uint8_t*)> >  TDeserializer;
typedef std::map<unsigned long, std::function<fnPrimitiveDeserialize> >  TDeserializer;
static TDeserializer _deserialize
{
	{  TypeInfo::Type::T_SInt16, [](TPARAMS(L, ti, p)){ lua_pushinteger(L, *((int16_t*)p));} },
	// ...
};
#else
// Deserialization functions for primitive types
// NOTE: The deserialization functions return the actual number of bytes consumed from the input stream
// TODO: Add more deserialization functions for special cases (e.g. byte length prefixed string, ...)
int _deser_bool8(TPARAMS(L, ti, p))		{ lua_pushboolean(L, *((int8_t*)p)); return ti.DataSize; };
int _deser_int16(TPARAMS(L, ti, p))		{ lua_pushinteger(L, *((int16_t*)p)); return ti.DataSize; };
int _deser_int32(TPARAMS(L, ti, p))		{ lua_pushinteger(L, *((int32_t*)p)); return ti.DataSize; };
int _deser_real32(TPARAMS(L, ti, p))	{ lua_pushnumber(L, *((float*)p)); return ti.DataSize; };
int _deser_real64(TPARAMS(L, ti, p))	{ lua_pushnumber(L, *((double*)p)); return ti.DataSize; };
int _deser_int8(TPARAMS(L, ti, p))		{ lua_pushinteger(L, *((int8_t*)p)); return ti.DataSize; };
int _deser_uint8(TPARAMS(L, ti, p))		{ lua_pushinteger(L, *((uint8_t*)p)); return ti.DataSize; };
int _deser_uint16(TPARAMS(L, ti, p))	{ lua_pushinteger(L, *((uint16_t*)p)); return ti.DataSize; };
int _deser_uint32(TPARAMS(L, ti, p))	{ lua_pushinteger(L, *((uint32_t*)p)); return ti.DataSize; };
int _deser_string(TPARAMS(L, ti, p))	{ lua_pushstring(L, (const char*)p); return strlen((const char*)p); };
int _deser_lstring(TPARAMS(L, ti, p))	{
	int n = *((int32_t*)p);
	lua_pushlstring(L, (const char*)p+4, n);
	return n+4;
};
#undef TI
//void _deser_bit(lua_State* L, const uint8_t* p, int len){ lua_pushinteger(L, *((uint8_t*)p));};
class TDeserializer : public std::map<TypeInfo::Type, fnPrimitiveDeserialize >
{
public:
	TDeserializer() {
		std::map<TypeInfo::Type, fnPrimitiveDeserialize >& p = *this;
		p[TypeInfo::Type::T_Bool8] 	= _deser_bool8;
		p[TypeInfo::Type::T_SInt16] = _deser_int16;
		p[TypeInfo::Type::T_SInt32] = _deser_int32;
		p[TypeInfo::Type::T_Float] 	= _deser_real32;
		p[TypeInfo::Type::T_Double] = _deser_real64;
		p[TypeInfo::Type::T_SInt8] 	= _deser_int8;
		p[TypeInfo::Type::T_UInt8] 	= _deser_uint8;
		p[TypeInfo::Type::T_UInt16] = _deser_uint16;
		p[TypeInfo::Type::T_UInt32] = _deser_uint32;
		p[TypeInfo::Type::T_StringL4] = _deser_lstring;
		p[TypeInfo::Type::T_ByteString] = _deser_lstring;
		// TODO: implement:
/*
		p[TypeInfo::Type::T_StringFix] = nullptr;
		p[TypeInfo::Type::T_Undefined] = nullptr;
		p[TypeInfo::Type::T_UInt64] = nullptr;
		p[TypeInfo::Type::T_SInt64] = nullptr;
		p[TypeInfo::Type::T_DateTime] = nullptr;
		p[TypeInfo::Type::T_Guid] = nullptr;
*/
	}
};
TDeserializer _deserialize;
#endif

// recursively walk the data type list - deserialize or get the type info (if pRawBuf == 0)
/*
static void PushType(lua_State* L, const ADS::TypeNode& node, const uint8_t* pRawBuf, bool isRoot = false)
{
	if (0 == node.children.size())
	{
		// plain type
		lua_pushstring(L, node.item.ItemName.c_str());
		if (!pSrcBuf) {
			// only report the data type
			lua_pushstring(L, node.item.ItemType.c_str());
		} else {
			// deserialize the value
			TDeserializer::iterator it = _deserialize.find(node.item.Entry->dataType);
			if (it != _deserialize.end() && it->second != nullptr) {
				it->second(L, &pRawBuf[node.item.Offset], node.item.Entry->size);
			}
			else {
				lua_pushstring(L, "!!!ERROR: cannot deserialize this type!!!");
			}
		}
		lua_settable(L, -3);
	}
	else {
		// structured type / subitems
		if (isRoot) {
			if (!pSrcBuf) {
				lua_pushstring(L, "_type");                 // debug
				if (node.item.ItemType.Length() > 0) {
					lua_pushstring(L, node.item.ItemType.c_str());
				} else {
					// the root item usually has no type but a name (as it is a type itself)
					lua_pushstring(L, node.item.ItemName.c_str());
				}
				lua_settable(L, -3);
			}
		}
		else {
			lua_pushstring(L, node.item.ItemName.c_str());
			lua_newtable(L);
			if (!pRawBuf) {
				lua_pushstring(L, "_type");                 // debug
				lua_pushstring(L, node.item.ItemType.c_str());
				lua_settable(L, -3);
			}
		}
		// now add the childrens
		for (int i = 0; i < node.children.size(); i++) {
			PushType(L, node.children[i], pRawBuf);
		}
		// finish table (if added before)
		if (!isRoot) {
			lua_settable(L, -3);
		}
	}
}
*/

// NOTE: there are actually two types of arrays:
//  a) Statically defined arrays. The size of the array is defined in the type
//     definition, the data size is fixed.
//  b) Dynamically defined arrays - they carry the size within the data
// This implementation currently only supports 1-dimensional dynamic arrays
// where the array size is a 32-bit uint as a first data word.
static int deserialize(lua_State* L, const he::Symbols::TypeNode& node, const uint8_t* pBuf, size_t iSrcLen, int addr = 0, int level = 0, bool isRoot = false)
{
	const he::Symbols::TypeInfo& ts = node.item;
	#ifdef XDUMP
	XTRACE(XPDIAG1, "%04d: %*s[%d] Struct %s (%s)", addr, level*4, " ", level, AnsiString(ts.ItemName).c_str(), AnsiString(ts.ItemType).c_str());
	#endif
	lua_pushstring(L, ts.ItemName.c_str());
	lua_newtable(L);
	const uint8_t* pBegin = pBuf;
	pBuf = pBuf + addr;
	// we don't support optional elements at the moment, so simply skip header!
	int iChildCount = node.children.size();
	if (ts.Offset != 0) {   // ARGH! Optional struct maybe? We must set the flags....
		uint32_t bits = 0;
		if (ts.DataType.isStruct && ts.DataType.type == he::Symbols::TypeInfo::Type::S_StructOptFld) {
			const uint32_t* p = (const uint32_t*)pBuf;
			bits = *p;
		}
	}
	addr = addr + ts.Offset;
	pBuf = pBuf + ts.Offset;
	for (int i = 0; i < iChildCount; i++) {
		const he::Symbols::TypeNode& tn = node.children[i];
		const he::Symbols::TypeInfo& ti = tn.item;
		int iCount = 1;
		if (ti.DataType.isArray) {
			iCount = *((uint32_t*)pBuf);
			addr += 4;
			pBuf += 4;
			#ifdef XDUMP
			XTRACE(XPDIAG1, "%04d: %*s    (%d) %s(%d) Name=%s Array[%d] ",
				addr, level*4, " ", i,
				AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
				AnsiString(ti.ItemName).c_str(), iCount);
			#endif
			// this entry is an array - so add a table under the given name here!
			lua_pushstring(L, ti.ItemName.c_str());
			lua_newtable(L);
		}
		for (int iSeq = 0; iSeq < iCount; iSeq++) {
			if (ti.DataType.isStruct)
			{
				// Struct field?
				int l = deserialize(L, tn, pBegin, iSrcLen, addr, level+1);
				addr = l;
				pBuf = pBegin + addr;
			}
			else {
				if (ti.DataType.isArray) {
					// Array entry - push the value first, then later use rawseti()
				}
				else {
					// Plain field - use the name
					lua_pushstring(L, ti.ItemName.c_str());
				}
				// deserialize the value
				TDeserializer::iterator it = _deserialize.find(ti.DataType.type);
				int iLen = 0;
				if (it != _deserialize.end() && it->second != nullptr) {
                    // push the deserialized value to the lua stack:
					iLen = it->second(L, ti, pBuf);
				}
				else {
					lua_pushstring(L, "!!!ERROR: cannot deserialize this type!!!");
				}
				if (ti.DataType.isArray) {
					// Array entry - add the index to set
					lua_rawseti(L, -2, iSeq + 1);       // lua arrays start from 1
				}
				else {
					// Plain field - add the table value
					lua_settable(L, -3);
				}
				#ifdef XDUMP
				AnsiString sVal;
				int iLen = 0;
				switch(ti.DataType.type) {
				case he::Symbols::TypeInfo::Type::T_Bool8: iLen = 1; sVal.printf("%s", *pBuf?"true":"false"); break;
				case he::Symbols::TypeInfo::Type::T_UInt8: iLen = 1; sVal.printf("%02Xh", *pBuf); break;
				case he::Symbols::TypeInfo::Type::T_SInt8: iLen = 1; sVal.printf("%02Xh", *pBuf); break;
				case he::Symbols::TypeInfo::Type::T_UInt16: iLen = 2; sVal.printf("%04Xh", *((uint16_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_SInt16: iLen = 2; sVal.printf("%04Xh", *((uint16_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_UInt32: iLen = 4; sVal.printf("%08Xh", *((uint32_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_SInt32: iLen = 4; sVal.printf("%08Xh", *((uint32_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_UInt64: iLen = 8; sVal = "u64"; break;
				case he::Symbols::TypeInfo::Type::T_SInt64: iLen = 8; sVal = "s64"; break;
				case he::Symbols::TypeInfo::Type::T_Float: iLen = 4; sVal.printf("%f", *((float*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_Double: iLen = 8; sVal.printf("%lf", *((double*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_StringL4: iLen = *((uint32_t*)pBuf); sVal = AnsiString((char*)&pBuf[4], iLen); iLen += 4; break;
				case he::Symbols::TypeInfo::Type::T_StringFix: iLen = *((uint32_t*)pBuf); sVal = AnsiString((char*)&pBuf[4], iLen); iLen += 4; break;
				case he::Symbols::TypeInfo::Type::T_DateTime:
				case he::Symbols::TypeInfo::Type::T_Guid:
				case he::Symbols::TypeInfo::Type::T_ByteString:
					break;
				}
				if (!ti.DataType.isArray) {
					XTRACE(XPDIAG1, "%04d: %*s    (%d) %s(%d) len=%d, Name=%s Val=%s",
						offset, level*4, " ", i,
						AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
						iLen, AnsiString(ti.ItemName).c_str(), sVal.c_str());
				} else {
					XTRACE(XPDIAG1, "%04d: %*s        [%d] Val=%s",
						offset, level*4, " ", iSeq, sVal.c_str());
				}
				#endif
				addr += iLen;
				pBuf += iLen;
			}
		} // for each array element
		if (ti.DataType.isArray) {
			// finish the array
			lua_settable(L, -3);
		}
	}
	// finish table
	if (!isRoot) {
		lua_settable(L, -3);
	}
	return addr;
}


// Deserialize the given binary buffer into a lua table according to the given type node description
// return 1 (table on lua stack) if ok
// return 2 (nil, error on lua stack) if error
int Serializer::Deserialize(lua_State* L, const he::Symbols::TypeNode& node, const uint8_t* pSrcBuf, size_t iSrcLen)
{
	return deserialize(L, node, pSrcBuf, iSrcLen, 0, 0, true);
}



/*
// recursively walk the data type list - deserialize or get the type info (if pRawBuf == 0)
static void PushType(lua_State* L, const ADS::TypeNode& node, const uint8_t* pRawBuf, bool isRoot = false)
{
	if (0 == node.children.size())
	{
		// plain type
		lua_pushstring(L, node.item.ItemName.c_str());
		if (!pRawBuf) {
			// only report the data type
			lua_pushstring(L, node.item.ItemType.c_str());
		} else {
			// deserialize the value
			TDeserializer::iterator it = _deserialize.find(node.item.Entry->dataType);
			if (it != _deserialize.end()) {
				it->second(L, &pRawBuf[node.item.Offset], node.item.Entry->size);
			}
			else {
				lua_pushstring(L, "!!!ERROR: cannot deserialize this type!!!");
			}
		}
		lua_settable(L, -3);
	}
	else {
		// structured type / subitems
		if (isRoot) {
			if (!pRawBuf) {
				lua_pushstring(L, "_type");                 // debug
				if (node.item.ItemType.Length() > 0) {
					lua_pushstring(L, node.item.ItemType.c_str());
				} else {
					// the root item usually has no type but a name (as it is a type itself)
					lua_pushstring(L, node.item.ItemName.c_str());
				}
				lua_settable(L, -3);
			}
		}
		else {
			lua_pushstring(L, node.item.ItemName.c_str());
			lua_newtable(L);
			if (!pRawBuf) {
				lua_pushstring(L, "_type");                 // debug
				lua_pushstring(L, node.item.ItemType.c_str());
				lua_settable(L, -3);
			}
		}
		// now add the childrens
		for (int i = 0; i < node.children.size(); i++) {
			PushType(L, node.children[i], pRawBuf);
		}
		// finish table (if added before)
		if (!isRoot) {
			lua_settable(L, -3);
		}
	}
}
*/

// Define a deserializer function for a "primitive" type (no struct, no array)
typedef int (*fnPrimitiveSerialize)(lua_State*, const he::Symbols::TypeInfo&, uint8_t*);
#define DPARAMS(L, ti, p) lua_State* L, const he::Symbols::TypeInfo& ti, uint8_t* p
//#define LCK(x) int t=lua_type(L,-1); if (t != x) return; else
//#define LCK_NUM int t=lua_type(L,-1); if (t != LUA_TNUMBER) return; else
//int _ser_int16(DPARAMS(L, ti, p)){ LCK(LUA_TNUMBER) *((int16_t*)p) = lua_tointeger(L, -1); return ti.DataSize; };
// If the LUA element is not the expected type, be nice and return a dummy value
int _ser_bool8(DPARAMS(L, ti, p))	{
	int t = lua_type(L, -1);
	int value = 0;      // default to false
	if (t == LUA_TBOOLEAN) {
		value = lua_toboolean(L, -1);
	} else if (t == LUA_TNUMBER) {
        luaL_optinteger(L, -1, 0);
	}
	*((int8_t*)p) = value;
	return ti.DataSize;
};
int _ser_int16(DPARAMS(L, ti, p))	{ *((int16_t*)p) = luaL_optinteger(L, -1, 0); return ti.DataSize; };
int _ser_int32(DPARAMS(L, ti, p))	{ *((int32_t*)p) = luaL_optnumber(L, -1, 0); return ti.DataSize; };
int _ser_real32(DPARAMS(L, ti, p))	{ *((float*)p) = luaL_optnumber(L, -1, 0); return ti.DataSize; };
int _ser_real64(DPARAMS(L, ti, p))	{ *((double*)p) = luaL_optnumber(L, -1, 0); return ti.DataSize; };
int _ser_int8(DPARAMS(L, ti, p))	{ *((int8_t*)p) = luaL_optinteger(L, -1, 0); return ti.DataSize; };
int _ser_uint8(DPARAMS(L, ti, p))	{ *((uint8_t*)p) = luaL_optinteger(L, -1, 0); return ti.DataSize; };
int _ser_uint16(DPARAMS(L, ti, p))	{ *((uint16_t*)p) = luaL_optinteger(L, -1, 0); return ti.DataSize; };
int _ser_uint32(DPARAMS(L, ti, p))	{ *((uint32_t*)p) = luaL_optnumber(L, -1, 0); return ti.DataSize; };
int _ser_stringfix(DPARAMS(L, ti, p)){
	size_t cnt = 0;
	memset(p, 0, ti.DataSize);
	if (!lua_isstring(L, -1)) {
		// WHAT???
		return 0;    // TODO: check if we could be nice and try converting the lua item tostring()
	} else {
		const char* s = lua_tolstring(L, -1, &cnt);
		if (cnt > ti.DataSize) cnt = ti.DataSize;
		memcpy(p, s, cnt);
	}
	return cnt;
};
int _ser_lstring(DPARAMS(L, ti, p)){       // write length prefixed string
	size_t cnt = 0;
	if (!lua_isstring(L, -1)) {
		// WHAT???
		*((uint32_t*)p) = 0;                // empty string
		return 4;    // TODO: check if we could be nice and try converting the lua item tostring()
	} else {
		const char* s = lua_tolstring(L, -1, &cnt);
		*((uint32_t*)p) = cnt;
		if (cnt > 0) {
			p += 4;
			memcpy(p, s, cnt);
		}
        return 4 + cnt;
	}
};
void _ser_bit(DPARAMS(L, ti, p)) {
	int t = lua_type(L, -1);
	switch(t) {
	case LUA_TNIL:  *((uint8_t*)p) = 0; break;
	case LUA_TNUMBER: *((uint8_t*)p) = lua_tointeger(L, -1); break;
	case LUA_TBOOLEAN: *((uint8_t*)p) = lua_toboolean(L, -1); break;
	}
};
class TSerializer : public std::map<TypeInfo::Type, fnPrimitiveSerialize >
{
public:
	TSerializer() {
		std::map<TypeInfo::Type, fnPrimitiveSerialize >& p = *this;
		p[TypeInfo::Type::T_SInt16] = _ser_int16;
		p[TypeInfo::Type::T_SInt32] = _ser_int32;
		p[TypeInfo::Type::T_Float] 	= _ser_real32;
		p[TypeInfo::Type::T_Double] = _ser_real64;
		p[TypeInfo::Type::T_Bool8] 	= _ser_bool8;
		p[TypeInfo::Type::T_SInt8] 	= _ser_int8;
		p[TypeInfo::Type::T_UInt8] 	= _ser_uint8;
		p[TypeInfo::Type::T_UInt16] = _ser_uint16;
		p[TypeInfo::Type::T_UInt32] = _ser_uint32;
		p[TypeInfo::Type::T_StringL4] = _ser_lstring;
		p[TypeInfo::Type::T_ByteString] = _ser_lstring;
		// TODO: implement:
/*
		p[TypeInfo::Type::T_StringFix] = nullptr;
		p[TypeInfo::Type::T_Undefined] = nullptr;
		p[TypeInfo::Type::T_UInt64] = nullptr;
		p[TypeInfo::Type::T_SInt64] = nullptr;
		p[TypeInfo::Type::T_DateTime] = nullptr;
		p[TypeInfo::Type::T_Guid] = nullptr;
*/
	}

};
TSerializer _serialize;


/*
// recursively walk the data type list and fill a memory buffer
static void Serialize(lua_State* L, const ADS::TypeNode& node, uint8_t* pDstBuf, bool isRoot = false)
{
#if 0 	// DEBUG -- assume there is a table at TOF, wlak all entries
	dump_table(L, -1);
#endif	// DEBUG END
	int tos = lua_gettop(L);
	int childCount = node.children.size();
	if (0 == childCount)
	{
		// plain type - get the LUA table entry
		lua_getfield(L, -1, node.item.ItemName.c_str());    // --> value is now TOS
		// deserialize the value
//		XTRACE(XPDIAG2, "    %s", AnsiString(node.item.ItemName.c_str()));
		TSerializer::iterator it = _serialize.find(node.item.Entry->dataType);
		if (it != _serialize.end()) {
			it->second(L, &pDstBuf[node.item.Offset], node.item.Entry->size);
		}
		else {
			lua_pushstring(L, "!!!ERROR: cannot deserialize this type!!!");
		}
		lua_pop(L, 1);      // remove the value
	}
	else {
		// structured type / subitems
		if (isRoot) {
		}
		else {
			lua_getfield(L, -1, node.item.ItemName.c_str());    // --> sub-table is now TOS
		}
		// now add the childrens
		for (int i = 0; i < node.children.size(); i++) {
			Serialize(L, node.children[i], pDstBuf);
		}
		// finish table (if added before)
		if (!isRoot) {
			//lua_settable(L, -3);
			lua_pop(L, 1);
		}
	}
	if (lua_gettop(L) != tos) {
		throw Exception("LUA stack error!");
	}
}
*/
static uint32_t getbits(int count)
{
	if (count >= 31) {
		return (uint32_t)0xFFFFFFFF;
	} else {
		uint32_t mask = 1 << (count);
		return mask - 1;
	}
}

static void write_optstruct(int count, const uint8_t* pWr)
{
	uint32_t* p = (uint32_t*)pWr;
	*p = getbits(count);
}


static int serialize(lua_State* L, const he::Symbols::TypeNode& node, uint8_t* pBuf, size_t iDstLen, int addr = 0, int level = 0, bool isRoot = false)
{
#if 0 	// DEBUG -- assume there is a table at TOF, walk all entries
	Serializer::DumpTable(L, -1);
#endif	// DEBUG END
	int tos = lua_gettop(L);

	const he::Symbols::TypeInfo& ts = node.item;
	uint8_t* pBegin = pBuf;
	pBuf = pBuf + addr;
	#ifdef XDUMP
	XTRACE(XPDIAG1, "%04d: %*s[%d] Struct %s (%s)", addr, level*4, " ", level, AnsiString(ts.ItemName).c_str(), AnsiString(ts.ItemType).c_str());
	#endif
	// we don't support optional elements at the moment, so simply skip header!
    int iChildCount = node.children.size();
	if (ts.Offset != 0) {   // ARGH! Optional struct maybe? We must set the flags....
		if (ts.DataType.isStruct && ts.DataType.type == he::Symbols::TypeInfo::Type::S_StructOptFld) {
			/// !!! CtrlX BUG !!!
			/// CtrlX falsely reports/requires bits-1 !!!!
			write_optstruct(iChildCount, pBuf);
		}
		addr = addr + ts.Offset;
		pBuf = pBuf + ts.Offset;
	}
	for (int i = 0; i < iChildCount; i++) {
		const he::Symbols::TypeNode& tn = node.children[i];
		const he::Symbols::TypeInfo& ti = tn.item;
		int iCount = 1;
		if (ti.DataType.isArray) {
			// NOTE: only 1-dimensional arrays are supported
			if (ti.ValueRank != 1) {
				XTRACE(XPERRORS, "%04d: %*s    (%d) %s(%d) Name=%s: ValueRank %d NOT SUPPORTED!",
					addr, level*4, " ", i,
					AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
					AnsiString(ti.ItemName).c_str(), ti.ValueRank);
			}
			// Get the table values, see https://github.com/mikolajgucki/lua-tutorial/blob/master/04-manipulating-lua-tables-in-cpp/manipluating-lua-tables-in-cpp.md
			iCount = ti.ArrayDimensions[0]; 				// *((uint32_t*)pBuf);
			#ifdef XDUMP
			XTRACE(XPDIAG1, "%04d: %*s    (%d) %s(%d) Name=%s Array[%d] ",
				addr, level*4, " ", i,
				AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
				AnsiString(ti.ItemName).c_str(), iCount);
			#endif
			// Get the (sub) table on the top of the stack with the given name
			lua_getfield(L, -1, ti.ItemName.c_str());    	// --> (table) value is now TOS
			int ttype = lua_type(L, -1);
			// get the actual table length
			int length = lua_objlen(L, -1);
			if (length < iCount) {
				iCount = length;                            // only read as much items as available (OPC-UA only defines the max. number of elements)
			}
			lua_pushnil(L);                                 // push the first key
			// add the length dword                         // TODO: support multidimensions, see!
			// see: https://reference.opcfoundation.org/Core/Part6/v105/docs/5.2.5
			uint32_t* p = (uint32_t*)pBuf;
			*p = iCount;
			addr = addr + sizeof(uint32_t);
			pBuf = pBuf + sizeof(uint32_t);
		}
		for (int iSeq = 0; iSeq < iCount; iSeq++) {
			if (ti.DataType.isArray) {
				// get the table element[iSeq]
				// table is at -2
				// key is at -1
				if (lua_next(L, -2)) {
					// key is at -2
					// value is at -1
				}
				else {
					// error!
					XTRACE(XPERRORS, "%04d: %*s    (%d) %s(%d) Name=%s: array index [%d] MISSING!",
						addr, level*4, " ", i,
						AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
						AnsiString(ti.ItemName).c_str(), iSeq+1);
					// simulate the value (next key and a nil value)
					lua_pushinteger(L, iSeq+1);
					lua_pushnil(L);
				}
			}
			else {
				// get the value
			//		XTRACE(XPDIAG2, "    %s", AnsiString(node.item.ItemName.c_str()));
				lua_getfield(L, -1, ti.ItemName.c_str());    	// --> (table) value is now TOS
            }
			if (ti.DataType.isStruct)
			{
				// Struct field?
				int l = serialize(L, tn, pBegin, iDstLen, addr, level+1);
				addr = l;
				pBuf = pBegin + addr;
				lua_pop(L, 1);      // remove the table
			}
			else {
				// serialize the value
				int iLen = 0;
				TSerializer::iterator it = _serialize.find(ti.DataType.type);
				if (it != _serialize.end()) {
					iLen = it->second(L, ti, pBuf);
				}
				else {
					lua_pushstring(L, "!!!ERROR: cannot deserialize this type!!!");
				}
				lua_pop(L, 1);      // remove the value
				#ifdef XDUMP
				AnsiString sVal;
				int iLen = 0;
				switch(ti.DataType.type) {
				case he::Symbols::TypeInfo::Type::T_Bool8: iLen = 1; sVal.printf("%s", *pBuf?"true":"false"); break;
				case he::Symbols::TypeInfo::Type::T_UInt8: iLen = 1; sVal.printf("%02Xh", *pBuf); break;
				case he::Symbols::TypeInfo::Type::T_SInt8: iLen = 1; sVal.printf("%02Xh", *pBuf); break;
				case he::Symbols::TypeInfo::Type::T_UInt16: iLen = 2; sVal.printf("%04Xh", *((uint16_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_SInt16: iLen = 2; sVal.printf("%04Xh", *((uint16_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_UInt32: iLen = 4; sVal.printf("%08Xh", *((uint32_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_SInt32: iLen = 4; sVal.printf("%08Xh", *((uint32_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_UInt64: iLen = 8; sVal = "u64"; break;
				case he::Symbols::TypeInfo::Type::T_SInt64: iLen = 8; sVal = "s64"; break;
				case he::Symbols::TypeInfo::Type::T_Float: iLen = 4; sVal.printf("%f", *((float*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_Double: iLen = 8; sVal.printf("%lf", *((double*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_StringL4: iLen = *((uint32_t*)pBuf); sVal = AnsiString((char*)&pBuf[4], iLen); iLen += 4; break;
				case he::Symbols::TypeInfo::Type::T_StringFix: iLen = *((uint32_t*)pBuf); sVal = AnsiString((char*)&pBuf[4], iLen); iLen += 4; break;
				case he::Symbols::TypeInfo::Type::T_DateTime:
				case he::Symbols::TypeInfo::Type::T_Guid:
				case he::Symbols::TypeInfo::Type::T_ByteString:
					break;
				}
				if (!ti.DataType.isArray) {
					XTRACE(XPDIAG1, "%04d: %*s    (%d) %s(%d) len=%d, Name=%s Val=%s",
						offset, level*4, " ", i,
						AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
						iLen, AnsiString(ti.ItemName).c_str(), sVal.c_str());
				} else {
					XTRACE(XPDIAG1, "%04d: %*s        [%d] Val=%s",
						offset, level*4, " ", iSeq, sVal.c_str());
				}
				#endif
				addr += iLen;
				pBuf += iLen;
			}
		} // for each array element
		if (ti.DataType.isArray) {
			// cleanup LUA stack
			lua_pop(L, 1);
		}
	}
	// finish table
	return addr;
}

// Serialize the table on top of the stack to the binary representation according to the type node description
// return 1 (bool true on lua stack) if ok
// return 2 (nil, error on lua stack) if error
int Serializer::Serialize(lua_State* L, const he::Symbols::TypeNode& node, uint8_t* pDstBuf, size_t iDstLen)
{
	return serialize(L, node, pDstBuf, iDstLen, 0, 0, true);
}

// ----------------------------Tools -----------------------------------

static void _dump(const he::Symbols::TypeNode& sym, int level=0)
{
	const he::Symbols::TypeInfo& ts = sym.item;
	XTRACE(XPDIAG1, "%*s[%d] Struct %s (%s)", level*4, " ", level, AnsiString(ts.ItemName).c_str(), AnsiString(ts.ItemType).c_str());
	// we don't support optional elements at the moment, so simply skip header!
	for (int i = 0; i < sym.children.size(); i++) {
		const he::Symbols::TypeNode& tn = sym.children[i];
		const he::Symbols::TypeInfo& ti = tn.item;
		int iCount = 1;
		if (ti.DataType.isArray) {
			XTRACE(XPDIAG1, "%*s    (%d) %s(%d) Name=%s Array[] ", level*4, " ", i, AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type, AnsiString(ti.ItemName).c_str());
		}
		for (int iSeq = 0; iSeq < iCount; iSeq++) {
			if (ti.DataType.isStruct)
			{
				// Struct field?
				_dump(tn, level+1);
			}
			else {
				// Plain field
				if (!ti.DataType.isArray) {
					XTRACE(XPDIAG1, "%*s    (%d) %s(%d) len=%d, Name=%s", level*4, " ", i, AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type, ti.DataSize, AnsiString(ti.ItemName).c_str());
				} else {
					XTRACE(XPDIAG1, "%*s        [%d]", level*4, " ", iSeq);
				}
			}
		}
	}
}

int _dump(const he::Symbols::TypeNode& sym, const uint8_t* pBuf, int offset = 0, int level = 0)
{
	const he::Symbols::TypeInfo& ts = sym.item;
	XTRACE(XPDIAG1, "%04d: %*s[%d] Struct %s (%s)", offset, level*4, " ", level,
		AnsiString(ts.ItemName).c_str(), AnsiString(ts.ItemType).c_str());
	// we don't support optional elements at the moment, so simply skip header!
	offset = offset + ts.Offset;
	const uint8_t* pBegin = pBuf;
	pBuf = pBuf + offset;
	for (int i = 0; i < sym.children.size(); i++) {
		const he::Symbols::TypeNode& tn = sym.children[i];
		const he::Symbols::TypeInfo& ti = tn.item;
		int iCount = 1;
		if (ti.DataType.isArray) {
			iCount = *((uint32_t*)pBuf);
			offset += 4;
			pBuf += 4;
			XTRACE(XPDIAG1, "%04d: %*s    (%d) %s(%d) Name=%s Array[%d] ",
				offset, level*4, " ", i,
				AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
				AnsiString(ti.ItemName).c_str(), iCount);
		}
		for (int iSeq = 0; iSeq < iCount; iSeq++) {
			if (ti.DataType.isStruct)
			{
				// Struct field?
				int l = _dump(tn, pBegin, offset, level+1);
				offset = l;
				pBuf = pBegin + offset;
			}
			else {
				// Plain field
				AnsiString sVal;
				int iLen = 0;
				switch(ti.DataType.type) {
				case he::Symbols::TypeInfo::Type::T_Bool8: iLen = 1; sVal.printf("%s", *pBuf?"true":"false"); break;
				case he::Symbols::TypeInfo::Type::T_UInt8: iLen = 1; sVal.printf("%02Xh", *pBuf); break;
				case he::Symbols::TypeInfo::Type::T_SInt8: iLen = 1; sVal.printf("%02Xh", *pBuf); break;
				case he::Symbols::TypeInfo::Type::T_UInt16: iLen = 2; sVal.printf("%04Xh", *((uint16_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_SInt16: iLen = 2; sVal.printf("%04Xh", *((uint16_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_UInt32: iLen = 4; sVal.printf("%08Xh", *((uint32_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_SInt32: iLen = 4; sVal.printf("%08Xh", *((uint32_t*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_UInt64: iLen = 8; sVal = "u64"; break;
				case he::Symbols::TypeInfo::Type::T_SInt64: iLen = 8; sVal = "s64"; break;
				case he::Symbols::TypeInfo::Type::T_Float: iLen = 4; sVal.printf("%f", *((float*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_Double: iLen = 8; sVal.printf("%lf", *((double*)pBuf)); break;
				case he::Symbols::TypeInfo::Type::T_StringL4: iLen = *((uint32_t*)pBuf); sVal = AnsiString((char*)&pBuf[4], iLen); iLen += 4; break;
				case he::Symbols::TypeInfo::Type::T_StringFix: iLen = *((uint32_t*)pBuf); sVal = AnsiString((char*)&pBuf[4], iLen); iLen += 4; break;
				case he::Symbols::TypeInfo::Type::T_DateTime:
				case he::Symbols::TypeInfo::Type::T_Guid:
				case he::Symbols::TypeInfo::Type::T_ByteString:
					break;
				}
				if (!ti.DataType.isArray) {
					XTRACE(XPDIAG1, "%04d: %*s    (%d) %s(%d) len=%d, Name=%s Val=%s",
						offset, level*4, " ", i,
						AnsiString(ti.ItemType).c_str(), (uint32_t)ti.DataType.type,
						iLen, AnsiString(ti.ItemName).c_str(), sVal.c_str());
				} else {
					XTRACE(XPDIAG1, "%04d: %*s        [%d] Val=%s",
						offset, level*4, " ", iSeq, sVal.c_str());
				}
				offset = offset + iLen;
				pBuf = pBuf + iLen;
			}
		}
	}
	return offset;
}

static void _pushTypeDef(lua_State* L, const he::Symbols::TypeInfo& ti)
{
	if (!ti.isValid()) {
        return;
	}
	lua_pushstring(L, "_type");         // parent type
	lua_pushstring(L, ti.ItemType.c_str());
	lua_settable(L, -3);
	lua_pushstring(L, "_typeid");
	lua_pushinteger(L, ti.DataType.raw);
	lua_settable(L, -3);
	if (ti.DataType.isStruct) {
		lua_pushstring(L, "_typeid");
		lua_pushinteger(L, ti.DataType.raw);
		lua_settable(L, -3);
	}
	lua_pushstring(L, "_flags");
	lua_pushinteger(L, ti.Flags.raw);
	lua_settable(L, -3);
	lua_pushstring(L, "_offset");
	lua_pushinteger(L, ti.Offset);
	lua_settable(L, -3);
}
// simple type definition dump
// TODO: instead of <name> = <type>, dump <name> = {type=type, options=options, ... }
static void _getTypeDef(lua_State* L, const he::Symbols::TypeNode& node, bool isRoot = false)
{
	const he::Symbols::TypeInfo& ts = node.item;
	lua_pushstring(L, ts.ItemName.c_str());
	lua_newtable(L);
	_pushTypeDef(L, ts);
	// we don't support optional elements at the moment, so simply skip header!
	int iChildCount = node.children.size();
	for (int i = 0; i < iChildCount; i++) {
		const he::Symbols::TypeNode& tn = node.children[i];
		const he::Symbols::TypeInfo& ti = tn.item;
		int iCount = 1;
		if (ti.DataType.isArray) {
			// NOT supported
		}
		for (int iSeq = 0; iSeq < iCount; iSeq++) {
			if (ti.DataType.isStruct)
			{
				// Struct field? Recurse...
				_getTypeDef(L, tn);
			}
			else {
				// Plain field
				// TODO: add this as children[i] to keep the typedef order
				lua_pushstring(L, ti.ItemName.c_str());
				lua_newtable(L);
				_pushTypeDef(L, ti);
				lua_settable(L, -3);
			}
		}
	}
	// finish table
	if (!isRoot) {
		lua_settable(L, -3);
	}
}

// Get the type definition as LUA table
// NOTE: this returns an empty table, if the node is empty (no symbol definition available)
int Serializer::GetTypeDef(lua_State* L, const he::Symbols::TypeNode& node)
{
	_getTypeDef(L, node, true);
//	lua_pushnil(L);
//	lua_pushstring(L, "not yet implemented!");
//    return 2;
	return 1;
}


int Serializer::Dump(const he::Symbols::TypeNode& node)
{
	_dump(node);
}


int Serializer::Dump(const he::Symbols::TypeNode& node, const uint8_t* pBuf)
{
	_dump(node, pBuf);
}


#if 0
/**
 * @function ctx:read_cyclic_io
 * @param type (default = 0/nil/raw data (bytestring), 1 = lua table with deserialized data, 2 = type definition only)
 * @return data depending on type given (or nil, error)
 */
int ctx_read_cyclic_io(lua_State *L)
{
	ctx_t *ctx = ctx_check(L, 1);
	int iType = luaL_optinteger(L, 2, 0);
	TADS_IOThread* pThread = (TADS_IOThread*)ctx->hThread;
	if (pThread == NULL) {
		return msg_to_nil_error(L, "IO thread not initialized!");
	}
	if (!pThread->IsCyclicIoRunning()) {
		return msg_to_nil_error(L, "Cyclic IO not running yet!");
	}

	// get symbol info (and serialize eventually)
	const TADS_IOThread::SymInfo& siRd = pThread->GetSymInfoRd();
	const TADS_IOThread::SymInfo& siWr = pThread->GetSymInfoWr();

	// get the current data
	int iRawLen = 0;
	uint8_t* pRawBuf = NULL;

	switch (iType) {
	case 0: // read raw input data as bytestring
		iRawLen = siRd.SymEntry->size;
		pRawBuf = new uint8_t[iRawLen];
		pThread->GetInputs(pRawBuf, iRawLen);
		lua_pushlstring(L, pRawBuf, iRawLen);
		delete[] pRawBuf;
		return 1;
	case 1: // read input, deserialize and return as LUA table
		iRawLen = siRd.SymEntry->size;
		pRawBuf = new uint8_t[iRawLen];
		pThread->GetInputs(pRawBuf, iRawLen);
		lua_newtable(L);
		PushType(L, siRd.Type, pRawBuf, true);
		delete[] pRawBuf;
		return 1;
	case 2: // return input data type definition as LUA table
		lua_newtable(L);
		PushType(L, siRd.Type, NULL, true);
		return 1;

	case 3: // readback output as bytestring
		iRawLen = siWr.SymEntry->size;
		pRawBuf = new uint8_t[iRawLen];
		pThread->GetOutputs(pRawBuf, iRawLen);      // readback!
		lua_pushlstring(L, pRawBuf, iRawLen);
		delete[] pRawBuf;
		return 1;
	case 4: // readback output, deserialize and return as LUA table
		iRawLen = siWr.SymEntry->size;
		pRawBuf = new uint8_t[iRawLen];
		pThread->GetOutputs(pRawBuf, iRawLen);      // readback!
		lua_newtable(L);
		PushType(L, siWr.Type, pRawBuf, true);
		delete[] pRawBuf;
		return 1;
	case 5: // return output data type definition as LUA table
		lua_newtable(L);
		PushType(L, pThread->GetSymInfoWr().Type, NULL, true);
		return 1;
	default:
		return msg_to_nil_error(L, "Unknown type code!");
	}
}

/**
 * @function ctx:write_cyclic_io
 * @param value as a lua binary string
 * @usage usually called as:
 *           ctx:write_cyclic_io(data)
 * @return number of bytes copied or nil,error
 */
int ctx_write_cyclic_io(lua_State *L)
{
	ctx_t *ctx = ctx_check(L, 1);
	int t = lua_type(L, 2);

	TADS_IOThread* pThread = (TADS_IOThread*)ctx->hThread;
	if (pThread == NULL) {
		return msg_to_nil_error(L, "IO thread not initialized!");
	}
	if (!pThread->IsCyclicIoRunning()) {
		return msg_to_nil_error(L, "Cyclic IO not running yet!");
	}

	if (t == LUA_TSTRING) {
		// write the binary string value
		size_t cnt = 0;
		const uint8_t* s = lua_tolstring(L, 2, &cnt);
		int ret = pThread->SetOutputs(s, cnt);
		lua_pushinteger(L, ret);
		return 1;
	}
	if (t != LUA_TTABLE) {
		return msg_to_nil_error(L, "Unsupported data provided (use table or string)!");
	}

	// Serialize the given table
	const TADS_IOThread::SymInfo& siWr = pThread->GetSymInfoWr();
	int iRawLen = siWr.SymEntry->size;;
	uint8_t* pRawBuf = new uint8_t[iRawLen];

	// move the table to the TOS
	lua_pushvalue(L, 2);
	Serialize(L, siWr.Type, pRawBuf, true);
	lua_pop(L, 1);                              // remove the table from the top again
	int ret = pThread->SetOutputs(pRawBuf, iRawLen);
    delete[] pRawBuf;
	lua_pushinteger(L, ret);
	return 1;
}
#endif

static void _dump_table(lua_State *L, int index, int level)
{
	int t = lua_type(L, index);
	if (t != LUA_TTABLE) {
		return;
	}

	// Push another reference to the table on top of the stack (so we know
	// where it is, and this function can work for negative, positive and
	// pseudo indices
	lua_pushvalue(L, index);
	// stack now contains: -1 => table
	lua_pushnil(L);
	// stack now contains: -1 => nil; -2 => table
	while (lua_next(L, -2))
	{
		// stack now contains: -1 => value; -2 => key; -3 => table
		// copy the key so that lua_tostring does not modify the original
		lua_pushvalue(L, -2);
		// stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
		const char *key = lua_tostring(L, -1);
		int iType = lua_type(L, -2);
		switch (iType) {
		case LUA_TTABLE:
			XTRACE(XPDIAG2, "%*s %s = {\n", level*4, " " , key);
			_dump_table(L, -2, level+1);
			XTRACE(XPDIAG2, "%*s }\n", level*4, " ");
			break;
		case LUA_TNUMBER: {
			double value = lua_tonumber(L, -2);
			XTRACE(XPDIAG2, "%*s %s => %f\n", level*4, " " , key, value);
		} break;
		case LUA_TBOOLEAN: {
			bool value = lua_tointeger(L, -2);
			XTRACE(XPDIAG2, "%*s %s => %s\n", level*4, " " , key, value ? "true" : "false");
		} break;
		default: {
			const char *value = lua_tostring(L, -2);
			XTRACE(XPDIAG2, "%*s %s => %s\n", level*4, " " , key, value ? value : "(nil)");
		} break;
        }
		// pop value + copy of key, leaving original key
		lua_pop(L, 2);
		// stack now contains: -1 => key; -2 => table
	}
	// stack now contains: -1 => table (when lua_next returns 0 it pops the key
	// but does not push anything.)
	// Pop table
	lua_pop(L, 1);
	// Stack is now the same as it was on entry to this function
}

void Serializer::DumpTable(lua_State *L, int index)
{
    _dump_table(L, index, 0);
}

