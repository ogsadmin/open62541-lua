//---------------------------------------------------------------------------

#ifndef OpcUA_IOThreadH
#define OpcUA_IOThreadH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
#include <open62541.h>
#include <string>
#include "IOThread_Params.h"
#include "Symbols.h"
//---------------------------------------------------------------------------
class TOpcUA_IOThread : public TThread
{
protected:
	void __fastcall Execute();
	void InitConnection();
public:
	__fastcall TOpcUA_IOThread(IOThread_Params* params);
	void Init(
		const char* endpoint_url,
		int ns,         		// namespace
		const char* wrNode,     // node name to use for writing to OPC UA server
		const char* wrEncoding,
		const char* rdNode,     // node name to use for reading from OPC UA server
		const char* rdEncoding,
		DWORD cycleMs,          // read/write cycle time
		const char* username = "",
		const char* password = ""
	);
	bool IsCyclicIoRunning();
	UA_StatusCode SetOutputs(const UA_ByteString *newValue);
	UA_StatusCode GetInputs(UA_ByteString *newValue);
	UA_StatusCode GetOutputs(UA_ByteString *newValue);
	void GetClientState(UA_SecureChannelState* chn_s, UA_SessionState* ss_s, UA_StatusCode* sc);
	const he::Symbols::TypeNode& GetSymDefRd();
	const he::Symbols::TypeNode& GetSymDefWr();
    const he::Symbols::TypeDB& GetDB() { return _typeDB; }              // the cache for the OPC-UA types

	class Stats {
	public:
		Stats() : cntCyclesTotal(0), cntCyclesCurrent(0), cntReconnects(0), msCycle(0)  {}
		TDateTime   tStarted;
		TDateTime   tLastConnected;
		uint32_t   	cntCyclesTotal;
		uint32_t	cntCyclesCurrent;
		uint32_t	cntReconnects;
		uint32_t    msCycle;
	};
	void GetStats(Stats& stats) {
		stats = _stats;
	}

private:
	he::Symbols::TypeDB _typeDB;              // the cache for the OPC-UA types
	class CyclicNode {
	public:
		CyclicNode() {
			UA_NodeId_init(&nidNodeId);
			UA_NodeId_init(&nidDataType);
			UA_NodeId_init(&nidEncoding);
			UA_NodeClass_init(&nidNodeClass);
			UA_Variant_init(&varInitVal);
		}
		~CyclicNode() {
			UA_NodeId_clear(&nidNodeId);
			UA_NodeId_clear(&nidDataType);
			UA_NodeId_clear(&nidEncoding);
			UA_NodeClass_clear(&nidNodeClass);
			UA_Variant_clear(&varInitVal);
		}
		void Init(int ns, const char* name, const char* encoding = "") {
			Namespace = ns;
			Name = name;
			Encoding = encoding;
		}
		int                 Namespace;
		std::string         Name;
		std::string         Encoding;
		UA_NodeId 			nidNodeId;          // the variable nodeID (to read/write)
		UA_NodeId 			nidDataType;        // the data type nodeID
		UA_NodeId 			nidEncoding;        // the encoding nodeID
		UA_NodeId 			ExpandedNodeId;     // The BLOB encoding as returned from the "ReadExtensionObject" initial call
		UA_NodeClass 		nidNodeClass;       // the variable nodeID (to read/write)
		UA_Variant          varInitVal;         // initial value (read to get size of extension objects)
		int                 InitialReadLength;
		he::Symbols::TypeNode SymbolDef;
	};
	UA_Client* 			_client;
    IOThread_Params*    _params;
	CRITICAL_SECTION 	_cs;
	int                 _state, _old_state;
	std::string         _url, _user, _pass;
	DWORD               _tCycleMs, _tLastRW;
	CyclicNode          _wr, _rd;
	UA_ByteString       _varWr, _varRd;
	UA_StatusCode       _wrStatus, _rdStatus, _oldConnectStatus;
	Stats               _stats;
	DWORD               _statsTicker;
	uint32_t            _statsLastCycles;
    UA_StatusCode       _lasterr;
	DWORD               _stateTicker;
    int                 _connectRetries;
	void StateMachine();
	void ThreadSleep(DWORD ms);
	void InitClientConfig();
	UA_StatusCode readExtensionObjectValue(const UA_NodeId nodeId, UA_Variant *outValue, UA_NodeId* pExpandedNodeId);
	UA_StatusCode writeExtensionObjectValue(const UA_NodeId nodeId, const UA_NodeId& dataTypeNodeId, const UA_ByteString *newValue);
	UA_StatusCode initCyclicInfo(TOpcUA_IOThread::CyclicNode& cycNode);
	UA_StatusCode readwriteCyclic();
	UA_StatusCode readStructureDefinition(UA_NodeId& nidNodeId, const std::string& Name, he::Symbols::TypeNode& sym, int offset = 0, int level = 0);
	UA_StatusCode readNodeNames(UA_NodeId& nidNodeId, String& nameBrowse, String& nameDisplay);
	UA_ClientConfig 	_origUserConfig;
	static void clientStateChangeTrampoline(
		UA_Client* client,
		UA_SecureChannelState channelState,
		UA_SessionState sessionState,
		UA_StatusCode connectStatus);
	void clientStateCallback(UA_Client *client, UA_SecureChannelState channelState,
						  UA_SessionState sessionState, UA_StatusCode connectStatus);
};
//---------------------------------------------------------------------------
#endif
