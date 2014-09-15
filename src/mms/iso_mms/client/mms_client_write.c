/*
 *  mms_client_write.c
 *
 *  Copyright 2013 Michael Zillgith
 *
 *	This file is part of libIEC61850.
 *
 *	libIEC61850 is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	libIEC61850 is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	See COPYING file for the complete license text.
 */

#include <MmsPdu.h>
#include "mms_common.h"
#include "mms_client_connection.h"
#include "byte_buffer.h"

#include "mms_client_internal.h"
#include "mms_common_internal.h"

#include "stack_config.h"

static MmsError
mapDataAccessErrorToMmsError(uint32_t dataAccessError)
{
    switch (dataAccessError) {
    case 0:
        return MMS_ERROR_ACCESS_OBJECT_INVALIDATED;
    case 1:
        return MMS_ERROR_HARDWARE_FAULT;
    case 2:
        return MMS_ERROR_ACCESS_TEMPORARILY_UNAVAILABLE;
    case 3:
        return MMS_ERROR_ACCESS_OBJECT_ACCESS_DENIED;
    case 4:
        return MMS_ERROR_DEFINITION_OBJECT_UNDEFINED;
    case 5:
        return MMS_ERROR_DEFINITION_INVALID_ADDRESS;
    case 6:
        return MMS_ERROR_DEFINITION_TYPE_UNSUPPORTED;
    case 7:
        return MMS_ERROR_DEFINITION_TYPE_INCONSISTENT;
    case 8:
        return MMS_ERROR_DEFINITION_OBJECT_ATTRIBUTE_INCONSISTENT;
    case 9:
        return MMS_ERROR_ACCESS_OBJECT_ACCESS_UNSUPPORTED;
    case 10:
        return MMS_ERROR_ACCESS_OBJECT_NON_EXISTENT;
    case 11:
        return MMS_ERROR_ACCESS_OBJECT_VALUE_INVALID;
    default:
        return MMS_ERROR_OTHER;
    }
}

void
mmsClient_parseWriteMultipleItemsResponse(ByteBuffer* message, int32_t bufPos, MmsError* mmsError,
        int itemCount, LinkedList* accessResults)
{
    uint8_t* buf = message->buffer;
    int size = message->size;

    int length;

    *mmsError = MMS_ERROR_NONE;

    uint8_t tag = buf[bufPos++];

    if (tag == 0xa5) {

       bufPos = BerDecoder_decodeLength(buf, &length, bufPos, size);

       if (bufPos == -1) {
           *mmsError = MMS_ERROR_PARSING_RESPONSE;
           return;
       }

       *accessResults = LinkedList_create();

       int endPos = bufPos + length;

       int numberOfAccessResults = 0;

       while (bufPos < endPos) {

           tag = buf[bufPos++];
           bufPos = BerDecoder_decodeLength(buf, &length, bufPos, size);

           if (bufPos == -1) goto exit_with_error;

           if (tag == 0x81) {
               MmsValue* value = MmsValue_newDataAccessError(DATA_ACCESS_ERROR_SUCCESS);
               LinkedList_add(*accessResults, (void*) value);
           }

           if (tag == 0x80) {
               uint32_t dataAccessErrorCode =
                       BerDecoder_decodeUint32(buf, length, bufPos);

               MmsValue* value = MmsValue_newDataAccessError((MmsDataAccessError) dataAccessErrorCode);

               LinkedList_add(*accessResults, (void*) value);
           }

           bufPos += length;

           numberOfAccessResults++;
       }

       if (itemCount != numberOfAccessResults)
           goto exit_with_error;
    }
    else
       *mmsError = MMS_ERROR_PARSING_RESPONSE;

    return;

exit_with_error:
    *mmsError = MMS_ERROR_PARSING_RESPONSE;
    LinkedList_destroyDeep(*accessResults, (LinkedListValueDeleteFunction) MmsValue_delete);
}


void
mmsClient_parseWriteResponse(ByteBuffer* message, int32_t bufPos, MmsError* mmsError)
{
    uint8_t* buf = message->buffer;
    int size = message->size;

    int length;

    *mmsError = MMS_ERROR_NONE;

    uint8_t tag = buf[bufPos++];

    if (tag == 0xa5) {

        bufPos = BerDecoder_decodeLength(buf, &length, bufPos, size);

        if (bufPos == -1) {
            *mmsError = MMS_ERROR_PARSING_RESPONSE;
            return;
        }

        tag = buf[bufPos++];

        if (tag == 0x81)
            return;

        if (tag == 0x80) {
            bufPos = BerDecoder_decodeLength(buf, &length, bufPos, size);

            uint32_t dataAccessErrorCode =
                    BerDecoder_decodeUint32(buf, length, bufPos);

            *mmsError = mapDataAccessErrorToMmsError(dataAccessErrorCode);
        }
    }
    else
        *mmsError = MMS_ERROR_PARSING_RESPONSE;
}

static VariableSpecification_t*
createNewDomainVariableSpecification(const char* domainId, const char* itemId)
{
	VariableSpecification_t* varSpec = (VariableSpecification_t*) calloc(1, sizeof(ListOfVariableSeq_t));

	//VariableSpecification_t* varSpec = (VariableSpecification_t*) calloc(1, sizeof(VariableSpecification_t));

	varSpec->present = VariableSpecification_PR_name;
	varSpec->choice.name.present = ObjectName_PR_domainspecific;
	varSpec->choice.name.choice.domainspecific.domainId.buf = (uint8_t*) domainId;
	varSpec->choice.name.choice.domainspecific.domainId.size = strlen(domainId);
	varSpec->choice.name.choice.domainspecific.itemId.buf = (uint8_t*) itemId;
	varSpec->choice.name.choice.domainspecific.itemId.size = strlen(itemId);

	return varSpec;
}

static void
deleteDataElement(Data_t* dataElement)
{
    if (dataElement == NULL ) {
        printf("deleteDataElement NULL argument\n");
        return;
    }

    if (dataElement->present == Data_PR_structure) {
        int elementCount = dataElement->choice.structure->list.count;

        int i;
        for (i = 0; i < elementCount; i++) {
            deleteDataElement(dataElement->choice.structure->list.array[i]);
        }

        free(dataElement->choice.structure->list.array);
        free(dataElement->choice.structure);
    }
    else if (dataElement->present == Data_PR_array) {
        int elementCount = dataElement->choice.array->list.count;

        int i;
        for (i = 0; i < elementCount; i++) {
            deleteDataElement(dataElement->choice.array->list.array[i]);
        }

        free(dataElement->choice.array->list.array);
        free(dataElement->choice.array);
    }
    else if (dataElement->present == Data_PR_floatingpoint) {
        free(dataElement->choice.floatingpoint.buf);
    }
    else if (dataElement->present == Data_PR_utctime) {
        free(dataElement->choice.utctime.buf);
    }

    free(dataElement);
}

int
mmsClient_createWriteMultipleItemsRequest(uint32_t invokeId, const char* domainId, LinkedList itemIds, LinkedList values,
        ByteBuffer* writeBuffer)
{
    MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

    mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
            ConfirmedServiceRequest_PR_write;
    WriteRequest_t* request =
            &(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.write);

    int numberOfItems = LinkedList_size(itemIds);

    /* Create list of variable specifications */
    request->variableAccessSpecification.present = VariableAccessSpecification_PR_listOfVariable;
    request->variableAccessSpecification.choice.listOfVariable.list.count = numberOfItems;
    request->variableAccessSpecification.choice.listOfVariable.list.size = numberOfItems;
    request->variableAccessSpecification.choice.listOfVariable.list.array =
            (ListOfVariableSeq_t**) calloc(numberOfItems, sizeof(ListOfVariableSeq_t*));

    /* Create list of data values */
    request->listOfData.list.count = numberOfItems;
    request->listOfData.list.size = numberOfItems;
    request->listOfData.list.array = (Data_t**) calloc(numberOfItems, sizeof(struct Data*));

    int i;

    LinkedList item = LinkedList_getNext(itemIds);
    LinkedList valueElement = LinkedList_getNext(values);

    for (i = 0; i < numberOfItems; i++) {
        if (item == NULL) return -1;
        if (valueElement == NULL) return -1;

        char* itemId = (char*) item->data;
        MmsValue* value = (MmsValue*) valueElement->data;

        request->variableAccessSpecification.choice.listOfVariable.list.array[i] = (ListOfVariableSeq_t*)
                    createNewDomainVariableSpecification(domainId, itemId);

        request->listOfData.list.array[i] = mmsMsg_createBasicDataElement(value);

        item = LinkedList_getNext(item);
        valueElement = LinkedList_getNext(valueElement);
    }

    asn_enc_rval_t rval;

    rval = der_encode(&asn_DEF_MmsPdu, mmsPdu,
            (asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

    /* Free ASN structure */
    request->variableAccessSpecification.choice.listOfVariable.list.count = 0;

    for (i = 0; i < numberOfItems; i++) {
        free(request->variableAccessSpecification.choice.listOfVariable.list.array[i]);
        deleteDataElement(request->listOfData.list.array[i]);

    }

    free(request->variableAccessSpecification.choice.listOfVariable.list.array);
    request->variableAccessSpecification.choice.listOfVariable.list.array = 0;

    request->listOfData.list.count = 0;
    free(request->listOfData.list.array);
    request->listOfData.list.array = 0;

    asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);

    return rval.encoded;

}

int
mmsClient_createWriteRequest(uint32_t invokeId, const char* domainId, const char* itemId, MmsValue* value,
		ByteBuffer* writeBuffer)
{
	MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

	mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
			ConfirmedServiceRequest_PR_write;
	WriteRequest_t* request =
			&(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.write);

	/* Create list of variable specifications */
	request->variableAccessSpecification.present = VariableAccessSpecification_PR_listOfVariable;
	request->variableAccessSpecification.choice.listOfVariable.list.count = 1;
	request->variableAccessSpecification.choice.listOfVariable.list.size = 1;
	request->variableAccessSpecification.choice.listOfVariable.list.array =
			(ListOfVariableSeq_t**) calloc(1, sizeof(ListOfVariableSeq_t*));
	request->variableAccessSpecification.choice.listOfVariable.list.array[0] = (ListOfVariableSeq_t*)
			createNewDomainVariableSpecification(domainId, itemId);

	/* Create list of typed data values */
	request->listOfData.list.count = 1;
	request->listOfData.list.size = 1;
	request->listOfData.list.array = (Data_t**) calloc(1, sizeof(struct Data*));
	request->listOfData.list.array[0] = mmsMsg_createBasicDataElement(value);

	asn_enc_rval_t rval;

	rval = der_encode(&asn_DEF_MmsPdu, mmsPdu,
			(asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

	/* Free ASN structure */
	request->variableAccessSpecification.choice.listOfVariable.list.count = 0;

	free(request->variableAccessSpecification.choice.listOfVariable.list.array[0]);
	free(request->variableAccessSpecification.choice.listOfVariable.list.array);
	request->variableAccessSpecification.choice.listOfVariable.list.array = 0;

	request->listOfData.list.count = 0;

	deleteDataElement(request->listOfData.list.array[0]);

	free(request->listOfData.list.array);
	request->listOfData.list.array = 0;

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);

	return rval.encoded;
}
