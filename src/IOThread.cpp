//---------------------------------------------------------------------------

#include <System.hpp>
#pragma hdrstop

#include "IOThread.h"
#include "logger.h"
#include "read_file.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(&UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TOpcUA_IOThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
extern bool gDllUnloadInProgress;

__fastcall TOpcUA_IOThread::TOpcUA_IOThread(IOThread_Params* params)
	: TThread(true), _params(params), _client(NULL), _state(0), _oldConnectStatus(0) // always create suspended
{
	XTRACE(XPDIAG2, "OPC-UA IOThread instantiated");
	InitializeCriticalSection(&_cs);
}
//---------------------------------------------------------------------------
// This function is called to create a OPC-UA client connection context
void TOpcUA_IOThread::InitClientConfig()
{
	if (_client != NULL) {
		UA_Client_delete(_client);
		_client = NULL;
	}
	//_stateCallbackNew = (UA_ClientState)-1;
	_client = UA_Client_new();
	if (!_client) {
		XTRACE(XPFATAL, "InitClientConfig::OUT OF MEMORY!");
		printf("Initialize UA_Client failure!!!");
		// OUT OF MEMORY! This should NEVER EVER happen!!!
		exit(-1);
	}
	UA_ClientConfig* cc = UA_Client_getConfig(_client);
	_params->UpdateConfig(cc);
/*
	UA_ClientConfig_setDefault(cc);
	_config = new UA_ClientConfig_Proxy(cc);
	_ioThread = new TOpcUA_IOThread(_client, params);
*/
	// We also get the client configuration and modify it to link our
	// local callbacks
	_origUserConfig = *cc;
	// Wire up the callbacks
	cc->clientContext = this;
	cc->stateCallback = &TOpcUA_IOThread::clientStateChangeTrampoline;
	//cc->subscriptionInactivityCallback = &UA_Client_Proxy::subscriptionInactivityCallback;
}
void TOpcUA_IOThread::GetClientState(
	UA_SecureChannelState* chn_s, UA_SessionState* ss_s, UA_StatusCode* sc)
{
	UA_Client_getState(_client, chn_s, ss_s, sc);
}

void __fastcall TOpcUA_IOThread::Execute()
{
	XTRACE(XPDIAG2, "OPC-UA IOThread instantiated");
	NameThreadForDebugging(System::String(L"OpcUA_IOThread"));
	_stats.tStarted = Now();
	//---- Place thread code here ----
	while (!Terminated && !gDllUnloadInProgress) {
		try {
			StateMachine();
		}
		catch (Exception& e) {

		}
		catch (...) {

		}
	}
}
void TOpcUA_IOThread::ThreadSleep(DWORD ms)
{
	DWORD dtTick = GetTickCount();
	while (GetTickCount() - dtTick < ms) {
		Sleep(10);
		if (Terminated) return;
	}
}

void TOpcUA_IOThread::clientStateChangeTrampoline(UA_Client* client,
	UA_SecureChannelState channelState,
	UA_SessionState sessionState,
	UA_StatusCode connectStatus)
{
	UA_ClientConfig* cc = UA_Client_getConfig(client);
	TOpcUA_IOThread* pThread = (TOpcUA_IOThread*)cc->clientContext;
	if (pThread->clientStateCallback) {
		// TODO: hier auch den client->Config->clientContext korrigieren, sonst
        //       wird das �ber crashen!
		pThread->clientStateCallback(client, channelState, sessionState, connectStatus);
	}
}

void TOpcUA_IOThread::clientStateCallback(UA_Client *client, UA_SecureChannelState channelState,
						  UA_SessionState sessionState, UA_StatusCode connectStatus)
{
	if (_state == 30) {
		// We are in cyclic IO. Check for disconnects.
//		printf("State change cb: chn=%08Xh, ses=%08Xh, con=%08Xh\n", channelState, sessionState, connectStatus);
	}
	if (_origUserConfig.stateCallback) {
		// Call user handler, too (if any)
		_origUserConfig.stateCallback(client, channelState, sessionState, connectStatus);
	}
}
void TOpcUA_IOThread::StateMachine()
{
	if (_state >= 10 && _state != 30 && _client != NULL) {
		// cyclic IO is not currently active - in this case timing is not relevant
		// anyway, so poll the UA client --> this allows handling connection tasks
		// properly.
		// poll the OPC-UA state machine...
		// --> see https://www.open62541.org/doc/1.3/client.html#client
		// --> and https://www.open62541.org/doc/1.3/client.html#client-async-services
		UA_StatusCode connectStatus = UA_Client_run_iterate(_client, 100/*ms*/);
	}

	int old_state = _state;
	switch(_state) {
	case 0:  // Idle. Wait until we are initialized.
		break;

	case 1: // initialized, now create an UA Client object
		InitClientConfig();
		_state = 10;
		break;

	case 10: { // "Connecting": Initialized, try to setup and start.
		// Do a blocking connect.
		XTRACE(XPDIAG2, "%s: Starting to connect...", _url.c_str());
		_lasterr = 0;
		if (this->_user.length() == 0) {
			_lasterr = UA_Client_connect(_client, _url.c_str());
		} else {
			_lasterr = UA_Client_connectUsername(_client, _url.c_str(), _user.c_str(), _pass.c_str());
		}
		if(_lasterr != UA_STATUSCODE_GOOD) {
			XTRACE(XPERRORS, "%s: Connect failed. Retcode=%d (%08Xh), Msg=%s", _url.c_str(), _lasterr, _lasterr, UA_StatusCode_name(_lasterr));
			_state = 99;
		}
		else {
			_state = 20;
		}
	}
	break;

	case 20: // Connected, reading node IDs
		XTRACE(XPDIAG2, "%s: Connected, reading type definitions...", _url.c_str());
		_stats.cntReconnects++;
		_stats.cntCyclesCurrent = 0;
		_statsLastCycles = 0;
		// get the write node info
		_lasterr = initCyclicInfo(_wr);
		if (UA_STATUSCODE_GOOD != _lasterr) {
			// Some error occurred.
			_state = 99;
			break;
		}
		// get the read node info
		_lasterr = initCyclicInfo(_rd);
		if (UA_STATUSCODE_GOOD != _lasterr) {
			// Some error occurred.
			_state = 99;
			break;
		}
		_connectRetries = 0;
		_state = 30;
//		// init write value
		_varWr = *(UA_ByteString*)_wr.varInitVal.data;
		_stats.tLastConnected = Now();
		_statsTicker = GetTickCount();
		break;

	case 30: {
		// Do the cyclic IO. Ignore any errors for now.
		_stats.msCycle = GetTickCount() - _tLastRW;
		_tLastRW = GetTickCount();
		_lasterr = readwriteCyclic();
		if (UA_STATUSCODE_GOOD != _lasterr) {
			// Read/write failed. Disconnect and reconnect...
			XTRACE(XPERRORS, "%s: Error reading/writing: %08Xh (%s)", _url.c_str(), _lasterr, UA_StatusCode_name(_lasterr));
			_state = 99;
			break;
		}
		if (_stats.cntCyclesCurrent == 0) {
			XTRACE(XPDIAG1, "%s: Cyclic IO running.", _url.c_str());
		}
		_stats.cntCyclesTotal++;
		_stats.cntCyclesCurrent++;
		DWORD tIO = GetTickCount();

		if (tIO - _statsTicker > 60000) { // report connection status every minute
			int diffCycles = _stats.cntCyclesCurrent - _statsLastCycles;
			int diffTime   = tIO - _statsTicker;
			if (diffTime > 0) {
				int msCycle = 0;
				if (diffCycles > 0) {
					msCycle = 60000 / diffCycles;
				}
				AnsiString sTotal = FormatDateTime("hh:nn:ss", Now() - _stats.tStarted);
				AnsiString sConn = FormatDateTime("hh:nn:ss", Now() - _stats.tLastConnected);
				XTRACE(XPDIAG2, "%s: OPC-UA connection stats: %d cycles/s (%d/%dms), uptime %s, connected %s", _url.c_str(),
					diffCycles*1000/diffTime, msCycle, _tCycleMs, sTotal.c_str(), sConn.c_str()
				);
			}
			_statsLastCycles = _stats.cntCyclesCurrent;
            _statsTicker = tIO;
		}

		// Warten, bis ein Zyklus durch *UND* OPC-UA Zustandsmaschine pollen
		DWORD tElapsed = GetTickCount() - _tLastRW;
		DWORD tEndTime = _tLastRW + _tCycleMs - 5;
//		while (GetTickCount() - _tLastRW < (_tCycleMs - 5))
		do
		{
			int tDelta = tEndTime - GetTickCount();
			if (tDelta > 10) tDelta = 10;
			if (tDelta < 0) tDelta = 0;
			UA_StatusCode connectStatus = UA_Client_run_iterate(_client, tDelta);
			if (_oldConnectStatus != connectStatus) {
	//			printf("Connect status : %08Xh --> %08Xh\n", _oldConnectStatus, connectStatus);
				_oldConnectStatus = connectStatus;
			}
			// Check connect status.
			if (UA_STATUSCODE_GOOD != connectStatus) {
				// Some error occurred.
				_lasterr = connectStatus;
				_state = 99;
				break;
			}
		}
		while (GetTickCount() <= tEndTime);
/*
		DWORD waitInterval = 0;
		if (tElapsed < _tCycleMs) {
			waitInterval = _tCycleMs - tElapsed;
		}
		UA_StatusCode connectStatus = UA_Client_run_iterate(_client, waitInterval);
		if (_oldConnectStatus != connectStatus) {
//			printf("Connect status : %08Xh --> %08Xh\n", _oldConnectStatus, connectStatus);
			_oldConnectStatus = connectStatus;
		}
		static DWORD dwCycles = 0;
		dwCycles++;
		DWORD tDelayTime = GetTickCount() - _tLastRW;
		int iDelta = tDelayTime - _tCycleMs;
		if (iDelta < -3 || iDelta > 3) {
			printf("Jitter: %2d/%3d (%d)\n", iDelta, tIO-_tLastRW, dwCycles);
		}
*/
	}
	break;

	case 99:
		// Some error occurred. Disconnect and retry later.
		UA_Client_disconnect(_client);
		_stateTicker = GetTickCount();
		_connectRetries++;
		_state = 900;
		break;

	case 900: {
		// wait a second - so the lib can complete its disconnect
		//UA_StatusCode connectStatus = UA_Client_run_iterate(_client, 100/*ms*/);
		UA_SecureChannelState chn_s;
		UA_SessionState ss_s;
		UA_StatusCode sc;
		UA_Client_getState(_client, &chn_s, &ss_s, &sc);
		if (chn_s == UA_SECURECHANNELSTATE_CLOSED && ss_s == UA_SESSIONSTATE_CLOSED) {
			// save to close...
			_state = 910;
		}
/*
		if (_client->connection.state > UA_CONNECTIONSTATE_CLOSED) {
			// still connected, wait a bit more...
		}
*/
/*
		if (GetTickCount() - _stateTicker > 1000) {
			_stateTicker = GetTickCount();
			_state = 910;
		}
*/
	}
	break;

	case 910:
		// --> if (the connection has an error, a new client must be created!
		// --> see https://www.open62541.org/doc/1.3/client.html#connect-to-a-server
		if (_lasterr & 0x80000000) {
			XTRACE(XPERRORS, "%s: Severe error occurred: %08Xh (%s)", _url.c_str(), _lasterr, UA_StatusCode_name(_lasterr));
			_state = 920;
		}
		else {
			// not a severe error, so reuse the client - but wait a bit more
			if (_connectRetries > 10) {
				_connectRetries = 10;       // limit to 10s retry time
			}
			if (GetTickCount() - _stateTicker > (1000*_connectRetries)) {
				XTRACE(XPDIAG1, "%s: Wait done, reconnecting", _url.c_str());
				_state = 10;
			}
		}
		break;

	case 920:
		// we had a severe error - create a new client, but wait 10 seconds!
		if (GetTickCount() - _stateTicker > 10000) {
			// recreate a new client
			XTRACE(XPWARN, "%s: Deleting OPC-UA client due to severe error.", _url.c_str());
			UA_Client_delete(_client);
			_client = 0;
			_state = 1;
			XTRACE(XPDIAG1, "%s: Recreating client and reconnecting...", _url.c_str());
		}
		break;

	case 999:
        // Dead as dead beef.
		break;
/*
	case 999:
		if (_client->connection.state > UA_CONNECTIONSTATE_CLOSED) {
			// still connected, wait a bit more...
		}
		break;
*/
	}
	if (old_state != _state) {
		XTRACE(XPDIAG2, "%s: State change --> %d (lasterr=%08Xh)", _url.c_str(), _state, _lasterr);
	}

}
//---------------------------------------------------------------------------
bool TOpcUA_IOThread::IsCyclicIoRunning()
{
	return (_state == 30);
}

//---------------------------------------------------------------------------
// NOTE: the returned value is a copy and *MUST be deallocated!
// i.e.
// - call UA_ByteString_init(newValue) *BEFORE* calling this function
// - call UA_ByteString_clear(newValue) *AFTER* calling this function
UA_StatusCode TOpcUA_IOThread::SetOutputs(const UA_ByteString *newValue)
{
	EnterCriticalSection(&_cs);
	if (_varWr.data != NULL) {
		UA_ByteString_clear(&_varWr);
    }
	UA_ByteString_copy(newValue, &_varWr);
	LeaveCriticalSection(&_cs);
	return _wrStatus;               // return the last write status
}
// NOTE: the returned value is a copy and *MUST be deallocated!
// i.e.
// - call UA_ByteString_init(newValue) *BEFORE* calling this function
// - call UA_ByteString_clear(newValue) *AFTER* calling this function
UA_StatusCode TOpcUA_IOThread::GetInputs(UA_ByteString *newValue)
{
	EnterCriticalSection(&_cs);
	UA_ByteString_copy(&_varRd, newValue);
	LeaveCriticalSection(&_cs);
	return _rdStatus;               // return the last read status
}
//---------------------------------------------------------------------------
UA_StatusCode TOpcUA_IOThread::readwriteCyclic()
{
	UA_ByteString tmp;
	UA_StatusCode retval_wr, retval_rd;

	// write:
	retval_wr = UA_STATUSCODE_GOOD;
	if (_varWr.length > 0) {
		// Copy
		EnterCriticalSection(&_cs);
		UA_ByteString_init(&tmp);
		UA_ByteString_copy(&_varWr, &tmp);
		LeaveCriticalSection(&_cs);
		// Now write:
		if (_wr.Encoding.length() > 0) {
			// CoDeSys RasPi hack:
			retval_wr = writeExtensionObjectValue(_wr.nidNodeId, _wr.nidEncoding, &tmp);
		} else {
			// Default: use data type
//			retval_wr = writeExtensionObjectValue(_wr.nidNodeId, _wr.nidDataType, &tmp);
			retval_wr = writeExtensionObjectValue(_wr.nidNodeId, _wr.ExpandedNodeId, &tmp);
		}
		// Clean the temporary variable
		UA_ByteString_clear(&tmp);
		_wrStatus = retval_wr;
		if (retval_wr != UA_STATUSCODE_GOOD) {
			XTRACE(XPERRORS, "%s: '%s': WriteExtensionObjectValue failed, err = %08Xh", _url.c_str(), _wr.Name.c_str(), retval_wr);
		}
	}

	// read:
	UA_Variant var;
	UA_Variant_init(&var);
	//UA_NodeId ExpandedNodeId;
	retval_rd = TOpcUA_IOThread::readExtensionObjectValue(_rd.nidNodeId, &var, NULL);
	if (UA_STATUSCODE_GOOD == retval_rd) {
		// check
		if (UA_Variant_isScalar(&var) && (var.type == &UA_TYPES[UA_TYPES_STRING] || var.type == &UA_TYPES[UA_TYPES_BYTESTRING])) {
			// copy
			EnterCriticalSection(&_cs);
			UA_ByteString_clear(&_varRd);
			tmp = *(UA_ByteString*)var.data;
			UA_ByteString_copy(&tmp, &_varRd);
			LeaveCriticalSection(&_cs);
		}
		else {
			XTRACE(XPERRORS, "%s: '%s': ReadExtensionObjectValue failed (not scalar or serializable), err = %08Xh", _url.c_str(), _rd.Name.c_str(), retval_rd);
			retval_rd = UA_STATUSCODE_BADENCODINGERROR; //RETURN_ERROR("not string type")
		}
	}
	else {
		XTRACE(XPERRORS, "%s: '%s': ReadExtensionObjectValue failed, err = %08Xh", _url.c_str(), _rd.Name.c_str(), retval_rd);
	}
	_rdStatus = retval_rd;
	UA_Variant_clear(&var);

	if (UA_STATUSCODE_GOOD != retval_wr) {
		return retval_wr;
	}
	return retval_rd;
}

//---------------------------------------------------------------------------
// WARNING: the returned object *must* be freed after use!
UA_StatusCode TOpcUA_IOThread::readExtensionObjectValue(const UA_NodeId nodeId, UA_Variant *outValue, UA_NodeId* pExpandedNodeId)
{
	// Similar to UA_Client_readValueAttribute, but returns the "raw" (undecoded)
	// data of an extension object as binary string.
	// This then allows LUA to decode the blob (e.g. using luastruct).
	// See:
	// - https://github.com/open62541/open62541/issues/3108
	// - https://github.com/open62541/open62541/issues/3787
	// - https://github.com/open62541/open62541/tree/master/examples/custom_datatype
	UA_ReadValueId item;
	UA_ReadValueId_init(&item);
	item.nodeId = nodeId;
	item.attributeId = UA_ATTRIBUTEID_VALUE;

	UA_ReadRequest request;
	UA_ReadRequest_init(&request);
	request.nodesToRead = &item;
	request.nodesToReadSize = 1;
	UA_ReadResponse response = UA_Client_Service_read(_client, request);
	// see: Variant_decodeBinaryUnwrapExtensionObject() ,
	//      UA_findDataTypeByBinaryInternal()
	UA_StatusCode retval = response.responseHeader.serviceResult;
	if (retval == UA_STATUSCODE_GOOD) {
		if (response.resultsSize == 1) {
			retval = response.results[0].status;
		} else {
			XTRACE(XPERRORS, "UA_Client_Service_read() result size != 1 (%d)", response.resultsSize);
			retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
		}
	}
	if (retval != UA_STATUSCODE_GOOD) {
		XTRACE(XPERRORS, "UA_Client_Service_read() failed, err = %08Xh", retval);
		UA_ReadResponse_clear(&response);
		return retval;
	}

	/* Set the StatusCode */
	UA_DataValue *res = response.results;
	if (res->hasStatus)
		retval = res->status;

	/* Return early of no value is given */
	if (!res->hasValue) {
		XTRACE(XPERRORS, "UA_Client_Service_read() no result value available");
		retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
		UA_ReadResponse_clear(&response);
		return retval;
	}

	/* Copy value into out */
	if (res->value.type->typeKind == UA_DATATYPEKIND_EXTENSIONOBJECT) {
		UA_ExtensionObject* eo = (UA_ExtensionObject*)(res->value.data);
		//UA_ExtensionObjectEncoding encoding = eo->encoding;
		if (pExpandedNodeId) {
			// create a new NodeID as a copy of the exisiting one - cleanup first!
			switch(pExpandedNodeId->identifierType) {
				case UA_NODEIDTYPE_STRING:
				case UA_NODEIDTYPE_BYTESTRING:      // malloced types...
					UA_String_clear(&pExpandedNodeId->identifier.string);
					break;
			}
			*pExpandedNodeId = eo->content.encoded.typeId;
			UA_String sTmp;
			switch(pExpandedNodeId->identifierType) {
				case UA_NODEIDTYPE_STRING:
				case UA_NODEIDTYPE_BYTESTRING:      // malloced types...
					UA_ByteString_allocBuffer(&sTmp, pExpandedNodeId->identifier.string.length);
					memcpy(sTmp.data, pExpandedNodeId->identifier.string.data, pExpandedNodeId->identifier.string.length);
					pExpandedNodeId->identifier.string = sTmp;  // use our newly allocated object
					break;
			}
		}
		int length = eo->content.encoded.body.length;
		UA_Byte* data = eo->content.encoded.body.data;
		UA_Variant_setScalarCopy(outValue, &eo->content.encoded.body,  &UA_TYPES[UA_TYPES_BYTESTRING]);
		//UA_Variant_init(&res->value);
	}
	else {
		XTRACE(XPERRORS, "UA_Client_Service_read() - object read is not an ExtensionObject, typeKind is %08Xh", res->value.type->typeKind);
		UA_Variant_init(outValue);
		retval = UA_STATUSCODE_BADTYPEMISMATCH;
		UA_ReadResponse_clear(&response);
		return retval;
	}

	UA_ReadResponse_clear(&response);
	return retval;
}


UA_StatusCode TOpcUA_IOThread::writeExtensionObjectValue(const UA_NodeId nodeId, const UA_NodeId& dataTypeNodeId, const UA_ByteString *newValue)
{
	// Similar to UA_Client_writeValueAttribute, but returns the "raw" (undecoded)
	// data of an extension object as binary string.
	// This then allows LUA to decode the blob (e.g. using luastruct).
	// See:
	// - https://github.com/open62541/open62541/issues/3108
	// - https://github.com/open62541/open62541/issues/3787
	// - https://github.com/open62541/open62541/tree/master/examples/custom_datatype
	// - https://groups.google.com/g/open62541/c/DIrQsGDQ8k4
	UA_Variant myVariant; /* Variants can hold scalar values and arrays of any type */
	UA_Variant_init(&myVariant);
	// build the extension object
	UA_ExtensionObject dataValue;
	dataValue.encoding = UA_EXTENSIONOBJECT_ENCODED_BYTESTRING;
	dataValue.content.encoded.typeId = dataTypeNodeId;
	dataValue.content.encoded.body.data = (UA_Byte*)newValue->data;
	dataValue.content.encoded.body.length = newValue->length;

	UA_Variant_setScalarCopy(&myVariant, &dataValue, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]);
	if (myVariant.type == &UA_TYPES[UA_TYPES_STRING]) {
		// enforce bytestring (if not yet)
		myVariant.type = &UA_TYPES[UA_TYPES_BYTESTRING];
	}
	UA_StatusCode retval = UA_Client_writeValueAttribute(_client, nodeId, &myVariant);
	UA_Variant_clear(&myVariant);
	return retval;
}


// Do the initial transactions to get all information we need after connecting
UA_StatusCode TOpcUA_IOThread::initCyclicInfo(TOpcUA_IOThread::CyclicNode& cycNode)
{
	//UA_NodeClass outNodeClass;
	UA_NodeId nodeId;

	// Clear everything to prevent memory leaks for multiple calls.
	UA_NodeId_clear(&cycNode.nidNodeId);
	UA_NodeId_clear(&cycNode.nidDataType);
	UA_NodeId_clear(&cycNode.nidEncoding);
	UA_NodeClass_clear(&cycNode.nidNodeClass);
//	UA_Variant_clear(&cycNode.varInitVal);

	// (try) to resolve the node name [string] into a node id [numeric]
	nodeId = UA_NODEID_STRING_ALLOC(cycNode.Namespace, cycNode.Name.c_str());
	UA_StatusCode retval = UA_Client_readNodeIdAttribute(_client, nodeId, &cycNode.nidNodeId);
	if (UA_STATUSCODE_GOOD == retval) {
		// return UA_Client_readDataTypeAttribute(_client, nodeId, outDataType);
		XTRACE(XPDIAG1, "%s: '%s': NodeID resolved, NodeType=%d/NodeID=%d", _url.c_str(), cycNode.Name.c_str(), cycNode.nidNodeId.identifierType, cycNode.nidNodeId.identifier.numeric);
		retval = UA_Client_readDataTypeAttribute(_client, cycNode.nidNodeId, &cycNode.nidDataType);
		if (UA_STATUSCODE_GOOD == retval) {
			if (cycNode.nidDataType.identifierType == UA_NODEIDTYPE_STRING) {
				const char* pName = (const char*)cycNode.nidDataType.identifier.string.data;
				XTRACE(XPDIAG1, "    NodeClass=%-16.*s", cycNode.nidDataType.identifier.string.length, pName);
			} else {
				XTRACE(XPDIAG1, "    NodeClass=%d", cycNode.nidDataType.identifier.numeric);
			}
			retval = UA_Client_readNodeClassAttribute(_client, cycNode.nidNodeId, &cycNode.nidNodeClass);
			if (UA_STATUSCODE_GOOD == retval) {
				UA_Variant_init(&cycNode.varInitVal);
				retval = readExtensionObjectValue(cycNode.nidNodeId, &cycNode.varInitVal, &cycNode.ExpandedNodeId);
				if (UA_STATUSCODE_GOOD == retval) {
					int bytes = ((UA_ByteString*)cycNode.varInitVal.data)->length;
					XTRACE(XPDIAG1, "    NodeClass resolved, inital value read, structure size=%d bytes, IdentifierType=%d", bytes, cycNode.ExpandedNodeId.identifierType);
					UA_String idStr = UA_STRING_NULL;
					UA_NodeId_print(&cycNode.ExpandedNodeId, &idStr);
					XTRACE(XPDIAG1, "    -> Identifier: '%s'", idStr.data);
					UA_String_clear(&idStr);
				}
				else {
					XTRACE(XPERRORS, "%s: Failed to read ExtensionObjectValue for '%s'", _url.c_str(), cycNode.Name.c_str());
				}
			}
			else {
				XTRACE(XPERRORS, "%s: Failed to read NodeClass for '%s'", _url.c_str(), cycNode.Name.c_str());
			}
		}
		else {
			XTRACE(XPERRORS, "%s: Failed to read DataType attribute for '%s'", _url.c_str(), cycNode.Name.c_str());
		}
	}
	else {
		XTRACE(XPERRORS, "%s: Failed to resolve NodeID for '%s'", _url.c_str(), cycNode.Name.c_str());
	}
	UA_NodeId_clear(&nodeId);

	if (UA_STATUSCODE_GOOD == retval && cycNode.Encoding.length() > 0) {
		XTRACE(XPDIAG1, "    Encoding is '%s'", cycNode.Encoding.c_str());
		nodeId = UA_NODEID_STRING_ALLOC(cycNode.Namespace, cycNode.Encoding.c_str());
		retval = UA_Client_readNodeIdAttribute(_client, nodeId, &cycNode.nidEncoding);
		if (UA_STATUSCODE_GOOD == retval) {
			if (cycNode.nidEncoding.identifierType == UA_NODEIDTYPE_STRING) {
				const char* pName = (const char*)cycNode.nidEncoding.identifier.string.data;
				XTRACE(XPDIAG1, "    Encoding=%-16.*s", cycNode.nidEncoding.identifier.string.length, pName);
			} else {
				XTRACE(XPDIAG1, "    Encoding=%d", cycNode.nidEncoding.identifier.numeric);
			}
		} else {
			XTRACE(XPERRORS, "%s: Failed to read encoding attribute '%s' for node '%s'", _url.c_str(), cycNode.Encoding.c_str(), cycNode.Name.c_str());
		}
		UA_NodeId_clear(&nodeId);
	}
	else {
		XTRACE(XPDIAG1, "    Encoding is (empty).");
	}
	if (UA_STATUSCODE_GOOD == retval) {
		XTRACE(XPDIAG1, "    Success, fully resolved '%s'", cycNode.Name.c_str());
	} else {
		XTRACE(XPERRORS, "%s: Error resolving '%s': Retcode=%d (%08Xh), Msg=%s", _url.c_str(), cycNode.Name.c_str(), retval, retval, UA_StatusCode_name(retval));
	}

	return retval;
}
//---------------------------------------------------------------------------
void TOpcUA_IOThread::Init(
	const char* endpoint_url,   // "opc.tcp://10.10.2.27:4840"
	int ns,         		// namespace
	const char* wrNode,     // node name to use for writing to OPC UA server
	const char* wrEncoding,
	const char* rdNode,     // node name to use for reading from OPC UA server
	const char* rdEncoding,
	DWORD cycleMs,          // read/write cycle time
	const char* username,
	const char* password)
{
	XTRACE(XPDIAG1, "OPCUA: URL='%s' wrNode='%s' rdNode='%s'", endpoint_url, wrNode, rdNode);
	if (_state != 0) {
		// Error!
		return;
	}
	_url            = endpoint_url;
	_user           = username;
	_pass           = password;
	_tCycleMs       = cycleMs;

	_wr.Init(ns, wrNode, wrEncoding);
	_rd.Init(ns, rdNode, rdEncoding);

/*
	// We also get the client configuration and modify it to link our
	// local callbacks
	UA_ClientConfig *cc = UA_Client_getConfig(_client);
	_origUserConfig = *cc;
	// Wire up the callbacks
	cc->clientContext = this;
	cc->stateCallback = &TOpcUA_IOThread::clientStateChangeTrampoline;
	//cc->subscriptionInactivityCallback = &UA_Client_Proxy::subscriptionInactivityCallback;
*/
	_state = 1;
	Resume();
}
//---------------------------------------------------------------------------
std::string formatNodeId(const UA_NodeId nodeId)
{
/*
	if (nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
		printf("%-9u %-16u", nodeId.namespaceIndex, nodeId.identifier.numeric);
	} else if(nodeId.identifierType == UA_NODEIDTYPE_STRING) {
		printf("%-9u %-16.*s", nodeId.namespaceIndex,
			nodeId.identifier.string.length, nodeId.identifier.string.data);
	} else if(nodeId.identifierType == UA_NODEIDTYPE_BYTESTRING) {
		printf("%-9u %-16.*s", nodeId.namespaceIndex,
			nodeId.identifier.byteString.length, nodeId.identifier.byteString.data);
	} else if(nodeId.identifierType == UA_NODEIDTYPE_GUID) {
			nodeId.identifier.guid;
	} else {
		// ERROR!
	}
*/
}
