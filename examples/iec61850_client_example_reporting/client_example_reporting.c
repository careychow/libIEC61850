/*
 * client_example_reporting.c
 *
 * This example is intended to be used with server_example3 or server_example_goose.
 */

#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#include "thread.h"

static int running = 0;

void sigint_handler(int signalId)
{
    running = 0;
}


void
reportCallbackFunction(void* parameter, ClientReport report)
{
    LinkedList dataSetDirectory = (LinkedList) parameter;

    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);

    printf("received report for %s with rptId %s\n", ClientReport_getRcbReference(report), ClientReport_getRptId(report));

    if (ClientReport_hasTimestamp(report)) {
        time_t unixTime = ClientReport_getTimestamp(report) / 1000;

#ifdef _MSC_VER
		char* timeBuf = ctime(&unixTime);
#else
		char timeBuf[30];
		ctime_r(&unixTime, timeBuf);
#endif

        printf("  report contains timestamp (%u):", (unsigned int) unixTime);
		printf("%s \n", timeBuf);
    }

    int i;
    for (i = 0; i < LinkedList_size(dataSetDirectory); i++) {
        ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);

        if (reason != REASON_NOT_INCLUDED) {

            LinkedList entry = LinkedList_get(dataSetDirectory, i);

            char* entryName = (char*) entry->data;

            printf("  %s (included for reason %i)\n", entryName, reason);
        }
    }
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

    running = 1;

    signal(SIGINT, sigint_handler);

    IedClientError error;

    IedConnection con = IedConnection_create();

    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {

        /* read data set directory */
        LinkedList dataSetDirectory =
                IedConnection_getDataSetDirectory(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);

        if (error != IED_ERROR_OK) {
            printf("Reading data set directory failed!\n");
            goto exit_error;
        }

        /* read data set */
        ClientDataSet clientDataSet;

        clientDataSet = IedConnection_readDataSetValues(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);

        if (clientDataSet == NULL) {
            printf("failed to read dataset\n");
            goto exit_error;
        }

        /* Read RCB values */
        ClientReportControlBlock rcb =
                IedConnection_getRCBValues(con, &error, "simpleIOGenericIO/LLN0.RP.EventsRCB", NULL);

        if (error != IED_ERROR_OK) {
            printf("getRCBValues service error!\n");
            goto exit_error;
        }

        /* prepare the parameters of the RCP */
        ClientReportControlBlock_setResv(rcb, true);
        ClientReportControlBlock_setDataSetReference(rcb, "simpleIOGenericIO/LLN0$Events"); /* NOTE the "$" instead of "." ! */
        ClientReportControlBlock_setRptEna(rcb, true);

        /* Configure the report receiver */
        IedConnection_installReportHandler(con, "simpleIOGenericIO/LLN0.RP.EventsRCB", ClientReportControlBlock_getRptId(rcb), reportCallbackFunction,
                (void*) dataSetDirectory);

        /* Write RCB parameters and enable report */
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RESV | RCB_ELEMENT_DATSET | RCB_ELEMENT_RPT_ENA, true);

        if (error != IED_ERROR_OK) {
            printf("setRCBValues service error!\n");
            goto exit_error;
        }

        Thread_sleep(1000);

        IedConnection_triggerGIReport(con, &error, "simpleIOGenericIO/LLN0.RP.EventsRCB");

        if (error != IED_ERROR_OK) {
            printf("Error triggering a GI report (code: %i)\n", error);
        }

        while (running) {
            Thread_sleep(10);

            IedConnectionState conState = IedConnection_getState(con);

            if (conState != IED_STATE_CONNECTED) {
                printf("Connection closed by server!\n");
                running = 0;
            }
        }

        exit_error:

        /* disable reporting */
        ClientReportControlBlock_setRptEna(rcb, false);
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);

        ClientDataSet_destroy(clientDataSet);

        IedConnection_close(con);
    }
    else {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    IedConnection_destroy(con);
}


