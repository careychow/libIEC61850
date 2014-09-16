/*
 *  mms_client_named_variable_list.c
 *
 *  MMS named variable list services (client)
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
#include "stack_config.h"
#include "mms_common.h"
#include "mms_client_connection.h"
#include "byte_buffer.h"
#include "string_utilities.h"
#include "mms_client_internal.h"
#include "mms_common_internal.h"


void
mmsClient_createDeleteNamedVariableListRequest(long invokeId, ByteBuffer* writeBuffer,
		const char* domainId, const char* listNameId)
{
	MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

	mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
				ConfirmedServiceRequest_PR_deleteNamedVariableList;

	DeleteNamedVariableListRequest_t* request =
			&(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.deleteNamedVariableList);

	request->listOfVariableListName = (struct DeleteNamedVariableListRequest__listOfVariableListName*) calloc(1,
			sizeof(struct DeleteNamedVariableListRequest__listOfVariableListName));

	request->listOfVariableListName->list.count = 1;
	request->listOfVariableListName->list.size = 1;

	request->listOfVariableListName->list.array = (ObjectName_t**) calloc(1, sizeof(ObjectName_t*));
	request->listOfVariableListName->list.array[0] = (ObjectName_t*) calloc(1, sizeof(ObjectName_t));

	request->listOfVariableListName->list.array[0]->present = ObjectName_PR_domainspecific;
	request->listOfVariableListName->list.array[0]->choice.domainspecific.domainId.size = strlen(domainId);
	request->listOfVariableListName->list.array[0]->choice.domainspecific.domainId.buf = (uint8_t*) copyString(domainId);
	request->listOfVariableListName->list.array[0]->choice.domainspecific.itemId.size = strlen(listNameId);
	request->listOfVariableListName->list.array[0]->choice.domainspecific.itemId.buf = (uint8_t*) copyString(listNameId);

	request->scopeOfDelete = (INTEGER_t*) calloc(1, sizeof(INTEGER_t));
	asn_long2INTEGER(request->scopeOfDelete, DeleteNamedVariableListRequest__scopeOfDelete_specific);

    der_encode(&asn_DEF_MmsPdu, mmsPdu,
			(asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
}

void
mmsClient_createDeleteAssociationSpecificNamedVariableListRequest(
		long invokeId,
		ByteBuffer* writeBuffer,
		const char* listNameId)
{
	MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

	mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
				ConfirmedServiceRequest_PR_deleteNamedVariableList;

	DeleteNamedVariableListRequest_t* request =
			&(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.deleteNamedVariableList);

	request->listOfVariableListName = (struct DeleteNamedVariableListRequest__listOfVariableListName*) calloc(1,
			sizeof(struct DeleteNamedVariableListRequest__listOfVariableListName));

	request->listOfVariableListName->list.count = 1;
	request->listOfVariableListName->list.size = 1;

	request->listOfVariableListName->list.array = (ObjectName_t**) calloc(1, sizeof(ObjectName_t*));
	request->listOfVariableListName->list.array[0] = (ObjectName_t*) calloc(1, sizeof(ObjectName_t));

	request->listOfVariableListName->list.array[0]->present = ObjectName_PR_aaspecific;

	request->listOfVariableListName->list.array[0]->choice.aaspecific.size = strlen(listNameId);
	request->listOfVariableListName->list.array[0]->choice.aaspecific.buf = (uint8_t*) copyString(listNameId);

	request->scopeOfDelete = (INTEGER_t*) calloc(1, sizeof(INTEGER_t));
	asn_long2INTEGER(request->scopeOfDelete, DeleteNamedVariableListRequest__scopeOfDelete_specific);

	der_encode(&asn_DEF_MmsPdu, mmsPdu,
	      (asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
}

bool
mmsClient_parseDeleteNamedVariableListResponse(ByteBuffer* message, uint32_t* invokeId)
{
	MmsPdu_t* mmsPdu = 0;

	bool retVal = false;

	asn_dec_rval_t rval = ber_decode(NULL, &asn_DEF_MmsPdu,
			(void**) &mmsPdu, ByteBuffer_getBuffer(message), ByteBuffer_getSize(message));

	if (rval.code == RC_OK) {
		if (mmsPdu->present == MmsPdu_PR_confirmedResponsePdu) {

		    if (invokeId != NULL)
		        *invokeId = mmsClient_getInvokeId(&mmsPdu->choice.confirmedResponsePdu);

			if (mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.present ==
					ConfirmedServiceResponse_PR_deleteNamedVariableList)
			{
				DeleteNamedVariableListResponse_t* response =
					&(mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.choice.deleteNamedVariableList);

				long numberDeleted;

				asn_INTEGER2long(&(response->numberDeleted), &numberDeleted);

				if (numberDeleted == 1)
					retVal = true;
			}
		}
	}

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);

	return retVal;
}


void
mmsClient_createGetNamedVariableListAttributesRequest(uint32_t invokeId, ByteBuffer* writeBuffer,
		const char* domainId, const char* listNameId)
{
	MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

	mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
				ConfirmedServiceRequest_PR_getNamedVariableListAttributes;

	GetNamedVariableListAttributesRequest_t* request =
			&(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.getNamedVariableListAttributes);

	request->present = ObjectName_PR_domainspecific;

	request->choice.domainspecific.domainId.size = strlen(domainId);
	request->choice.domainspecific.domainId.buf = (uint8_t*) copyString(domainId);

	request->choice.domainspecific.itemId.size = strlen(listNameId);
	request->choice.domainspecific.itemId.buf = (uint8_t*) copyString(listNameId);

	der_encode(&asn_DEF_MmsPdu, mmsPdu,
			(asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
}

void
mmsClient_createGetNamedVariableListAttributesRequestAssociationSpecific(uint32_t invokeId,
        ByteBuffer* writeBuffer, const char* listNameId)
{
    MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

    mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
                ConfirmedServiceRequest_PR_getNamedVariableListAttributes;

    GetNamedVariableListAttributesRequest_t* request =
            &(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.getNamedVariableListAttributes);

    request->present = ObjectName_PR_aaspecific;

    request->choice.aaspecific.size = strlen(listNameId);
    request->choice.aaspecific.buf = (uint8_t*) copyString(listNameId);

    der_encode(&asn_DEF_MmsPdu, mmsPdu,
            (asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

    asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
}

static LinkedList /* <MmsVariableAccessSpecification*> */
parseNamedVariableAttributes(GetNamedVariableListAttributesResponse_t* response, bool* deletable)
{
	if (deletable != NULL)
		*deletable = response->mmsDeletable;

	int attributesCount = response->listOfVariable.list.count;
	int i;

	LinkedList attributes = LinkedList_create();

	for (i = 0; i < attributesCount; i++) {
	    char* domainId = mmsMsg_createStringFromAsnIdentifier(response->listOfVariable.list.array[i]->
	            variableSpecification.choice.name.choice.domainspecific.domainId);

		char* itemId = mmsMsg_createStringFromAsnIdentifier(response->listOfVariable.list.array[i]->
				variableSpecification.choice.name.choice.domainspecific.itemId);

		MmsVariableAccessSpecification* listEntry = MmsVariableAccessSpecification_create(domainId, itemId);

		LinkedList_add(attributes, listEntry);
	}

	return attributes;
}

LinkedList /* <MmsVariableAccessSpecification*> */
mmsClient_parseGetNamedVariableListAttributesResponse(ByteBuffer* message, uint32_t* invokeId,
		bool* /*OUT*/ deletable)
{
	MmsPdu_t* mmsPdu = 0;

	LinkedList attributes = NULL;

	asn_dec_rval_t rval = ber_decode(NULL, &asn_DEF_MmsPdu,
			(void**) &mmsPdu, ByteBuffer_getBuffer(message), ByteBuffer_getSize(message));


	if (rval.code == RC_OK) {
		if (mmsPdu->present == MmsPdu_PR_confirmedResponsePdu) {

		    if (invokeId != NULL)
		        *invokeId = mmsClient_getInvokeId(&mmsPdu->choice.confirmedResponsePdu);

			if (mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.present ==
					ConfirmedServiceResponse_PR_getNamedVariableListAttributes)
			{
				attributes = parseNamedVariableAttributes(
						&(mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.choice.getNamedVariableListAttributes),
						deletable);
			}
		}
	}

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);

	return attributes;
}


void
mmsClient_createDefineNamedVariableListRequest(
		uint32_t invokeId,
		ByteBuffer* writeBuffer,
		const char* domainId,
		const char* listNameId,
		LinkedList /*<MmsVariableSpecification*>*/ listOfVariables,
		bool associationSpecific)
{
	MmsPdu_t* mmsPdu = mmsClient_createConfirmedRequestPdu(invokeId);

	mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.present =
				ConfirmedServiceRequest_PR_defineNamedVariableList;

	DefineNamedVariableListRequest_t* request =
		&(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.defineNamedVariableList);

	if (associationSpecific) {
		request->variableListName.present = ObjectName_PR_aaspecific;

		request->variableListName.choice.aaspecific.size = strlen(listNameId);
		request->variableListName.choice.aaspecific.buf = (uint8_t*) copyString(listNameId);
	}
	else {
		request->variableListName.present = ObjectName_PR_domainspecific;

		request->variableListName.choice.domainspecific.domainId.size = strlen(domainId);
		request->variableListName.choice.domainspecific.domainId.buf = (uint8_t*) copyString(domainId);

		request->variableListName.choice.domainspecific.itemId.size = strlen(listNameId);
		request->variableListName.choice.domainspecific.itemId.buf = (uint8_t*) copyString(listNameId);
	}

	int listSize = LinkedList_size(listOfVariables);

	request->listOfVariable.list.count = listSize;
	request->listOfVariable.list.size = listSize;

	request->listOfVariable.list.array = 
		(struct DefineNamedVariableListRequest__listOfVariable__Member**) calloc(listSize, sizeof(void*));

	int i = 0;
	LinkedList element = LinkedList_getNext(listOfVariables);
	while (i < listSize) {

		MmsVariableAccessSpecification* variableSpec = (MmsVariableAccessSpecification*) element->data;

		request->listOfVariable.list.array[i] = (struct DefineNamedVariableListRequest__listOfVariable__Member*)
				calloc(1, sizeof(struct DefineNamedVariableListRequest__listOfVariable__Member));

		request->listOfVariable.list.array[i]->variableSpecification.present =
				VariableSpecification_PR_name;

		request->listOfVariable.list.array[i]->variableSpecification.choice.name.present =
				ObjectName_PR_domainspecific;

		request->listOfVariable.list.array[i]->variableSpecification.choice.name.choice.
			domainspecific.domainId.size = strlen(variableSpec->domainId);

		request->listOfVariable.list.array[i]->variableSpecification.choice.name.choice.
			domainspecific.domainId.buf = (uint8_t*) copyString(variableSpec->domainId);

		request->listOfVariable.list.array[i]->variableSpecification.choice.name.choice.
			domainspecific.itemId.size = strlen(variableSpec->itemId);

		request->listOfVariable.list.array[i]->variableSpecification.choice.name.choice.
			domainspecific.itemId.buf = (uint8_t*) copyString(variableSpec->itemId);

		//TODO add alternate access
		if (variableSpec->arrayIndex != -1) {

			AlternateAccess_t* alternateAccess = (AlternateAccess_t*) calloc(1, sizeof(AlternateAccess_t));
			alternateAccess->list.count = 1;
			alternateAccess->list.array = (struct AlternateAccess__Member**) calloc(1, sizeof(struct AlternateAccess__Member*));
			alternateAccess->list.array[0] = (struct AlternateAccess__Member*) calloc(1, sizeof(struct AlternateAccess__Member));

			alternateAccess->list.array[0]->present = AlternateAccess__Member_PR_unnamed;
			alternateAccess->list.array[0]->choice.unnamed = (AlternateAccessSelection_t*) calloc(1, sizeof(AlternateAccessSelection_t));

			alternateAccess->list.array[0]->choice.unnamed->present =
					AlternateAccessSelection_PR_selectAlternateAccess;

			alternateAccess->list.array[0]->choice.unnamed->choice.selectAlternateAccess.accessSelection.present =
					AlternateAccessSelection__selectAlternateAccess__accessSelection_PR_index;

			asn_long2INTEGER(&(alternateAccess->list.array[0]->choice.unnamed->choice.selectAlternateAccess.accessSelection.choice.index),
					variableSpec->arrayIndex);

			if (variableSpec->componentName != NULL) {

				AlternateAccess_t* componentAccess = (AlternateAccess_t*) calloc(1, sizeof(AlternateAccess_t));

				componentAccess->list.count = 1;
				componentAccess->list.array = (struct AlternateAccess__Member**) calloc(1, sizeof(struct AlternateAccess__Member*));
				componentAccess->list.array[0] = (struct AlternateAccess__Member*) calloc(1, sizeof(struct AlternateAccess__Member));

				componentAccess->list.array[0]->present = AlternateAccess__Member_PR_unnamed;
				componentAccess->list.array[0]->choice.unnamed = (AlternateAccessSelection_t*) calloc(1, sizeof(AlternateAccessSelection_t));


				componentAccess->list.array[0]->choice.unnamed->present = AlternateAccessSelection_PR_selectAccess;
				componentAccess->list.array[0]->choice.unnamed->choice.selectAccess.present =
						AlternateAccessSelection__selectAccess_PR_component;

				Identifier_t* componentIdentifier =
						&(componentAccess->list.array[0]->choice.unnamed->choice.selectAccess.choice.component);

				componentIdentifier->size = strlen(variableSpec->componentName);
				componentIdentifier->buf = (uint8_t*) copyString(variableSpec->componentName);

				alternateAccess->list.array[0]->choice.unnamed->choice.selectAlternateAccess.alternateAccess
					= componentAccess;
			}

			request->listOfVariable.list.array[i]->alternateAccess = alternateAccess;
		}


		element = LinkedList_getNext(element);

		i++;
	}

	der_encode(&asn_DEF_MmsPdu, mmsPdu,
		(asn_app_consume_bytes_f*) mmsClient_write_out, (void*) writeBuffer);

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
}

bool
mmsClient_parseDefineNamedVariableResponse(ByteBuffer* message, uint32_t* invokeId)
{
	MmsPdu_t* mmsPdu = 0;
	bool retVal =  false;

	asn_dec_rval_t rval;

	rval = ber_decode(NULL, &asn_DEF_MmsPdu,
			(void**) &mmsPdu, ByteBuffer_getBuffer(message), ByteBuffer_getSize(message));


	if (rval.code == RC_OK) {
		if (mmsPdu->present == MmsPdu_PR_confirmedResponsePdu) {
		    if (invokeId != NULL)
		        *invokeId = mmsClient_getInvokeId(&mmsPdu->choice.confirmedResponsePdu);

			if (mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.present ==
					ConfirmedServiceResponse_PR_defineNamedVariableList)
				retVal = true;
		}
	}

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);

	return retVal;
}
