/*
 *  server_example4.c
 *
 *  Example server application with password authentication.
 *
 *  - How to use a authenticator with password authentication
 *  - How to distinguish between different clients for control actions and set points
 *
 *  The server accepts only connections by clients using one of the two passwords:
 *
 *      user1@testpw
 *      user2@testpw
 *
 *  Only clients using the second password are allowed to perform control actions.
 */

#include "iec61850_server.h"
#include "static_model.h"
#include "thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

/* import IEC 61850 device model created from SCL-File */
extern IedModel iedModel;

static int running = 0;

static IedServer iedServer;

void sigint_handler(int signalId)
{
	running = 0;
}

/* password "database" */
static char* password1 = "user1@testpw";
static char* password2 = "user2@testpw";

/**
 * This is the AcseAuthenticator callback function that is invoked on each client connection attempt.
 * When returning true the server stack accepts the client. Otherwise the connection is rejected.
 */
static bool
clientAuthenticator(void* parameter, AcseAuthenticationParameter authParameter, void** securityToken)
{
    if (authParameter->mechanism == ACSE_AUTH_PASSWORD) {
        if (authParameter->value.password.passwordLength == strlen(password1)) {
            if (memcmp(authParameter->value.password.octetString, password1,
                    authParameter->value.password.passwordLength) == 0)
            {
                *securityToken = (void*) password1;
                return true;
            }

        }
        if (authParameter->value.password.passwordLength == strlen(password2)) {
            if (memcmp(authParameter->value.password.octetString, password2,
                    authParameter->value.password.passwordLength) == 0)
            {
                *securityToken = (void*) password2;
                return true;
            }
        }
    }

    return false;
}

static CheckHandlerResult
performCheckHandler (void* parameter, MmsValue* ctlVal, bool test, bool interlockCheck, ClientConnection connection)
{
    void* securityToken = ClientConnection_getSecurityToken(connection);

    if (securityToken == password2)
        return CONTROL_ACCEPTED;
    else
        return CONTROL_OBJECT_ACCESS_DENIED;
}

static void
controlHandlerForBinaryOutput(void* parameter, MmsValue* value, bool test)
{
    MmsValue* timeStamp = MmsValue_newUtcTimeByMsTime(Hal_getTimeInMs());

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1) {
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, value);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timeStamp);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO2) {
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_stVal, value);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_t, timeStamp);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO3) {
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_stVal, value);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_t, timeStamp);
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO4) {
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, value);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_t, timeStamp);
    }

    MmsValue_delete(timeStamp);
}

int main(int argc, char** argv) {

	iedServer = IedServer_create(&iedModel);

	/* Activate authentication */
	IedServer_setAuthenticator(iedServer, clientAuthenticator, NULL);

	/* Set handler for control permission check for each control object */
	IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,
	        (ControlPerformCheckHandler) performCheckHandler, NULL);
	IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2,
	        (ControlPerformCheckHandler) performCheckHandler, NULL);
	IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3,
	        (ControlPerformCheckHandler) performCheckHandler, NULL);
	IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4,
	        (ControlPerformCheckHandler) performCheckHandler, NULL);

	/* Set control handler for each control object */
    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,
            (ControlHandler) controlHandlerForBinaryOutput, IEDMODEL_GenericIO_GGIO1_SPCSO1);
    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2,
            (ControlHandler) controlHandlerForBinaryOutput, IEDMODEL_GenericIO_GGIO1_SPCSO2);
    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3,
            (ControlHandler) controlHandlerForBinaryOutput, IEDMODEL_GenericIO_GGIO1_SPCSO3);
    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4,
            (ControlHandler) controlHandlerForBinaryOutput, IEDMODEL_GenericIO_GGIO1_SPCSO4);

	/* MMS server will be instructed to start listening to client connections. */
	IedServer_start(iedServer, 102);

	if (!IedServer_isRunning(iedServer)) {
		printf("Starting server failed! Exit.\n");
		IedServer_destroy(iedServer);
		exit(-1);
	}

	running = 1;

	signal(SIGINT, sigint_handler);

	while (running)
		Thread_sleep(100);

	/* stop MMS server - close TCP server socket and all client sockets */
	IedServer_stop(iedServer);

	/* Cleanup - free all resources */
	IedServer_destroy(iedServer);
} /* main() */
