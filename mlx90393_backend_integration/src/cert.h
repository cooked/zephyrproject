#ifndef APP_CERT_H
#define APP_CERT_H


const char cert[] = {
	#include "../cert/Baltimore_CyberTrust_Root_Api_Dev_AC_Backend.crt"
};

BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");

#endif // APP_CERT_H
