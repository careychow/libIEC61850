/*
 *  mms_write_service.c
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

#include "mms_server_internal.h"
#include "mms_common_internal.h"
#include "mms_types.h"
/**********************************************************************************************
 * MMS Write Service
 *********************************************************************************************/

int
mmsServer_createMmsWriteResponse(MmsServerConnection* connection,
		int invokeId, ByteBuffer* response, MmsValueIndication indication)
{
	MmsPdu_t* mmsPdu = mmsServer_createConfirmedResponse(invokeId);

	mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.present =
			ConfirmedServiceResponse_PR_write;

	WriteResponse_t* writeResponse =
			&(mmsPdu->choice.confirmedResponsePdu.confirmedServiceResponse.choice.write);

	writeResponse->list.count = 1;
	writeResponse->list.size = 1;
	writeResponse->list.array = (struct WriteResponse__Member**) calloc(1, sizeof(struct WriteResponse__Member*));
	writeResponse->list.array[0] =  (struct WriteResponse__Member*) calloc(1, sizeof(struct WriteResponse__Member));


	if (indication == MMS_VALUE_OK)
		writeResponse->list.array[0]->present = WriteResponse__Member_PR_success;
	else {
		writeResponse->list.array[0]->present = WriteResponse__Member_PR_failure;

		if (indication == MMS_VALUE_VALUE_INVALID)
			asn_long2INTEGER(&writeResponse->list.array[0]->choice.failure,
					DataAccessError_objectvalueinvalid);
		else if (indication == MMS_VALUE_ACCESS_DENIED)
			asn_long2INTEGER(&writeResponse->list.array[0]->choice.failure,
					DataAccessError_objectaccessdenied);
		else if (indication == MMS_VALUE_TEMPORARILY_UNAVAILABLE)
		    asn_long2INTEGER(&writeResponse->list.array[0]->choice.failure,
		            DataAccessError_temporarilyunavailable);
	}

	asn_enc_rval_t rval;

	rval = der_encode(&asn_DEF_MmsPdu, mmsPdu,
				mmsServer_write_out, (void*) response);

	if (DEBUG) xer_fprint(stdout, &asn_DEF_MmsPdu, mmsPdu);

	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);

 	return 0;
}


void
MmsServerConnection_sendWriteResponse(MmsServerConnection* self, uint32_t invokeId, MmsValueIndication indication)
{
    ByteBuffer* response = ByteBuffer_create(NULL, self->maxPduSize);

    mmsServer_createMmsWriteResponse(self, invokeId, response, indication);

    IsoConnection_sendMessage(self->isoConnection, response);

    ByteBuffer_destroy(response);
}

int /* MmsServiceError */
mmsServer_handleWriteRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response)
{
	WriteRequest_t* writeRequest = 0;

	MmsPdu_t* mmsPdu = 0;

	asn_dec_rval_t rval; /* Decoder return value  */

	rval = ber_decode(NULL, &asn_DEF_MmsPdu, (void**) &mmsPdu, buffer, MMS_MAXIMUM_PDU_SIZE);

	writeRequest = &(mmsPdu->choice.confirmedRequestPdu.confirmedServiceRequest.choice.write);

	if (writeRequest->variableAccessSpecification.choice.listOfVariable.list.count != 1)
		return -1;

	ListOfVariableSeq_t* varSpec =
			writeRequest->variableAccessSpecification.choice.listOfVariable.list.array[0];

	if (varSpec->variableSpecification.present != VariableSpecification_PR_name) {
		mmsServer_createMmsWriteResponse(connection, invokeId, response, MMS_VALUE_ACCESS_DENIED);
		return 0;
	}

	if (varSpec->variableSpecification.choice.name.present != ObjectName_PR_domainspecific) {
		mmsServer_createMmsWriteResponse(connection, invokeId, response, MMS_VALUE_ACCESS_DENIED);
		return 0;
	}


	Identifier_t domainId = varSpec->variableSpecification.choice.name.choice.domainspecific.domainId;
	char* domainIdStr = createStringFromBuffer(domainId.buf, domainId.size);

	MmsDevice* device = MmsServer_getDevice(connection->server);

	MmsDomain* domain = MmsDevice_getDomain(device, domainIdStr);

	free(domainIdStr);

	if (domain == NULL) {
		mmsServer_createMmsWriteResponse(connection, invokeId, response, MMS_VALUE_ACCESS_DENIED);
		return 0;
	}

	Identifier_t nameId = varSpec->variableSpecification.choice.name.choice.domainspecific.itemId;
	char* nameIdStr = createStringFromBuffer(nameId.buf, nameId.size);

	MmsVariableSpecification* variable = MmsDomain_getNamedVariable(domain, nameIdStr);

	if (variable == NULL)
		goto return_access_denied;

	if (writeRequest->listOfData.list.count != 1)
		goto return_access_denied;

	AlternateAccess_t* alternateAccess = varSpec->alternateAccess;

	if (alternateAccess != NULL) {
		if (variable->type != MMS_ARRAY)
			goto return_access_denied;

		if (!mmsServer_isIndexAccess(alternateAccess))
			goto return_access_denied;
	}

	Data_t* dataElement = writeRequest->listOfData.list.array[0];

	MmsValue* value = mmsMsg_parseDataElement(dataElement);

	if (value == NULL)
		goto return_access_denied;

	if (alternateAccess != NULL) {
		MmsValue* cachedArray = MmsServer_getValueFromCache(connection->server, domain, nameIdStr);

		if (cachedArray == NULL) {
			MmsValue_delete(value);
			goto return_access_denied;
		}

		int index = mmsServer_getLowIndex(alternateAccess);

		MmsValue* elementValue = MmsValue_getElement(cachedArray, index);

		if (elementValue == NULL) {
			MmsValue_delete(value);
			goto return_access_denied;
		}

		if (MmsValue_update(elementValue, value) == false) {
			MmsValue_delete(value);
			goto return_access_denied;
		}
	}

	MmsServer_lockModel(connection->server);

	MmsValueIndication valueIndication =
			mmsServer_setValue(connection->server, domain, nameIdStr, value, connection);

	MmsServer_unlockModel(connection->server);

	if (valueIndication != MMS_VALUE_NO_RESPONSE)
	    mmsServer_createMmsWriteResponse(connection, invokeId, response, valueIndication);

	MmsValue_delete(value);

	free(nameIdStr);


	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
	return 0;

return_access_denied:

	mmsServer_createMmsWriteResponse(connection, invokeId, response, MMS_VALUE_ACCESS_DENIED);
	free(nameIdStr);
	asn_DEF_MmsPdu.free_struct(&asn_DEF_MmsPdu, mmsPdu, 0);
	return 0;
}


