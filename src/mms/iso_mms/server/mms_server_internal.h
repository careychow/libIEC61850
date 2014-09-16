/*
 *  mms_server_internal.h
 *
 *  Copyright 2013, 2014 Michael Zillgith
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

#ifndef MMS_SERVER_INTERNAL_H_
#define MMS_SERVER_INTERNAL_H_

#include "libiec61850_platform_includes.h"

#include <MmsPdu.h>
#include "mms_common.h"
#include "mms_server_connection.h"
#include "mms_device_model.h"
#include "mms_common_internal.h"
#include "stack_config.h"
#include "mms_server.h"

#include "byte_buffer.h"
#include "string_utilities.h"
#include "map.h"
#include "thread.h"

#include "ber_encoder.h"
#include "ber_decode.h"

#ifndef DEBUG_MMS_SERVER
#define DEBUG_MMS_SERVER 0
#endif

#ifndef MMS_STATUS_SERVICE
#define MMS_STATUS_SERVICE 1
#endif

#ifndef MMS_IDENTIFY_SERVICE
#define MMS_IDENTIFY_SERVICE 1
#endif

#ifndef MMS_FILE_SERVICE
#define MMS_FILE_SERVICE 1
#endif

struct sMmsServer {
    IsoServer isoServer;
    MmsDevice* device;

    ReadVariableHandler readHandler;
    void* readHandlerParameter;

    WriteVariableHandler writeHandler;
    void* writeHandlerParameter;

    MmsConnectionHandler connectionHandler;
    void* connectionHandlerParameter;

    Map openConnections;
    Map valueCaches;
    bool isLocked;
    Semaphore modelMutex;

#if MMS_STATUS_SERVICE == 1
    int vmdLogicalStatus;
    int vmdPhysicalStatus;
    MmsStatusRequestListener statusRequestListener;
    void* statusRequestListenerParameter;
#endif /* MMS_STATUS_SERVICE == 1 */

#if MMS_IDENTIFY_SERVICE == 1
    char* vendorName;
    char* modelName;
    char* revision;
#endif /* MMS_IDENTIFY_SERVICE == 1 */
};

/* write_out function required for ASN.1 encoding */
int
mmsServer_write_out(const void *buffer, size_t size, void *app_key);

void
mmsServer_handleDeleteNamedVariableListRequest(MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleGetNamedVariableListAttributesRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response);

void
mmsServer_handleReadRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response);

MmsPdu_t*
mmsServer_createConfirmedResponse(uint32_t invokeId);

void
mmsServer_createConfirmedErrorPdu(uint32_t invokeId, ByteBuffer* response, MmsError errorType);

void
mmsServer_writeConcludeResponsePdu(ByteBuffer* response);

void
mmsServer_handleInitiateRequest (
        MmsServerConnection* self,
        uint8_t* buffer, int bufPos, int maxBufPos,
        ByteBuffer* response);

int
mmsServer_handleGetVariableAccessAttributesRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response);

void
mmsServer_handleDefineNamedVariableListRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleGetNameListRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response);

void
mmsServer_handleWriteRequest(
		MmsServerConnection* connection,
		uint8_t* buffer, int bufPos, int maxBufPos,
		uint32_t invokeId,
		ByteBuffer* response);

void
mmsServer_handleIdentifyRequest(
        MmsServerConnection* connection,
        int invokeId,
        ByteBuffer* response);

void
mmsServer_handleStatusRequest(
        MmsServerConnection* connection,
        uint8_t* requestBuffer,
        int bufPos,
        int invokeId,
        ByteBuffer* response);

void
mmsServer_handleFileDirectoryRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleFileOpenRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleFileDeleteRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleFileRenameRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleFileReadRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

void
mmsServer_handleFileCloseRequest(
        MmsServerConnection* connection,
        uint8_t* buffer, int bufPos, int maxBufPos,
        uint32_t invokeId,
        ByteBuffer* response);

int
mmsServer_isIndexAccess(AlternateAccess_t* alternateAccess);

int
mmsServer_getLowIndex(AlternateAccess_t* alternateAccess);

int
mmsServer_getNumberOfElements(AlternateAccess_t* alternateAccess);

void
mmsServer_deleteVariableList(LinkedList namedVariableLists, char* variableListName);

MmsDataAccessError
mmsServer_setValue(MmsServer self, MmsDomain* domain, char* itemId, MmsValue* value,
        MmsServerConnection* connection);

MmsValue*
mmsServer_getValue(MmsServer self, MmsDomain* domain, char* itemId, MmsServerConnection* connection);

int
mmsServer_createMmsWriteResponse(MmsServerConnection* connection,
        int invokeId, ByteBuffer* response, int numberOfItems, MmsDataAccessError* accessResults);

void
mmsServer_writeMmsRejectPdu(uint32_t* invokeId, int reason, ByteBuffer* response);

#endif /* MMS_SERVER_INTERNAL_H_ */
