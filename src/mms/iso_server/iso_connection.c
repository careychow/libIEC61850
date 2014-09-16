/*
 *  iso_connection.c
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

#include "stack_config.h"
#include "buffer_chain.h"
#include "cotp.h"
#include "iso_session.h"
#include "iso_presentation.h"
#include "acse.h"
#include "iso_server.h"
#include "socket.h"
#include "thread.h"

#include "iso_server_private.h"

#ifndef DEBUG_ISO_SERVER
#ifdef DEBUG
#define DEBUG_ISO_SERVER 1
#else
#define DEBUG_ISO_SERVER 0
#endif /*DEBUG */
#endif /* DEBUG_ISO_SERVER */

#define RECEIVE_BUF_SIZE CONFIG_MMS_MAXIMUM_PDU_SIZE + 100
#define SEND_BUF_SIZE CONFIG_MMS_MAXIMUM_PDU_SIZE + 100

#define ISO_CON_STATE_RUNNING 1
#define ISO_CON_STATE_STOPPED 0

struct sIsoConnection
{
    uint8_t* receiveBuffer;
    uint8_t* sendBuffer;
    ByteBuffer rcvBuffer;

    MessageReceivedHandler msgRcvdHandler;
    void* msgRcvdHandlerParameter;

    IsoServer isoServer;

    Socket socket;
    int state;
    IsoSession* session;
    IsoPresentation* presentation;
    CotpConnection* cotpConnection;

    AcseConnection* acseConnection;

    char* clientAddress;

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    Thread thread;
    Semaphore conMutex;

    bool isInsideCallback;
#endif
};


static void
initIsoConnection(IsoConnection self)
{
    if (DEBUG_ISO_SERVER)
        printf("ISO_SERVER: connection %p initialized\n", self);

    self->cotpConnection = (CotpConnection*) calloc(1, sizeof(CotpConnection));
    CotpConnection_init(self->cotpConnection, self->socket, &(self->rcvBuffer));

    self->session = (IsoSession*) calloc(1, sizeof(IsoSession));
    IsoSession_init(self->session);

    self->presentation = (IsoPresentation*) calloc(1, sizeof(IsoPresentation));
    IsoPresentation_init(self->presentation);

    self->acseConnection = (AcseConnection*) calloc(1, sizeof(AcseConnection));
    AcseConnection_init(self->acseConnection, IsoServer_getAuthenticator(self->isoServer),
            IsoServer_getAuthenticatorParameter(self->isoServer));

    ByteBuffer_wrap(&self->rcvBuffer, self->receiveBuffer, 0, RECEIVE_BUF_SIZE);
}

static void
finalizeIsoConnection(IsoConnection self)
{
    if (DEBUG_ISO_SERVER)
        printf("ISO_SERVER: finalizeIsoConnection --> close transport connection\n");

    IsoServer_closeConnection(self->isoServer, self);
    if (self->socket != NULL)
        Socket_destroy(self->socket);

    free(self->session);
    free(self->presentation);
    AcseConnection_destroy(self->acseConnection);
    free(self->acseConnection);
    CotpConnection_destroy(self->cotpConnection);
    free(self->cotpConnection);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    Semaphore_destroy(self->conMutex);
#endif

    free(self->receiveBuffer);
    free(self->sendBuffer);
    free(self->clientAddress);
    IsoServer isoServer = self->isoServer;
    free(self);
    if (DEBUG_ISO_SERVER)
        printf("ISO_SERVER: connection %p closed\n", self);

    private_IsoServer_decreaseConnectionCounter(isoServer);
}

void
IsoConnection_handleTcpConnection(IsoConnection self)
{
    TpktState tpktState = CotpConnection_readToTpktBuffer(self->cotpConnection);

    if (tpktState == TPKT_ERROR)
        self->state = ISO_CON_STATE_STOPPED;

    if (tpktState != TPKT_PACKET_COMPLETE)
        return;

    CotpIndication cotpIndication = CotpConnection_parseIncomingMessage(self->cotpConnection);

    switch (cotpIndication) {
    case COTP_MORE_FRAGMENTS_FOLLOW:
        return;
    case COTP_CONNECT_INDICATION:
        if (DEBUG_ISO_SERVER)
            printf("ISO_SERVER: COTP connection indication\n");

#if (CONFIG_MMS_THREADLESS_STACK != 1)
        Semaphore_wait(self->conMutex);
#endif

        CotpConnection_sendConnectionResponseMessage(self->cotpConnection);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
        Semaphore_post(self->conMutex);
#endif

        break;
    case COTP_DATA_INDICATION:
        {
            ByteBuffer* cotpPayload = CotpConnection_getPayload(self->cotpConnection);

            if (DEBUG_ISO_SERVER)
                printf("ISO_SERVER: COTP data indication (payload size = %i)\n", cotpPayload->size);

            IsoSessionIndication sIndication = IsoSession_parseMessage(self->session, cotpPayload);

            ByteBuffer* sessionUserData = IsoSession_getUserData(self->session);

            switch (sIndication) {
            case SESSION_CONNECT:
                if (DEBUG_ISO_SERVER)
                    printf("ISO_SERVER: iso_connection: session connect indication\n");

                if (IsoPresentation_parseConnect(self->presentation, sessionUserData)) {
                    if (DEBUG_ISO_SERVER)
                        printf("ISO_SERVER: iso_connection: presentation ok\n");

                    ByteBuffer* acseBuffer = &(self->presentation->nextPayload);

                    AcseIndication aIndication = AcseConnection_parseMessage(self->acseConnection, acseBuffer);

                    if (aIndication == ACSE_ASSOCIATE) {

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                        Semaphore_wait(self->conMutex);
#endif

                        if (DEBUG_ISO_SERVER)
                            printf("ISO_SERVER: cotp_server: acse associate\n");

                        ByteBuffer mmsRequest;

                        ByteBuffer_wrap(&mmsRequest, self->acseConnection->userDataBuffer,
                                self->acseConnection->userDataBufferSize, self->acseConnection->userDataBufferSize);
                        ByteBuffer mmsResponseBuffer; /* new */

                        ByteBuffer_wrap(&mmsResponseBuffer, self->sendBuffer, 0, SEND_BUF_SIZE);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                        self->isInsideCallback = true;
#endif

                        self->msgRcvdHandler(self->msgRcvdHandlerParameter,
                                &mmsRequest, &mmsResponseBuffer);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                        self->isInsideCallback = false;
#endif

                        struct sBufferChain mmsBufferPartStruct;
                        BufferChain mmsBufferPart = &mmsBufferPartStruct;

                        BufferChain_init(mmsBufferPart, mmsResponseBuffer.size, mmsResponseBuffer.size, NULL,
                                self->sendBuffer);

                        if (mmsResponseBuffer.size > 0) {
                            if (DEBUG_ISO_SERVER)
                                printf("ISO_SERVER: iso_connection: application payload size: %i\n",
                                        mmsResponseBuffer.size);

                            struct sBufferChain acseBufferPartStruct;
                            BufferChain acseBufferPart = &acseBufferPartStruct;

                            acseBufferPart->buffer = self->sendBuffer + mmsBufferPart->length;
                            acseBufferPart->partMaxLength = SEND_BUF_SIZE - mmsBufferPart->length;

                            AcseConnection_createAssociateResponseMessage(self->acseConnection,
                            ACSE_RESULT_ACCEPT, acseBufferPart, mmsBufferPart);

                            struct sBufferChain presentationBufferPartStruct;
                            BufferChain presentationBufferPart = &presentationBufferPartStruct;

                            presentationBufferPart->buffer = self->sendBuffer + acseBufferPart->length;
                            presentationBufferPart->partMaxLength = SEND_BUF_SIZE - acseBufferPart->length;

                            IsoPresentation_createCpaMessage(self->presentation, presentationBufferPart,
                                    acseBufferPart);

                            struct sBufferChain sessionBufferPartStruct;
                            BufferChain sessionBufferPart = &sessionBufferPartStruct;
                            sessionBufferPart->buffer = self->sendBuffer + presentationBufferPart->length;
                            sessionBufferPart->partMaxLength = SEND_BUF_SIZE - presentationBufferPart->length;

                            IsoSession_createAcceptSpdu(self->session, sessionBufferPart, presentationBufferPart);

                            CotpConnection_sendDataMessage(self->cotpConnection, sessionBufferPart);
                        }
                        else {
                            if (DEBUG_ISO_SERVER)
                                printf("ISO_SERVER: iso_connection: association error. No response from application!\n");
                        }

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                        Semaphore_post(self->conMutex);
#endif
                    }
                    else {
                        if (DEBUG_ISO_SERVER)
                            printf("ISO_SERVER: iso_connection: acse association failed\n");
                        self->state = ISO_CON_STATE_STOPPED;
                    }

                }
                break;
            case SESSION_DATA:
                if (DEBUG_ISO_SERVER)
                    printf("ISO_SERVER: iso_connection: session data indication\n");

                if (!IsoPresentation_parseUserData(self->presentation, sessionUserData)) {
                    if (DEBUG_ISO_SERVER)
                        printf("ISO_SERVER: presentation layer error\n");
					self->state = ISO_CON_STATE_STOPPED;
                    break;
                }

                if (self->presentation->nextContextId == self->presentation->mmsContextId) {
                    if (DEBUG_ISO_SERVER)
                        printf("ISO_SERVER: iso_connection: mms message\n");

                    ByteBuffer* mmsRequest = &(self->presentation->nextPayload);

                    ByteBuffer mmsResponseBuffer;

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                    IsoServer_userLock(self->isoServer);
                    Semaphore_wait(self->conMutex);
#endif

                    ByteBuffer_wrap(&mmsResponseBuffer, self->sendBuffer, 0, SEND_BUF_SIZE);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                    self->isInsideCallback = true;
#endif

                    self->msgRcvdHandler(self->msgRcvdHandlerParameter,
                            mmsRequest, &mmsResponseBuffer);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                    self->isInsideCallback = false;
#endif

                    /* send a response if required */
                    if (mmsResponseBuffer.size > 0) {

                        struct sBufferChain mmsBufferPartStruct;
						BufferChain mmsBufferPart = &mmsBufferPartStruct;

                        BufferChain_init(mmsBufferPart, mmsResponseBuffer.size,
                                mmsResponseBuffer.size, NULL, self->sendBuffer);

                        struct sBufferChain presentationBufferPartStruct;
                        BufferChain presentationBufferPart = &presentationBufferPartStruct;
                        presentationBufferPart->buffer = self->sendBuffer + mmsBufferPart->length;
                        presentationBufferPart->partMaxLength = SEND_BUF_SIZE - mmsBufferPart->length;

                        IsoPresentation_createUserData(self->presentation,
                                presentationBufferPart, mmsBufferPart);

                        struct sBufferChain sessionBufferPartStruct;
                        BufferChain sessionBufferPart = &sessionBufferPartStruct;
                        sessionBufferPart->buffer = self->sendBuffer + presentationBufferPart->length;
                        sessionBufferPart->partMaxLength = SEND_BUF_SIZE - presentationBufferPart->length;

                        IsoSession_createDataSpdu(self->session, sessionBufferPart, presentationBufferPart);

                        CotpConnection_sendDataMessage(self->cotpConnection, sessionBufferPart);
                    }

#if (CONFIG_MMS_THREADLESS_STACK != 1)
                    Semaphore_post(self->conMutex);
                    IsoServer_userUnlock(self->isoServer);
#endif
                }
                else {
                    if (DEBUG_ISO_SERVER)
                        printf("ISO_SERVER: iso_connection: unknown presentation layer context!");
                }

                break;

            case SESSION_FINISH:
                if (DEBUG_ISO_SERVER)
                    printf("ISO_SERVER: iso_connection: session finish indication\n");

                if (IsoPresentation_parseUserData(self->presentation, sessionUserData)) {
                    if (DEBUG_ISO_SERVER)
                        printf("ISO_SERVER: iso_connection: presentation ok\n");

                    struct sBufferChain acseBufferPartStruct;
                    BufferChain acseBufferPart = &acseBufferPartStruct;
                    acseBufferPart->buffer = self->sendBuffer;
                    acseBufferPart->partMaxLength = SEND_BUF_SIZE;

                    AcseConnection_createReleaseResponseMessage(self->acseConnection, acseBufferPart);

                    struct sBufferChain presentationBufferPartStruct;
                    BufferChain presentationBufferPart = &presentationBufferPartStruct;
                    presentationBufferPart->buffer = self->sendBuffer + acseBufferPart->length;
                    presentationBufferPart->partMaxLength = SEND_BUF_SIZE - acseBufferPart->length;

                    IsoPresentation_createUserDataACSE(self->presentation, presentationBufferPart, acseBufferPart);

                    struct sBufferChain sessionBufferPartStruct;
                    BufferChain sessionBufferPart = &sessionBufferPartStruct;
                    sessionBufferPart->buffer = self->sendBuffer + presentationBufferPart->length;
                    sessionBufferPart->partMaxLength = SEND_BUF_SIZE - presentationBufferPart->length;

                    IsoSession_createDisconnectSpdu(self->session, sessionBufferPart, presentationBufferPart);

                    CotpConnection_sendDataMessage(self->cotpConnection, sessionBufferPart);
                }

                self->state = ISO_CON_STATE_STOPPED;

                break;

            case SESSION_ABORT:
                if (DEBUG_ISO_SERVER)
                    printf("ISO_SERVER: iso_connection: session abort indication\n");
                self->state = ISO_CON_STATE_STOPPED;
                break;

            case SESSION_ERROR:
                if (DEBUG_ISO_SERVER)
                    printf("ISO_SERVER: iso_connection: session error indication\n");
                self->state = ISO_CON_STATE_STOPPED;
                break;

            default: /* illegal state */
                if (DEBUG_ISO_SERVER)
                    printf("ISO_SERVER: iso_connection: session illegal state\n");

                self->state = ISO_CON_STATE_STOPPED;
                break;
            }

            CotpConnection_resetPayload(self->cotpConnection);
        }
        break;
    case COTP_ERROR:
        if (DEBUG_ISO_SERVER)
            printf("ISO_SERVER: Connection closed\n");
        self->state = ISO_CON_STATE_STOPPED;
        break;
    default:
        if (DEBUG_ISO_SERVER)
            printf("ISO_SERVER: COTP unknown indication: %i\n", cotpIndication);
        self->state = ISO_CON_STATE_STOPPED;
        break;
    }
}

#if (CONFIG_MMS_SINGLE_THREADED == 0)
static void
handleTcpConnection(void* parameter)
{
    IsoConnection self = (IsoConnection) parameter;

    while(self->state == ISO_CON_STATE_RUNNING) {
        IsoConnection_handleTcpConnection(self);
        Thread_sleep(1);
    }

    finalizeIsoConnection(self);
}
#endif /* (CONFIG_MMS_SINGLE_THREADED == 0) */

IsoConnection
IsoConnection_create(Socket socket, IsoServer isoServer)
{
    IsoConnection self = (IsoConnection) calloc(1, sizeof(struct sIsoConnection));
    self->socket = socket;
    self->receiveBuffer = (uint8_t*) malloc(RECEIVE_BUF_SIZE);
    self->sendBuffer = (uint8_t*) malloc(SEND_BUF_SIZE);
    self->msgRcvdHandler = NULL;
    self->msgRcvdHandlerParameter = NULL;
    self->isoServer = isoServer;
    self->state = ISO_CON_STATE_RUNNING;
    self->clientAddress = Socket_getPeerAddress(self->socket);

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    self->isInsideCallback = false;
    self->conMutex = Semaphore_create(1);
#endif

    initIsoConnection(self);

    assert(self->msgRcvdHandlerParameter != NULL);

    if (DEBUG_ISO_SERVER)
        printf("ISO_SERVER: IsoConnection: Start to handle connection for client %s\n", self->clientAddress);

#if (CONFIG_MMS_SINGLE_THREADED == 0)
#if (CONFIG_MMS_THREADLESS_STACK == 0)
    self->thread = Thread_create((ThreadExecutionFunction) handleTcpConnection, self, true);

    Thread_start(self->thread);
#endif
#endif

    return self;
}

void
IsoConnection_destroy(IsoConnection self)
{
    if (DEBUG_ISO_SERVER)
        printf("ISO_SERVER: destroy called for IsoConnection.\n");

    finalizeIsoConnection(self);
}

char*
IsoConnection_getPeerAddress(IsoConnection self)
{
    return self->clientAddress;
}

void
IsoConnection_sendMessage(IsoConnection self, ByteBuffer* message, bool handlerMode)
{
    if (self->state == ISO_CON_STATE_STOPPED) {
        if (DEBUG_ISO_SERVER)
            printf("DEBUG_ISO_SERVER: sendMessage: connection already stopped!\n");
        return;
    }

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    if (self->isInsideCallback == false)
        Semaphore_wait(self->conMutex);
#endif

    struct sBufferChain payloadBufferStruct;
    BufferChain payloadBuffer = &payloadBufferStruct;
    payloadBuffer->length = message->size;
    payloadBuffer->partLength = message->size;
    payloadBuffer->partMaxLength = message->size;
    payloadBuffer->buffer = message->buffer;
    payloadBuffer->nextPart = NULL;

    struct sBufferChain presentationBufferStruct;
    BufferChain presentationBuffer = &presentationBufferStruct;
    presentationBuffer->buffer = self->sendBuffer;
    presentationBuffer->partMaxLength = SEND_BUF_SIZE;

    IsoPresentation_createUserData(self->presentation,
            presentationBuffer, payloadBuffer);

    struct sBufferChain sessionBufferStruct;
    BufferChain sessionBuffer = &sessionBufferStruct;
    sessionBuffer->buffer = self->sendBuffer + presentationBuffer->partLength;

    IsoSession_createDataSpdu(self->session, sessionBuffer, presentationBuffer);

    CotpIndication indication;

    indication = CotpConnection_sendDataMessage(self->cotpConnection, sessionBuffer);

    if (DEBUG_ISO_SERVER) {
        if (indication != COTP_OK)
            printf("ISO_SERVER: IsoConnection_sendMessage failed!\n");
        else
            printf("ISO_SERVER: IsoConnection_sendMessage success!\n");
    }

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    if (self->isInsideCallback == false)
        Semaphore_post(self->conMutex);
#endif
}

void
IsoConnection_close(IsoConnection self)
{
    if (self->state != ISO_CON_STATE_STOPPED) {
        Socket socket = self->socket;
        self->state = ISO_CON_STATE_STOPPED;
        self->socket = NULL;

        Socket_destroy(socket);
    }
}

void
IsoConnection_installListener(IsoConnection self, MessageReceivedHandler handler,
        void* parameter)
{
    self->msgRcvdHandler = handler;
    self->msgRcvdHandlerParameter = parameter;
}

void*
IsoConnection_getSecurityToken(IsoConnection self)
{
    return self->acseConnection->securityToken;
}

bool
IsoConnection_isRunning(IsoConnection self)
{
    if (self->state == ISO_CON_STATE_RUNNING)
        return true;
    else
        return false;
}
