
#include <stdlib.h>
#include <stdio.h>
#include "mms_client_connection.h"

int main(int argc, char** argv) {

	char* hostname;
	int tcpPort = 102;

	if (argc > 1)
		hostname = argv[1];
	else
		hostname = "localhost";

	if (argc > 2)
		tcpPort = atoi(argv[2]);

	MmsConnection con = MmsConnection_create();

	MmsClientError error;
	MmsIndication indication;

	indication = MmsConnection_connect(con, &error, hostname, tcpPort);

	if (indication != MMS_OK) {
		printf("MMS connect failed!\n");
		goto exit;
	}
	else
		printf("MMS connected.\n\n");



	MmsValue* value =
			MmsConnection_readVariable(con, &error, "simpleIOGenericIO", "LLN0$GO");

	if (value == NULL)
		printf("reading value failed!\n");
	else
		MmsValue_delete(value);

exit:
	MmsConnection_destroy(con);
}

