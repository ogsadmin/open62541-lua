//---------------------------------------------------------------------------

#ifndef IOThread_ParamsH
#define IOThread_ParamsH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
//---------------------------------------------------------------------------
#include <open62541.h>
#include <string>
//---------------------------------------------------------------------------
class IOThread_Params {
public:
	IOThread_Params();
	virtual ~IOThread_Params();
	UA_StatusCode UpdateConfig(UA_ClientConfig *cc);

	UA_MessageSecurityMode SecurityMode;
	std::string CertificateFile;
	std::string PrivateKeyFile;
	int timeout;
	int secureChannelLifeTime;
    UA_Logger *logger;

private:
	UA_ByteString Certificate;
	UA_ByteString PrivateKey;
	UA_ByteString *TrustList;
	size_t TrustListSize;
	UA_ByteString *RevocationList;
	size_t RevocationListSize;
};
//---------------------------------------------------------------------------
#endif
