/*
 *  ied_server.c
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

#include "iec61850_server.h"
#include "mms_mapping.h"
#include "control.h"
#include "stack_config.h"
#include "ied_server_private.h"
#include "thread.h"
#include "reporting.h"

#ifndef DEBUG_IED_SERVER
#define DEBUG_IED_SERVER 0
#endif

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
static void
createControlObjects(IedServer self, MmsDomain* domain, char* lnName, MmsVariableSpecification* typeSpec, char* namePrefix)
{
    MmsMapping* mapping = self->mmsMapping;

    if (typeSpec->type == MMS_STRUCTURE) {
        int coCount = typeSpec->typeSpec.structure.elementCount;
        int i;
        for (i = 0; i < coCount; i++) {

            char objectName[65];
            objectName[0] = 0;

            if (namePrefix != NULL) {
                strcat(objectName, namePrefix);
                strcat(objectName, "$");
            }

            bool isControlObject = false;
            bool hasCancel = false;
            int cancelIndex = 0;
            bool hasSBOw = false;
            int sBOwIndex = 0;
            int operIndex = 0;

            MmsVariableSpecification* coSpec = typeSpec->typeSpec.structure.elements[i];

            if (coSpec->type == MMS_STRUCTURE) {

                int coElementCount = coSpec->typeSpec.structure.elementCount;

                int j;
                for (j = 0; j < coElementCount; j++) {
                    MmsVariableSpecification* coElementSpec = coSpec->typeSpec.structure.elements[j];

                    if (strcmp(coElementSpec->name, "Oper") == 0) {
                        isControlObject = true;
                        operIndex = j;
                    }
                    else if (strcmp(coElementSpec->name, "Cancel") == 0) {
                        hasCancel = true;
                        cancelIndex = j;
                    }
                    else if (strcmp(coElementSpec->name, "SBOw") == 0) {
                        hasSBOw = true;
                        sBOwIndex = j;
                    }
                    else if (!(strcmp(coElementSpec->name, "SBO") == 0)) {
                        if (DEBUG_IED_SERVER)
                            printf("IED_SERVER: createControlObjects: Unknown element in CO: %s! --> seems not to be a control object\n", coElementSpec->name);

                        assert(false);

                        break;
                    }
                }

                if (isControlObject) {

                    strcat(objectName, coSpec->name);

                    if (DEBUG_IED_SERVER)
                        printf("IED_SERVER: create control object LN:%s DO:%s\n", lnName, objectName);
                    ControlObject* controlObject = ControlObject_create(self, domain, lnName, objectName);

                    MmsValue* structure = MmsValue_newDefaultValue(coSpec);

                    ControlObject_setMmsValue(controlObject, structure);

                    ControlObject_setTypeSpec(controlObject, coSpec);

                    MmsValue* operVal = MmsValue_getElement(structure, operIndex);
                    ControlObject_setOper(controlObject, operVal);

                    if  (hasCancel) {
                        MmsValue* cancelVal = MmsValue_getElement(structure, cancelIndex);
                        ControlObject_setCancel(controlObject, cancelVal);
                    }

                    if (hasSBOw) {
                        MmsValue* sbowVal = MmsValue_getElement(structure, sBOwIndex);
                        ControlObject_setSBOw(controlObject, sbowVal);
                    }

                    MmsMapping_addControlObject(mapping, controlObject);
                }
                else {
                    strcat(objectName, coSpec->name);
                    createControlObjects(self, domain, lnName, coSpec, objectName);
                }
            }
        }
    }
}
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */

static void
createMmsServerCache(IedServer self)
{
    assert(self != NULL);

    int domain = 0;

    for (domain = 0; domain < self->mmsDevice->domainCount; domain++) {

        /* Install all top level MMS named variables (=Logical nodes) in the MMS server cache */
        MmsDomain* logicalDevice = self->mmsDevice->domains[domain];

        int i;

        for (i = 0; i < logicalDevice->namedVariablesCount; i++) {
            char* lnName = logicalDevice->namedVariables[i]->name;

            if (DEBUG_IED_SERVER)
                printf("ied_server.c: Insert into cache %s - %s\n", logicalDevice->domainName, lnName);

            int fcCount = logicalDevice->namedVariables[i]->typeSpec.structure.elementCount;
            int j;

            for (j = 0; j < fcCount; j++) {
                MmsVariableSpecification* fcSpec = logicalDevice->namedVariables[i]->typeSpec.structure.elements[j];

                char* fcName = fcSpec->name;

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
                if (strcmp(fcName, "CO") == 0) {
                    createControlObjects(self, logicalDevice, lnName, fcSpec, NULL);
                }
                else
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */

                if ((strcmp(fcName, "BR") != 0) && (strcmp(fcName, "RP") != 0)
                        && (strcmp(fcName, "GO") != 0))
                        {

                    char* variableName = createString(3, lnName, "$", fcName);

                    MmsValue* defaultValue = MmsValue_newDefaultValue(fcSpec);

                    if (DEBUG_IED_SERVER)
                        printf("ied_server.c: Insert into cache %s - %s\n", logicalDevice->domainName, variableName);

                    MmsServer_insertIntoCache(self->mmsServer, logicalDevice, variableName, defaultValue);

                    free(variableName);
                }
            }
        }
    }
}

static void
installDefaultValuesForDataAttribute(IedServer self, DataAttribute* dataAttribute,
        char* objectReference, int position)
{
    sprintf(objectReference + position, ".%s", dataAttribute->name);

    char mmsVariableName[65]; /* maximum size is 64 according to 61850-8-1 */

    MmsValue* value = dataAttribute->mmsValue;

    MmsMapping_createMmsVariableNameFromObjectReference(objectReference, dataAttribute->fc, mmsVariableName);

    char domainName[65];

    MmsMapping_getMmsDomainFromObjectReference(objectReference, domainName);

    MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, domainName);

    if (domain == NULL) {
        if (DEBUG_IED_SERVER)
            printf("Error domain (%s) not found!\n", domainName);
        return;
    }

    MmsValue* cacheValue = MmsServer_getValueFromCache(self->mmsServer, domain, mmsVariableName);

    dataAttribute->mmsValue = cacheValue;

    if (value != NULL) {
        MmsValue_update(cacheValue, value);
        MmsValue_delete(value);
    }

    int childPosition = strlen(objectReference);
    DataAttribute* subDataAttribute = (DataAttribute*) dataAttribute->firstChild;

    while (subDataAttribute != NULL) {
        installDefaultValuesForDataAttribute(self, subDataAttribute, objectReference, childPosition);

        subDataAttribute = (DataAttribute*) subDataAttribute->sibling;
    }
}

static void
installDefaultValuesForDataObject(IedServer self, DataObject* dataObject,
        char* objectReference, int position)
{
    if (dataObject->elementCount > 0) {
        if (DEBUG_IED_SERVER)
            printf("IED_SERVER: DataObject is an array. Skip installing default values in cache\n");

        return;
    }

    sprintf(objectReference + position, ".%s", dataObject->name);

    ModelNode* childNode = dataObject->firstChild;

    int childPosition = strlen(objectReference);

    while (childNode != NULL) {
        if (childNode->modelType == DataObjectModelType) {
            installDefaultValuesForDataObject(self, (DataObject*) childNode, objectReference, childPosition);
        }
        else if (childNode->modelType == DataAttributeModelType) {
            installDefaultValuesForDataAttribute(self, (DataAttribute*) childNode, objectReference, childPosition);
        }

        childNode = childNode->sibling;
    }
}

static void
installDefaultValuesInCache(IedServer self)
{
    IedModel* model = self->model;

    char objectReference[130];

    LogicalDevice* logicalDevice = model->firstChild;

    while (logicalDevice != NULL) {
        sprintf(objectReference, "%s", logicalDevice->name);

        LogicalNode* logicalNode = (LogicalNode*) logicalDevice->firstChild;

        char* nodeReference = objectReference + strlen(objectReference);

        while (logicalNode != NULL) {
            sprintf(nodeReference, "/%s", logicalNode->name);

            DataObject* dataObject = (DataObject*) logicalNode->firstChild;

            int refPosition = strlen(objectReference);

            while (dataObject != NULL) {
                installDefaultValuesForDataObject(self, dataObject, objectReference, refPosition);

                dataObject = (DataObject*) dataObject->sibling;
            }

            logicalNode = (LogicalNode*) logicalNode->sibling;
        }

        logicalDevice = (LogicalDevice*) logicalDevice->sibling;
    }
}

static void
updateDataSetsWithCachedValues(IedServer self)
{
    DataSet* dataSet = self->model->dataSets;

    while (dataSet != NULL) {

        DataSetEntry* dataSetEntry = dataSet->fcdas;

        while (dataSetEntry != NULL) {

            MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, dataSetEntry->logicalDeviceName);

            MmsValue* value = MmsServer_getValueFromCache(self->mmsServer, domain, dataSetEntry->variableName);

            if (value == NULL) {
                if (DEBUG_IED_SERVER) {
                    printf("LD: %s dataset: %s : error cannot get value from cache for %s -> %s!\n",
                            dataSet->logicalDeviceName, dataSet->name,
                            dataSetEntry->logicalDeviceName,
                            dataSetEntry->variableName);
                }
            }
            else
                dataSetEntry->value = value;

            dataSetEntry = dataSetEntry->sibling;
        }

        dataSet = dataSet->sibling;
    }
}

IedServer
IedServer_create(IedModel* iedModel)
{
    IedServer self = (IedServer) calloc(1, sizeof(struct sIedServer));

    self->model = iedModel;

    self->mmsMapping = MmsMapping_create(iedModel);

    self->mmsDevice = MmsMapping_getMmsDeviceModel(self->mmsMapping);

    self->isoServer = IsoServer_create();

    self->mmsServer = MmsServer_create(self->isoServer, self->mmsDevice);

    MmsMapping_setMmsServer(self->mmsMapping, self->mmsServer);

    MmsMapping_installHandlers(self->mmsMapping);

    MmsMapping_setIedServer(self->mmsMapping, self);

    createMmsServerCache(self);

    iedModel->initializer();

    installDefaultValuesInCache(self); /* This will also connect cached MmsValues to DataAttributes */

    updateDataSetsWithCachedValues(self);

    self->clientConnections = LinkedList_create();

    /* default write access policy allows access to SP and SV FCDAs but denies access to DC and CF FCDAs */
    self->writeAccessPolicies = ALLOW_WRITE_ACCESS_SP | ALLOW_WRITE_ACCESS_SV;

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    Reporting_activateBufferedReports(self->mmsMapping);
#endif

    return self;
}

void
IedServer_destroy(IedServer self)
{
    //TODO wait for control and other threads to finish??

    MmsServer_destroy(self->mmsServer);
    IsoServer_destroy(self->isoServer);
    MmsMapping_destroy(self->mmsMapping);

    LinkedList_destroyDeep(self->clientConnections, (LinkedListValueDeleteFunction) private_ClientConnection_destroy);
    free(self);
}

void
IedServer_setAuthenticator(IedServer self, AcseAuthenticator authenticator, void* authenticatorParameter)
{
    MmsServer_setClientAuthenticator(self->mmsServer, authenticator, authenticatorParameter);
}

MmsServer
IedServer_getMmsServer(IedServer self)
{
    return self->mmsServer;
}

IsoServer
IedServer_getIsoServer(IedServer self)
{
    return self->isoServer;
}

void
IedServer_start(IedServer self, int tcpPort)
{
    MmsServer_startListening(self->mmsServer, tcpPort);
    MmsMapping_startEventWorkerThread(self->mmsMapping);
}

bool
IedServer_isRunning(IedServer self)
{
    if (IsoServer_getState(self->isoServer) == ISO_SVR_STATE_RUNNING)
        return true;
    else
        return false;
}

IedModel*
IedServer_getDataModel(IedServer self)
{
    return self->model;
}

void
IedServer_stop(IedServer self)
{
    MmsMapping_stopEventWorkerThread(self->mmsMapping);

    MmsServer_stopListening(self->mmsServer);
}

void
IedServer_lockDataModel(IedServer self)
{
    MmsServer_lockModel(self->mmsServer);
}

void
IedServer_unlockDataModel(IedServer self)
{
    MmsServer_unlockModel(self->mmsServer);
}

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
static ControlObject*
lookupControlObject(IedServer self, DataObject* node)
{
    char objectReference[130];

    ModelNode_getObjectReference((ModelNode*) node, objectReference);

    char* separator = strchr(objectReference, '/');

    *separator = 0;

    MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, objectReference);

    char* lnName = separator + 1;

    separator = strchr(lnName, '.');

    assert(separator != NULL);

    *separator = 0;

    char* objectName = separator + 1;

    StringUtils_replace(objectName, '.', '$');

    if (DEBUG_IED_SERVER)
        printf("IED_SERVER: looking for control object: %s\n", objectName);

    ControlObject* controlObject = MmsMapping_getControlObject(self->mmsMapping, domain,
            lnName, objectName);

    return controlObject;
}

void
IedServer_setControlHandler(
        IedServer self,
        DataObject* node,
        ControlHandler listener,
        void* parameter)
{
    ControlObject* controlObject = lookupControlObject(self, node);

    if (controlObject != NULL) {
        ControlObject_installListener(controlObject, listener, parameter);
        if (DEBUG_IED_SERVER)
            printf("IED_SERVER: Installed control handler for %s!\n", node->name);
    }
    else
        if (DEBUG_IED_SERVER)
            printf("IED_SERVER: Failed to install control handler!\n");
}

void
IedServer_setPerformCheckHandler(IedServer self, DataObject* node, ControlPerformCheckHandler handler, void* parameter)
{
    ControlObject* controlObject = lookupControlObject(self, node);

    if (controlObject != NULL)
        ControlObject_installCheckHandler(controlObject, handler, parameter);
}

void
IedServer_setWaitForExecutionHandler(IedServer self, DataObject* node, ControlWaitForExecutionHandler handler,
        void* parameter)
{
    ControlObject* controlObject = lookupControlObject(self, node);

    if (controlObject != NULL)
        ControlObject_installWaitForExecutionHandler(controlObject, handler, parameter);
}
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */

MmsValue*
IedServer_getAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    return dataAttribute->mmsValue;
}

bool
IedServer_getBooleanAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_BOOLEAN);

    return MmsValue_getBoolean(dataAttribute->mmsValue);
}

int32_t
IedServer_getInt32AttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert((MmsValue_getType(dataAttribute->mmsValue) == MMS_INTEGER) ||
            (MmsValue_getType(dataAttribute->mmsValue) == MMS_UNSIGNED));

    return MmsValue_toInt32(dataAttribute->mmsValue);
}

int64_t
IedServer_getInt64AttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert((MmsValue_getType(dataAttribute->mmsValue) == MMS_INTEGER) ||
            (MmsValue_getType(dataAttribute->mmsValue) == MMS_UNSIGNED));

    return MmsValue_toInt64(dataAttribute->mmsValue);
}

uint32_t
IedServer_getUInt32AttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert((MmsValue_getType(dataAttribute->mmsValue) == MMS_INTEGER) ||
            (MmsValue_getType(dataAttribute->mmsValue) == MMS_UNSIGNED));

    return MmsValue_toUint32(dataAttribute->mmsValue);
}

float
IedServer_getFloatAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_FLOAT);

    return MmsValue_toFloat(dataAttribute->mmsValue);
}

uint64_t
IedServer_getUTCTimeAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_UTC_TIME);

    return MmsValue_getUtcTimeInMs(dataAttribute->mmsValue);
}

uint32_t
IedServer_getBitStringAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_BIT_STRING);
    assert(MmsValue_getBitStringSize(dataAttribute->mmsValue) < 33);

    return MmsValue_getBitStringAsInteger(dataAttribute->mmsValue);
}

char*
IedServer_getStringAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(dataAttribute->mmsValue != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_VISIBLE_STRING);

    return MmsValue_toString(dataAttribute->mmsValue);
}

static inline void
checkForUpdateTrigger(IedServer self, DataAttribute* dataAttribute)
{
#if (CONFIG_IEC61850_REPORT_SERVICE== 1)
    if (dataAttribute->triggerOptions & TRG_OPT_DATA_UPDATE) {
        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue,
                REPORT_CONTROL_VALUE_UPDATE);
    }
#endif
}

static inline void
checkForChangedTriggers(IedServer self, DataAttribute* dataAttribute)
{
#if (CONFIG_IEC61850_REPORT_SERVICE== 1)
    if (dataAttribute->triggerOptions & TRG_OPT_DATA_CHANGED)
        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue,
                REPORT_CONTROL_VALUE_CHANGED);
    else if (dataAttribute->triggerOptions & TRG_OPT_QUALITY_CHANGED)
        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue,
                REPORT_CONTROL_QUALITY_CHANGED);
#endif /* (CONFIG_IEC61850_REPORT_SERVICE== 1) */

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
    MmsMapping_triggerGooseObservers(self->mmsMapping, dataAttribute->mmsValue);
#endif
}

void
IedServer_updateAttributeValue(IedServer self, DataAttribute* dataAttribute, MmsValue* value)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MmsValue_getType(value));

    if (MmsValue_equals(dataAttribute->mmsValue, value))
        checkForUpdateTrigger(self, dataAttribute);
    else {
        MmsValue_update(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateFloatAttributeValue(IedServer self, DataAttribute* dataAttribute, float value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_FLOAT);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    float currentValue = MmsValue_toFloat(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setFloat(dataAttribute->mmsValue, value);
        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateInt32AttributeValue(IedServer self, DataAttribute* dataAttribute, int32_t value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_INTEGER);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    int32_t currentValue = MmsValue_toInt32(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setInt32(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateInt64AttributeValue(IedServer self, DataAttribute* dataAttribute, int64_t value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_INTEGER);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    int64_t currentValue = MmsValue_toInt64(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setInt64(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateUnsignedAttributeValue(IedServer self, DataAttribute* dataAttribute, uint32_t value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_UNSIGNED);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    uint32_t currentValue = MmsValue_toUint32(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setUint32(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateBitStringAttributeValue(IedServer self, DataAttribute* dataAttribute, uint32_t value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_BIT_STRING);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    uint32_t currentValue = MmsValue_getBitStringAsInteger(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setBitStringFromInteger(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateBooleanAttributeValue(IedServer self, DataAttribute* dataAttribute, bool value)
{
    assert(self != NULL);
    assert(dataAttribute != NULL);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_BOOLEAN);

    bool currentValue = MmsValue_getBoolean(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setBoolean(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateVisibleStringAttributeValue(IedServer self, DataAttribute* dataAttribute, char *value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_VISIBLE_STRING);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    char *currentValue = MmsValue_toString(dataAttribute->mmsValue);

    if (!strcmp(currentValue ,value)) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setVisibleString(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateUTCTimeAttributeValue(IedServer self, DataAttribute* dataAttribute, uint64_t value)
{
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_UTC_TIME);
    assert(dataAttribute != NULL);
    assert(self != NULL);

    uint64_t currentValue = MmsValue_getUtcTimeInMs(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setUtcTimeMs(dataAttribute->mmsValue, value);

        checkForChangedTriggers(self, dataAttribute);
    }
}

void
IedServer_updateQuality(IedServer self, DataAttribute* dataAttribute, Quality quality)
{
    assert(strcmp(dataAttribute->name, "q") == 0);
    assert(MmsValue_getType(dataAttribute->mmsValue) == MMS_BIT_STRING);
    assert(MmsValue_getBitStringSize(dataAttribute->mmsValue) >= 12);
    assert(MmsValue_getBitStringSize(dataAttribute->mmsValue) <= 15);

    uint32_t oldQuality = MmsValue_getBitStringAsInteger(dataAttribute->mmsValue);

    if (oldQuality != (uint32_t) quality) {
        MmsValue_setBitStringFromInteger(dataAttribute->mmsValue, (uint32_t) quality);

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
        if (dataAttribute->triggerOptions & TRG_OPT_QUALITY_CHANGED)
            MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue,
                                REPORT_CONTROL_QUALITY_CHANGED);
#endif

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
        MmsMapping_triggerGooseObservers(self->mmsMapping, dataAttribute->mmsValue);
#endif
    }


}

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
void
IedServer_enableGoosePublishing(IedServer self)
{
    MmsMapping_enableGoosePublishing(self->mmsMapping);
}
#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

void
IedServer_observeDataAttribute(IedServer self, DataAttribute* dataAttribute,
        AttributeChangedHandler handler)
{
    MmsMapping_addObservedAttribute(self->mmsMapping, dataAttribute, handler);
}

void
IedServer_setWriteAccessPolicy(IedServer self, FunctionalConstraint fc, AccessPolicy policy)
{
    if (policy == ACCESS_POLICY_ALLOW) {
        switch (fc) {
        case DC:
            self->writeAccessPolicies |= ALLOW_WRITE_ACCESS_DC;
            break;
        case CF:
            self->writeAccessPolicies |= ALLOW_WRITE_ACCESS_CF;
            break;
        case SP:
            self->writeAccessPolicies |= ALLOW_WRITE_ACCESS_SP;
            break;
        case SV:
            self->writeAccessPolicies |= ALLOW_WRITE_ACCESS_SV;
            break;
        default: /* ignore - request is invalid */
            break;
        }
    }
    else {
        switch (fc) {
        case DC:
            self->writeAccessPolicies &= ~ALLOW_WRITE_ACCESS_DC;
            break;
        case CF:
            self->writeAccessPolicies &= ~ALLOW_WRITE_ACCESS_CF;
            break;
        case SP:
            self->writeAccessPolicies &= ~ALLOW_WRITE_ACCESS_SP;
            break;
        case SV:
            self->writeAccessPolicies &= ~ALLOW_WRITE_ACCESS_SV;
            break;
        default: /* ignore - request is invalid */
            break;
        }
    }
}

void
IedServer_handleWriteAccess(IedServer self, DataAttribute* dataAttribute, WriteAccessHandler handler)
{
    MmsMapping_installWriteAccessHandler(self->mmsMapping, dataAttribute, handler);
}

void
IedServer_setConnectionIndicationHandler(IedServer self, IedConnectionIndicationHandler handler, void* parameter)
{
    MmsMapping_setConnectionIndicationHandler(self->mmsMapping, handler, parameter);
}

MmsValue*
IedServer_getFunctionalConstrainedData(IedServer self, DataObject* dataObject, FunctionalConstraint fc)
{
    char buffer[128]; /* buffer for variable name string */
    char* currentStart = buffer + 127;
    currentStart[0] = 0;

    int nameLen;

    while (dataObject->modelType == DataObjectModelType) {
        nameLen = strlen(dataObject->name);
        currentStart -= nameLen;
        memcpy(currentStart, dataObject->name, nameLen);
        currentStart--;
        *currentStart = '$';

        if (dataObject->parent->modelType == DataObjectModelType)
            dataObject = (DataObject*) dataObject->parent;
        else
            break;
    }

    char* fcString = FunctionalConstraint_toString(fc);

    currentStart--;
    *currentStart = fcString[1];
    currentStart--;
    *currentStart = fcString[0];
    currentStart--;
    *currentStart = '$';

    LogicalNode* ln = (LogicalNode*) dataObject->parent;

    nameLen = strlen(ln->name);

    currentStart -= nameLen;
    memcpy(currentStart, ln->name, nameLen);

    LogicalDevice* ld = (LogicalDevice*) ln->parent;

    MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, ld->name);

    MmsValue* value = MmsServer_getValueFromCache(self->mmsServer, domain, currentStart);

    return value;
}

ClientConnection
private_IedServer_getClientConnectionByHandle(IedServer self, void* serverConnectionHandle)
{
    LinkedList element = LinkedList_getNext(self->clientConnections);
    ClientConnection matchingConnection = NULL;

    while (element != NULL) {
        ClientConnection connection = (ClientConnection) element->data;

        if (private_ClientConnection_getServerConnectionHandle(connection) == serverConnectionHandle) {
            matchingConnection = connection;
            break;
        }

        element = LinkedList_getNext(element);
    }

    return matchingConnection;
}

void
private_IedServer_addNewClientConnection(IedServer self, ClientConnection newClientConnection)
{
    LinkedList_add(self->clientConnections, (void*) newClientConnection);
}

void
private_IedServer_removeClientConnection(IedServer self, ClientConnection clientConnection)
{
    LinkedList_remove(self->clientConnections, clientConnection);
}


