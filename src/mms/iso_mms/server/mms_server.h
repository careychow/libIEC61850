/*
 *  mms_server.h
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

#ifndef MMS_SERVER_H_
#define MMS_SERVER_H_

/** \addtogroup mms_server_api_group
 *  @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "mms_device_model.h"
#include "mms_value.h"

#include "iso_server.h"

typedef enum {
	MMS_SERVER_NEW_CONNECTION, MMS_SERVER_CONNECTION_CLOSED
} MmsServerEvent;

typedef struct sMmsServer* MmsServer;

#if (MMS_FILE_SERVICE == 1)

#ifndef CONFIG_MMS_MAX_NUMBER_OF_OPEN_FILES_PER_CONNECTION
#define CONFIG_MMS_MAX_NUMBER_OF_OPEN_FILES_PER_CONNECTION 5
#endif

#include "filesystem.h"

typedef struct {
        int32_t frsmId;
        uint32_t readPosition;
        uint32_t fileSize;
        FileHandle fileHandle;
} MmsFileReadStateMachine;

#endif /* (MMS_FILE_SERVICE == 1) */

typedef struct sMmsServerConnection {
	int maxServOutstandingCalling;
	int maxServOutstandingCalled;
	int dataStructureNestingLevel;
	int maxPduSize; /* local detail */
	IsoConnection isoConnection;
	MmsServer server;
	LinkedList /*<MmsNamedVariableList>*/namedVariableLists; /* aa-specific named variable lists */
	uint32_t lastInvokeId;


#if (MMS_FILE_SERVICE == 1)
	int32_t nextFrsmId;
	MmsFileReadStateMachine frsms[CONFIG_MMS_MAX_NUMBER_OF_OPEN_FILES_PER_CONNECTION];
#endif

} MmsServerConnection;

typedef MmsValue* (*ReadVariableHandler)(void* parameter, MmsDomain* domain,
		char* variableId, MmsServerConnection* connection);

typedef MmsDataAccessError (*WriteVariableHandler)(void* parameter,
		MmsDomain* domain, char* variableId, MmsValue* value,
		MmsServerConnection* connection);

typedef void (*MmsConnectionHandler)(void* parameter,
		MmsServerConnection* connection, MmsServerEvent event);

MmsServer
MmsServer_create(IsoServer isoServer, MmsDevice* device);

void
MmsServer_destroy(MmsServer self);

void
MmsServer_installReadHandler(MmsServer self, ReadVariableHandler,
		void* parameter);

void
MmsServer_installWriteHandler(MmsServer self, WriteVariableHandler,
		void* parameter);

/**
 * A connection handler will be invoked whenever a new client connection is opened or closed
 */
void
MmsServer_installConnectionHandler(MmsServer self, MmsConnectionHandler,
		void* parameter);

void
MmsServer_setClientAuthenticator(MmsServer self, AcseAuthenticator authenticator, void* authenticatorParameter);

MmsDevice*
MmsServer_getDevice(MmsServer self);

MmsValue*
MmsServer_getValueFromCache(MmsServer self, MmsDomain* domain, char* itemId);

bool
MmsServer_isLocked(MmsServer self);

/**
 * \brief lock the cached server data model
 *
 * NOTE: This method should never be called inside of a library callback function. In the context of
 * a library callback the data model is always already locked! Calling this function inside of a
 * library callback may lead to a deadlock condition.
 *
 * \param self the MmsServer instance to operate on
 */
void
MmsServer_lockModel(MmsServer self);

/**
 * \brief unlock the cached server data model
 *
 * NOTE: This method should never be called inside of a library callback function. In the context of
 * a library callback the data model is always already locked!
 *
 * \param self the MmsServer instance to operate on
 */
void
MmsServer_unlockModel(MmsServer self);

void
MmsServer_insertIntoCache(MmsServer self, MmsDomain* domain, char* itemId,
		MmsValue* value);

void
MmsServer_setDevice(MmsServer self, MmsDevice* device);

/***************************************************
 * Functions for multi-threaded operation mode
 ***************************************************/

/**
 * \brief Start a new server thread and listen for incoming connections
 *
 * \param self the MmsServer instance to operate on
 * \param tcpPort the TCP port the server is listening on.
 */
void
MmsServer_startListening(MmsServer self, int tcpPort);

/**
 * \brief Stop server thread an all open connection threads
 *
 * \param self the MmsServer instance to operate on
 */
void
MmsServer_stopListening(MmsServer self);

/***************************************************
 * Functions for threadless operation mode
 ***************************************************/

/**
 * \brief Start a new server in threadless operation mode
 *
 * \param self the MmsServer instance to operate on
 * \param tcpPort the TCP port the server is listening on.
 */
void
MmsServer_startListeningThreadless(MmsServer self, int tcpPort);

/**
 * \brief Handle client connections (for threadless operation mode)
 *
 * This function is listening for new client connections and handles incoming
 * requests for existing client connections.
 *
 * \param self the MmsServer instance to operate on
 */
void
MmsServer_handleIncomingMessages(MmsServer self);

/**
 * \brief Stop the server (for threadless operation mode)
 *
 * \param self the MmsServer instance to operate on
 */
void
MmsServer_stopListeningThreadless(MmsServer self);


/***************************************************
 * Functions for MMS identify service
 ***************************************************/

/**
 * \brief set the values that the server will give as response for an MMS identify request
 *
 * With this function the VMD identity attributes can be set programmatically. If not set by this
 * function the default values form stack_config.h are used.
 *
 * \param self the MmsServer instance to operate on
 * \param vendorName the vendor name attribute of the VMD
 * \param modelName the model name attribute of the VMD
 * \param revision the revision attribute of the VMD
 */
void
MmsServer_setServerIdentity(MmsServer self, char* vendorName, char* modelName, char* revision);

/**
 * \brief get the vendor name attribute of the VMD identity
 *
 * \param self the MmsServer instance to operate on
 * \return the vendor name attribute of the VMD as C string
 */
char*
MmsServer_getVendorName(MmsServer self);

/**
 * \brief get the model name attribute of the VMD identity
 *
 * \param self the MmsServer instance to operate on
 * \return the model name attribute of the VMD as C string
 */
char*
MmsServer_getModelName(MmsServer self);

/**
 * \brief get the revision attribute of the VMD identity
 *
 * \param self the MmsServer instance to operate on
 * \return the revision attribute of the VMD as C string
 */
char*
MmsServer_getRevision(MmsServer self);

/***************************************************
 * Functions for MMS status service
 ***************************************************/

#define MMS_LOGICAL_STATE_STATE_CHANGES_ALLOWED 0
#define MMS_LOGICAL_STATE_NO_STATE_CHANGES_ALLOWED 1
#define MMS_LOGICAL_STATE_LIMITED_SERVICES_PERMITTED 2
#define MMS_LOGICAL_STATE_SUPPORT_SERVICES_ALLOWED 3

#define MMS_PHYSICAL_STATE_OPERATIONAL 0
#define MMS_PHYSICAL_STATE_PARTIALLY_OPERATIONAL 1
#define MMS_PHYSICAL_STATE_INOPERATIONAL 2
#define MMS_PHYSICAL_STATE_NEEDS_COMMISSIONING 3

/**
 * \brief User provided handler that is invoked on a MMS status request
 *
 * The extendedDerivation parameter indicates that the client requests the server to perform
 * self diagnosis tests before answering the request.
 *
 * \param parameter is a user provided parameter
 * \param mmsServer is the MmsServer instance
 * \param connection is the MmsServerConnection instance that received the MMS status request
 * \param extendedDerivation indicates if the request contains the extendedDerivation parameter
 */
typedef void (*MmsStatusRequestListener)(void* parameter, MmsServer mmsServer, MmsServerConnection* connection, bool extendedDerivation);

/**
 * \brief set the VMD state values for the VMD status service
 *
 * \param self the MmsServer instance to operate on
 * \param vmdLogicalStatus the logical status attribute of the VMD
 * \param vmdPhysicalStatus the physical status attribute of the VMD
 */
void
MmsServer_setVMDStatus(MmsServer self, int vmdLogicalStatus, int vmdPhysicalStatus);

/**
 * \brief get the logical status attribute of the VMD
 *
 * \param self the MmsServer instance to operate on
 */
int
MmsServer_getVMDLogicalStatus(MmsServer self);

/**
 * \brief get the physical status attribute of the VMD
 *
 * \param self the MmsServer instance to operate on
 */
int
MmsServer_getVMDPhysicalStatus(MmsServer self);

/**
 * \brief set the MmsStatusRequestListener for this MmsServer
 *
 * With this function the API user can register a listener that is invoked whenever a
 * MMS status request is received from a client. Inside of the handler the user can
 * provide new status values with the MmsServer_setVMDStatus function.
 *
 * \param self the MmsServer instance to operate on
 * \param listener the listener that is called when a MMS status request is received
 * \param parameter a user provided parameter that is handed over to the listener
 */
void
MmsServer_setStatusRequestListener(MmsServer self, MmsStatusRequestListener listener, void* parameter);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* MMS_SERVER_H_ */
