/*
 *  mms_client_example_reporting.c
 *
 *  This example shows how to use reporting with the MMS client API.
 *  It is intended to be used together with server_example2.
 *
 *  Copyright 2013 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "mms_client_connection.h"

static int rptCount = 0;

static int running = 1;

void sigint_handler(int signalId)
{
    running = 0;
}

static void
informationReportHandler (void* parameter, char* domainName, char* variableListName, MmsValue* value)
{
    if ((domainName == NULL) && (strcmp(variableListName, "RPT") == 0)) {

        printf("received report no %i: ", rptCount++);

        MmsValue* inclusionField = MmsValue_getElement(value, 3);

        if (MmsValue_getBitStringBit(inclusionField, 4)) {
            int valueIndex = MmsValue_getArraySize(value) - 1;

            MmsValue* totW$Mag = MmsValue_getElement(value, valueIndex);

            MmsValue* totW$Mag$f = MmsValue_getElement(totW$Mag, 0);

            float measuredValue = MmsValue_toFloat(totW$Mag$f);

            printf("MMXU1.TotW.mag.f = %f", measuredValue);
        }

        printf("\n");
    }

    MmsValue_delete(value);
}

int
main(int argc, char** argv)
{
    signal(SIGINT, sigint_handler);

	MmsConnection con = MmsConnection_create();

	MmsClientError mmsError;

	MmsIndication indication =
			MmsConnection_connect(con, &mmsError, "localhost", 102);

	if (indication == MMS_OK) {
	    MmsConnection_setInformationReportHandler(con, informationReportHandler, NULL);

	    /* include data set name in the report */
	    MmsValue* optFlds = MmsValue_newBitString(10);
	    MmsValue_setBitStringBit(optFlds, 4, true);
	    MmsConnection_writeVariable(con, &mmsError, "ied1Inverter", "LLN0$RP$rcb1$OptFlds", optFlds);
	    MmsValue_delete(optFlds);

	    /* trigger only on data change or data update */
	    MmsValue* trgOps = MmsValue_newBitString(6);
	    MmsValue_setBitStringBit(trgOps, 1, true);
	    MmsValue_setBitStringBit(trgOps, 3, true);
	    MmsConnection_writeVariable(con, &mmsError, "ied1Inverter", "LLN0$RP$rcb1$TrgOps", optFlds);
	    MmsValue_delete(trgOps);

	    /* Enable report control block */
	    MmsValue* rptEnable = MmsValue_newBoolean(true);
	    MmsConnection_writeVariable(con, &mmsError, "ied1Inverter", "LLN0$RP$rcb1$RptEna", rptEnable);
	    MmsValue_delete(rptEnable);

	    while (running) {
	        usleep(100000);
	    }
	}

	MmsConnection_destroy(con);
}

