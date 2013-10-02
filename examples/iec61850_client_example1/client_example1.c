/*
 * client_example1.c
 *
 * This example is intended to be used with server_example3 or server_example_goose.
 */

#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>

#include "thread.h"

static void
printDataSetValues(MmsValue* dataSet)
{
    int i;
    for (i = 0; i < 4; i++) {
        printf("  GGIO1.SPCSO%i.stVal: %i\n", i,
                MmsValue_getBoolean(MmsValue_getElement(dataSet, i)));
    }
}

void
reportCallbackFunction(void* parameter, ClientReport report)
{
    MmsValue* dataSetValues = (MmsValue*) parameter;

    printf("received report\n");

    printDataSetValues(dataSetValues);
}

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

    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {

        /* read an analog measurement value from server */
        MmsValue* value = IedConnection_readObject(con, &error, "simpleIOGenericIO/GGIO1.AnIn1.mag.f", MX);

        if (value != NULL) {
            float fval = MmsValue_toFloat(value);
            printf("read float value: %f\n", fval);
            MmsValue_delete(value);
        }

        /* write a variable to the server */
        value = MmsValue_newVisibleString("libiec61850.com");
        IedConnection_writeObject(con, &error, "simpleIOGenericIO/GGIO1.NamPlt.vendor", DC, value);

        if (error != IED_ERROR_OK)
            printf("failed to write simpleIOGenericIO/GGIO1.NamPlt.vendor!\n");

        MmsValue_delete(value);

        /* read data set */
        ClientDataSet clientDataSet;

        clientDataSet = IedConnection_getDataSet(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);

        if (clientDataSet == NULL)
            printf("failed to read dataset\n");

        printDataSetValues(ClientDataSet_getDataSetValues(clientDataSet));

        IedConnection_enableReporting(con, &error, "simpleIOGenericIO/LLN0.RP.EventsRCB", clientDataSet,
                TRG_OPT_DATA_UPDATE | TRG_OPT_INTEGRITY, reportCallbackFunction, ClientDataSet_getDataSetValues(
                        clientDataSet));

        Thread_sleep(5000);

        IedConnection_disableReporting(con, &error, "simpleIOGenericIO/LLN0.RP.EventsRCB");

        IedConnection_close(con);
    }

    IedConnection_destroy(con);
}


