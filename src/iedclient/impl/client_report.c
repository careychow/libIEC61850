/*
 *  client_report.c
 *
 *  Client implementation for IEC 61850 reporting.
 *
 *  Copyright 2013, 2014 Michael Zillgith
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

#include "iec61850_client.h"

#include "stack_config.h"

#include "ied_connection_private.h"

#include "mms_mapping.h"

struct sClientReport
{
    ClientDataSet dataSet;
    ReportCallbackFunction callback;
    void* callbackParameter;
    char* rcbReference;
    MmsValue* entryId;
    ReasonForInclusion* reasonForInclusion;
    bool hasTimestamp;
    uint64_t timestamp;
};

char*
ReasonForInclusion_getValueAsString(ReasonForInclusion reasonCode)
{
    switch (reasonCode) {
    case REASON_NOT_INCLUDED:
        return "not-included";
    case REASON_DATA_CHANGE:
        return "data-change";
    case REASON_DATA_UPDATE:
        return "data-update";
    case REASON_QUALITY_CHANGE:
        return "quality-change";
    case REASON_GI:
        return "GI";
    case REASON_INTEGRITY:
        return "integrity";
    default:
        return "unknown";
    }
}

ClientReport
ClientReport_create(ClientDataSet dataSet)
{
    ClientReport self = (ClientReport) calloc(1, sizeof(struct sClientReport));

    self->dataSet = dataSet;

    int dataSetSize = ClientDataSet_getDataSetSize(dataSet);

    self->reasonForInclusion = (ReasonForInclusion*) calloc(dataSetSize, sizeof(ReasonForInclusion));

    return self;
}

void
ClientReport_destroy(ClientReport self)
{
    if (self->entryId)
        MmsValue_delete(self->entryId);

    free(self->rcbReference);
    free(self->reasonForInclusion);
    free(self);
}

char*
ClientReport_getRcbReference(ClientReport self)
{
    return self->rcbReference;
}

ClientDataSet
ClientReport_getDataSet(ClientReport self)
{
    return self->dataSet;
}

ReasonForInclusion
ClientReport_getReasonForInclusion(ClientReport self, int elementIndex)
{
    return self->reasonForInclusion[elementIndex];
}

MmsValue*
ClientReport_getEntryId(ClientReport self)
{
    return self->entryId;
}

bool
ClientReport_hasTimestamp(ClientReport self)
{
    return self->hasTimestamp;
}

uint64_t
ClientReport_getTimestamp(ClientReport self)
{
    return self->timestamp;
}

static void
writeReportResv(IedConnection self, IedClientError* error, char* rcbReference, bool resvValue)
{
    char domainId[65];
    char itemId[129];

    MmsError mmsError;

    MmsMapping_getMmsDomainFromObjectReference(rcbReference, domainId);

    strcpy(itemId, rcbReference + strlen(domainId) + 1);

    StringUtils_replace(itemId, '.', '$');

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: reserve report for (%s) (%s)\n", domainId, itemId);

    int itemIdLen = strlen(itemId);

    strcpy(itemId + itemIdLen, "$Resv");

    MmsValue* resv = MmsValue_newBoolean(resvValue);

    MmsConnection_writeVariable(self->connection, &mmsError, domainId, itemId, resv);

    MmsValue_delete(resv);

    *error = iedConnection_mapMmsErrorToIedError(mmsError);

    if (DEBUG_IED_CLIENT) {
        if (mmsError != MMS_ERROR_NONE)
            printf("DEBUG_IED_CLIENT:  failed to write to RCB!\n");
    }

}

void
IedConnection_reserveRCB(IedConnection self, IedClientError* error, char* rcbReference)
{
    writeReportResv(self, error, rcbReference, true);
}

void
IedConnection_releaseRCB(IedConnection self, IedClientError* error, char* rcbReference)
{
    writeReportResv(self, error, rcbReference, false);
}

void
IedConnection_enableReporting(IedConnection self, IedClientError* error,
        char* rcbReference,
        ClientDataSet dataSet,
        int triggerOptions,
        ReportCallbackFunction callback,
        void* callbackParameter)
{
    MmsError mmsError = MMS_ERROR_NONE;

    char domainId[65];
    char itemId[129];

    MmsMapping_getMmsDomainFromObjectReference(rcbReference, domainId);

    strcpy(itemId, rcbReference + strlen(domainId) + 1);

    StringUtils_replace(itemId, '.', '$');

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: enable report for (%s) (%s)\n", domainId, itemId);

    int itemIdLen = strlen(itemId);

    // check if data set is matching
    strcpy(itemId + itemIdLen, "$DatSet");
    MmsValue* datSet = MmsConnection_readVariable(self->connection, &mmsError, domainId, itemId);

    if (datSet != NULL) {

        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT: RCB has dataset: %s\n", MmsValue_toString(datSet));

        bool matching = false;

        if (strcmp(MmsValue_toString(datSet), ClientDataSet_getReference(dataSet)) == 0) {
            if (DEBUG_IED_CLIENT)
                printf("DEBUG_IED_CLIENT:   data sets are matching!\n");
            matching = true;
        }
        else {
            if (DEBUG_IED_CLIENT)
                printf("DEBUG_IED_CLIENT:  data sets (%s) - (%s) not matching!", MmsValue_toString(datSet),
                        ClientDataSet_getReference(dataSet));
        }

        MmsValue_delete(datSet);

        if (!matching) {
            *error = IED_ERROR_ENABLE_REPORT_FAILED_DATASET_MISMATCH;
            goto cleanup_and_exit;
        }

    }
    else {
        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT: Error accessing RCB!\n");
        *error = IED_ERROR_ACCESS_DENIED;
        return;
    }

    // set include data set reference

    strcpy(itemId + itemIdLen, "$OptFlds");
    MmsValue* optFlds = MmsConnection_readVariable(self->connection, &mmsError,
            domainId, itemId);

    if (MmsValue_getBitStringBit(optFlds, 4) == true) {
        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT:  data set reference is included in report.\n");
        MmsValue_delete(optFlds);
    }
    else {
        MmsValue_setBitStringBit(optFlds, 4, true);

        MmsConnection_writeVariable(self->connection, &mmsError, domainId, itemId, optFlds);

        MmsValue_delete(optFlds);

        if (mmsError != MMS_ERROR_NONE) {
            if (DEBUG_IED_CLIENT)
                printf("DEBUG_IED_CLIENT:  failed to write to RCB!\n");
            *error = iedConnection_mapMmsErrorToIedError(mmsError);
            goto cleanup_and_exit;
        }
    }

    // set trigger options
    if (triggerOptions != 0) {
        strcpy(itemId + itemIdLen, "$TrgOps");

        MmsValue* trgOps = MmsConnection_readVariable(self->connection, &mmsError,
                domainId, itemId);

        if (trgOps != NULL) {
            MmsValue_deleteAllBitStringBits(trgOps);

            if (triggerOptions & TRG_OPT_DATA_CHANGED)
                MmsValue_setBitStringBit(trgOps, 1, true);
            if (triggerOptions & TRG_OPT_QUALITY_CHANGED)
                MmsValue_setBitStringBit(trgOps, 2, true);
            if (triggerOptions & TRG_OPT_DATA_UPDATE)
                MmsValue_setBitStringBit(trgOps, 3, true);
            if (triggerOptions & TRG_OPT_INTEGRITY)
                MmsValue_setBitStringBit(trgOps, 4, true);
            if (triggerOptions & TRG_OPT_GI)
                MmsValue_setBitStringBit(trgOps, 5, true);

            MmsConnection_writeVariable(self->connection, &mmsError, domainId,
                    itemId, trgOps);

            MmsValue_delete(trgOps);
        }
        else {
            if (DEBUG_IED_CLIENT)
                printf("DEBUG_IED_CLIENT:  failed to read trigger options!\n");
            *error = iedConnection_mapMmsErrorToIedError(mmsError);
            MmsValue_delete(trgOps);
            goto cleanup_and_exit;
        }
    }

    // enable report
    strcpy(itemId + itemIdLen, "$RptEna");
    MmsValue* rptEna = MmsValue_newBoolean(true);

    MmsConnection_writeVariable(self->connection, &mmsError, domainId, itemId, rptEna);

    MmsValue_delete(rptEna);

    if (mmsError == MMS_ERROR_NONE) {
        IedConnection_installReportHandler(self, rcbReference, callback, callbackParameter, dataSet);
    }
    else {
        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT:Failed to enable report!\n");
        *error = iedConnection_mapMmsErrorToIedError(mmsError);
    }

    cleanup_and_exit:
    return;
}

static ClientReport
lookupReportHandler(IedConnection self, char* rcbReference)
{
    LinkedList element = LinkedList_getNext(self->enabledReports);

    while (element != NULL) {
        ClientReport report = (ClientReport) element->data;

        if (strcmp(report->rcbReference, rcbReference) == 0)
            return report;

        element = LinkedList_getNext(element);
    }

    return NULL;
}

void
IedConnection_installReportHandler(IedConnection self, char* rcbReference, ReportCallbackFunction handler,
        void* handlerParameter, ClientDataSet dataSet)
{
    ClientReport report = lookupReportHandler(self, rcbReference);

    if (report != NULL) {
        IedConnection_uninstallReportHandler(self, rcbReference);

        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT: Removed existing report callback handler for %s\n", rcbReference);
    }

    report = ClientReport_create(dataSet);
    report->callback = handler;
    report->callbackParameter = handlerParameter;
    report->rcbReference = copyString(rcbReference);
    LinkedList_add(self->enabledReports, report);

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: Installed new report callback handler for %s\n", rcbReference);
}

void
IedConnection_uninstallReportHandler(IedConnection self, char* rcbReference)
{
    ClientReport report = lookupReportHandler(self, rcbReference);

    if (report != NULL) {
        LinkedList_remove(self->enabledReports, report);
        ClientReport_destroy(report);
    }
}

void
IedConnection_disableReporting(IedConnection self, IedClientError* error, char* rcbReference)
{
    char domainId[65];
    char itemId[129];

    MmsMapping_getMmsDomainFromObjectReference(rcbReference, domainId);

    strcpy(itemId, rcbReference + strlen(domainId) + 1);

    StringUtils_replace(itemId, '.', '$');

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: disable reporting for (%s) (%s)\n", domainId, itemId);

    int itemIdLen = strlen(itemId);

    strcpy(itemId + itemIdLen, "$RptEna");
    MmsValue* rptEna = MmsValue_newBoolean(false);

    MmsError mmsError;

    MmsConnection_writeVariable(self->connection, &mmsError, domainId, itemId, rptEna);

    MmsValue_delete(rptEna);

    if (mmsError != MMS_ERROR_NONE) {
        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT:  failed to disable RCB!\n");

        *error = iedConnection_mapMmsErrorToIedError(mmsError);
    }
    else {
        IedConnection_uninstallReportHandler(self, rcbReference);

        *error = IED_ERROR_OK;
    }
}

void
IedConnection_triggerGIReport(IedConnection self, IedClientError* error, char* rcbReference)
{
    char domainId[65];
    char itemId[129];

    MmsMapping_getMmsDomainFromObjectReference(rcbReference, domainId);

    strcpy(itemId, rcbReference + strlen(domainId) + 1);

    StringUtils_replace(itemId, '.', '$');

    int itemIdLen = strlen(itemId);

    strcpy(itemId + itemIdLen, "$GI");

    MmsConnection mmsCon = IedConnection_getMmsConnection(self);

    MmsError mmsError;

    MmsValue* gi = MmsValue_newBoolean(true);

    MmsConnection_writeVariable(mmsCon, &mmsError, domainId, itemId, gi);

    MmsValue_delete(gi);

    if (mmsError != MMS_ERROR_NONE) {
        if (DEBUG_IED_CLIENT)
            printf("DEBUG_IED_CLIENT: failed to trigger GI for %s!\n", rcbReference);

        *error = iedConnection_mapMmsErrorToIedError(mmsError);
    }
    else {
        *error = IED_ERROR_OK;
    }
}

void
private_IedConnection_handleReport(IedConnection self, MmsValue* value)
{
    MmsValue* optFlds = MmsValue_getElement(value, 1);
    if (MmsValue_getBitStringBit(optFlds, 4) == false)
       return;

    bool hasTimestamp = false;
    uint64_t timestamp;

    int datSetIndex = 2;

    /* has sequence-number */
    if (MmsValue_getBitStringBit(optFlds, 1) == true)
        datSetIndex++;

    /* has report-timestamp */
    if (MmsValue_getBitStringBit(optFlds, 2) == true) {
        MmsValue* timeStampValue = MmsValue_getElement(value, datSetIndex);

        if (MmsValue_getType(timeStampValue) == MMS_BINARY_TIME) {
            timestamp = MmsValue_getBinaryTimeAsUtcMs(timeStampValue);
            hasTimestamp = true;
        }

        datSetIndex++;
    }

    MmsValue* datSet = MmsValue_getElement(value, datSetIndex);
    char* datSetName = MmsValue_toString(datSet);
    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT:  with datSet %s\n", datSetName);

    int inclusionIndex = datSetIndex + 1;
    LinkedList element = LinkedList_getNext(self->enabledReports);
    ClientReport report = NULL;
    while (element != NULL) {
        report = (ClientReport) element->data;

        ClientDataSet dataSet = report->dataSet;

        if (strcmp(datSetName, ClientDataSet_getReference(dataSet)) == 0) {
            break;
        }

        element = LinkedList_getNext(element);
    }
    if (report == NULL)
        return;

    if (hasTimestamp) {
        report->hasTimestamp = true;
        report->timestamp = timestamp;
    }

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: Found enabled report!\n");

    /* skip bufOvfl */
    if (MmsValue_getBitStringBit(optFlds, 6) == true)
        inclusionIndex++;

    /* skip entryId */
    if (MmsValue_getBitStringBit(optFlds, 7) == true) {
        MmsValue* entryId = MmsValue_getElement(value, inclusionIndex);

        if (report->entryId != NULL) {

            if (!MmsValue_update(report->entryId, entryId)) {
                MmsValue_delete(report->entryId);
                report->entryId = MmsValue_clone(entryId);
            }
        }
        else {
            report->entryId = MmsValue_clone(entryId);
        }

        inclusionIndex++;
    }

    /* skip confRev */
    if (MmsValue_getBitStringBit(optFlds, 8) == true)
        inclusionIndex++;

    /* skip segmentation fields */
    if (MmsValue_getBitStringBit(optFlds, 9) == true)
        inclusionIndex += 2;

    MmsValue* inclusion = MmsValue_getElement(value, inclusionIndex);
    int includedElements = MmsValue_getNumberOfSetBits(inclusion);
    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: Report includes %i data set elements\n", includedElements);

    int valueIndex = inclusionIndex + 1;
    /* skip data-reference fields */
    if (MmsValue_getBitStringBit(optFlds, 5) == true)
        valueIndex += includedElements;

    int i;
    ClientDataSet dataSet = report->dataSet;
    MmsValue* dataSetValues = ClientDataSet_getValues(dataSet);
    bool hasReasonForInclusion = MmsValue_getBitStringBit(optFlds, 3);
    int reasonForInclusionIndex = valueIndex + includedElements;
    for (i = 0; i < ClientDataSet_getDataSetSize(dataSet); i++) {
        if (MmsValue_getBitStringBit(inclusion, i) == true) {

            MmsValue* dataSetElement = MmsValue_getElement(dataSetValues, i);

            MmsValue* newElementValue = MmsValue_getElement(value, valueIndex);

            if (DEBUG_IED_CLIENT)
                printf("DEBUG_IED_CLIENT:  update element value type: %i\n", MmsValue_getType(newElementValue));

            MmsValue_update(dataSetElement, newElementValue);

            valueIndex++;

            if (hasReasonForInclusion) {
                MmsValue* reasonForInclusion = MmsValue_getElement(value, reasonForInclusionIndex);

                if (MmsValue_getBitStringBit(reasonForInclusion, 1) == true)
                    report->reasonForInclusion[i] = REASON_DATA_CHANGE;
                else if (MmsValue_getBitStringBit(reasonForInclusion, 2) == true)
                    report->reasonForInclusion[i] = REASON_QUALITY_CHANGE;
                else if (MmsValue_getBitStringBit(reasonForInclusion, 3) == true)
                    report->reasonForInclusion[i] = REASON_DATA_UPDATE;
                else if (MmsValue_getBitStringBit(reasonForInclusion, 4) == true)
                    report->reasonForInclusion[i] = REASON_INTEGRITY;
                else if (MmsValue_getBitStringBit(reasonForInclusion, 5) == true)
                    report->reasonForInclusion[i] = REASON_GI;
            }
            else {
                report->reasonForInclusion[i] = REASON_UNKNOWN;
            }
        }
        else {
            report->reasonForInclusion[i] = REASON_NOT_INCLUDED;
        }
    }
    if (report->callback != NULL) {
        report->callback(report->callbackParameter, report);
    }
}

