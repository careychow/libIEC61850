/*
 *  ied_connection.c
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

#include "libiec61850_platform_includes.h"

#include "iec61850_client.h"

#include "stack_config.h"

#include "mms_client_connection.h"
#include "mms_mapping.h"

#include "ied_connection_private.h"
#include "mms_value_internal.h"

typedef struct sICLogicalDevice
{
    char* name;
    LinkedList variables;
    LinkedList dataSets;
} ICLogicalDevice;

struct sClientDataSet
{
    char* dataSetReference; /* data set reference in MMS format */
    MmsValue* dataSetValues; /* MmsValue instance of type MMS_ARRAY */
};

struct sFileDirectoryEntry {
    char* fileName;
    uint32_t fileSize;
    uint64_t lastModified;
};

IedClientError
iedConnection_mapMmsErrorToIedError(MmsError mmsError)
{
    switch (mmsError) {
    case MMS_ERROR_NONE:
        return IED_ERROR_OK;

    case MMS_ERROR_CONNECTION_LOST:
        return IED_ERROR_CONNECTION_LOST;

    case MMS_ERROR_ACCESS_OBJECT_ACCESS_DENIED:
        return IED_ERROR_ACCESS_DENIED;

    case MMS_ERROR_ACCESS_OBJECT_NON_EXISTENT:
        return IED_ERROR_OBJECT_DOES_NOT_EXIST;

    case MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED:
        return IED_ERROR_OBJECT_ACCESS_UNSUPPORTED;

    case MMS_ERROR_DEFINITION_OBJECT_EXISTS:
        return IED_ERROR_OBJECT_EXISTS;

    case MMS_ERROR_SERVICE_TIMEOUT:
        return IED_ERROR_TIMEOUT;

    case MMS_ERROR_FILE_FILE_NON_EXISTENT:
        return IED_ERROR_OBJECT_DOES_NOT_EXIST;

    case MMS_ERROR_CONNECTION_REJECTED:
        return IED_ERROR_CONNECTION_REJECTED;

    default:
        return IED_ERROR_UNKNOWN;
    }
}

IedClientError
iedConnection_mapDataAccessErrorToIedError(MmsDataAccessError mmsError)
{
    switch (mmsError) {
    case DATA_ACCESS_ERROR_OBJECT_NONE_EXISTENT:
        return IED_ERROR_OBJECT_DOES_NOT_EXIST;
    case DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED:
        return IED_ERROR_ACCESS_DENIED;
    case DATA_ACCESS_ERROR_OBJECT_ACCESS_UNSUPPORTED:
        return IED_ERROR_OBJECT_ACCESS_UNSUPPORTED;
    default:
        return IED_ERROR_UNKNOWN;
    }
}


static ICLogicalDevice*
ICLogicalDevice_create(char* name)
{
    ICLogicalDevice* self = (ICLogicalDevice*) calloc(1, sizeof(struct sICLogicalDevice));

    self->name = copyString(name);

    return self;
}

static void
ICLogicalDevice_setVariableList(ICLogicalDevice* self, LinkedList variables)
{
    self->variables = variables;
}

static void
ICLogicalDevice_setDataSetList(ICLogicalDevice* self, LinkedList dataSets)
{
    self->dataSets = dataSets;
}

static void
ICLogicalDevice_destroy(ICLogicalDevice* self)
{
    free(self->name);

    if (self->variables != NULL)
        LinkedList_destroy(self->variables);

    if (self->dataSets != NULL)
        LinkedList_destroy(self->dataSets);

    free(self);
}

static ClientDataSet
ClientDataSet_create(const char* dataSetReference)
{
    ClientDataSet self = (ClientDataSet) calloc(1, sizeof(struct sClientDataSet));

    self->dataSetReference = copyString(dataSetReference);
    StringUtils_replace(self->dataSetReference, '.', '$');

    self->dataSetValues = NULL;

    return self;
}

void
ClientDataSet_destroy(ClientDataSet self)
{
    if (self->dataSetValues != NULL)
        MmsValue_delete(self->dataSetValues);

    free(self->dataSetReference);

    free(self);
}

static void
ClientDataSet_setDataSetValues(ClientDataSet self, MmsValue* dataSetValues)
{
    self->dataSetValues = dataSetValues;
}

MmsValue*
ClientDataSet_getValues(ClientDataSet self)
{
    return self->dataSetValues;
}

char*
ClientDataSet_getReference(ClientDataSet self)
{
    return self->dataSetReference;
}

int
ClientDataSet_getDataSetSize(ClientDataSet self)
{
    if (self->dataSetValues != NULL) {
        return MmsValue_getArraySize(self->dataSetValues);
    }
    else
        return 0;
}

static bool
doesControlObjectMatch(char* objRef, char* cntrlObj)
{
    int objRefLen = strlen(objRef);

    char* separator = strchr(cntrlObj, '$');

    if (separator == NULL)
        return false;

    int sepLen = separator - cntrlObj;

    if (sepLen >= objRefLen)
        return false;

    if (memcmp(objRef, cntrlObj, sepLen) != 0)
        return false;

    char* cntrlObjName = objRef + sepLen + 1;

    if (separator[1] != 'C')
        return false;
    if (separator[2] != 'O')
        return false;
    if (separator[3] != '$')
        return false;

    char* nextSeparator = strchr(separator + 4, '$');

    if (nextSeparator == NULL)
        return false;

    int cntrlObjNameLen = strlen(cntrlObjName);

    if (cntrlObjNameLen != nextSeparator - (separator + 4))
        return false;

    if (memcmp(cntrlObjName, separator + 4, cntrlObjNameLen) == 0)
        return true;

    return false;
}

static bool
doesReportMatchControlObject(char* domainName, char* itemName, char* objectRef)
{
    int i = 0;

    while (domainName[i] != 0) {
        if (domainName[i] != objectRef[i])
            return false;

        i++;
    }

    if (objectRef[i] != '/')
        return false;

    // --> LD is equal

    i++;
    int j = 0;

    while (objectRef[i] != '.') {
        if (objectRef[i] != itemName[j])
            return false;
        j++;
        i++;
    }

    if (itemName[j++] != '$')
        return false;

    // --> LN is equal

    if (itemName[j++] != 'C')
        return false;
    if (itemName[j++] != 'O')
        return false;
    if (itemName[j++] != '$')
        return false;

    // --> FC is ok

    i++;

    while (objectRef[i] != 0) {
        if (itemName[j] == 0)
            return false;

        if (objectRef[i] == '.') {
            if (itemName[j] != '$')
                return false;
        }
        else {
            if (objectRef[i] != itemName[j])
                return false;
        }

        i++;
        j++;
    }

    if (itemName[j++] != '$')
        return false;

    // --> object name is equal

    if (itemName[j++] != 'O')
        return false;
    if (itemName[j++] != 'p')
        return false;
    if (itemName[j++] != 'e')
        return false;
    if (itemName[j++] != 'r')
        return false;
    if (itemName[j++] != 0)
        return false;

    return true;
}

static void
handleLastApplErrorMessage(IedConnection self, MmsValue* value)
{
    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: received LastApplError\n");

    MmsValue* lastApplError = value;
    MmsValue* cntrlObj = MmsValue_getElement(lastApplError, 0);
    MmsValue* error = MmsValue_getElement(lastApplError, 1);
    //MmsValue* origin = MmsValue_getElement(lastApplError, 2);
    MmsValue* ctlNum = MmsValue_getElement(lastApplError, 3);
    MmsValue* addCause = MmsValue_getElement(lastApplError, 4);
    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT:  CntrlObj: %s\n", MmsValue_toString(cntrlObj));

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT:  ctlNum: %u\n", MmsValue_toUint32(ctlNum));

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT:  addCause: %i\n", MmsValue_toInt32(addCause));

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT:  error: %i\n", MmsValue_toInt32(error));

    self->lastApplError.ctlNum = MmsValue_toUint32(ctlNum);
    self->lastApplError.addCause = MmsValue_toInt32(addCause);
    self->lastApplError.error = MmsValue_toInt32(error);
    LinkedList control = LinkedList_getNext(self->clientControls);
    while (control != NULL) {
        ControlObjectClient object = (ControlObjectClient) control->data;

        char* objectRef = ControlObjectClient_getObjectReference(object);

        if (doesControlObjectMatch(objectRef, MmsValue_toString(cntrlObj)))
            ControlObjectClient_setLastApplError(object, self->lastApplError);

        control = LinkedList_getNext(control);
    }
}

static void
informationReportHandler(void* parameter, char* domainName,
        char* variableListName, MmsValue* value, bool isVariableListName)
{
    IedConnection self = (IedConnection) parameter;

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: received information report for %s\n", variableListName);

    if (domainName == NULL) {

        if (isVariableListName) {

            private_IedConnection_handleReport(self, value);
        }
        else {
            if (strcmp(variableListName, "LastApplError") == 0)
                handleLastApplErrorMessage(self, value);
            else {
                if (DEBUG_IED_CLIENT)
                    printf("IED_CLIENT: Received unknown variable list report for list: %s\n", variableListName);
            }
        }
    }
    else {
        if (DEBUG_IED_CLIENT)
            printf("IED_CLIENT: RCVD CommandTermination for %s/%s\n", domainName, variableListName);

        LinkedList control = LinkedList_getNext(self->clientControls);

        while (control != NULL) {
           ControlObjectClient object = (ControlObjectClient) control->data;

           char* objectRef = ControlObjectClient_getObjectReference(object);

           if (doesReportMatchControlObject(domainName, variableListName, objectRef))
               private_ControlObjectClient_invokeCommandTerminationHandler(object);

           control = LinkedList_getNext(control);
        }
    }

    MmsValue_delete(value);
}

IedConnection
IedConnection_create()
{
    IedConnection self = (IedConnection) calloc(1, sizeof(struct sIedConnection));

    self->enabledReports = LinkedList_create();
    self->logicalDevices = NULL;
    self->clientControls = LinkedList_create();

    self->connection = MmsConnection_create();

    self->state = IED_STATE_IDLE;

    self->stateMutex = Semaphore_create(1);

    return self;
}

IedConnectionState
IedConnection_getState(IedConnection self)
{
    IedConnectionState state;

    Semaphore_wait(self->stateMutex);
    state = self->state;
    Semaphore_post(self->stateMutex);

    return state;
}

static void
IedConnection_setState(IedConnection self, IedConnectionState newState)
{
    Semaphore_wait(self->stateMutex);
    self->state = newState;
    Semaphore_post(self->stateMutex);
}

void
IedConnection_installConnectionClosedHandler(IedConnection self, IedConnectionClosedHandler handler,
        void* parameter)
{
    self->connectionCloseHandler = handler;
    self->connectionClosedParameter = parameter;
}

static void
connectionLostHandler(MmsConnection connection, void* parameter)
{
    IedConnection self = (IedConnection) parameter;

    IedConnection_setState(self, IED_STATE_CLOSED);

    if (self->connectionCloseHandler != NULL)
        self->connectionCloseHandler(self->connectionClosedParameter, self);

    if (DEBUG_IED_CLIENT)
        printf("IedConnection closed!\n");
}

void
IedConnection_connect(IedConnection self, IedClientError* error, char* hostname, int tcpPort)
{
    MmsError mmsError;

    if (IedConnection_getState(self) != IED_STATE_CONNECTED) {

        MmsConnection_setConnectionLostHandler(self->connection, connectionLostHandler, (void*) self);
        MmsConnection_setInformationReportHandler(self->connection, informationReportHandler, self);

        if (MmsConnection_connect(self->connection, &mmsError, hostname, tcpPort)) {
            *error = IED_ERROR_OK;
            IedConnection_setState(self, IED_STATE_CONNECTED);
        }
        else {
            *error = iedConnection_mapMmsErrorToIedError(mmsError);
            MmsConnection_destroy(self->connection);
            self->connection = NULL;
        }
    }
    else
        *error = IED_ERROR_ALREADY_CONNECTED;
}

void
IedConnection_abort(IedConnection self, IedClientError* error)
{
    if (IedConnection_getState(self) == IED_STATE_CONNECTED) {
        IedConnection_setState(self, IED_STATE_CLOSED);

        MmsError mmsError;

        MmsConnection_abort(self->connection, &mmsError);

        MmsConnection_destroy(self->connection);
        self->connection = NULL;

        *error = iedConnection_mapMmsErrorToIedError(mmsError);
    }
    else
        *error = IED_ERROR_NOT_CONNECTED;
}

void
IedConnection_release(IedConnection self, IedClientError* error)
{
    if (IedConnection_getState(self) == IED_STATE_CONNECTED) {
        MmsError mmsError;

        MmsConnection_conclude(self->connection, &mmsError);

        *error = iedConnection_mapMmsErrorToIedError(mmsError);
    }
    else
        *error = IED_ERROR_NOT_CONNECTED;
}

void
IedConnection_close(IedConnection self)
{
    if (IedConnection_getState(self) == IED_STATE_CONNECTED) {
        IedConnection_setState(self, IED_STATE_CLOSED);
    }

    if (self->connection != NULL) {
        MmsConnection_destroy(self->connection);
        self->connection = NULL;
    }
}

void
IedConnection_destroy(IedConnection self)
{
    IedConnection_close(self);

    if (self->logicalDevices != NULL)
        LinkedList_destroyDeep(self->logicalDevices, (LinkedListValueDeleteFunction) ICLogicalDevice_destroy);

    if (self->enabledReports != NULL)
        LinkedList_destroyDeep(self->enabledReports, (LinkedListValueDeleteFunction) ClientReport_destroy);

    LinkedList_destroyStatic(self->clientControls);

    Semaphore_destroy(self->stateMutex);

    free(self);
}

MmsVariableSpecification*
IedConnection_getVariableSpecification(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc)
{
    char domainIdBuffer[65];
    char itemIdBuffer[129];

    char* domainId;
    char* itemId;

    MmsError mmsError;
    MmsVariableSpecification* varSpec = NULL;

    domainId = MmsMapping_getMmsDomainFromObjectReference(objectReference, domainIdBuffer);
    itemId = MmsMapping_createMmsVariableNameFromObjectReference(objectReference, fc, itemIdBuffer);

    if ((domainId == NULL) || (itemId == NULL)) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        goto cleanup_and_exit;
    }

    varSpec =
            MmsConnection_getVariableAccessAttributes(self->connection, &mmsError, domainId, itemId);

    if (varSpec != NULL)
        *error = IED_ERROR_OK;
    else
        *error = iedConnection_mapMmsErrorToIedError(mmsError);

    cleanup_and_exit:

    return varSpec;
}

MmsValue*
IedConnection_readObject(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc)
{
    char domainIdBuffer[65];
    char itemIdBuffer[129];
    MmsValue* value = NULL;

    char* domainId;
    char* itemId;

    domainId = MmsMapping_getMmsDomainFromObjectReference(objectReference, domainIdBuffer);
    itemId = MmsMapping_createMmsVariableNameFromObjectReference(objectReference, fc, itemIdBuffer);

    if ((domainId == NULL) || (itemId == NULL)) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return NULL;
    }

    MmsError mmsError;

    value = MmsConnection_readVariable(self->connection, &mmsError, domainId, itemId);

    if (value != NULL)
        *error = IED_ERROR_OK;
    else
        *error = iedConnection_mapMmsErrorToIedError(mmsError);

    return value;
}

bool
IedConnection_readBooleanValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    bool retVal = false;

    if (value != NULL) {
        if (MmsValue_getType(value) == MMS_BOOLEAN)
            retVal = MmsValue_getBoolean(value);
        else {
            if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
                *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
            else
                *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
        }

        MmsValue_delete(value);
    }

    return retVal;
}

float
IedConnection_readFloatValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    float retVal = 0.f;

    if (value != NULL) {
        if (MmsValue_getType(value) == MMS_FLOAT)
            retVal = MmsValue_toFloat(value);
        else {
            if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
                *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
            else
                *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
        }

        MmsValue_delete(value);
    }

    return retVal;
}

char*
IedConnection_readStringValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    char* retVal = NULL;

    if (value != NULL) {
        if ((MmsValue_getType(value) == MMS_VISIBLE_STRING) || (MmsValue_getType(value) == MMS_STRING))
            retVal = copyString(MmsValue_toString(value));
        else {
            if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
                *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
            else
                *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
        }

        MmsValue_delete(value);
    }

    return retVal;
}

int32_t
IedConnection_readInt32Value(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    int32_t retVal = 0.f;

    if (value != NULL) {
        if ((MmsValue_getType(value) == MMS_INTEGER) || (MmsValue_getType(value) == MMS_UNSIGNED))
            retVal = MmsValue_toInt32(value);
        else {
            if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
                *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
            else
                *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
        }

        MmsValue_delete(value);
    }

    return retVal;
}

uint32_t
IedConnection_readUnsigned32Value(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    uint32_t retVal = 0.f;

    if (value != NULL) {
        if ((MmsValue_getType(value) == MMS_INTEGER) || (MmsValue_getType(value) == MMS_UNSIGNED))
            retVal = MmsValue_toUint32(value);
        else {
            if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
                *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
            else
                *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
        }

        MmsValue_delete(value);
    }

    return retVal;
}

Timestamp*
IedConnection_readTimestampValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc,
        Timestamp* timeStamp)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    Timestamp* retVal = timeStamp;

    if (retVal == NULL)
        retVal = (Timestamp*) malloc(sizeof(Timestamp));

    if (value != NULL) {
        if (MmsValue_getType(value) == MMS_UTC_TIME) {
            memcpy(retVal->val, value->value.utcTime, 8);
        }
        else {
            if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
                *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
            else
                *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
        }

        MmsValue_delete(value);

    }

    return retVal;
}

Quality
IedConnection_readQualityValue(IedConnection self, IedClientError* error, const char* objectReference, FunctionalConstraint fc)
{
    MmsValue* value = IedConnection_readObject(self, error, objectReference, fc);

    Quality quality = QUALITY_VALIDITY_GOOD;

    if ((MmsValue_getType(value) == MMS_BIT_STRING) && (MmsValue_getBitStringSize(value) == 13)) {
        quality = Quality_fromMmsValue(value);
    }
    else {
        if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR)
            *error = iedConnection_mapDataAccessErrorToIedError(MmsValue_getDataAccessError(value));
        else
            *error = IED_ERROR_UNEXPECTED_VALUE_RECEIVED;
    }

    MmsValue_delete(value);

    return quality;
}

void
IedConnection_writeObject(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, MmsValue* value)
{
    char domainIdBuffer[65];
    char itemIdBuffer[129];

    char* domainId;
    char* itemId;

    domainId = MmsMapping_getMmsDomainFromObjectReference(objectReference, domainIdBuffer);
    itemId = MmsMapping_createMmsVariableNameFromObjectReference(objectReference, fc, itemIdBuffer);

    if ((domainId == NULL) || (itemId == NULL)) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return;
    }

    MmsError mmsError;

    MmsConnection_writeVariable(self->connection, &mmsError, domainId, itemId, value);

    *error = iedConnection_mapMmsErrorToIedError(mmsError);
}

void
IedConnection_writeBooleanValue(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, bool value)
{
    MmsValue mmsValue;
    mmsValue.deleteValue = 0;
    mmsValue.type = MMS_BOOLEAN;
    mmsValue.value.boolean = value;

    IedConnection_writeObject(self, error, objectReference, fc, &mmsValue);
}

void
IedConnection_writeInt32Value(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, int32_t value)
{
    uint8_t valueBuffer[4];

    Asn1PrimitiveValue pVal;
    pVal.maxSize = 4;
    pVal.size = 4;
    pVal.octets = valueBuffer;

    MmsValue mmsValue;
    mmsValue.type = MMS_INTEGER;
    mmsValue.deleteValue = 0;
    mmsValue.value.integer = &pVal;

    MmsValue_setInt32(&mmsValue, value);

    IedConnection_writeObject(self, error, objectReference, fc, &mmsValue);
}


void
IedConnection_writeUnsigned32Value(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, uint32_t value)
{
    uint8_t valueBuffer[4];

    Asn1PrimitiveValue pVal;
    pVal.maxSize = 4;
    pVal.size = 4;
    pVal.octets = valueBuffer;

    MmsValue mmsValue;
    mmsValue.type = MMS_UNSIGNED;
    mmsValue.deleteValue = 0;
    mmsValue.value.integer = &pVal;

    MmsValue_setUint32(&mmsValue, value);

    IedConnection_writeObject(self, error, objectReference, fc, &mmsValue);
}

void
IedConnection_writeFloatValue(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, float value)
{
    MmsValue mmsValue;
    mmsValue.type = MMS_FLOAT;
    mmsValue.value.floatingPoint.exponentWidth = 8;
    mmsValue.value.floatingPoint.formatWidth = 32;
    mmsValue.value.floatingPoint.buf = (uint8_t*) &value;

    IedConnection_writeObject(self, error, objectReference, fc, &mmsValue);
}

void
IedConnection_writeOctetString(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, uint8_t* value, int valueLength)
{
    MmsValue mmsValue;
    mmsValue.type = MMS_OCTET_STRING;
    mmsValue.value.octetString.buf = value;
    mmsValue.value.octetString.size = valueLength;
    mmsValue.value.octetString.size = valueLength;

    IedConnection_writeObject(self, error, objectReference, fc, &mmsValue);
}

void
IedConnection_writeVisibleStringValue(IedConnection self, IedClientError* error, const char* objectReference,
        FunctionalConstraint fc, char* value)
{
    MmsValue mmsValue;
    mmsValue.deleteValue = 0;
    mmsValue.type = MMS_VISIBLE_STRING;
    mmsValue.value.visibleString.buf = value;

    IedConnection_writeObject(self, error, objectReference, fc, &mmsValue);
}

void
IedConnection_getDeviceModelFromServer(IedConnection self, IedClientError* error)
{
    MmsError mmsError = MMS_ERROR_NONE;
    *error = IED_ERROR_OK;

    LinkedList logicalDeviceNames = MmsConnection_getDomainNames(self->connection, &mmsError);

    if (logicalDeviceNames != NULL) {

        if (self->logicalDevices != NULL) {
            LinkedList_destroyDeep(self->logicalDevices, (LinkedListValueDeleteFunction) ICLogicalDevice_destroy);
            self->logicalDevices = NULL;
        }

        LinkedList logicalDevice = LinkedList_getNext(logicalDeviceNames);

        LinkedList logicalDevices = LinkedList_create();

        while (logicalDevice != NULL) {
            char* name = (char*) logicalDevice->data;

            ICLogicalDevice* icLogicalDevice = ICLogicalDevice_create(name);

            LinkedList variables = MmsConnection_getDomainVariableNames(self->connection,
                    &mmsError, name);

            if (variables != NULL)
                ICLogicalDevice_setVariableList(icLogicalDevice, variables);
            else {
                *error = iedConnection_mapMmsErrorToIedError(mmsError);
                break;
            }

            LinkedList dataSets = MmsConnection_getDomainVariableListNames(self->connection,
                    &mmsError, name);

            if (dataSets != NULL)
                ICLogicalDevice_setDataSetList(icLogicalDevice, dataSets);
            else {
                *error = iedConnection_mapMmsErrorToIedError(mmsError);
                break;
            }

            LinkedList_add(logicalDevices, icLogicalDevice);

            logicalDevice = LinkedList_getNext(logicalDevice);
        }

        self->logicalDevices = logicalDevices;

        LinkedList_destroy(logicalDeviceNames);
    }
    else {
        *error = iedConnection_mapMmsErrorToIedError(mmsError);
    }
}

LinkedList /*<char*>*/
IedConnection_getLogicalDeviceList(IedConnection self, IedClientError* error)
{
    *error = IED_ERROR_OK;

    if (self->logicalDevices == NULL) {
        IedConnection_getDeviceModelFromServer(self, error);

        if (*error != IED_ERROR_OK)
            return NULL;
    }

    if (self->logicalDevices != NULL) {
        LinkedList logicalDevice = LinkedList_getNext(self->logicalDevices);

        LinkedList logicalDeviceList = LinkedList_create();

        while (logicalDevice != NULL) {
            ICLogicalDevice* icLogicalDevice = (ICLogicalDevice*) logicalDevice->data;

            char* logicalDeviceName = copyString(icLogicalDevice->name);

            LinkedList_add(logicalDeviceList, logicalDeviceName);

            logicalDevice = LinkedList_getNext(logicalDevice);
        }

        *error = IED_ERROR_OK;
        return logicalDeviceList;
    }
    else {
        *error = IED_ERROR_UNKNOWN;
        return NULL;
    }
}


static void
mmsFileDirectoryHandler(void* parameter, char* filename, uint32_t size, uint64_t lastModified)
{
    LinkedList fileNames = (LinkedList) parameter;

    FileDirectoryEntry newDirectoryEntry = FileDirectoryEntry_create(filename, size, lastModified);

    LinkedList_add(fileNames, (void*) newDirectoryEntry);
}

LinkedList /*<FileDirectoryEntry>*/
IedConnection_getFileDirectory(IedConnection self, IedClientError* error, const char* directoryName)
{
    *error = IED_ERROR_OK;

    MmsError mmsError = MMS_ERROR_NONE;

    LinkedList fileNames = LinkedList_create();

    char* continueAfter = NULL;

    bool moreFollows = false;

    do {
        moreFollows =
                MmsConnection_getFileDirectory(self->connection, &mmsError, directoryName, continueAfter,
                        mmsFileDirectoryHandler, fileNames);

        if (mmsError != MMS_ERROR_NONE) {
            *error = iedConnection_mapMmsErrorToIedError(mmsError);
            LinkedList_destroyDeep(fileNames, (LinkedListValueDeleteFunction) FileDirectoryEntry_destroy);

            return NULL;
        }

        if (moreFollows) {
            FileDirectoryEntry lastDirectoryEntry = (FileDirectoryEntry)
                    LinkedList_getData(LinkedList_getLastElement(fileNames));

            continueAfter = lastDirectoryEntry->fileName;
        }

    } while (moreFollows == true);

    return fileNames;
}


struct sClientProvidedFileReadHandler {
    IedClientGetFileHandler handler;
    void* handlerParameter;
    bool retVal;
    uint32_t byteReceived;
};

static void
mmsFileReadHandler(void* parameter, int32_t frsmId, uint8_t* buffer, uint32_t bytesReceived)
{
    struct sClientProvidedFileReadHandler* handler = (struct sClientProvidedFileReadHandler*) parameter;


    handler->retVal = handler->handler(handler->handlerParameter, buffer, bytesReceived);

    handler->byteReceived += bytesReceived;
}

uint32_t
IedConnection_getFile(IedConnection self, IedClientError* error, const char* fileName, IedClientGetFileHandler handler,
        void* handlerParameter)
{
    *error = IED_ERROR_OK;

    MmsError mmsError;

    uint32_t fileSize;

    int32_t frsmId =
            MmsConnection_fileOpen(self->connection, &mmsError, fileName, 0, &fileSize, NULL);

    if (mmsError != MMS_ERROR_NONE) {
        *error = iedConnection_mapMmsErrorToIedError(mmsError);
        return 0;
    }

    struct sClientProvidedFileReadHandler clientFileReadHandler;
    clientFileReadHandler.handler = handler;
    clientFileReadHandler.handlerParameter = handlerParameter;
    clientFileReadHandler.retVal = true;
    clientFileReadHandler.byteReceived = 0;

    bool continueRead = true;

    while (continueRead == true) {
        bool moreFollows =
                MmsConnection_fileRead(self->connection, &mmsError, frsmId, mmsFileReadHandler,
                        &clientFileReadHandler);

        if (mmsError != MMS_ERROR_NONE) {
            *error = iedConnection_mapMmsErrorToIedError(mmsError);
            return 0;
        }

        if (clientFileReadHandler.retVal == true) {
            *error = IED_ERROR_UNKNOWN;
            break;
        }

        if (moreFollows == false)
            break;
    }

    MmsConnection_fileClose(self->connection, &mmsError, frsmId);

    *error = iedConnection_mapMmsErrorToIedError(mmsError);

    return clientFileReadHandler.byteReceived;
}

void
IedConnection_deleteFile(IedConnection self, IedClientError* error, const char* fileName)
{
    *error = IED_ERROR_OK;

    MmsError mmsError;

    MmsConnection_fileDelete(self->connection, &mmsError, fileName);

    *error = iedConnection_mapMmsErrorToIedError(mmsError);
}


LinkedList /*<char*>*/
IedConnection_getServerDirectory(IedConnection self, IedClientError* error, bool getFileNames)
{
    if (getFileNames)
        return IedConnection_getFileDirectory(self, error, NULL);
    else
        return IedConnection_getLogicalDeviceList(self, error);
}

LinkedList /*<char*>*/
IedConnection_getLogicalDeviceDirectory(IedConnection self, IedClientError* error,
        const char* logicalDeviceName)
{
    if (self->logicalDevices == NULL)
        IedConnection_getDeviceModelFromServer(self, error);

    if (self->logicalDevices == NULL)
        return NULL;

    LinkedList logicalDevice = LinkedList_getNext(self->logicalDevices);

    while (logicalDevice != NULL) {
        ICLogicalDevice* device = (ICLogicalDevice*) logicalDevice->data;

        if (strcmp(device->name, logicalDeviceName) == 0) {
            LinkedList logicalNodeNames = LinkedList_create();

            LinkedList variable = LinkedList_getNext(device->variables);

            while (variable != NULL) {
                char* variableName = (char*) variable->data;

                if (strchr(variableName, '$') == NULL)
                    LinkedList_add(logicalNodeNames, copyString((char*) variable->data));

                variable = LinkedList_getNext(variable);
            }

            return logicalNodeNames;
        }

        logicalDevice = LinkedList_getNext(logicalDevice);
    }

    *error = IED_ERROR_OBJECT_REFERENCE_INVALID;

    return NULL;
}

static bool
addToStringSet(LinkedList set, char* string)
{
    LinkedList element = set;

    while (LinkedList_getNext(element) != NULL) {
        if (strcmp((char*) LinkedList_getNext(element)->data, string) == 0)
            return false;

        element = LinkedList_getNext(element);
    }

    LinkedList_insertAfter(element, string);
    return true;
}

static void
addVariablesWithFc(char* fc, char* lnName, LinkedList variables, LinkedList lnDirectory)
{
    LinkedList variable = LinkedList_getNext(variables);

    while (variable != NULL) {
        char* variableName = (char*) variable->data;

        char* fcPos = strchr(variableName, '$');

        if (fcPos != NULL) {
            if (memcmp(fcPos + 1, fc, 2) != 0)
                goto next_element;

            int lnNameLen = fcPos - variableName;

            if (strncmp(variableName, lnName, lnNameLen) == 0) {
                char* fcEndPos = strchr(fcPos + 1, '$');

                if (fcEndPos != NULL) {
                    char* nameEndPos = strchr(fcEndPos + 1, '$');

                    if (nameEndPos == NULL)
                        addToStringSet(lnDirectory, copyString(fcEndPos + 1));
                }
            }
        }

        next_element:

        variable = LinkedList_getNext(variable);
    }
}

LinkedList /*<char*>*/
IedConnection_getLogicalNodeDirectory(IedConnection self, IedClientError* error,
        const char* logicalNodeReference, ACSIClass acsiClass)
{
    if (self->logicalDevices == NULL)
        IedConnection_getDeviceModelFromServer(self, error);

    char lnRefCopy[193];

    strncpy(lnRefCopy, logicalNodeReference, 192);
	lnRefCopy[192] = 0;

    char* ldSep = strchr(lnRefCopy, '/');

    if (ldSep == NULL) {
        *error = IED_ERROR_USER_PROVIDED_INVALID_ARGUMENT;
        return NULL;
    }

    *ldSep = 0;

    char* logicalDeviceName = lnRefCopy;

    char* logicalNodeName = ldSep + 1;

    // search for logical device

    LinkedList device = LinkedList_getNext(self->logicalDevices);

    bool deviceFound = false;

    ICLogicalDevice* ld;

    while (device != NULL) {
        ld = (ICLogicalDevice*) device->data;

        if (strcmp(logicalDeviceName, ld->name) == 0) {
            deviceFound = true;
            break;
        }

        device = LinkedList_getNext(device);
    }

    if (!deviceFound) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return NULL;
    }

    LinkedList lnDirectory = LinkedList_create();

    switch (acsiClass) {

    case ACSI_CLASS_DATA_OBJECT:
        {
            LinkedList variable = LinkedList_getNext(ld->variables);

            while (variable != NULL) {
                char* variableName = (char*) variable->data;

                char* fcPos = strchr(variableName, '$');

                if (fcPos != NULL) {
                    if (memcmp(fcPos + 1, "RP", 2) == 0)
                        goto next_element;

                    if (memcmp(fcPos + 1, "BR", 2) == 0)
                        goto next_element;

                    if (memcmp(fcPos + 1, "GO", 2) == 0)
                        goto next_element;

                    int lnNameLen = fcPos - variableName;

                    if (strncmp(variableName, logicalNodeName, lnNameLen) == 0) {
                        char* fcEndPos = strchr(fcPos + 1, '$');

                        if (fcEndPos != NULL) {
                            char* nameEndPos = strchr(fcEndPos + 1, '$');

                            if (nameEndPos == NULL) {
                                char* dataObjectName = copyString(fcEndPos + 1);

                                if (!addToStringSet(lnDirectory, dataObjectName))
                                    free(dataObjectName);
                            }
                        }
                    }
                }

                next_element:

                variable = LinkedList_getNext(variable);
            }
        }
        break;

    case ACSI_CLASS_BRCB:
        addVariablesWithFc("BR", logicalNodeName, ld->variables, lnDirectory);
        break;

    case ACSI_CLASS_URCB:
        addVariablesWithFc("RP", logicalNodeName, ld->variables, lnDirectory);
        break;

    case ACSI_CLASS_GoCB:
        addVariablesWithFc("GO", logicalNodeName, ld->variables, lnDirectory);
        break;

    case ACSI_CLASS_DATA_SET:
        {
            LinkedList dataSet = LinkedList_getNext(ld->dataSets);

            while (dataSet != NULL) {
                char* dataSetName = (char*) dataSet->data;

                char* fcPos = strchr(dataSetName, '$');

                if (fcPos == NULL)
                    goto next_data_set_element;

                size_t lnNameLen = fcPos - dataSetName;

                if (strlen(logicalNodeName) != lnNameLen)
                    goto next_data_set_element;

                if (memcmp(dataSetName, logicalNodeName, lnNameLen) != 0)
                    goto next_data_set_element;

                LinkedList_add(lnDirectory, copyString(fcPos + 1));

                next_data_set_element:

                dataSet = LinkedList_getNext(dataSet);
            }
        }
        break;

    default:
        printf("ACSI class not yet supported!\n");
        break;
    }

    *error = IED_ERROR_OK;
    return lnDirectory;
}

LinkedList /*<char*>*/
IedConnection_getLogicalNodeVariables(IedConnection self, IedClientError* error,
        const char* logicalNodeReference)
{
    if (self->logicalDevices == NULL)
        IedConnection_getDeviceModelFromServer(self, error);

    char lnRefCopy[193];

    strncpy(lnRefCopy, logicalNodeReference, 192);
	lnRefCopy[192] = 0;

    char* ldSep = strchr(lnRefCopy, '/');

    *ldSep = 0;

    char* logicalDeviceName = lnRefCopy;

    char* logicalNodeName = ldSep + 1;

    // search for logical device

    LinkedList device = LinkedList_getNext(self->logicalDevices);

    bool deviceFound = false;

    ICLogicalDevice* ld;

    while (device != NULL) {
        ld = (ICLogicalDevice*) device->data;

        if (strcmp(logicalDeviceName, ld->name) == 0) {
            deviceFound = true;
            break;
        }

        device = LinkedList_getNext(device);
    }

    if (!deviceFound) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return NULL;
    }

    if (DEBUG_IED_CLIENT)
        printf("DEBUG_IED_CLIENT: Found LD %s search for variables of LN %s ...\n", logicalDeviceName, logicalNodeName);

    LinkedList variable = LinkedList_getNext(ld->variables);

    LinkedList lnDirectory = LinkedList_create();

    while (variable != NULL) {
        char* variableName = (char*) variable->data;

        char* fcPos = strchr(variableName, '$');

        if (fcPos != NULL) {
            int lnNameLen = fcPos - variableName;

            if (strncmp(variableName, logicalNodeName, lnNameLen) == 0) {
                LinkedList_add(lnDirectory, copyString(fcPos + 1));
            }
        }

        variable = LinkedList_getNext(variable);
    }

    *error = IED_ERROR_OK;
    return lnDirectory;
}

static LinkedList
getDataDirectory(IedConnection self, IedClientError* error,
        const char* dataReference, bool withFc)
{
    if (self->logicalDevices == NULL)
        IedConnection_getDeviceModelFromServer(self, error);

    char dataRefCopy[193];

    strncpy(dataRefCopy, dataReference, 192);
	dataRefCopy[192] = 0;

    char* ldSep = strchr(dataRefCopy, '/');

    *ldSep = 0;

    char* logicalDeviceName = dataRefCopy;

    char* logicalNodeName = ldSep + 1;

    char* logicalNodeNameEnd = strchr(logicalNodeName, '.');

    if (logicalNodeNameEnd == NULL) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return NULL;
    }

    int logicalNodeNameLen = logicalNodeNameEnd - logicalNodeName;

    char* dataNamePart = logicalNodeNameEnd + 1;

    int dataNamePartLen = strlen(dataNamePart);

    if (dataNamePartLen < 1) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return NULL;
    }

    StringUtils_replace(dataNamePart, '.', '$');

    // search for logical device

    LinkedList device = LinkedList_getNext(self->logicalDevices);

    bool deviceFound = false;

    ICLogicalDevice* ld;

    while (device != NULL) {
        ld = (ICLogicalDevice*) device->data;

        if (strcmp(logicalDeviceName, ld->name) == 0) {
            deviceFound = true;
            break;
        }

        device = LinkedList_getNext(device);
    }

    if (!deviceFound) {
        *error = IED_ERROR_OBJECT_REFERENCE_INVALID;
        return NULL;
    }

    LinkedList variable = LinkedList_getNext(ld->variables);

    LinkedList dataDirectory = LinkedList_create();

    while (variable != NULL) {
        char* variableName = (char*) variable->data;

        char* fcPos = strchr(variableName, '$');

        if (fcPos != NULL) {
            int lnNameLen = fcPos - variableName;

            if (logicalNodeNameLen == lnNameLen) {

                if (memcmp(variableName, logicalNodeName, lnNameLen) == 0) {

                    /* ok we are in the correct logical node */

                    /* skip FC */
                    char* fcEnd = strchr(fcPos + 1, '$');

                    if (fcEnd == NULL)
                        goto next_variable;

                    char* remainingPart = fcEnd + 1;

                    int remainingLen = strlen(remainingPart);

                    if (remainingLen <= dataNamePartLen)
                        goto next_variable;

                    if (remainingPart[dataNamePartLen] == '$') {

                        if (memcmp(dataNamePart, remainingPart, dataNamePartLen) == 0) {

                            char* subElementName = remainingPart + dataNamePartLen + 1;

                            char* subElementNameSep = strchr(subElementName, '$');

                            if (subElementNameSep != NULL)
                                goto next_variable;

                            char* elementName;

                            if (withFc) {
                                int elementNameLen = strlen(subElementName);

                                elementName = (char*) malloc(elementNameLen + 5);
                                memcpy(elementName, subElementName, elementNameLen);
                                elementName[elementNameLen] = '[';
                                elementName[elementNameLen + 1] = *(fcPos + 1);
                                elementName[elementNameLen + 2] = *(fcPos + 2);
                                elementName[elementNameLen + 3] = ']';
                                elementName[elementNameLen + 4] = 0;
                            }
                            else
                                elementName = copyString(subElementName);

                            if (!addToStringSet(dataDirectory, elementName))
                                free(elementName);
                        }
                    }
                }
            }
        }

        next_variable:

        variable = LinkedList_getNext(variable);
    }

    *error = IED_ERROR_OK;
    return dataDirectory;

}

LinkedList
IedConnection_getDataDirectory(IedConnection self, IedClientError* error, const char* dataReference)
{
    return getDataDirectory(self, error, dataReference, false);
}

LinkedList
IedConnection_getDataDirectoryFC(IedConnection self, IedClientError* error, const char* dataReference)
{
    return getDataDirectory(self, error, dataReference, true);
}

void
IedConnection_createDataSet(IedConnection self, IedClientError* error, const char* dataSetReference,
        LinkedList /* <char*> */dataSetElements)
{

    char domainIdBuffer[65];
    char itemIdBuffer[129];

    const char* domainId;
    const char* itemId;
    bool isAssociationSpecific = false;

    if (dataSetReference[0] != '@') {
        domainId = MmsMapping_getMmsDomainFromObjectReference(dataSetReference, domainIdBuffer);
        char* itemIdRef = copyStringToBuffer(dataSetReference + strlen(domainId) + 1, itemIdBuffer);
        StringUtils_replace(itemIdRef, '.', '$');
        itemId = itemIdRef;
    }
    else {
        itemId = dataSetReference;
        isAssociationSpecific = true;
    }

    MmsError mmsError;

    LinkedList dataSetEntries = LinkedList_create();

    LinkedList dataSetElement = LinkedList_getNext(dataSetElements);

    while (dataSetElement != NULL) {

        MmsVariableAccessSpecification* dataSetEntry =
                MmsMapping_ObjectReferenceToVariableAccessSpec((char*) dataSetElement->data);

        LinkedList_add(dataSetEntries, (void*) dataSetEntry);

        dataSetElement = LinkedList_getNext(dataSetElement);
    }

    if (isAssociationSpecific)
        MmsConnection_defineNamedVariableListAssociationSpecific(self->connection, &mmsError,
                itemId, dataSetEntries);
    else
        MmsConnection_defineNamedVariableList(self->connection, &mmsError,
                domainId, itemId, dataSetEntries);

    /* delete list and all elements */
    LinkedList_destroyDeep(dataSetEntries, (LinkedListValueDeleteFunction) MmsVariableAccessSpecification_destroy);

    *error = iedConnection_mapMmsErrorToIedError(mmsError);
}

void
IedConnection_deleteDataSet(IedConnection self, IedClientError* error, const char* dataSetReference)
{
    char domainId[65];
    char itemId[129];
    bool isAssociationSpecific = false;

    if (dataSetReference[0] != '@') {
        MmsMapping_getMmsDomainFromObjectReference(dataSetReference, domainId);
        copyStringToBuffer(dataSetReference + strlen(domainId) + 1, itemId);
        StringUtils_replace(itemId, '.', '$');
    }
    else {
        strncpy(itemId, dataSetReference, 128);
        itemId[128] = 0;
        isAssociationSpecific = true;
    }

    MmsError mmsError;

    if (isAssociationSpecific)
        MmsConnection_deleteAssociationSpecificNamedVariableList(self->connection, &mmsError, itemId);
    else
        MmsConnection_deleteNamedVariableList(self->connection, &mmsError, domainId, itemId);

    *error = iedConnection_mapMmsErrorToIedError(mmsError);
}

LinkedList /* <char*> */
IedConnection_getDataSetDirectory(IedConnection self, IedClientError* error, const char* dataSetReference, bool* isDeletable)
{
    bool deletable = false;

    LinkedList dataSetMembers = NULL;

    char domainIdBuffer[65];
    char itemIdBuffer[129];

    const char* domainId = NULL;
    const char* itemId = NULL;

    bool isAssociationSpecific = false;

    if (dataSetReference[0] != '@') {
        domainId = MmsMapping_getMmsDomainFromObjectReference(dataSetReference, domainIdBuffer);
        char* itemIdRef = copyStringToBuffer(dataSetReference + strlen(domainId) + 1, itemIdBuffer);
        StringUtils_replace(itemIdRef, '.', '$');
        itemId = itemIdRef;
    }
    else {
        itemId = dataSetReference;
        isAssociationSpecific = true;
    }

    MmsError mmsError;

    LinkedList entries;

    if (isAssociationSpecific)
        entries = MmsConnection_readNamedVariableListDirectoryAssociationSpecific(self->connection,
                &mmsError, itemId, &deletable);
    else
        entries = MmsConnection_readNamedVariableListDirectory(self->connection,
                &mmsError, domainId, itemId, &deletable);

    if (mmsError == MMS_ERROR_NONE) {

        LinkedList entry = LinkedList_getNext(entries);

        dataSetMembers = LinkedList_create();

        while (entry != NULL) {
            MmsVariableAccessSpecification* varAccessSpec = (MmsVariableAccessSpecification*) entry->data;

            char* objectReference = MmsMapping_varAccessSpecToObjectReference(varAccessSpec);

            LinkedList_add(dataSetMembers, objectReference);

            entry = LinkedList_getNext(entry);
        }

        if (isDeletable != NULL)
            *isDeletable = deletable;

        LinkedList_destroyDeep(entries, (LinkedListValueDeleteFunction) MmsVariableAccessSpecification_destroy);
    }

    *error = iedConnection_mapMmsErrorToIedError(mmsError);

    return dataSetMembers;
}

ClientDataSet
IedConnection_readDataSetValues(IedConnection self, IedClientError* error, const char* dataSetReference,
        ClientDataSet dataSet)
{
    char domainIdBuffer[65];
    char itemIdBuffer[129];

    const char* domainId = NULL;
    const char* itemId = NULL;


    bool isAssociationSpecific = false;

    if (dataSetReference[0] != '@') {
        domainId = MmsMapping_getMmsDomainFromObjectReference(dataSetReference, domainIdBuffer);
        char* itemIdRef = copyStringToBuffer(dataSetReference + strlen(domainId) + 1, itemIdBuffer);
        StringUtils_replace(itemIdRef, '.', '$');
        itemId = itemIdRef;
    }
    else {
        itemId = dataSetReference;
        isAssociationSpecific = true;
    }

    MmsError mmsError;

    MmsValue* dataSetVal;

    if (isAssociationSpecific)
        dataSetVal = MmsConnection_readNamedVariableListValuesAssociationSpecific(self->connection,
                &mmsError, itemId, true);
    else
        dataSetVal= MmsConnection_readNamedVariableListValues(self->connection, &mmsError,
                    domainId, itemId, true);

    if (dataSetVal == NULL) {
        *error = iedConnection_mapMmsErrorToIedError(mmsError);
        goto cleanup_and_exit;
    }
    else
        *error = IED_ERROR_OK;

    if (dataSet == NULL) {
        dataSet = ClientDataSet_create(dataSetReference);
        ClientDataSet_setDataSetValues(dataSet, dataSetVal);
    }
    else {
        MmsValue* dataSetValues = ClientDataSet_getValues(dataSet);
        MmsValue_update(dataSetValues, dataSetVal);
    }

    cleanup_and_exit:
    return dataSet;
}


MmsConnection
IedConnection_getMmsConnection(IedConnection self)
{
    return self->connection;
}

LastApplError
IedConnection_getLastApplError(IedConnection self)
{
    return self->lastApplError;
}

void
private_IedConnection_addControlClient(IedConnection self, ControlObjectClient control)
{
    LinkedList_add(self->clientControls, control);
}

void
private_IedConnection_removeControlClient(IedConnection self, ControlObjectClient control)
{
    LinkedList_remove(self->clientControls, control);
}

FileDirectoryEntry
FileDirectoryEntry_create(const char* fileName, uint32_t fileSize, uint64_t lastModified)
{
    FileDirectoryEntry self = (FileDirectoryEntry) calloc(1, sizeof(struct sFileDirectoryEntry));

    self->fileName = copyString(fileName);
    self->fileSize = fileSize;
    self->lastModified = lastModified;

    return self;
}

void
FileDirectoryEntry_destroy(FileDirectoryEntry self)
{
    free(self->fileName);
    free(self);
}

char*
FileDirectoryEntry_getFileName(FileDirectoryEntry self)
{
    return self->fileName;
}

uint32_t
FileDirectoryEntry_getFileSize(FileDirectoryEntry self)
{
    return self->fileSize;
}

uint64_t
FileDirectoryEntry_getLastModified(FileDirectoryEntry self)
{
    return self->lastModified;
}

