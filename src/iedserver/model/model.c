/*
 *  model.c
 *
 *  Copyright 2013 Michael Zillgith
 *
 *	This file is part of libIEC61850.
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

#include "model.h"

#include "libiec61850_platform_includes.h"

static void
setAttributeValuesToNull(ModelNode* node)
{
    if (node->modelType == DataAttributeModelType) {
        DataAttribute* da = (DataAttribute*) node;

        da->mmsValue = NULL;
    }

    ModelNode* child = node->firstChild;

    while (child != NULL) {
        setAttributeValuesToNull(child);
        child = child->sibling;
    }
}

void
IedModel_setAttributeValuesToNull(IedModel* iedModel)
{
    LogicalDevice* ld = iedModel->firstChild;

    while (ld != NULL) {

        LogicalNode* ln = (LogicalNode*) ld->firstChild;

        while (ln != NULL) {
            ModelNode* node = ln->firstChild;

            while (node != NULL) {
                setAttributeValuesToNull(node);
                node = node->sibling;
            }

            ln = (LogicalNode*) ln->sibling;
        }

        ld = (LogicalDevice*) ld->sibling;
    }
}



int
IedModel_getLogicalDeviceCount(IedModel* iedModel)
{
	if (iedModel->firstChild == NULL)
		return 0;

	LogicalDevice* logicalDevice = iedModel->firstChild;

	int ldCount = 1;

	while (logicalDevice->sibling != NULL) {
		logicalDevice = (LogicalDevice*) logicalDevice->sibling;
		ldCount++;
	}

	return ldCount;
}

DataSet*
IedModel_lookupDataSet(IedModel* model, const char* dataSetReference  /* e.g. ied1Inverter/LLN0$dataset1 */)
{
	DataSet* dataSet = model->dataSets;

	const char* separator = strchr(dataSetReference, '/');

	if (separator == NULL)
		return NULL;

	int ldNameLen = separator - dataSetReference;

	while (dataSet != NULL) {
		if (strncmp(dataSet->logicalDeviceName, dataSetReference, ldNameLen) == 0) {
			if (strcmp(dataSet->name, separator + 1) == 0) {
				return dataSet;
			}
		}

		dataSet = dataSet->sibling;
	}

	return NULL;
}

LogicalDevice*
IedModel_getDevice(IedModel* model, const char* deviceName)
{
    LogicalDevice* device = model->firstChild;

    while (device != NULL) {
        if (strcmp(device->name, deviceName) == 0)
            return device;

        device = (LogicalDevice*) device->sibling;
    }

    return NULL;
}

static DataAttribute*
ModelNode_getDataAttributeByMmsValue(ModelNode* self, MmsValue* value)
{
    ModelNode* node = self->firstChild;

    while (node != NULL) {
        if (node->modelType == DataAttributeModelType) {
            DataAttribute* da = (DataAttribute*) node;

            if (da->mmsValue == value)
                return da;
        }

        DataAttribute* da = ModelNode_getDataAttributeByMmsValue(node, value);

        if (da != NULL)
            return da;

        node = node->sibling;
    }

    return NULL;
}

DataAttribute*
IedModel_lookupDataAttributeByMmsValue(IedModel* model, MmsValue* value)
{
    LogicalDevice* ld = model->firstChild;

    while (ld != NULL) {

        DataAttribute* da =
                ModelNode_getDataAttributeByMmsValue((ModelNode*) ld, value);

        if (da != NULL)
            return da;


        ld = (LogicalDevice*) ld->sibling;
    }

    return NULL;
}

static ModelNode*
getChildWithShortAddress(ModelNode* node, uint32_t sAddr)
{
    ModelNode* child;

    child = node->firstChild;

    while (child != NULL) {
        if (child->modelType == DataAttributeModelType) {
            DataAttribute* da = (DataAttribute*) child;

            if (da->sAddr == sAddr)
                return child;
        }

        ModelNode* childChild = getChildWithShortAddress(child, sAddr);

        if (childChild != NULL)
            return childChild;

        child = child->sibling;
    }

    return NULL;
}

ModelNode*
IedModel_getModelNodeByShortAddress(IedModel* model, uint32_t sAddr)
{
    ModelNode* node = NULL;

    LogicalDevice* ld = (LogicalDevice*) model->firstChild;

    while (ld != NULL) {

        LogicalNode* ln = (LogicalNode*) ld->firstChild;

        while (ln != NULL) {

            ModelNode* doNode = ln->firstChild;

            while (doNode != NULL) {
                ModelNode* matchingNode = getChildWithShortAddress(doNode, sAddr);

                if (matchingNode != NULL)
                    return matchingNode;

                doNode = doNode->sibling;
            }

            ln = (LogicalNode*) ln->sibling;
        }

        ld = (LogicalDevice*) ld->sibling;
    }

    return node;
}

ModelNode*
IedModel_getModelNodeByObjectReference(IedModel* model, const char* objectReference)
{
    assert(strlen(objectReference) < 129);

    char objRef[130];

    strncpy(objRef, objectReference, 129);
    objRef[129] = 0;

    char* separator = strchr(objRef, '/');

    if (separator == NULL)
        return NULL;

    *separator = 0;

    LogicalDevice* ld = IedModel_getDevice(model, objRef);

    if (ld == NULL) return NULL;

    return ModelNode_getChild((ModelNode*) ld, separator + 1);
}

ModelNode*
IedModel_getModelNodeByShortObjectReference(IedModel* model, const char* objectReference)
{
    assert((strlen(model->name) + strlen(objectReference)) < 130);

    char objRef[130];

    strncpy(objRef, objectReference, 129);
    objRef[129] = 0;

    char* separator = strchr(objRef, '/');

    if (separator == NULL)
        return NULL;

    *separator = 0;

    char ldName[65];
    strcpy(ldName, model->name);
    strcat(ldName, objRef);

    LogicalDevice* ld = IedModel_getDevice(model, ldName);

    if (ld == NULL) return NULL;

    return ModelNode_getChild((ModelNode*) ld, separator + 1);
}


bool
DataObject_hasFCData(DataObject* dataObject, FunctionalConstraint fc)
{
	ModelNode* modelNode = dataObject->firstChild;

	while (modelNode != NULL) {

		if (modelNode->modelType == DataAttributeModelType) {
			DataAttribute* dataAttribute = (DataAttribute*) modelNode;

			if (dataAttribute->fc == fc)
				return true;
		}
		else if (modelNode->modelType == DataObjectModelType) {

			if (DataObject_hasFCData((DataObject*) modelNode, fc))
				return true;
		}

		modelNode = modelNode->sibling;
	}

	return false;
}

bool
LogicalNode_hasFCData(LogicalNode* node, FunctionalConstraint fc)
{
	DataObject* dataObject = (DataObject*) node->firstChild;

	while (dataObject != NULL) {
		if (DataObject_hasFCData(dataObject, fc))
			return true;

		dataObject = (DataObject*) dataObject->sibling;
	}

	return false;
}

int
LogicalDevice_getLogicalNodeCount(LogicalDevice* logicalDevice)
{
	int lnCount = 0;

	LogicalNode* logicalNode = (LogicalNode*) logicalDevice->firstChild;

	while (logicalNode != NULL) {
		logicalNode = (LogicalNode*) logicalNode->sibling;
		lnCount++;
	}

	return lnCount;
}

static int
createObjectReference(ModelNode* node, char* objectReference)
{
    int bufPos;

    if (node->modelType != LogicalNodeModelType) {
        bufPos = createObjectReference(node->parent, objectReference);

        objectReference[bufPos++] = '.';
    }
    else {
        LogicalNode* lNode = (LogicalNode*) node;

        LogicalDevice* lDevice = (LogicalDevice*) lNode->parent;

        bufPos = 0;

        int nameLength = strlen(lDevice->name);

        int i;
        for (i = 0; i < nameLength; i++) {
            objectReference[bufPos++] = lDevice->name[i];
        }

        objectReference[bufPos++] = '/';
    }

    /* append own name */
    int nameLength = strlen(node->name);

    int i;
    for (i = 0; i < nameLength; i++) {
        objectReference[bufPos++] = node->name[i];
    }

    return bufPos;
}

char*
ModelNode_getObjectReference(ModelNode* node, char* objectReference)
{
    if (objectReference == NULL)
        objectReference = (char*) malloc(130);

    int bufPos = createObjectReference(node, objectReference);

    objectReference[bufPos] = 0;

    return objectReference;
}

int
ModelNode_getChildCount(ModelNode* modelNode) {
	int childCount = 0;

	ModelNode* child = modelNode->firstChild;

	while (child != NULL) {
		childCount++;
		child = child->sibling;
	}

	return childCount;
}


ModelNode*
ModelNode_getChild(ModelNode* self, const char* name)
{
    // check for separator
   const char* separator = strchr(name, '.');

   int nameElementLength = 0;

   if (separator != NULL)
       nameElementLength = (separator - name);
   else
       nameElementLength = strlen(name);

   ModelNode* nextNode = self->firstChild;

   ModelNode* matchingNode = NULL;

   while (nextNode != NULL) {
       int nodeNameLen = strlen(nextNode->name);

       if (nodeNameLen == nameElementLength) {
           if (memcmp(nextNode->name, name, nodeNameLen) == 0) {
               matchingNode = nextNode;
               break;
           }
       }

       nextNode = nextNode->sibling;
   }

   if ((separator != NULL) && (matchingNode != NULL)) {
       return ModelNode_getChild(matchingNode, separator + 1);
   }
   else
       return matchingNode;
}


inline
LogicalNode*
LogicalDevice_getLogicalNode(LogicalDevice* device, const char* nodeName)
{
    return (LogicalNode*) ModelNode_getChild((ModelNode*) device, nodeName);
}
