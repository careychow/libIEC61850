/*
 *  mms_get_namelist_service.c
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

#include "mms_server_internal.h"
#include "ber_encoder.h"
#include "ber_decode.h"

#if (MMS_GET_NAME_LIST == 1)

/**********************************************************************************************
 * MMS GetNameList Service
 *********************************************************************************************/

#define OBJECT_CLASS_NAMED_VARIABLE 0
#define OBJECT_CLASS_NAMED_VARIABLE_LIST 2
#define OBJECT_CLASS_JOURNAL 8
#define OBJECT_CLASS_DOMAIN 9

#define OBJECT_SCOPE_VMD 0
#define OBJECT_SCOPE_DOMAIN 1
#define OBJECT_SCOPE_ASSOCIATION 2

static LinkedList
getDomainNames(MmsServerConnection* connection)
{
	MmsDevice* device = MmsServer_getDevice(connection->server);

	LinkedList list = LinkedList_create();

	int i;

	for (i = 0; i < device->domainCount; i++) {
		LinkedList_add(list, device->domains[i]->domainName);
	}

	return list;
}


#if (CONFIG_MMS_SUPPORT_VMD_SCOPE_NAMED_VARIABLES == 1)
static LinkedList
getNameListVMDSpecific(MmsServerConnection* connection)
{
    MmsDevice* device = MmsServer_getDevice(connection->server);

    LinkedList list = LinkedList_create();

    int i;

    for (i = 0; i < device->namedVariablesCount; i++) {
        LinkedList_add(list, device->namedVariables[i]->name);
    }

    return list;
}
#endif /* (CONFIG_MMS_SUPPORT_VMD_SCOPE_NAMED_VARIABLES == 1) */

#if (CONFIG_MMS_SUPPORT_FLATTED_NAME_SPACE == 1)
static char*
appendMmsSubVariable(char* name, char* child)
{
    int nameLen = strlen(name);
    int childLen = strlen(child);

    int newSize = nameLen + childLen + 2;

    char* newName = (char*) malloc(newSize);

    int bufPos = 0;
    int i;
    for (i = 0; i < nameLen; i++) {
        newName[bufPos++] = name[i];
    }
    newName[bufPos++] = '$';

    for (i = 0; i < childLen; i++) {
        newName[bufPos++] = child[i];
    }

    newName[bufPos] = 0;

    return newName;
}

static LinkedList
addSubNamedVaribleNamesToList(LinkedList nameList, char* prefix, MmsVariableSpecification* variable)
{
	LinkedList listElement = nameList;

	if (variable->type == MMS_STRUCTURE) {

		int i;

		MmsVariableSpecification** variables = variable->typeSpec.structure.elements;

		for (i = 0; i < variable->typeSpec.structure.elementCount; i++) {
			char* variableName = appendMmsSubVariable(prefix, variables[i]->name);

			listElement = LinkedList_insertAfter(listElement, variableName);

			listElement = addSubNamedVaribleNamesToList(listElement, variableName, variables[i]);
		}
	}

	return listElement;
}
#endif /* (CONFIG_MMS_SUPPORT_FLATTED_NAME_SPACE == 1) */

static LinkedList
getNameListDomainSpecific(MmsServerConnection* connection, char* domainName) {
	MmsDevice* device = MmsServer_getDevice(connection->server);

	LinkedList nameList = NULL;

	MmsDomain* domain = MmsDevice_getDomain(device, domainName);

	if (domain != NULL) {
		nameList = LinkedList_create();
		MmsVariableSpecification** variables = domain->namedVariables;

		int i;

		LinkedList element = nameList;

		for (i = 0; i < domain->namedVariablesCount; i++) {
			element = LinkedList_insertAfter(element, copyString(variables[i]->name));

#if (CONFIG_MMS_SUPPORT_FLATTED_NAME_SPACE == 1)
			char* prefix = variables[i]->name;
			element = addSubNamedVaribleNamesToList(element, prefix, variables[i]);
#endif

		}
	}

	return nameList;
}

#if (MMS_DATA_SET_SERVICE == 1)

static LinkedList
createStringsFromNamedVariableList(LinkedList nameList, LinkedList variableLists)
{
	nameList = LinkedList_create();
	LinkedList variableListsElement = LinkedList_getNext(variableLists);
	while (variableListsElement != NULL ) {
		MmsNamedVariableList variableList =
				(MmsNamedVariableList) variableListsElement->data;

		LinkedList_add(nameList,
				copyString(MmsNamedVariableList_getName(variableList)));

		variableListsElement = LinkedList_getNext(variableListsElement);
	}
	return nameList;
}

static LinkedList
getNamedVariableListsDomainSpecific(MmsServerConnection* connection, char* domainName)
{
	MmsDevice* device = MmsServer_getDevice(connection->server);

	LinkedList nameList = NULL;

	MmsDomain* domain = MmsDevice_getDomain(device, domainName);

	if (domain != NULL) {
		LinkedList variableLists = MmsDomain_getNamedVariableLists(domain);

		nameList = createStringsFromNamedVariableList(nameList, variableLists);
	}

	return nameList;
}

static LinkedList
getnamedVariableListsVMDSpecific(MmsServerConnection* connection)
{
    MmsDevice* device = MmsServer_getDevice(connection->server);

    LinkedList nameList = NULL;

    LinkedList variableLists = MmsDevice_getNamedVariableLists(device);

    nameList = createStringsFromNamedVariableList(nameList, variableLists);

    return nameList;
}

static LinkedList
getNamedVariableListAssociationSpecific(MmsServerConnection* connection)
{
	LinkedList nameList = NULL;

	LinkedList variableLists = MmsServerConnection_getNamedVariableLists(connection);

	nameList = createStringsFromNamedVariableList(nameList, variableLists);

	return nameList;
}
#endif

static void
createNameListResponse(
        MmsServerConnection* connection,
        int invokeId,
        LinkedList nameList,
        ByteBuffer* response,
        char* continueAfter)
{
    LinkedList startElement = NULL;

    if (continueAfter != NULL) {
        LinkedList element = nameList;

        while ((element = LinkedList_getNext(element)) != NULL) {
            if (strcmp((char*) (element->data), continueAfter) == 0) {
                startElement = element;
                break;
            }
        }

        if (startElement == NULL) {
            mmsServer_createConfirmedErrorPdu(invokeId, response, MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED);
            return;
        }
    }

    /* determine number of identifiers to include in response */
    if (startElement == NULL)
        startElement = nameList;

    int nameCount = 0;
    int estimatedMmsPduLength = 27; /* estimated overhead size of PDU encoding */
    int maxPduSize = connection->maxPduSize;

    bool moreFollows = false;

    LinkedList element = startElement;

    uint32_t identifierListSize = 0;

    while ((element = LinkedList_getNext(element)) != NULL) {
        int elementLength;

        elementLength = BerEncoder_determineEncodedStringSize((char*) element->data);

        if ((estimatedMmsPduLength + elementLength) > maxPduSize) {
            moreFollows = true;
            break;
        }
        else {
            estimatedMmsPduLength += elementLength;
            identifierListSize += elementLength;
            nameCount++;
        }

    }

    uint32_t listOfIdentifierSize = 1 + BerEncoder_determineLengthSize(identifierListSize) + identifierListSize;

    uint32_t getNameListSize = listOfIdentifierSize;

    if (moreFollows == false)
    	getNameListSize += 3;

    uint32_t confirmedServiceResponseSize = 1 + BerEncoder_determineLengthSize(getNameListSize) + getNameListSize;

    uint32_t invokeIdSize = BerEncoder_UInt32determineEncodedSize((uint32_t) invokeId) + 2;

    uint32_t confirmedResponsePDUSize = confirmedServiceResponseSize + invokeIdSize;

    /* encode response */
    element = startElement;

    uint8_t* buffer = response->buffer;
    int bufPos = 0;

    bufPos = BerEncoder_encodeTL(0xa1, confirmedResponsePDUSize, buffer, bufPos);

    bufPos = BerEncoder_encodeTL(0x02, invokeIdSize - 2, buffer, bufPos);
    bufPos = BerEncoder_encodeUInt32((uint32_t) invokeId, buffer, bufPos);

    bufPos = BerEncoder_encodeTL(0xa1, getNameListSize, buffer, bufPos);
    bufPos = BerEncoder_encodeTL(0xa0, identifierListSize, buffer, bufPos);

    int i = 0;

    while ((element = LinkedList_getNext(element)) != NULL) {
        bufPos = BerEncoder_encodeStringWithTag(0x1a, (char*) element->data, buffer, bufPos);

        i++;

        if (i == nameCount)
            break;
    }

    if (moreFollows == false)
    	bufPos = BerEncoder_encodeBoolean(0x81, moreFollows, buffer, bufPos);

    response->size = bufPos;

    if (DEBUG_MMS_SERVER)
        printf("getNameList: encoded %i bytes\n", response->size);
}

void
mmsServer_handleGetNameListRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response)
{
	int objectClass = -1;

	int objectScope = -1;

	char* domainId = NULL;
	int domainIdLength;

	char* continueAfter = NULL;
	int continueAfterLength;

	while (bufPos < maxBufPos) {
		uint8_t tag = buffer[bufPos++];
		int length;

		bufPos = BerDecoder_decodeLength(buffer, &length, bufPos, maxBufPos);

		if (bufPos < 0)  {
			//writeMmsRejectPdu(&invokeId, REJECT_UNRECOGNIZED_SERVICE, response);
			return;
		}

		switch (tag) {

		case 0xa0: /* objectClass */
			bufPos++;
			length = buffer[bufPos++];
			objectClass = BerDecoder_decodeUint32(buffer, length, bufPos);
			break;

		case 0xa1: /* objectScope */
			{
				uint8_t objectScopeTag = buffer[bufPos++];
				bufPos = BerDecoder_decodeLength(buffer, &length, bufPos, maxBufPos);

				switch (objectScopeTag) {
				case 0x80: /* vmd-specific */
					objectScope = OBJECT_SCOPE_VMD;
					break;
				case 0x81: /* domain-specific */
				    domainIdLength = length;
					domainId = (char*) (buffer + bufPos);
					objectScope = OBJECT_SCOPE_DOMAIN;
					break;
				case 0x82: /* association-specific */
					objectScope = OBJECT_SCOPE_ASSOCIATION;
					break;
				default:
					mmsServer_writeMmsRejectPdu(&invokeId, MMS_ERROR_REJECT_UNRECOGNIZED_MODIFIER, response);
					return;
				}
			}
			break;

		case 0x82: /* continueAfter */
			continueAfter = (char*) (buffer + bufPos);
			continueAfterLength = length;
			break;
		default:
			/* ignore unknown tag */
			break;
		}

		bufPos += length;
	}


	char continueAfterIdMemory[130];
	char* continueAfterId = NULL;

	if (continueAfter != NULL) {
		continueAfterId = continueAfterIdMemory;
		memcpy(continueAfterId, continueAfter, continueAfterLength);
		continueAfterId[continueAfterLength] = 0;

		if (DEBUG_MMS_SERVER)
		    printf("continue after: (%s)\n", continueAfterId);
	}

	if (objectScope == OBJECT_SCOPE_DOMAIN) {
		char domainSpecificName[130];

		memcpy(domainSpecificName, domainId, domainIdLength);
		domainSpecificName[domainIdLength] = 0;

		if (objectClass == OBJECT_CLASS_NAMED_VARIABLE) {
		    if (DEBUG_MMS_SERVER)
		        printf("get namelist for (%s)\n", domainSpecificName);

			LinkedList nameList = getNameListDomainSpecific(connection, domainSpecificName);

			if (nameList == NULL)
				mmsServer_createConfirmedErrorPdu(invokeId, response, MMS_ERROR_ACCESS_OBJECT_NON_EXISTENT);
			else {
				createNameListResponse(connection, invokeId, nameList, response, continueAfterId);
				LinkedList_destroy(nameList);
			}
		}
#if (MMS_DATA_SET_SERVICE == 1)
		else if (objectClass == OBJECT_CLASS_NAMED_VARIABLE_LIST) {
			LinkedList nameList = getNamedVariableListsDomainSpecific(connection, domainSpecificName);

			createNameListResponse(connection, invokeId, nameList, response, continueAfter);

			LinkedList_destroy(nameList);
		}
#endif /* (MMS_DATA_SET_SERVICE == 1) */

		else {
			if (DEBUG_MMS_SERVER) printf("MMS_SERVER: getNameList domain specific objectClass %i not supported!\n", objectClass);

			mmsServer_createConfirmedErrorPdu(invokeId, response, MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED);
		}

	}

	else if (objectScope == OBJECT_SCOPE_VMD) { /* vmd-specific */

	    if (objectClass == OBJECT_CLASS_DOMAIN) {

	        LinkedList nameList = getDomainNames(connection);

	        createNameListResponse(connection, invokeId, nameList, response, continueAfter);

	        LinkedList_destroyStatic(nameList);
	    }

#if (CONFIG_MMS_SUPPORT_VMD_SCOPE_NAMED_VARIABLES == 1)
	    else if (objectClass == OBJECT_CLASS_NAMED_VARIABLE) {
            LinkedList nameList = getNameListVMDSpecific(connection);

            createNameListResponse(connection, invokeId, nameList, response, continueAfter);

            LinkedList_destroyStatic(nameList);
	    }
#endif /* (CONFIG_MMS_SUPPORT_VMD_SCOPE_NAMED_VARIABLES == 1) */

#if (MMS_DATA_SET_SERVICE == 1)
	    else if (objectClass == OBJECT_CLASS_NAMED_VARIABLE_LIST) {
	        LinkedList nameList = getnamedVariableListsVMDSpecific(connection);

	        createNameListResponse(connection, invokeId, nameList, response, continueAfter);

	        LinkedList_destroy(nameList);
	    }
#endif /* (MMS_DATA_SET_SERVICE == 1) */

	    else if (objectClass == OBJECT_CLASS_JOURNAL) {
	        LinkedList nameList = LinkedList_create();

	        createNameListResponse(connection, invokeId, nameList, response, continueAfter);

	        LinkedList_destroy(nameList);
	    }

        else {
            if (DEBUG_MMS_SERVER) printf("MMS_SERVER: getNameList VMD specific objectClass %i not supported!\n", objectClass);

            mmsServer_createConfirmedErrorPdu(invokeId, response, MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED);
        }
	}

#if (MMS_DATA_SET_SERVICE == 1)
	else if (objectScope == OBJECT_SCOPE_ASSOCIATION) { /* association-specific */

		if (objectClass == OBJECT_CLASS_NAMED_VARIABLE_LIST) {
			LinkedList nameList = getNamedVariableListAssociationSpecific(connection);

			createNameListResponse(connection, invokeId, nameList, response, continueAfter);

			LinkedList_destroy(nameList);
		}
		else
			mmsServer_createConfirmedErrorPdu(invokeId, response, MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED);
	}
#endif /* (MMS_DATA_SET_SERVICE == 1) */

	else {
		if (DEBUG_MMS_SERVER) printf("mms_server: getNameList(%i) not supported!\n", objectScope);
		mmsServer_createConfirmedErrorPdu(invokeId, response, MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED);
	}

}


#endif /* (MMS_GET_NAME_LIST == 1) */
