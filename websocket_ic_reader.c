#include "mongoose.h"
#include <stdio.h>
#include <winscard.h>
#include <scarderr.h>
#include <string.h>
#include <windows.h>
#include "parson.h"

#ifndef SCARD_E_NO_READERS_AVAILABLE
#define SCARD_E_NO_READERS_AVAILABLE ((DWORD)0x8010002E)
#endif

void TrimString(char *out, char *in, int count);
void DateString(char *out, unsigned char *in);
void PostcodeString(char *out, unsigned char *in);
const char* ic_reader();

const unsigned char CmdSelectAppJPN[] =
{ 0x00, 0xA4, 0x04, 0x00, 0x0A, 0x0A0, 0x00, 0x00, 0x00, 0x74, 0x4A, 0x50, 0x4E, 0x00, 0x10 };
const unsigned char CmdAppResponse[] =
{ 0x00, 0xC0, 0x00, 0x00, 0x05 };
const unsigned char CmdSetLength[] =
{ 0xC8, 0x32, 0x00, 0x00, 0x05, 0x08, 0x00, 0x00 };		//append with ss ss
const unsigned char CmdSelectFile[] =
{ 0xCC, 0x00, 0x00, 0x00, 0x08 }; //append with pp pp qq qq rr rr ss ss
									//pppp = file id, qqqq = file group
									//rrrr = offset, ssss = length
const unsigned char CmdGetData[] =
{ 0xCC, 0x06, 0x00, 0x00 };		//append with ss
// const int fileLengths[] = {0, 459, 4011, 0};
const int fileLengths[] = { 0, 459, 0 };

SCARD_IO_REQUEST pciT0 = { 1, 8 };


#pragma comment(lib, "ws2_32.lib") /* Linking with winsock library */
#pragma comment(lib, "advapi32.lib")  /* the newly added */

// Print websocket response and signal that we're done
static void fn(struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
	
	
    if (ev == MG_EV_ERROR) {
        // On error, log error message
        LOG(LL_ERROR, ("%p %s", c->fd, (char*)ev_data));
    }
    else if (ev == MG_EV_WS_OPEN) {
		char* ic_data = ic_reader();
        // When websocket handshake is successful, send message
        mg_ws_send(c, ic_data, strlen(ic_data), WEBSOCKET_OP_TEXT);
    }

    if (ev == MG_EV_ERROR) {
        *(bool*)fn_data = true;  // Signal that we're done
    }
    if (ev == MG_EV_CLOSE) {
        *(bool*)fn_data = true;  // Signal that we're done
    }
    if (ev == MG_EV_WS_MSG) {
        *(bool*)fn_data = true;  // Signal that we're done
    }
}

void TrimString(char* out, char* in, int count)
{
    int i, j;
    for (i = count - 1; i >= 0 && in[i] == 0x20; i--);
    for (j = 0; j < i + 1; out[j++] = in[j]);
    out[j] = 0;
}

void DateString(char* out, unsigned char* in)
{
    sprintf(out, "%02x", in[0]);
    sprintf(out + 2, "%02x-", in[1]);
    sprintf(out + 5, "%02x-", in[2]);
    sprintf(out + 8, "%02x", in[3]);
    out[10] = 0;
}

void PostcodeString(char* out, unsigned char* in)
{
    sprintf(out, "%02x", in[0]);
    sprintf(out + 2, "%02x", in[1]);
    sprintf(out + 4, "%02x", in[2]);
    out[5] = 0;
}

const char* ic_reader()
{
	SCARDCONTEXT hSC;
	SCARDHANDLE	hCard;
	char RxBuffer[512];
	char TxBuffer[64];
	char ReaderName[64];
	int retval, dCount, i, dProtocol, dLength, FileNum;
	int split_offset, split_length;
	FILE* outfile;

	JSON_Value* root_value = json_value_init_object();
	JSON_Object* root_object = json_value_get_object(root_value);
	char* serialized_string = NULL;

	retval = SCardEstablishContext(SCARD_SCOPE_USER, 0, 0, &hSC);
	if (retval == SCARD_E_NO_SERVICE) {
		 printf("Smart card service not started\n");
		return;
	}
	else if (retval != 0) {
		 printf("SCardEstablishContext Error: %x\n", retval);
		return;
	}
	dCount = 256;
	retval = SCardListReadersA(hSC, 0, RxBuffer, &dCount);
	if (retval == SCARD_E_NO_READERS_AVAILABLE) {
		printf("SCardListReaders: No readers available\n");
		SCardReleaseContext(hSC);
		return;
	}
	else if (retval != 0) {
		printf("SCardListReaders: Error %x\n", retval);
		SCardReleaseContext(hSC);
		return;
	}
	for (i = 0; (ReaderName[i] = RxBuffer[i]) && i < 64; i++);
	if (!i) {
		printf("SCardListReaders: No readers available\n");
		SCardReleaseContext(hSC);
		return;
	}

	retval = SCardConnectA(hSC, ReaderName, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0, &hCard, &dProtocol);
	if (retval == SCARD_W_REMOVED_CARD || retval == SCARD_E_NO_SMARTCARD) {
		printf("Smart card removed\n");
		SCardReleaseContext(hSC);
		return;
	}
	else if (retval != 0) {
		printf("SCardConnect: Error %x\n", retval);
		SCardReleaseContext(hSC);
		return;
	}

	// printf("Selecting JPN application\n");
	dLength = 256;
	retval = SCardTransmit(hCard, &pciT0, CmdSelectAppJPN, 15, &pciT0, RxBuffer, &dLength);
	if (retval) {
		printf("SCardTransmit (Select App): Error %x\n", retval);
		SCardReleaseContext(hSC);
		// quit();
		return;
	}
	else if (RxBuffer[0] != 0x61 || RxBuffer[1] != 0x05) {
		printf("Not MyKad\n");
		SCardReleaseContext(hSC);
		// quit();
		return;
	}
	dLength = 256;
	retval = SCardTransmit(hCard, &pciT0, CmdAppResponse, 5, &pciT0, RxBuffer, &dLength);
	if (retval) {
		printf("SCardTransmit (App Response): Error %x\n", retval);
		SCardReleaseContext(hSC);
		return;
	}

	for (FileNum = 1; fileLengths[FileNum]; FileNum++) {
		for (split_offset = 0, split_length = 252; split_offset < fileLengths[FileNum]; split_offset += split_length) {
			if (split_offset + split_length > fileLengths[FileNum])
				split_length = fileLengths[FileNum] - split_offset;
			dLength = 256;
			for (i = 0; i < 8; TxBuffer[i++] = CmdSetLength[i]);
			*(short*)(TxBuffer + i) = split_length;	i += 2;
			retval = SCardTransmit(hCard, &pciT0, TxBuffer, i, &pciT0, RxBuffer, &dLength);

			dLength = 256;
			for (i = 0; i < 5; TxBuffer[i++] = CmdSelectFile[i]);
			*(short*)(TxBuffer + i) = FileNum;	i += 2;
			*(short*)(TxBuffer + i) = 1;	i += 2;
			*(short*)(TxBuffer + i) = split_offset;	i += 2;
			*(short*)(TxBuffer + i) = split_length;	i += 2;
			retval = SCardTransmit(hCard, &pciT0, TxBuffer, i, &pciT0, RxBuffer, &dLength);

			dLength = 256;
			for (i = 0; i < 4; TxBuffer[i++] = CmdGetData[i]);
			TxBuffer[i++] = (unsigned char)split_length;
			retval = SCardTransmit(hCard, &pciT0, TxBuffer, i, &pciT0, RxBuffer, &dLength);

			if (FileNum == 1 && split_offset == 0) {
				TrimString(TxBuffer, RxBuffer + 0x03, 0x28);
				json_object_set_string(root_object, "name", TxBuffer);
			}
			else if (FileNum == 1 && split_offset == 252) {
				TrimString(TxBuffer, RxBuffer + 0x111 - 252, 0x0D);
				printf("%s", TxBuffer);
				json_object_set_string(root_object, "ic_number", TxBuffer);

				if (RxBuffer[0x11E - 252] == 'P') {
					json_object_set_string(root_object, "gender", "FEMALE");
				}
				else if (RxBuffer[0x11E - 252] == 'L') {
					json_object_set_string(root_object, "gender", "MALE");
				}
				else {
					json_object_set_string(root_object, "gender", "NA");
				}

				//TrimString(TxBuffer, RxBuffer + 0x11F - 252, 0x08);
				//sprintf(ic, "Old IC: %s\n", TxBuffer);
				//memset(ic, 0, sizeof ic);

				//DateString(TxBuffer, RxBuffer + 0x127 - 252);
				//sprintf(ic, "DOB: %s\n", TxBuffer);
				//json_object_set_string(root_object, "dob", TxBuffer);
				//memset(ic, 0, sizeof ic);

				//TrimString(TxBuffer, RxBuffer + 0x12B - 252, 0x19);
				//sprintf(ic, "State of birth: %s\n", TxBuffer);
				//memset(ic, 0, sizeof ic);

				//DateString(TxBuffer, RxBuffer + 0x144 - 252);
				//sprintf(ic, "Validity Date: %s\n", TxBuffer);
				//memset(ic, 0, sizeof ic);

				//TrimString(TxBuffer, RxBuffer + 0x148 - 252, 0x12);
				//sprintf(ic, "Nationality: %s\n", TxBuffer);
				//memset(ic, 0, sizeof ic);

				TrimString(TxBuffer, RxBuffer + 0x15A - 252, 0x19);
				json_object_set_string(root_object, "race", TxBuffer);

				TrimString(TxBuffer, RxBuffer + 0x173 - 252, 0x0B);
				json_object_set_string(root_object, "religion", TxBuffer);
			}
			 /*else if (FileNum==4 && split_offset==0) {
				char address_buffer[256];
			 	TrimString(TxBuffer, RxBuffer+0x03, 0x1E);
				strcpy(address_buffer, TxBuffer);
				strcat(address_buffer, ", ");

			 	TrimString(TxBuffer, RxBuffer+0x21, 0x1E);

				strcat(address_buffer, TxBuffer);
				strcat(address_buffer, ", ");
			 	TrimString(TxBuffer, RxBuffer+0x3F, 0x1E);

			 	PostcodeString(TxBuffer, RxBuffer+0x5D);
				strcat(address_buffer, TxBuffer);
				strcat(address_buffer, ", ");

			 	TrimString(TxBuffer, RxBuffer+0x60, 0x19);

				strcat(address_buffer, TxBuffer);
				strcat(address_buffer, ", ");
			 	TrimString(TxBuffer, RxBuffer+0x79, 0x1E);

				strcat(address_buffer, TxBuffer);

				json_object_set_string(root_object, "address", address_buffer);
			 }*/
			/* End displaying stuffs */
		}

	}
	serialized_string = json_serialize_to_string_pretty(root_value);

	return serialized_string;
}

int main(int argc, char* argv[]) {
	FreeConsole();
	char s_url[200] = {"ws://192.168.68.134:8000/visitor/"};
	if (argc > 1) {
		// split argument by ":" and take the session id
		char* token;
		char* delimiter = ":";

		token = strtok(argv[1], delimiter);

		token = strtok(NULL, delimiter);

		// concatenate the session id into the websocket url
		strcat(s_url, token);

		strcat(s_url, "/");
	}
	else {
		return -1;
	}
	
    struct mg_mgr mgr;        // Event manager
    bool done = false;        // Event handler flips it to true
    struct mg_connection* c;  // Client connection
    mg_mgr_init(&mgr);        // Initialise event manager
    c = mg_ws_connect(&mgr, s_url, fn, &done, NULL);     // Create client
    while (c && done == false) mg_mgr_poll(&mgr, 1000);  // Wait for echo
    mg_mgr_free(&mgr);                                   // Deallocate resources
    //getchar();
    return 0;
}

