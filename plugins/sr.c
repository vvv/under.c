#include <stdint.h>
#include <stdio.h>

#include "ctt.h"

/* #define _printf(FORMAT, ...)  printf("[" FORMAT "]", ##__VA_ARGS__) */

int
decode_callTransactionType(const uint8_t *bytes, size_t len)
{
	if (len != 1) {
		fprintf(stderr, "decode_callTransactionType: 1 byte expected,"
			" %lu received\n", (unsigned long) len);
		return -1;
	}

	switch (*bytes) {
	case 0:
		fputs("[default", stdout);
		break;
	case 1:
		fputs("[moc", stdout);
		break;
	case 2:
		fputs("[mtc", stdout);
		break;
	case 3:
		fputs("[emergencyCall", stdout);
		break;
	case 6:
		fputs("[mocOACSU", stdout);
		break;
	case 7:
		fputs("[mtcOACSU", stdout);
		break;
	case 8:
		fputs("[inCallModMoc", stdout);
		break;
	case 9:
		fputs("[inCallModMtc", stdout);
		break;
	case 10:
		fputs("[sSRegistration", stdout);
		break;
	case 11:
		fputs("[sSErasure", stdout);
		break;
	case 12:
		fputs("[sSActivation", stdout);
		break;
	case 13:
		fputs("[sSDeactivation", stdout);
		break;
	case 14:
		fputs("[sSInterrogation", stdout);
		break;
	case 15:
		fputs("[sSUnstructuredProcessing", stdout);
		break;
	case 17:
		fputs("[moMOBOX", stdout);
		break;
	case 18:
		fputs("[mtMOBOX", stdout);
		break;
	case 19:
		fputs("[moDPAD", stdout);
		break;
	case 26:
		fputs("[roaming", stdout);
		break;
	case 27:
		fputs("[transit", stdout);
		break;
	case 29:
		fputs("[callForwarding", stdout);
		break;
	case 30:
		fputs("[mtcSMS", stdout);
		break;
	case 31:
		fputs("[mocSMS", stdout);
		break;
	case 32:
		fputs("[emergencyCallTrace", stdout);
		break;
	case 33:
		fputs("[sSInvocation", stdout);
		break;
	case 34:
		fputs("[roaAttempt", stdout);
		break;
	case 35:
		fputs("[mtLocationRequest", stdout);
		break;
	case 36:
		fputs("[mtLocationRequestAttempt", stdout);
		break;
	case 37:
		fputs("[moLocationRequest", stdout);
		break;
	case 38:
		fputs("[moLocationRequestAttempt", stdout);
		break;
	case 39:
		fputs("[niLocationRequest", stdout);
		break;
	case 40:
		fputs("[niLocationRequestAttempt", stdout);
		break;
	case 42:
		fputs("[cAMELCF", stdout);
		break;
	case 43:
		fputs("[tIR", stdout);
		break;
	case 44:
		fputs("[voiceGroupServiceAMSC", stdout);
		break;
	case 45:
		fputs("[tIRAttempt", stdout);
		break;
	case 46:
		fputs("[voiceGroupServiceRMSC", stdout);
		break;
	case 59:
		fputs("[processUnstructuredSsRequestMo", stdout);
		break;
	case 60:
		fputs("[unstructuredSsRequestNi", stdout);
		break;
	case 61:
		fputs("[unstructuredSsNotifyNi", stdout);
		break;
	case 65:
		fputs("[mocAttempt", stdout);
		break;
	case 66:
		fputs("[mtcAttempt", stdout);
		break;
	case 67:
		fputs("[emergencyCallAttempt", stdout);
		break;
	case 93:
		fputs("[callForwardingAttempt", stdout);
		break;
	case 119:
		fputs("[mtcBadMMS", stdout);
		break;
	case 120:
		fputs("[mocBadMMS", stdout);
		break;
	case 121:
		fputs("[mtcMMS", stdout);
		break;
	case 122:
		fputs("[mocMMS", stdout);
		break;
	case 123:
		fputs("[mtcInterMMS", stdout);
		break;
	case 124:
		fputs("[mocInterMMS", stdout);
		break;
	case 125:
		fputs("[ussdCall", stdout);
		break;
	default:
		return -1;
	}

	printf(" (%u)]", *bytes);
	return 0;
}

/* XXX */
/* encode_callTransactionType(XXX) */
/* { */
/* } */
