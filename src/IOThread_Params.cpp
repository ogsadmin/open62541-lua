//---------------------------------------------------------------------------

#pragma hdrstop

#include "IOThread_Params.h"
#include "read_file.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
IOThread_Params::IOThread_Params()
 : SecurityMode(UA_MESSAGESECURITYMODE_NONE), TrustList(NULL),
   TrustListSize(0), RevocationList(NULL), RevocationListSize(0),
   timeout(5000), secureChannelLifeTime(10 * 60 * 1000)
{
	UA_ByteString_init(&Certificate);
	UA_ByteString_init(&PrivateKey);
}

IOThread_Params::~IOThread_Params()
{
	UA_ByteString_clear(&Certificate);
	UA_ByteString_clear(&PrivateKey);
}

UA_StatusCode IOThread_Params::UpdateConfig(UA_ClientConfig *cc)
{
	if (logger) {
		cc->logger = *logger;
	}
	UA_StatusCode rc = UA_STATUSCODE_GOOD;
	if (UA_MESSAGESECURITYMODE_NONE == SecurityMode) {
		UA_ClientConfig_setDefault(cc);
	}
	else {
		// Load certificate and private key
		UA_ByteString_clear(&Certificate);
		UA_ByteString_clear(&PrivateKey);
		Certificate = loadFile(CertificateFile.c_str());
		PrivateKey  = loadFile(PrivateKeyFile.c_str());

		/* Load the trustList. Load revocationList is not supported now */
		/* trust list not support for now
		const size_t trustListSize = 0;
		if(argc > MIN_ARGS)
			trustListSize = (size_t)argc-MIN_ARGS;
		UA_STACKARRAY(UA_ByteString, trustList, trustListSize);
		for(size_t trustListCount = 0; trustListCount < trustListSize; trustListCount++)
			trustList[trustListCount] = loadFile(argv[trustListCount+4]);
		*/
		cc->securityMode = SecurityMode;
#ifdef UA_ENABLE_ENCRYPTION
		rc = UA_ClientConfig_setDefaultEncryption(cc, Certificate, PrivateKey,
				TrustList, TrustListSize,
				RevocationList, RevocationListSize);
#endif
	}
	cc->timeout = timeout;
	cc->secureChannelLifeTime = secureChannelLifeTime;
}
