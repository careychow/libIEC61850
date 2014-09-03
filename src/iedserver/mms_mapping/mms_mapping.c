/*
 *  mms_mapping.c
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

#include "libiec61850_platform_includes.h"
#include "mms_mapping.h"
#include "mms_mapping_internal.h"
#include "array_list.h"
#include "stack_config.h"

#include "mms_goose.h"
#include "reporting.h"
#include "control.h"
#include "ied_server_private.h"

#ifndef DEBUG_IDE_SERVER
#define DEBUG_IDE_SERVER 0
#endif

typedef struct
{
    DataAttribute* attribute;
    AttributeChangedHandler handler;
} AttributeObserver;

typedef struct
{
    DataAttribute* attribute;
    WriteAccessHandler handler;
} AttributeAccessHandler;

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
MmsValue*
Control_readAccessControlObject(MmsMapping* self, MmsDomain* domain, char* variableIdOrig,
        MmsServerConnection* connection);
#endif

void /* Create PHYCOMADDR ACSI type instance */
MmsMapping_createPhyComAddrStructure(MmsVariableSpecification* namedVariable)
{
    namedVariable->type = MMS_STRUCTURE;
    namedVariable->typeSpec.structure.elementCount = 4;
    namedVariable->typeSpec.structure.elements = (MmsVariableSpecification**) calloc(4,
            sizeof(MmsVariableSpecification*));

    MmsVariableSpecification* element;

    element = (MmsVariableSpecification*) calloc(1, sizeof(MmsVariableSpecification));
    element->name = copyString("Addr");
    element->type = MMS_OCTET_STRING;
    element->typeSpec.octetString = 6;
    namedVariable->typeSpec.structure.elements[0] = element;

    element = (MmsVariableSpecification*) calloc(1, sizeof(MmsVariableSpecification));
    element->name = copyString("PRIORITY");
    element->type = MMS_UNSIGNED;
    element->typeSpec.unsignedInteger = 8;
    namedVariable->typeSpec.structure.elements[1] = element;

    element = (MmsVariableSpecification*) calloc(1, sizeof(MmsVariableSpecification));
    element->name = copyString("VID");
    element->type = MMS_UNSIGNED;
    element->typeSpec.unsignedInteger = 16;
    namedVariable->typeSpec.structure.elements[2] = element;

    element = (MmsVariableSpecification*) calloc(1, sizeof(MmsVariableSpecification));
    element->name = copyString("APPID");
    element->type = MMS_UNSIGNED;
    element->typeSpec.unsignedInteger = 16;
    namedVariable->typeSpec.structure.elements[3] = element;
}

static MmsVariableSpecification*
createNamedVariableFromDataAttribute(DataAttribute* attribute)
{
    MmsVariableSpecification* origNamedVariable = (MmsVariableSpecification*) calloc(1,
            sizeof(MmsVariableSpecification));
    origNamedVariable->name = copyString(attribute->name);

    MmsVariableSpecification* namedVariable = origNamedVariable;

    if (attribute->elementCount > 0) {
        namedVariable->type = MMS_ARRAY;
        namedVariable->typeSpec.array.elementCount = attribute->elementCount;
        namedVariable->typeSpec.array.elementTypeSpec = (MmsVariableSpecification*) calloc(1,
                sizeof(MmsVariableSpecification));
        namedVariable = namedVariable->typeSpec.array.elementTypeSpec;
    }

    if (attribute->firstChild != NULL) {
        namedVariable->type = MMS_STRUCTURE;

        int componentCount = ModelNode_getChildCount((ModelNode*) attribute);

        namedVariable->typeSpec.structure.elements = (MmsVariableSpecification**) calloc(componentCount,
                sizeof(MmsVariableSpecification*));

        DataAttribute* subDataAttribute = (DataAttribute*) attribute->firstChild;

        int i = 0;
        while (subDataAttribute != NULL) {
            namedVariable->typeSpec.structure.elements[i] =
                    createNamedVariableFromDataAttribute(subDataAttribute);

            subDataAttribute = (DataAttribute*) subDataAttribute->sibling;
            i++;
        }

        namedVariable->typeSpec.structure.elementCount = i;
    }
    else {
        switch (attribute->type) {
        case BOOLEAN:
            namedVariable->type = MMS_BOOLEAN;
            break;
        case INT8:
            namedVariable->typeSpec.integer = 8;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT16:
            namedVariable->typeSpec.integer = 16;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT32:
            namedVariable->typeSpec.integer = 32;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT64:
            namedVariable->typeSpec.integer = 64;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT128:
            namedVariable->typeSpec.integer = 128;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT8U:
            namedVariable->typeSpec.unsignedInteger = 8;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case INT16U:
            namedVariable->typeSpec.unsignedInteger = 16;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case INT24U:
            namedVariable->typeSpec.unsignedInteger = 24;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case INT32U:
            namedVariable->typeSpec.unsignedInteger = 32;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case FLOAT32:
            namedVariable->typeSpec.floatingpoint.formatWidth = 32;
            namedVariable->typeSpec.floatingpoint.exponentWidth = 8;
            namedVariable->type = MMS_FLOAT;
            break;
        case FLOAT64:
            namedVariable->typeSpec.floatingpoint.formatWidth = 64;
            namedVariable->typeSpec.floatingpoint.exponentWidth = 11;
            namedVariable->type = MMS_FLOAT;
            break;
        case ENUMERATED:
            namedVariable->typeSpec.integer = 8; /* 8 bit integer should be enough for all enumerations */
            namedVariable->type = MMS_INTEGER;
            break;
        case CHECK:
            namedVariable->typeSpec.bitString = -2;
            namedVariable->type = MMS_BIT_STRING;
            break;
        case CODEDENUM:
            namedVariable->typeSpec.bitString = 2;
            namedVariable->type = MMS_BIT_STRING;
            break;
        case OCTET_STRING_6:
            namedVariable->typeSpec.octetString = -6;
            namedVariable->type = MMS_OCTET_STRING;
            break;
        case OCTET_STRING_8:
            namedVariable->typeSpec.octetString = 8;
            namedVariable->type = MMS_OCTET_STRING;
            break;
        case OCTET_STRING_64:
            namedVariable->typeSpec.octetString = -64;
            namedVariable->type = MMS_OCTET_STRING;
            break;
        case VISIBLE_STRING_32:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_64:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_65:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_129:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_255:
            namedVariable->typeSpec.visibleString = -255;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case UNICODE_STRING_255:
            namedVariable->typeSpec.mmsString = -255;
            namedVariable->type = MMS_STRING;
            break;
        case GENERIC_BITSTRING:
            namedVariable->type = MMS_BIT_STRING;
            break;
        case TIMESTAMP:
            namedVariable->type = MMS_UTC_TIME;
            break;
        case QUALITY:
            namedVariable->typeSpec.bitString = -13; // -13 = up to 13 bits
            namedVariable->type = MMS_BIT_STRING;
            break;
        case ENTRY_TIME:
            namedVariable->type = MMS_BINARY_TIME;
            namedVariable->typeSpec.binaryTime = 6;
            break;
        case PHYCOMADDR:
            MmsMapping_createPhyComAddrStructure(namedVariable);
            break;
        default:
            if (DEBUG_IDE_SERVER)
                printf("MMS-MAPPING: type cannot be mapped %i\n", attribute->type);
            break;
        }
    }

    return origNamedVariable;
}

static int
countChildrenWithFc(DataObject* dataObject, FunctionalConstraint fc)
{
    int elementCount = 0;

    ModelNode* child = dataObject->firstChild;

    while (child != NULL) {
        if (child->modelType == DataAttributeModelType) {
            DataAttribute* dataAttribute = (DataAttribute*) child;

            if (dataAttribute->fc == fc)
                elementCount++;
        }
        else if (child->modelType == DataObjectModelType) {
            DataObject* subDataObject = (DataObject*) child;

            if (DataObject_hasFCData(subDataObject, fc))
                elementCount++;
        }

        child = child->sibling;
    }

    return elementCount;
}

static MmsVariableSpecification*
createFCNamedVariableFromDataObject(DataObject* dataObject,
        FunctionalConstraint fc)
{
    MmsVariableSpecification* namedVariable = (MmsVariableSpecification*) calloc(1,
            sizeof(MmsVariableSpecification));

    MmsVariableSpecification* completeNamedVariable = namedVariable;

    namedVariable->name = copyString(dataObject->name);

    if (dataObject->elementCount > 0) {
        namedVariable->type = MMS_ARRAY;
        namedVariable->typeSpec.array.elementCount = dataObject->elementCount;
        namedVariable->typeSpec.array.elementTypeSpec = (MmsVariableSpecification*) calloc(1,
                sizeof(MmsVariableSpecification));
        namedVariable = namedVariable->typeSpec.array.elementTypeSpec;
    }

    namedVariable->type = MMS_STRUCTURE;

    int elementCount = countChildrenWithFc(dataObject, fc);

    /* Allocate memory for components */
    namedVariable->typeSpec.structure.elements = (MmsVariableSpecification**) calloc(elementCount,
            sizeof(MmsVariableSpecification*));

    int i = 0;
    ModelNode* component = dataObject->firstChild;

    while (component != NULL) {
        if (component->modelType == DataAttributeModelType) {
            DataAttribute* dataAttribute = (DataAttribute*) component;

            if (dataAttribute->fc == fc) {
                namedVariable->typeSpec.structure.elements[i] =
                        createNamedVariableFromDataAttribute(dataAttribute);
                i++;
            }
        }
        else if (component->modelType == DataObjectModelType) {
            DataObject* subDataObject = (DataObject*) component;

            if (DataObject_hasFCData(subDataObject, fc)) {
                namedVariable->typeSpec.structure.elements[i] =
                        createFCNamedVariableFromDataObject(subDataObject, fc);
                i++;
            }

        }

        component = component->sibling;
    }

    namedVariable->typeSpec.structure.elementCount = elementCount;

    return completeNamedVariable;
}

static MmsVariableSpecification*
createFCNamedVariable(LogicalNode* logicalNode, FunctionalConstraint fc)
{
    MmsVariableSpecification* namedVariable = (MmsVariableSpecification*) calloc(1,
            sizeof(MmsVariableSpecification));
    namedVariable->name = copyString(FunctionalConstraint_toString(fc));
    namedVariable->type = MMS_STRUCTURE;

    int dataObjectCount = 0;

    DataObject* dataObject = (DataObject*) logicalNode->firstChild;

    while (dataObject != NULL) {
        if (DataObject_hasFCData(dataObject, fc))
            dataObjectCount++;

        dataObject = (DataObject*) dataObject->sibling;
    }

    namedVariable->typeSpec.structure.elementCount = dataObjectCount;
    namedVariable->typeSpec.structure.elements = (MmsVariableSpecification**) calloc(dataObjectCount,
            sizeof(MmsVariableSpecification*));

    dataObjectCount = 0;

    dataObject = (DataObject*) logicalNode->firstChild;

    while (dataObject != NULL) {
        if (DataObject_hasFCData(dataObject, fc)) {

            namedVariable->typeSpec.structure.elements[dataObjectCount] =
                    createFCNamedVariableFromDataObject(dataObject, fc);

            dataObjectCount++;
        }

        dataObject = (DataObject*) dataObject->sibling;
    }

    return namedVariable;
}

static int
determineLogicalNodeComponentCount(LogicalNode* logicalNode)
{
    int componentCount = 0;

    if (LogicalNode_hasFCData(logicalNode, ST))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, MX))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SP))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SV))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, CF))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, DC))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SG))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SE))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SR))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, OR))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, BL))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, EX))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, CO))
        componentCount++;

    return componentCount;
}


#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
static int
countReportControlBlocksForLogicalNode(MmsMapping* self, LogicalNode* logicalNode, bool buffered)
{
    int rcbCount = 0;

    ReportControlBlock* rcb = self->model->rcbs;

    /* Iterate list of RCBs */
    while (rcb != NULL) {
        if (rcb->parent == logicalNode) {
            if (rcb->buffered == buffered)
                rcbCount++;
        }

        rcb = rcb->sibling;
    }

    return rcbCount;
}
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */


#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

static int
countGSEControlBlocksForLogicalNode(MmsMapping* self, LogicalNode* logicalNode)
{
    int gseCount = 0;

    GSEControlBlock* gcb = self->model->gseCBs;

    while (gcb != NULL) {
        if (gcb->parent == logicalNode) {
            gseCount++;
        }

        gcb = gcb->sibling;
    }

    return gseCount;
}

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

static MmsVariableSpecification*
createNamedVariableFromLogicalNode(MmsMapping* self, MmsDomain* domain,
        LogicalNode* logicalNode)
{
    MmsVariableSpecification* namedVariable = (MmsVariableSpecification*)
            malloc(sizeof(MmsVariableSpecification));

    namedVariable->name = copyString(logicalNode->name);

    namedVariable->type = MMS_STRUCTURE;

    int componentCount = determineLogicalNodeComponentCount(logicalNode);

    if (DEBUG_IDE_SERVER)
        printf("LogicalNode %s has %i fc components\n", logicalNode->name,
                componentCount);

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    int brcbCount = countReportControlBlocksForLogicalNode(self, logicalNode,
    true);

    if (brcbCount > 0) {
        if (DEBUG_IDE_SERVER)
            printf("  and %i buffered RCBs\n", brcbCount);
        componentCount++;
    }

    int urcbCount = countReportControlBlocksForLogicalNode(self, logicalNode,
    false);

    if (urcbCount > 0) {
        if (DEBUG_IDE_SERVER)
            printf("  and %i unbuffered RCBs\n", urcbCount);
        componentCount++;
    }
#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

    int gseCount = countGSEControlBlocksForLogicalNode(self, logicalNode);

    if (gseCount > 0) {
        if (DEBUG_IDE_SERVER)
            printf("   and %i GSE control blocks\n", gseCount);
        componentCount++;
    }

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

    namedVariable->typeSpec.structure.elements = (MmsVariableSpecification**) calloc(componentCount,
            sizeof(MmsVariableSpecification*));

    /* Create a named variable of type structure for each functional constrained */
    int currentComponent = 0;

    if (LogicalNode_hasFCData(logicalNode, MX)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, MX);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, ST)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, ST);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, CO)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, CO);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, CF)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, CF);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, DC)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, DC);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, SP)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, SP);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, SG)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, SG);
        currentComponent++;
    }

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    if (urcbCount > 0) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                Reporting_createMmsUnbufferedRCBs(self, domain, logicalNode,
                        urcbCount);
        currentComponent++;
    }
#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */

    /* TODO create LCBs here */

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    if (brcbCount > 0) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                Reporting_createMmsBufferedRCBs(self, domain, logicalNode,
                        brcbCount);
        currentComponent++;
    }
#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
    if (gseCount > 0) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                GOOSE_createGOOSEControlBlocks(self, domain, logicalNode, gseCount);

        currentComponent++;
    }
#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

    if (LogicalNode_hasFCData(logicalNode, SV)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, SV);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, SE)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, SE);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, EX)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, EX);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, SR)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, SR);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, OR)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, OR);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, BL)) {
        namedVariable->typeSpec.structure.elements[currentComponent] =
                createFCNamedVariable(logicalNode, BL);
        currentComponent++;
    }

    namedVariable->typeSpec.structure.elementCount = currentComponent;

    return namedVariable;
}

static MmsDomain*
createMmsDomainFromIedDevice(MmsMapping* self, LogicalDevice* logicalDevice)
{
    MmsDomain* domain = MmsDomain_create(logicalDevice->name);

    int nodesCount = LogicalDevice_getLogicalNodeCount(logicalDevice);

    /* Logical nodes are first level named variables */
    domain->namedVariablesCount = nodesCount;
    domain->namedVariables = (MmsVariableSpecification**) malloc(nodesCount * sizeof(MmsVariableSpecification*));

    LogicalNode* logicalNode = (LogicalNode*) logicalDevice->firstChild;

    int i = 0;
    while (logicalNode != NULL) {
        domain->namedVariables[i] = createNamedVariableFromLogicalNode(self,
                domain, logicalNode);

        logicalNode = (LogicalNode*) logicalNode->sibling;
        i++;
    }

    return domain;
}

static void
createMmsDataModel(MmsMapping* self, int iedDeviceCount,
        MmsDevice* mmsDevice, IedModel* iedModel)
{
    mmsDevice->domains = (MmsDomain**) malloc((iedDeviceCount) * sizeof(MmsDomain*));
    mmsDevice->domainCount = iedDeviceCount;

    LogicalDevice* logicalDevice = iedModel->firstChild;

    int i = 0;
    while (logicalDevice != NULL) {
        mmsDevice->domains[i] = createMmsDomainFromIedDevice(self,
                logicalDevice);
        i++;
        logicalDevice = (LogicalDevice*) logicalDevice->sibling;
    }
}

static void
createDataSets(MmsDevice* mmsDevice, IedModel* iedModel)
{
    DataSet* dataset = iedModel->dataSets;

    while (dataset != NULL) {
        MmsDomain* dataSetDomain = MmsDevice_getDomain(mmsDevice, dataset->logicalDeviceName);

        MmsNamedVariableList varList = MmsNamedVariableList_create(dataset->name, false);

        DataSetEntry* dataSetEntry = dataset->fcdas;

        while (dataSetEntry != NULL) {

            MmsAccessSpecifier accessSpecifier;

            accessSpecifier.domain = MmsDevice_getDomain(mmsDevice,
                    dataSetEntry->logicalDeviceName);

            accessSpecifier.variableName = dataSetEntry->variableName;
            accessSpecifier.arrayIndex = dataSetEntry->index;
            accessSpecifier.componentName = dataSetEntry->componentName;

            MmsNamedVariableListEntry variableListEntry =
                    MmsNamedVariableListEntry_create(accessSpecifier);

            MmsNamedVariableList_addVariable(varList, variableListEntry);

            dataSetEntry = dataSetEntry->sibling;
        }

        MmsDomain_addNamedVariableList(dataSetDomain, varList);

        dataset = dataset->sibling;
    }
}

static MmsDevice*
createMmsModelFromIedModel(MmsMapping* self, IedModel* iedModel)
{
    MmsDevice* mmsDevice = NULL;

    if (iedModel->firstChild != NULL) {

        mmsDevice = MmsDevice_create(iedModel->name);

        int iedDeviceCount = IedModel_getLogicalDeviceCount(iedModel);

        createMmsDataModel(self, iedDeviceCount, mmsDevice, iedModel);

        createDataSets(mmsDevice, iedModel);
    }

    return mmsDevice;
}

MmsMapping*
MmsMapping_create(IedModel* model)
{
    MmsMapping* self = (MmsMapping*) calloc(1, sizeof(struct sMmsMapping));

    self->model = model;

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    self->reportControls = LinkedList_create();
#endif

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
        self->gseControls = LinkedList_create();
#endif

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
    self->controlObjects = LinkedList_create();
#endif

    self->observedObjects = LinkedList_create();

    self->attributeAccessHandlers = LinkedList_create();

    self->mmsDevice = createMmsModelFromIedModel(self, model);

    return self;
}

void
MmsMapping_destroy(MmsMapping* self)
{

    if (self->reportWorkerThread != NULL) {
        self->reportThreadRunning = false;
        Thread_destroy(self->reportWorkerThread);
    }

    if (self->mmsDevice != NULL)
        MmsDevice_destroy(self->mmsDevice);

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    LinkedList_destroyDeep(self->reportControls, (LinkedListValueDeleteFunction) ReportControl_destroy);
#endif

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
    LinkedList_destroyDeep(self->gseControls, (LinkedListValueDeleteFunction) MmsGooseControlBlock_destroy);
#endif

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
    LinkedList_destroyDeep(self->controlObjects, (LinkedListValueDeleteFunction) ControlObject_destroy);
#endif

    LinkedList_destroy(self->observedObjects);

    LinkedList_destroy(self->attributeAccessHandlers);

    IedModel_setAttributeValuesToNull(self->model);

    free(self);
}

MmsDevice*
MmsMapping_getMmsDeviceModel(MmsMapping* mapping)
{
    return mapping->mmsDevice;
}

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
static bool
isReportControlBlock(char* separator)
{
    if (strncmp(separator + 1, "RP", 2) == 0)
        return true;

    if (strncmp(separator + 1, "BR", 2) == 0)
        return true;

    return false;
}
#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */

static bool
isFunctionalConstraintCF(char* separator)
{
    if (strncmp(separator + 1, "CF", 2) == 0)
        return true;
    else
        return false;
}

static bool
isFunctionalConstraintDC(char* separator)
{
    if (strncmp(separator + 1, "DC", 2) == 0)
        return true;
    else
        return false;
}

static bool
isFunctionalConstraintSP(char* separator)
{
    if (strncmp(separator + 1, "SP", 2) == 0)
        return true;
    else
        return false;
}

static bool
isFunctionalConstraintSV(char* separator)
{
    if (strncmp(separator + 1, "SV", 2) == 0)
        return true;
    else
        return false;
}

static bool
isWritableFC(char* separator)
{
    if (isFunctionalConstraintCF(separator))
        return true;

    if (isFunctionalConstraintDC(separator))
        return true;

    if (isFunctionalConstraintSP(separator))
        return true;

    if (isFunctionalConstraintSV(separator))
        return true;

    return false;
}

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
static bool
isControllable(char* separator)
{
    if (strncmp(separator + 1, "CO", 2) == 0)
        return true;
    else
        return false;
}
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

static bool
isGooseControlBlock(char* separator)
{
    if (strncmp(separator + 1, "GO", 2) == 0)
        return true;
    else
        return false;
}

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

char*
MmsMapping_getNextNameElement(char* name)
{
    char* separator = strchr(name, '$');

    if (separator == NULL)
        return NULL;

    separator++;

    if (*separator == 0)
        return NULL;

    return separator;
}

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

static MmsGooseControlBlock
lookupGCB(MmsMapping* self, MmsDomain* domain, char* lnName, char* objectName)
{
    LinkedList element = LinkedList_getNext(self->gseControls);

    while (element != NULL) {
        MmsGooseControlBlock mmsGCB = (MmsGooseControlBlock) element->data;

        if (MmsGooseControlBlock_getDomain(mmsGCB) == domain) {
            if (strcmp(MmsGooseControlBlock_getLogicalNodeName(mmsGCB), lnName) == 0) {
                if (strcmp(MmsGooseControlBlock_getName(mmsGCB), objectName) == 0) {
                    return mmsGCB;
                }
            }
        }

        element = LinkedList_getNext(element);
    }

    return NULL;
}

static MmsDataAccessError
writeAccessGooseControlBlock(MmsMapping* self, MmsDomain* domain, char* variableIdOrig,
        MmsValue* value)
{
    char variableId[130];

    strncpy(variableId, variableIdOrig, 129);

    char* separator = strchr(variableId, '$');

    *separator = 0;

    char* lnName = variableId;

    if (lnName == NULL)
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;

    char* objectName = MmsMapping_getNextNameElement(separator + 1);

    if (objectName == NULL)
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;

    char* varName = MmsMapping_getNextNameElement(objectName);

    if (varName != NULL)
        *(varName - 1) = 0;
    else
       return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;

    MmsGooseControlBlock mmsGCB = lookupGCB(self, domain, lnName, objectName);

    if (mmsGCB == NULL)
        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;

    if (strcmp(varName, "GoEna") == 0) {
        if (MmsValue_getType(value) != MMS_BOOLEAN)
            return DATA_ACCESS_ERROR_TYPE_INCONSISTENT;

        if (MmsValue_getBoolean(value))
            MmsGooseControlBlock_enable(mmsGCB);
        else
            MmsGooseControlBlock_disable(mmsGCB);

        return DATA_ACCESS_ERROR_SUCCESS;
    }
    else {
        if (MmsGooseControlBlock_isEnabled(mmsGCB))
            return DATA_ACCESS_ERROR_TEMPORARILY_UNAVAILABLE;
        else {
            MmsValue* subValue = MmsValue_getSubElement(MmsGooseControlBlock_getMmsValues(mmsGCB),
                    MmsGooseControlBlock_getVariableSpecification(mmsGCB), varName);

            if (subValue == NULL)
                return DATA_ACCESS_ERROR_INVALID_ADDRESS;

            if (MmsValue_update(subValue, value))
                return DATA_ACCESS_ERROR_SUCCESS;
            else
                return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
        }
    }
}

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

static bool
checkIfValueBelongsToModelNode(DataAttribute* dataAttribute, MmsValue* value)
{
    if (dataAttribute->mmsValue == value)
        return true;

    DataAttribute* child = (DataAttribute*) dataAttribute->firstChild;

    while (child != NULL) {
        if (checkIfValueBelongsToModelNode(child, value))
            return true;
        else
            child = (DataAttribute*) child->sibling;
    }

    if (MmsValue_getType(value) == MMS_STRUCTURE) {
        int elementCount = MmsValue_getArraySize(value);

        int i;
        for (i = 0; i < elementCount; i++) {
            MmsValue* childValue = MmsValue_getElement(value, i);

            if (checkIfValueBelongsToModelNode(dataAttribute, childValue))
                return true;
        }
    }

    return false;
}

static MmsDataAccessError
mmsWriteHandler(void* parameter, MmsDomain* domain,
        char* variableId, MmsValue* value, MmsServerConnection* connection)
{
    MmsMapping* self = (MmsMapping*) parameter;

    if (DEBUG_IED_SERVER)
        printf("Write requested %s\n", variableId);

    /* Access control based on functional constraint */

    char* separator = strchr(variableId, '$');

    if (separator == NULL)
        return DATA_ACCESS_ERROR_INVALID_ADDRESS;

    int lnNameLength = separator - variableId;

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
    /* Controllable objects - CO */
    if (isControllable(separator)) {
        return Control_writeAccessControlObject(self, domain, variableId, value,
                connection);
    }
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
    /* Goose control block - GO */
    if (isGooseControlBlock(separator)) {
        return writeAccessGooseControlBlock(self, domain, variableId, value);
    }
#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    /* Report control blocks - BR, RP */
    if (isReportControlBlock(separator)) {

        char* reportName = MmsMapping_getNextNameElement(separator + 1);

        if (reportName == NULL)
            return DATA_ACCESS_ERROR_OBJECT_NONE_EXISTENT;

        separator = strchr(reportName, '$');

        int variableIdLen;

        if (separator != NULL)
            variableIdLen = separator - variableId;
        else
            variableIdLen = strlen(variableId);

        LinkedList nextElement = self->reportControls;

        while ((nextElement = LinkedList_getNext(nextElement)) != NULL) {
            ReportControl* rc = (ReportControl*) nextElement->data;

            if (rc->domain == domain) {

                int parentLNNameStrLen = strlen(rc->parentLN->name);

                if (parentLNNameStrLen != lnNameLength)
                    continue;

                if (memcmp(rc->parentLN->name, variableId, parentLNNameStrLen) != 0)
                    continue;

                int rcNameLen = strlen(rc->name);

                if (rcNameLen == variableIdLen) {

                    if (strncmp(variableId, rc->name, variableIdLen) == 0) {
                        char* elementName = variableId + rcNameLen + 1;

                        return Reporting_RCBWriteAccessHandler(self, rc, elementName, value, connection);
                    }
                }
            }
        }

        return DATA_ACCESS_ERROR_OBJECT_NONE_EXISTENT;
    }
#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */


    /* writable data model elements - SP, SV, CF, DC */
    if (isWritableFC(separator)) {

        MmsValue* cachedValue;

        cachedValue = MmsServer_getValueFromCache(self->mmsServer, domain, variableId);

        if (cachedValue != NULL) {

            if (!MmsValue_equalTypes(cachedValue, value))
                return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;

            bool handlerFound = false;

            /* Call writer access handlers */
            LinkedList writeHandlerListElement = LinkedList_getNext(self->attributeAccessHandlers);

            while (writeHandlerListElement != NULL) {
                AttributeAccessHandler* accessHandler = (AttributeAccessHandler*) writeHandlerListElement->data;
                DataAttribute* dataAttribute = accessHandler->attribute;

                if (checkIfValueBelongsToModelNode(dataAttribute, cachedValue)) {
                    if (accessHandler->handler(dataAttribute, value, (ClientConnection) connection)) {
                        handlerFound = true;
                        break;
                    }
                    else
                        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
                }

                writeHandlerListElement = LinkedList_getNext(writeHandlerListElement);
            }

            /* if no access handler is found check for default policy for FC */
            if (!handlerFound) {
                if (isFunctionalConstraintCF(separator)) {
                    if (!(self->iedServer->writeAccessPolicies & ALLOW_WRITE_ACCESS_CF))
                        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
                }
                else if (isFunctionalConstraintDC(separator)) {
                    if (!(self->iedServer->writeAccessPolicies & ALLOW_WRITE_ACCESS_DC))
                        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
                }
                else if (isFunctionalConstraintSP(separator)) {
                    if (!(self->iedServer->writeAccessPolicies & ALLOW_WRITE_ACCESS_SP))
                        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
                }
                else if (isFunctionalConstraintSV(separator)) {
                    if (!(self->iedServer->writeAccessPolicies & ALLOW_WRITE_ACCESS_SV))
                        return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
                }
            }

            DataAttribute* da = IedModel_lookupDataAttributeByMmsValue(self->model, cachedValue);

            if (da != NULL)
                IedServer_updateAttributeValue(self->iedServer, da, value);

            /* Call observer callback */
            LinkedList observerListElement = LinkedList_getNext(self->observedObjects);

            while (observerListElement != NULL) {
                AttributeObserver* observer = (AttributeObserver*) observerListElement->data;
                DataAttribute* dataAttribute = observer->attribute;

                if (checkIfValueBelongsToModelNode(dataAttribute, cachedValue)) {
                    observer->handler(dataAttribute, (ClientConnection) connection);
                    break; /* only all one handler per data attribute */
                }

                observerListElement = LinkedList_getNext(observerListElement);
            }

            return DATA_ACCESS_ERROR_SUCCESS;
        }
        else
            return DATA_ACCESS_ERROR_OBJECT_VALUE_INVALID;
    }

    return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
}

void
MmsMapping_addObservedAttribute(MmsMapping* self, DataAttribute* dataAttribute,
        void* handler)
{
    AttributeObserver* observer = (AttributeObserver*) malloc(sizeof(AttributeObserver));

    observer->attribute = dataAttribute;
    observer->handler = (AttributeChangedHandler) handler;

    LinkedList_add(self->observedObjects, observer);
}

static AttributeAccessHandler*
getAccessHandlerForAttribute(MmsMapping* self, DataAttribute* dataAttribute)
{
    LinkedList element = LinkedList_getNext(self->attributeAccessHandlers);

    while (element != NULL) {
        AttributeAccessHandler* accessHandler = (AttributeAccessHandler*) element->data;

        if (accessHandler->attribute == dataAttribute)
            return accessHandler;

        element = LinkedList_getNext(element);
    }

    return NULL;
}

void
MmsMapping_installWriteAccessHandler(MmsMapping* self, DataAttribute* dataAttribute, WriteAccessHandler handler)
{
    AttributeAccessHandler* accessHandler = getAccessHandlerForAttribute(self, dataAttribute);

    if (accessHandler == NULL) {
        accessHandler = (AttributeAccessHandler*) malloc(sizeof(AttributeAccessHandler));

        accessHandler->attribute = dataAttribute;
        LinkedList_add(self->attributeAccessHandlers, (void*) accessHandler);
    }

    accessHandler->handler = handler;
}

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

static MmsValue*
readAccessGooseControlBlock(MmsMapping* self, MmsDomain* domain, char* variableIdOrig)
{
    MmsValue* value = NULL;

    char variableId[130];

    strncpy(variableId, variableIdOrig, 129);

    char* separator = strchr(variableId, '$');

    *separator = 0;

    char* lnName = variableId;

    if (lnName == NULL)
        return NULL;

    char* objectName = MmsMapping_getNextNameElement(separator + 1);

    if (objectName == NULL)
        return NULL;

    char* varName = MmsMapping_getNextNameElement(objectName);

    if (varName != NULL)
        *(varName - 1) = 0;

    MmsGooseControlBlock mmsGCB = lookupGCB(self, domain, lnName, objectName);

    if (mmsGCB != NULL) {
        if (varName != NULL) {
            value = MmsValue_getSubElement(MmsGooseControlBlock_getMmsValues(mmsGCB),
                    MmsGooseControlBlock_getVariableSpecification(mmsGCB), varName);
        }
        else {
            value = MmsGooseControlBlock_getMmsValues(mmsGCB);
        }
    }

    return value;
}

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

static MmsValue*
mmsReadHandler(void* parameter, MmsDomain* domain, char* variableId, MmsServerConnection* connection)
{
    MmsMapping* self = (MmsMapping*) parameter;

    if (DEBUG_IDE_SERVER)
        printf("mmsReadHandler: Requested %s\n", variableId);

    char* separator = strchr(variableId, '$');

    if (separator == NULL)
        return NULL;

    int lnNameLength = separator - variableId;

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
    /* Controllable objects - CO */
    if (isControllable(separator)) {
        return Control_readAccessControlObject(self, domain, variableId, connection);
    }
#endif

    /* GOOSE control blocks - GO */
#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
    if (isGooseControlBlock(separator)) {
        return readAccessGooseControlBlock(self, domain, variableId);
    }
#endif

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
    /* Report control blocks - BR, RP */
    if (isReportControlBlock(separator)) {

        LinkedList reportControls = self->reportControls;

        LinkedList nextElement = reportControls;

        char* reportName = MmsMapping_getNextNameElement(separator + 1);

        if (reportName == NULL)
            return NULL;

        separator = strchr(reportName, '$');

        size_t variableIdLen;

        if (separator != NULL)
            variableIdLen = separator - variableId;
        else
            variableIdLen = strlen(variableId);

        while ((nextElement = LinkedList_getNext(nextElement)) != NULL) {
            ReportControl* rc = (ReportControl*) nextElement->data;

            if (rc->domain == domain) {

                int parentLNNameStrLen = strlen(rc->parentLN->name);

                if (parentLNNameStrLen != lnNameLength)
                    continue;

                if (memcmp(rc->parentLN->name, variableId, parentLNNameStrLen) != 0)
                    continue;

                if (strlen(rc->name) == variableIdLen) {
                    if (strncmp(variableId, rc->name, variableIdLen) == 0) {

                        char* elementName = MmsMapping_getNextNameElement(reportName);

                        MmsValue* value = NULL;

                        if (elementName != NULL)
                            value = ReportControl_getRCBValue(rc, elementName);
                        else
                            value = rc->rcbValues;

                        return value;
                    }
                }

            }

        }
    }
#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */

    return NULL;
}

void
MmsMapping_setMmsServer(MmsMapping* self, MmsServer server)
{
    self->mmsServer = server;
}

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
static void
unselectControlsForConnection(MmsMapping* self, MmsServerConnection* connection)
{
    LinkedList controlObjectElement = LinkedList_getNext(self->controlObjects);

    while (controlObjectElement != NULL) {
        ControlObject* controlObject = (ControlObject*) controlObjectElement->data;

        if (ControlObject_unselect(controlObject, connection))
            break;

        controlObjectElement = LinkedList_getNext(controlObjectElement);
    }
}
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */

static void /* is called by MMS server layer */
mmsConnectionHandler(void* parameter, MmsServerConnection* connection, MmsServerEvent event)
{
    MmsMapping* self = (MmsMapping*) parameter;

    if (event == MMS_SERVER_CONNECTION_CLOSED) {
        ClientConnection clientConnection = private_IedServer_getClientConnectionByHandle(self->iedServer, connection);

        /* call user provided handler function */
        if (self->connectionIndicationHandler != NULL)
            self->connectionIndicationHandler(self->iedServer, clientConnection, false,
                    self->connectionIndicationHandlerParameter);

        private_IedServer_removeClientConnection(self->iedServer, clientConnection);

        /* wait until control threads are finished */
        while (private_ClientConnection_getTasksCount(clientConnection) > 0)
            Thread_sleep(10);

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
        Reporting_deactivateReportsForConnection(self, connection);
#endif

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
        unselectControlsForConnection(self, connection);
#endif

        private_ClientConnection_destroy(clientConnection);
    }
    else if (event == MMS_SERVER_NEW_CONNECTION) {
        /* call user provided handler function */
        ClientConnection newClientConnection = private_ClientConnection_create(connection);

        private_IedServer_addNewClientConnection(self->iedServer, newClientConnection);

        /* call user provided handler function */
        if (self->connectionIndicationHandler != NULL)
            self->connectionIndicationHandler(self->iedServer, newClientConnection, true,
                    self->connectionIndicationHandlerParameter);
    }
}

void
MmsMapping_installHandlers(MmsMapping* self)
{
    MmsServer_installReadHandler(self->mmsServer, mmsReadHandler, (void*) self);
    MmsServer_installWriteHandler(self->mmsServer, mmsWriteHandler, (void*) self);
    MmsServer_installConnectionHandler(self->mmsServer, mmsConnectionHandler, (void*) self);
}

void
MmsMapping_setIedServer(MmsMapping* self, IedServer iedServer)
{
    self->iedServer = iedServer;
}

void
MmsMapping_setConnectionIndicationHandler(MmsMapping* self, IedConnectionIndicationHandler handler, void* parameter)
{
    self->connectionIndicationHandler = handler;
    self->connectionIndicationHandlerParameter = parameter;
}

#if ((CONFIG_IEC61850_REPORT_SERVICE == 1) || (CONFIG_INCLUDE_GOOSE_SUPPORT == 1))

static bool
isMemberValueRecursive(MmsValue* container, MmsValue* value)
{
    if (container == value)
        return true;
    else {
        if ((MmsValue_getType(container) == MMS_STRUCTURE) ||
                (MmsValue_getType(container) == MMS_ARRAY))
        {

            int compCount = MmsValue_getArraySize(container);
            int i;
            for (i = 0; i < compCount; i++) {
                if (isMemberValueRecursive(MmsValue_getElement(container, i), value))
                    return true;
            }

        }
    }

    return false;
}

static bool
DataSet_isMemberValue(DataSet* dataSet, MmsValue* value, int* index)
{
    int i = 0;

    DataSetEntry* dataSetEntry = dataSet->fcdas;

    while (dataSetEntry != NULL) {

        MmsValue* dataSetValue = dataSetEntry->value;

        if (isMemberValueRecursive(dataSetValue, value)) {
            if (index != NULL)
                *index = i;

            return true;
        }

        i++;

        dataSetEntry = dataSetEntry->sibling;
    }

    return false;
}
#endif /* ((CONFIG_IEC61850_REPORT_SERVICE == 1) || (CONFIG_INCLUDE_GOOSE_SUPPORT)) */

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
void
MmsMapping_triggerReportObservers(MmsMapping* self, MmsValue* value, ReportInclusionFlag flag)
{
    LinkedList element = self->reportControls;

    while ((element = LinkedList_getNext(element)) != NULL) {
        ReportControl* rc = (ReportControl*) element->data;

        if (rc->enabled || (rc->buffered && rc->dataSet != NULL)) {
            int index;

            switch (flag) {
            case REPORT_CONTROL_VALUE_UPDATE:
                if ((rc->triggerOps & TRG_OPT_DATA_UPDATE) == 0)
                    continue;
                break;
            case REPORT_CONTROL_VALUE_CHANGED:
                if (((rc->triggerOps & TRG_OPT_DATA_CHANGED) == 0) &&
                        ((rc->triggerOps & TRG_OPT_DATA_UPDATE) == 0))
                    continue;
                break;
            case REPORT_CONTROL_QUALITY_CHANGED:
                if ((rc->triggerOps & TRG_OPT_QUALITY_CHANGED) == 0)
                    continue;
                break;
            default:
                continue;
            }

            if (DataSet_isMemberValue(rc->dataSet, value, &index)) {
                ReportControl_valueUpdated(rc, index, flag, value);
            }
        }
    }
}

#endif /* (CONFIG_IEC61850_REPORT_SERVICE == 1) */

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

void
MmsMapping_triggerGooseObservers(MmsMapping* self, MmsValue* value)
{
    LinkedList element = self->gseControls;

    while ((element = LinkedList_getNext(element)) != NULL) {
        MmsGooseControlBlock gcb = (MmsGooseControlBlock) element->data;

        if (MmsGooseControlBlock_isEnabled(gcb)) {
            DataSet* dataSet = MmsGooseControlBlock_getDataSet(gcb);

            if (DataSet_isMemberValue(dataSet, value, NULL)) {
                MmsGooseControlBlock_observedObjectChanged(gcb);
            }
        }
    }
}

void
MmsMapping_enableGoosePublishing(MmsMapping* self)
{

    LinkedList element = self->gseControls;

    while ((element = LinkedList_getNext(element)) != NULL) {
        MmsGooseControlBlock gcb = (MmsGooseControlBlock) element->data;

        MmsGooseControlBlock_enable(gcb);
    }

}

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
void
MmsMapping_addControlObject(MmsMapping* self, ControlObject* controlObject)
{
    LinkedList_add(self->controlObjects, controlObject);
}

ControlObject*
MmsMapping_getControlObject(MmsMapping* self, MmsDomain* domain, char* lnName, char* coName)
{
    return Control_lookupControlObject(self, domain, lnName, coName);
}
#endif /* (CONFIG_IEC61850_CONTROL_SERVICE == 1) */


char*
MmsMapping_getMmsDomainFromObjectReference(char* objectReference, char* buffer)
{
    int objRefLength = strlen(objectReference);

    //check if LD name is present
    int i;
    for (i = 0; i < objRefLength; i++) {
        if (objectReference[i] == '/') {
            break;
        }
    }

    if (i == objRefLength)
        return NULL;

    char* domainName;

    if (buffer == NULL)
        domainName = (char*) malloc(i + 1);
    else
        domainName = buffer;

    int j;
    for (j = 0; j < i; j++) {
        domainName[j] = objectReference[j];
    }

    domainName[j] = 0;

    return domainName;
}

char*
MmsMapping_createMmsVariableNameFromObjectReference(char* objectReference,
        FunctionalConstraint fc, char* buffer)
{
    int objRefLength = strlen(objectReference);

    //check if LD name is present
    int i;
    for (i = 0; i < objRefLength; i++) {
        if (objectReference[i] == '/') {
            break;
        }
    }

    if (i == objRefLength)
        i = 0;
    else
        i++;

    char* fcString = FunctionalConstraint_toString(fc);

    if (fcString == NULL)
        return NULL;

    char* mmsVariableName;

    if (buffer == NULL)
        mmsVariableName = (char*) malloc(objRefLength - i + 4);
    else
        mmsVariableName = buffer;

    int sourceIndex = i;
    int destIndex = 0;

    while (objectReference[sourceIndex] != '.')
        mmsVariableName[destIndex++] = objectReference[sourceIndex++];

    sourceIndex++;

    mmsVariableName[destIndex++] = '$';
    mmsVariableName[destIndex++] = fcString[0];
    mmsVariableName[destIndex++] = fcString[1];
    mmsVariableName[destIndex++] = '$';

    while (sourceIndex < objRefLength) {
        if (objectReference[sourceIndex] != '.')
            mmsVariableName[destIndex++] = objectReference[sourceIndex++];
        else {
            mmsVariableName[destIndex++] = '$';
            sourceIndex++;
        }
    }

    mmsVariableName[destIndex] = 0;

    return mmsVariableName;
}

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)

static void
GOOSE_processGooseEvents(MmsMapping* self, uint64_t currentTimeInMs)
{
    LinkedList element = LinkedList_getNext(self->gseControls);

    while (element != NULL) {
        MmsGooseControlBlock mmsGCB = (MmsGooseControlBlock) element->data;

        if (MmsGooseControlBlock_isEnabled(mmsGCB)) {
            MmsGooseControlBlock_checkAndPublish(mmsGCB, currentTimeInMs);
        }

        element = LinkedList_getNext(element);
    }
}

#endif /* (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */

/* single worker thread for all enabled GOOSE and report control blocks
 *
 * TODO move GOOSE processing to other (high-priority) thread
 * */
static void
eventWorkerThread(MmsMapping* self)
{
    bool running = true;
    self->reportThreadFinished = false;

    while (running) {
        uint64_t currentTimeInMs = Hal_getTimeInMs();

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
        GOOSE_processGooseEvents(self, currentTimeInMs);
#endif

#if (CONFIG_IEC61850_CONTROL_SERVICE == 1)
        Control_processControlActions(self, currentTimeInMs);
#endif

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
        Reporting_processReportEvents(self, currentTimeInMs);
#endif

        Thread_sleep(1); /* hand-over control to other threads */

        running = self->reportThreadRunning;
    }

    if (DEBUG_IDE_SERVER)
        printf("IED_SERVER: event worker thread finished!\n");

    self->reportThreadFinished = true;
}

void
MmsMapping_startEventWorkerThread(MmsMapping* self)
{
    self->reportThreadRunning = true;

    Thread thread = Thread_create((ThreadExecutionFunction) eventWorkerThread, self, false);
    self->reportWorkerThread = thread;
    Thread_start(thread);
}

void
MmsMapping_stopEventWorkerThread(MmsMapping* self)
{
    if (self->reportThreadRunning) {

        self->reportThreadRunning = false;

        while (self->reportThreadFinished == false)
            Thread_sleep(1);
    }
}

static DataSet*
createDataSetByNamedVariableList(MmsMapping* self, MmsNamedVariableList variableList)
{
    DataSet* dataSet = (DataSet*) malloc(sizeof(DataSet));

    dataSet->logicalDeviceName = NULL; /* name is not relevant for dynamically created data set */

    dataSet->name = variableList->name;
    dataSet->elementCount = LinkedList_size(variableList->listOfVariables);

    LinkedList element = LinkedList_getNext(variableList->listOfVariables);

    DataSetEntry* lastDataSetEntry = NULL;

    while (element != NULL) {
        MmsAccessSpecifier* listEntry = (MmsAccessSpecifier*) element->data;

        DataSetEntry* dataSetEntry = (DataSetEntry*) malloc(sizeof(DataSetEntry));

        dataSetEntry->logicalDeviceName = MmsDomain_getName(listEntry->domain);
        dataSetEntry->variableName = listEntry->variableName;
        dataSetEntry->index = listEntry->arrayIndex;
        dataSetEntry->componentName = listEntry->componentName;
        dataSetEntry->sibling = NULL;

        if (lastDataSetEntry == NULL)
            dataSet->fcdas =dataSetEntry;
        else
            lastDataSetEntry->sibling = dataSetEntry;

        dataSetEntry->value =
                MmsServer_getValueFromCache(self->mmsServer, listEntry->domain, listEntry->variableName);

        lastDataSetEntry = dataSetEntry;

        element = LinkedList_getNext(element);
    }

    return dataSet;
}

MmsNamedVariableList
MmsMapping_getDomainSpecificVariableList(MmsMapping* self, char* variableListReference)
{
    char variableListReferenceCopy[193];

    strncpy(variableListReferenceCopy, variableListReference, 192);
	variableListReferenceCopy[192] = 0;

    char* separator = strchr(variableListReferenceCopy, '/');

    if (separator == NULL)
        return NULL;

    char* domainName = variableListReferenceCopy;

    char* variableListName = separator + 1;

    *separator = 0;

    MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, domainName);

    if (domain == NULL)
        return NULL;

    MmsNamedVariableList variableList = MmsDomain_getNamedVariableList(domain, variableListName);

    return variableList;
}

DataSet*
MmsMapping_getDomainSpecificDataSet(MmsMapping* self, char* dataSetName)
{
    MmsNamedVariableList variableList = MmsMapping_getDomainSpecificVariableList(self, dataSetName);

    if (variableList == NULL)
        return NULL;

    return createDataSetByNamedVariableList(self, variableList);
}

void
MmsMapping_freeDynamicallyCreatedDataSet(DataSet* dataSet)
{
    DataSetEntry* dataSetEntry = dataSet->fcdas;

    while (dataSetEntry != NULL) {
        DataSetEntry* nextEntry = dataSetEntry->sibling;

        free (dataSetEntry);

        dataSetEntry = nextEntry;
    }

    free(dataSet);
}

MmsVariableAccessSpecification*
MmsMapping_ObjectReferenceToVariableAccessSpec(char* objectReference)
{
    char* domainIdEnd = strchr(objectReference, '/');

    if (domainIdEnd == NULL) /* no logical device name present */
        return NULL;

    int domainIdLen = domainIdEnd - objectReference;

    char* fcStart = strchr(objectReference, '[');

    if (fcStart == NULL) /* no FC present */
        return NULL;

    char* fcEnd = strchr(fcStart, ']');

    if (fcEnd == NULL) /* syntax error in FC */
        return NULL;

    if ((fcEnd - fcStart) != 3) /* syntax error in FC */
        return NULL;

    FunctionalConstraint fc = FunctionalConstraint_fromString(fcStart + 1);

    MmsVariableAccessSpecification* accessSpec =
            (MmsVariableAccessSpecification*) calloc(1, sizeof(MmsVariableAccessSpecification));

    accessSpec->domainId = createStringFromBuffer((uint8_t*) objectReference, domainIdLen);

    char* indexBrace = strchr(domainIdEnd, '(');

    char* itemIdEnd = indexBrace;

    if (itemIdEnd == NULL)
        itemIdEnd = strchr(domainIdEnd, '[');

    int objRefLen = strlen(objectReference);

    accessSpec->arrayIndex = -1; /* -1 --> not present */

    if (itemIdEnd != NULL) {
        int itemIdLen = itemIdEnd - domainIdEnd - 1;

        char itemIdStr[129];

        memcpy(itemIdStr, (domainIdEnd + 1), itemIdLen);
        itemIdStr[itemIdLen] = 0;

        accessSpec->itemId = MmsMapping_createMmsVariableNameFromObjectReference(itemIdStr, fc, NULL);

        if (indexBrace != NULL) {

            char* indexStart = itemIdEnd + 1;

            char* indexEnd = strchr(indexStart, ')');

            int indexLen = indexEnd - indexStart;

            int index = StringUtils_digitsToInt(indexStart, indexLen);

            accessSpec->arrayIndex = (int32_t) index;

            int componentNameLen = objRefLen - ((indexEnd + 2) - objectReference) - 4;

            if (componentNameLen > 0) {
                accessSpec->componentName = createStringFromBuffer((uint8_t*) (indexEnd + 2), componentNameLen);
                StringUtils_replace(accessSpec->componentName, '.', '$');
            }
        }
    }

    return accessSpec;
}

static int
getNumberOfDigits(int value)
{
    int numberOfDigits = 1;

    while (value > 9) {
        numberOfDigits++;
        value /= 10;
    }

    return numberOfDigits;
}

char*
MmsMapping_varAccessSpecToObjectReference(MmsVariableAccessSpecification* varAccessSpec)
{
    char* domainId = varAccessSpec->domainId;

    int domainIdLen = strlen(domainId);

    char* itemId = varAccessSpec->itemId;

    char* separator = strchr(itemId, '$');

    int itemIdLen = strlen(itemId);

    int arrayIndexLen = 0;

    int componentPartLen = 0;

    if (varAccessSpec->componentName != NULL)
        componentPartLen = strlen(varAccessSpec->componentName);

    if (varAccessSpec->arrayIndex > -1)
        arrayIndexLen = 2 + getNumberOfDigits(varAccessSpec->arrayIndex);

    int newStringLen = (domainIdLen + 1) + (itemIdLen - 2) + arrayIndexLen + 4 /* for FC */+ componentPartLen + 1;

    char* newString = (char*) malloc(newStringLen);

    char* targetPos = newString;

    /* Copy domain id part */
    char* currentPos = domainId;

    while (currentPos < (domainId + domainIdLen)) {
        *targetPos = *currentPos;
        targetPos++;
        currentPos++;
    }

    *targetPos = '/';
    targetPos++;

    /* Copy item id parts */
    currentPos = itemId;

    while (currentPos < separator) {
        *targetPos = *currentPos;
        targetPos++;
        currentPos++;
    }

    *targetPos = '.';
    targetPos++;

    currentPos = separator + 4;

    while (currentPos < (itemId + itemIdLen)) {
        if (*currentPos == '$')
            *targetPos = '.';
        else
            *targetPos = *currentPos;

        targetPos++;
        currentPos++;
    }

    /* Add array index part */
    if (varAccessSpec->arrayIndex > -1) {
        sprintf(targetPos, "(%i)", varAccessSpec->arrayIndex);
        targetPos += arrayIndexLen;
    }

    /* Add component part */
    if (varAccessSpec->componentName != NULL) {
        *targetPos = '.';
        targetPos++;

        int i;
        for (i = 0; i < componentPartLen; i++) {
            if (varAccessSpec->componentName[i] == '$')
                *targetPos = '.';
            else
                *targetPos = varAccessSpec->componentName[i];

            targetPos++;
        }
    }

    /* add FC part */
    *targetPos = '[';
    targetPos++;
    *targetPos = *(separator + 1);
    targetPos++;
    *targetPos = *(separator + 2);
    targetPos++;
    *targetPos = ']';
    targetPos++;

    *targetPos = 0; /* add terminator */

    return newString;
}

