/*
 * client_example5.c
 *
 * - How to change connection parameters of the lower layers of MMS
 * - How to use password authentication
 *
 */

#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>

#include "thread.h"


int main(int argc, char** argv) {

    char* hostname;
    int tcpPort = 102;

    if (argc > 1)
        hostname = argv[1];
    else
        hostname = "localhost";

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    IedClientError error;

    IedConnection con = IedConnection_create();

    /* To change MMS parameters you need to get access to the underlying MmsConnection */
    MmsConnection mmsConnection = IedConnection_getMmsConnection(con);

    /* Get the container for the parameters */
    IsoConnectionParameters parameters = MmsConnection_getIsoConnectionParameters(mmsConnection);

    /* set remote AP-Title according to SCL file example from IEC 61850-8-1 */
    IsoConnectionParameters_setRemoteApTitle(parameters, "1.3.9999.13", 12);

    /* just some arbitrary numbers */
    IsoConnectionParameters_setLocalApTitle(parameters, "1.2.1200.15.3", 1);

    /* use this to skip AP-Title completely - this may be required by some "obscure" servers */
//    IsoConnectionParameters_setRemoteApTitle(parameters, NULL, 0);
//    IsoConnectionParameters_setLocalApTitle(parameters, NULL, 0);

    /* change parameters for presentation, session and transport layers */
    IsoConnectionParameters_setRemoteAddresses(parameters, 0x12345678, 12 , 13);
    IsoConnectionParameters_setLocalAddresses(parameters, 0x87654321, 1234 , 2345);

    char* password = "top secret";

    /* use authentication */
    AcseAuthenticationParameter auth = (AcseAuthenticationParameter) calloc(1, sizeof(struct sAcseAuthenticationParameter));
    auth->mechanism = ACSE_AUTH_PASSWORD;
    auth->value.password.octetString = (uint8_t*) password;
    auth->value.password.passwordLength = strlen(password);

    IsoConnectionParameters_setAcseAuthenticationParameter(parameters, auth);

    /* call connect when all parameters are set */
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {

        Thread_sleep(1000);

        IedConnection_abort(con, &error);
    }
    else {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    IedConnection_destroy(con);
}


