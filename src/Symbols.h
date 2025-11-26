//---------------------------------------------------------------------------
#ifndef SymbolsH
#define SymbolsH
//---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <map>
//---------------------------------------------------------------------------

namespace he {
namespace Symbols {

class TypeDB;
/*
typedef struct
{
	uint32_t		entryLength;	// length of complete datatype entry
	uint32_t		version;		// version of datatype structure
	union {
	uint32_t		hashValue;		// hashValue of datatype to compare datatypes
	uint32_t		offsGetCode;	// code offset to getter method
	};
	union {
	uint32_t		typeHashValue;	// hashValue of base type
	uint32_t		offsSetCode;	// code offset to setter method
	};
	uint32_t		size;			// size of datatype ( in bytes )
	uint32_t		offs;			// offs of dataitem in parent datatype ( in bytes )
	uint32_t		dataType;		// adsDataType of symbol (if alias)
	uint32_t		flags;			//
	uint16_t		nameLength;		// length of datatype name (excl. \0)
	uint16_t		typeLength;		// length of dataitem type name (excl. \0)
	uint16_t		commentLength;	// length of comment (excl. \0)
	uint16_t		arrayDim;		//
	uint16_t		subItems;		//
	// ADS_INT8		name[];			// name of datatype with terminating \0
	// ADS_INT8		type[];			// type name of dataitem with terminating \0
	// ADS_INT8		comment[];		// comment of datatype with terminating \0
	// AdsDatatypeArrayInfo	array[];
	// AdsDatatypeEntry		subItems[];
	// GUID			typeGuid;		// typeGuid of this type if ADSDATATYPEFLAG_TYPEGUID is set
	// ADS_UINT8	copyMask[];		// "size" bytes containing 0xff or 0x00 - 0x00 means ignore byte (ADSIGRP_SYM_VALBYHND_WITHMASK)
} AdsDatatypeEntry, *PAdsDatatypeEntry, **PPAdsDatatypeEntry;
#define	PADSDATATYPENAME(p)			((PCHAR)(((PAdsDatatypeEntry)p)+1))
#define	PADSDATATYPETYPE(p)			(((PCHAR)(((PAdsDatatypeEntry)p)+1))+((PAdsDatatypeEntry)p)->nameLength+1)
#define	PADSDATATYPECOMMENT(p)		(((PCHAR)(((PAdsDatatypeEntry)p)+1))+((PAdsDatatypeEntry)p)->nameLength+1+((PAdsDatatypeEntry)p)->typeLength+1)
#define	PADSDATATYPEARRAYINFO(p)	(PAdsDatatypeArrayInfo)(((PCHAR)(((PAdsDatatypeEntry)p)+1))+((PAdsDatatypeEntry)p)->nameLength+1+((PAdsDatatypeEntry)p)->typeLength+1+((PAdsDatatypeEntry)p)->commentLength+1)
*/
class TypeInfo       // the per-symbol info
{
public:
	enum class Type : uint32_t {
/*// Masks
		MaskStruct		= 0x80000000,
		MaskArray		= 0x40000000,
		MaskFields		= 0x0000FFFF,*/
		// Struct types
		S_StructFixed   = 1,
		S_StructOptFld 	= 2,
		// Integral types
		T_Undefined     = 0,
		T_Bool8,
		T_UInt8,
		T_SInt8,
		T_UInt16,
		T_SInt16,
		T_UInt32,
		T_SInt32,
		T_UInt64,
		T_SInt64,
		T_Float,
		T_Double,
		//T_StringZ,        // zero terminated variable length string
		T_StringL4,        	// length prefixed (4-byte) string
		T_StringFix,
		T_DateTime,
		T_Guid,
        T_ByteString,
	};
public:
	TypeInfo();
	//int               t;          // 0 = native, 1 = structure, 2 = array, 3 structure with optional fields
	//Type            	DataType;   // 0x80000000 Flag für structure, 0x40000000 Flag für Array
	union {
		struct {
			uint32_t	isArray 	: 1;   	// 0x00000001
			uint32_t    isStruct    : 1;   	// 0x00000002
			Type        type : 30;
		};
		uint32_t 		raw;
	} DataType;
    // TODO: add MaxSize?
	uint32_t            DataSize;   		// For fixed size data items
	// ValueRank: see https://reference.opcfoundation.org/Core/Part3/v104/docs/5.6.2
	int                 ValueRank;  		// Type of the variable: Scalar(-1), Any(-2), ScalarOrOneDimension(-3), OneOrMoreDimensions(0), OneDimension(1), >= 1 array with specified number of dimesions
	std::vector<int>    ArrayDimensions;    // empty, if ValueRank <= 0, else max. supported length per dimension (0 if unknown)
	union {
		struct {
			uint32_t 	isOptional 	: 1;
			uint32_t 	hasOffset 	: 1;
			uint32_t    reserved : 30;
		} Bits;
		uint32_t        raw;
	} Flags;
	// TODO: replace with std::string to make it more portable!
	std::string  		ItemName;
	std::string  		ItemType;
	std::string  		ItemEncoding;
	//String              nameNative, nameBrowse, nameDisplay;
	int                 Offset;   	// Absolute offset (if available)
	//CAdsSymbolInfo 		Info;
	//PAdsDatatypeEntry 	Entry;
	bool isValid() const;
	bool IsRoot() const;
	void Set(Type t, const std::string& ItemName, const std::string& ItemType, int DataSize = -1);
};

class TypeNode       // the symbol "tree"
{
public:
	TypeNode();
	TypeNode(const TypeInfo& i);
	//TypeNode(const PAdsDatatypeEntry p, int Offset);
	TypeInfo             	item;
	std::vector<TypeNode> 	children;
	void Clear();
	const char* GetItemName() { return item.ItemName.c_str(); }
	void Set(const TypeDB* pDB, const TypeInfo& i);
	TypeNode& AddChild(const TypeDB* pDB, const TypeInfo& i, int Offset);
	//void Set(const TypeDB* pDB, const TypeInfo& i, bool Recursive = false);
	//void Set(const TypeDB* pDB, const PAdsDatatypeEntry p, int Offset, bool Recursive = false);
	//void AddChild(const TypeDB* pDB, PAdsDatatypeEntry p, int Offset, bool Recursive);
	//void AddArray(const TypeDB* pDB, PAdsDatatypeEntry p, int Offset, bool Recursive);
private:
	//void AddSubitems(const TypeDB* pDB, PAdsDatatypeEntry pEntry, int Offset, bool Recursive);
};

class TypeDB
{
public:
	TypeDB();
	//TypeDB(void* pDatatypes, size_t nDTSize);
	//~TypeDB();
	//void Init(void* pDatatypes, size_t nDTSize);
	//const PAdsDatatypeEntry FindTypeByName(const char* name) const;
	bool HasTypeByName(const std::string& ItemType);
	const TypeNode& FindTypeByName(const std::string& ItemType);
	void Add(const std::string& varName, const TypeNode& node);
	void Clear();

	typedef std::map<std::string, std::string> tVariableMap;    // map variable name to type name
	typedef std::map<std::string, TypeNode> tNameMap;           // map type name to type definition
	const tNameMap& GetTypeMap() { return _types; };
private:
	//PBYTE							m_pDatatypes;
	//std::vector<PAdsDatatypeEntry>  m_vDatatypeArray;
	tNameMap _types;
    tVariableMap _vars;
};

}; // namespace Symbols
}; // namespace he

#endif
