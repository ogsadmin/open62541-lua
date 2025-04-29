//---------------------------------------------------------------------------

#pragma hdrstop

#include "Symbols.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)

using namespace he::Symbols;

//PAdsDatatypeEntry AdsDatatypeStructItem(PAdsDatatypeEntry p, unsigned short iItem);

// -----------------------------------------------------------------------------
TypeDB::TypeDB()
//: m_pDatatypes(NULL)
{
}

TypeDB::TypeDB(void* pDatatypes, size_t nDTSize)
{
	Init(pDatatypes, nDTSize);
}

TypeDB::~TypeDB()
{
	//if (m_pDatatypes)  delete[] m_pDatatypes;
}

void TypeDB::Init(void* pDatatypes, size_t nDTSize)
{    /*
	m_pDatatypes = new BYTE[nDTSize+sizeof(ULONG)];
	if (!m_pDatatypes) {
		throw "EOutOfMemory";
	}
	memcpy(m_pDatatypes, pDatatypes, nDTSize);
	*(PULONG)&m_pDatatypes[nDTSize] = 0;
	UINT offs = 0;
	while (*(PULONG)&m_pDatatypes[offs])
	{
		m_vDatatypeArray.push_back((PAdsDatatypeEntry)&m_pDatatypes[offs]);
		offs += *(PULONG)&m_pDatatypes[offs];
	}*/
}
/*
const PAdsDatatypeEntry TypeDB::FindTypeByName(const char* name) const
{
	for (int j = 0; j < m_vDatatypeArray.size(); j++) {
		PAdsDatatypeEntry p = m_vDatatypeArray[j];
		if (!strcmp(PADSDATATYPENAME(p), name)) {
			return p;
		}
	}
	return NULL;
}
*/
const TypeNode& TypeDB::FindTypeByName(const char* name) const
{
	TypeNode node;
	return node;
}
// -----------------------------------------------------------------------------
// the per-symbol info

TypeInfo::TypeInfo()
: Offset(0)
{
	DataType.raw = 0;
	Flags.raw = 0;
}

bool TypeInfo::IsRoot() const
{
	return (DataType.type != Type::T_Undefined) && DataSize == 0;
}

bool TypeInfo::isValid() const
{
	return (this->DataType.raw != 0);
}

void TypeInfo::Set(TypeInfo::Type t, const UTF8String& ItemName, const UTF8String& ItemType, int DataSize)
{
	this->ItemName      = ItemName;
	this->ItemType 		= ItemType;
	this->DataType.type = t;
	if (DataSize == -1) {
		switch(t) {
		case TypeInfo::Type::T_Undefined:
		case TypeInfo::Type::T_StringL4:
		case TypeInfo::Type::T_StringFix:
		case TypeInfo::Type::T_ByteString:  this->DataSize = 0; break;
		case TypeInfo::Type::T_Bool8:
		case TypeInfo::Type::T_UInt8:
		case TypeInfo::Type::T_SInt8:       this->DataSize = 1; break;
		case TypeInfo::Type::T_UInt16:
		case TypeInfo::Type::T_SInt16:      this->DataSize = 2; break;
		case TypeInfo::Type::T_UInt32:
		case TypeInfo::Type::T_SInt32:
		case TypeInfo::Type::T_Float:       this->DataSize = 4; break;
		case TypeInfo::Type::T_UInt64:
		case TypeInfo::Type::T_SInt64:
		case TypeInfo::Type::T_Double:      this->DataSize = 8; break;
		case TypeInfo::Type::T_Guid:        this->DataSize = 16; break;
		case TypeInfo::Type::T_DateTime:
		default:							this->DataSize = 0; break;
		}
	}
}

// -----------------------------------------------------------------------------
// the symbol "tree"
TypeNode::TypeNode()
{
}

TypeNode::TypeNode(const TypeInfo& i)
{
	Set(NULL, i);
}

void TypeNode::Clear()
{
	children.clear();
	TypeInfo tmp;
    item = tmp;
}

void TypeNode::Set(const TypeDB* pDB, const TypeInfo& i)
{
	item = i;
}
TypeNode& TypeNode::AddChild(const TypeDB* pDB, const TypeInfo& i, int Offset)
{
	TypeNode tmp(i);
	children.push_back(tmp);
	TypeNode& node = children[children.size()-1];
	return node;
}
#if 0
void TypeNode::AddArray(const TypeDB* pDB, PAdsDatatypeEntry p, int Offset, bool Recursive)
{
/*
	int cnt = 1;
	PAdsDatatypeArrayInfo pAI = PADSDATATYPEARRAYINFO(pEntry);
	for (USHORT i = 0; i < pEntry->arrayDim; i++) {
		cnt *= pAI[i].elements;
	}
	for (int j = 0; j < cnt; j++) {
		TcxTreeListNode* pNew = pNode->AddChild();
#if ADD_RECURSIVELY
		SetNodeData(pNew, pAI[j]);
#endif
	}
*/
}
#endif



