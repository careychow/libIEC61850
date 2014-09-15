/*
 *  cotp.c
 *
 *  ISO 8073 Connection Oriented Transport Protocol over TCP (RFC1006)
 *
 *  Partial implementation of the ISO 8073 COTP (ISO TP0) protocol for MMS.
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

#include "stack_config.h"
#include "cotp.h"
#include "byte_buffer.h"
#include "buffer_chain.h"

#define TPKT_RFC1006_HEADER_SIZE 4

#define COTP_DATA_HEADER_SIZE 3

#ifdef CONFIG_COTP_MAX_TPDU_SIZE
#define COTP_MAX_TPDU_SIZE CONFIG_COTP_MAX_TPDU_SIZE
#else
#define COTP_MAX_TPDU_SIZE 8192
#endif

#ifndef DEBUG_COTP
#define DEBUG_COTP 0
#endif

static bool
addPayloadToBuffer(CotpConnection* self, uint8_t* buffer, int payloadLength);

static inline uint16_t
getUint16(uint8_t* buffer)
{
    return (buffer[0] * 0x100) + buffer[1];
}

static inline uint8_t
getUint8(uint8_t* buffer)
{
    return buffer[0];
}

static void
writeOptions(CotpConnection* self)
{
    // max size = 11 byte
    uint8_t* buffer = self->writeBuffer->buffer;
    int bufPos = self->writeBuffer->size;

    if (self->options.tpdu_size != 0) {

        if (DEBUG_COTP)
            printf("COTP: send TPDU size: %i\n", CotpConnection_getTpduSize(self));

        buffer[bufPos++] = 0xc0;
        buffer[bufPos++] = 0x01;
        buffer[bufPos++] = self->options.tpdu_size;
    }

    if (self->options.tsap_id_dst != -1) {
        buffer[bufPos++] = 0xc2;
        buffer[bufPos++] = 0x02;
        buffer[bufPos++] = (uint8_t) (self->options.tsap_id_dst / 0x100);
        buffer[bufPos++] = (uint8_t) (self->options.tsap_id_dst & 0xff);
    }

    if (self->options.tsap_id_src != -1) {
        buffer[bufPos++] = 0xc1;
        buffer[bufPos++] = 0x02;
        buffer[bufPos++] = (uint8_t) (self->options.tsap_id_src / 0x100);
        buffer[bufPos++] = (uint8_t) (self->options.tsap_id_src & 0xff);
    }

    self->writeBuffer->size = bufPos;
}

static int
getOptionsLength(CotpConnection* self)
{
    int optionsLength = 0;
    if (self->options.tpdu_size != 0)
        optionsLength += 3;
    if (self->options.tsap_id_dst != -1)
        optionsLength += 4;
    if (self->options.tsap_id_src != -1)
        optionsLength += 4;
    return optionsLength;
}

static inline void
writeStaticConnectResponseHeader(CotpConnection* self, int optionsLength)
{
    /* always same size (7) and same position in buffer */
    uint8_t* buffer = self->writeBuffer->buffer;

    buffer[4] = 6 + optionsLength;
    buffer[5] = 0xd0;
    buffer[6] = (uint8_t) (self->srcRef / 0x100);
    buffer[7] = (uint8_t) (self->srcRef & 0xff);
    buffer[8] = (uint8_t) (self->dstRef / 0x100);
    buffer[9] = (uint8_t) (self->dstRef & 0xff);
    buffer[10] = (uint8_t) (self->protocolClass);

    self->writeBuffer->size = 11;
}

static void
writeRfc1006Header(CotpConnection* self, int len)
{
    uint8_t* buffer = self->writeBuffer->buffer;

    buffer[0] = 0x03;
    buffer[1] = 0x00;
    buffer[2] = (uint8_t) (len / 0x100);
    buffer[3] = (uint8_t) (len & 0xff);

    self->writeBuffer->size = 4;
}

static void
writeDataTpduHeader(CotpConnection* self, int isLastUnit)
{
    /* always 3 byte starting from byte 5 in buffer */
    uint8_t* buffer = self->writeBuffer->buffer;

    buffer[4] = 0x02;
    buffer[5] = 0xf0;
    if (isLastUnit)
        buffer[6] = 0x80;
    else
        buffer[6] = 0x00;

    self->writeBuffer->size = 7;
}

static bool
sendBuffer(CotpConnection* self)
{
    int writeBufferPosition = ByteBuffer_getSize(self->writeBuffer);

    bool retVal = false;

    if (Socket_write(self->socket, ByteBuffer_getBuffer(self->writeBuffer), writeBufferPosition) == writeBufferPosition)
        retVal = true;

    ByteBuffer_setSize(self->writeBuffer, 0);

    return retVal;
}

CotpIndication
CotpConnection_sendDataMessage(CotpConnection* self, BufferChain payload)
{
    int fragments = 1;

    int fragmentPayloadSize = CotpConnection_getTpduSize(self) - COTP_DATA_HEADER_SIZE;

    if (payload->length > fragmentPayloadSize) { /* Check if segmentation is required? */
        fragments = payload->length / fragmentPayloadSize;

        if ((payload->length % fragmentPayloadSize) != 0)
            fragments += 1;
    }

    int currentBufPos = 0;
    int currentLimit;
    int lastUnit;

    BufferChain currentChain = payload;
    int currentChainIndex = 0;

    if (DEBUG_COTP)
        printf("\nCOTP: nextBufferPart: len:%i partLen:%i\n", currentChain->length, currentChain->partLength);

    uint8_t* buffer = self->writeBuffer->buffer;

    while (fragments > 0) {
        if (fragments > 1) {
            currentLimit = currentBufPos + fragmentPayloadSize;
            lastUnit = 0;
        }
        else {
            currentLimit = payload->length;
            lastUnit = 1;
        }

        writeRfc1006Header(self, 7 + (currentLimit - currentBufPos));

        writeDataTpduHeader(self, lastUnit);

        int bufPos = 7;

        int i;
        for (i = currentBufPos; i < currentLimit; i++) {

            if (currentChainIndex >= currentChain->partLength) {
                currentChain = currentChain->nextPart;
                if (DEBUG_COTP)
                    printf("\nCOTP: nextBufferPart: len:%i partLen:%i\n", currentChain->length, currentChain->partLength);
                currentChainIndex = 0;
            }

            if (DEBUG_COTP)
                printf("%02x ", currentChain->buffer[currentChainIndex]);

            buffer[bufPos++] = currentChain->buffer[currentChainIndex];

            currentChainIndex++;

            currentBufPos++;
        }

        self->writeBuffer->size = bufPos;

        if (DEBUG_COTP)
            printf("COTP: Send COTP fragment %i bufpos: %i\n", fragments, currentBufPos);

        if (!sendBuffer(self))
            return COTP_ERROR;

        fragments--;
    }

    return COTP_OK;
}

static void
allocateWriteBuffer(CotpConnection* self)
{
    if (self->writeBuffer == NULL )
        self->writeBuffer = ByteBuffer_create(NULL,
                CotpConnection_getTpduSize(self) + TPKT_RFC1006_HEADER_SIZE);
}

/* client side */
CotpIndication
CotpConnection_sendConnectionRequestMessage(CotpConnection* self, IsoConnectionParameters isoParameters)
{
    allocateWriteBuffer(self);

    const int CON_REQUEST_SIZE = 22;

    if(self->writeBuffer->maxSize < CON_REQUEST_SIZE)
        return COTP_ERROR;

    uint8_t* buffer = self->writeBuffer->buffer;

    writeRfc1006Header(self, CON_REQUEST_SIZE);

    /* LI */
    buffer[4] = 17;

    /* TPDU CODE */
    buffer[5] = 0xe0;

    /* DST REF */
    buffer[6] = 0x00;
    buffer[7] = 0x00;

    /* SRC REF */
    buffer[8] = 0x00;
    buffer[9] = 0x02; /* or 0x01 ? */

    /* Class */
    buffer[10] = 0x00;

    self->writeBuffer->size = 11;

    self->options.tsap_id_dst = isoParameters->remoteTSelector;
    self->options.tsap_id_src = 0;

    writeOptions(self);

    if (sendBuffer(self))
        return COTP_OK;
    else
        return COTP_ERROR;
}

CotpIndication
CotpConnection_sendConnectionResponseMessage(CotpConnection* self)
{
    allocateWriteBuffer(self);

    int optionsLength = getOptionsLength(self);
    int messageLength = 11 + optionsLength;

    writeRfc1006Header(self, messageLength);

    writeStaticConnectResponseHeader(self, optionsLength);

    writeOptions(self);

    if (sendBuffer(self))
        return COTP_OK;
    else
        return COTP_ERROR;
}

static bool
parseOptions(CotpConnection* self, uint8_t* buffer, int bufLen)
{
    int bufPos = 0;

    while (bufPos < bufLen) {
        uint8_t optionType = buffer[bufPos++];
        uint8_t optionLen = buffer[bufPos++];

        if (optionLen > (bufLen - bufPos)) {
            if (DEBUG_COTP)
                printf("COTP: option to long optionLen:%i bufPos:%i bufLen:%i\n", optionLen, bufPos, bufLen);
            goto cpo_error;
        }

        if (DEBUG_COTP)
            printf("COTP: option: %02x len: %02x\n", optionType, optionLen);

        switch (optionType) {
        case 0xc0:
			if (optionLen == 1) {
				int requestedTpduSize = (1 << buffer[bufPos++]);

                CotpConnection_setTpduSize(self, requestedTpduSize);

				if (DEBUG_COTP)
				    printf("COTP: requested TPDU size: %i\n", requestedTpduSize);
			}
			else
			    goto cpo_error;
            break;

        case 0xc1:
            if (optionLen == 2) {
                self->options.tsap_id_src = (int32_t) getUint16(buffer + bufPos);
                bufPos += 2;
            }
            else
                goto cpo_error;
            break;

        case 0xc2:
            if (optionLen == 2) {
                self->options.tsap_id_dst = (int32_t) getUint16(buffer + bufPos);
                bufPos += 2;
            }
            else
                goto cpo_error;
            break;

        case 0xc6: /* additional option selection */
            if (optionLen == 1)
                bufPos++; /* ignore value */
            else
                goto cpo_error;
            break;

        default:
            if (DEBUG_COTP)
                printf("COTP: Unknown option %02x\n", optionType);

            bufPos += optionLen; /* ignore value */

            break;
        }
    }

    return true;

cpo_error:
    if (DEBUG_COTP)
        printf("COTP: cotp_parse_options: error parsing options!\n");
    return false;
}

void
CotpConnection_init(CotpConnection* self, Socket socket,
        ByteBuffer* payloadBuffer)
{
    self->state = 0;
    self->socket = socket;
    self->srcRef = -1;
    self->dstRef = -1;
    self->protocolClass = -1;
	self->options.tpdu_size = 0;
	self->options.tsap_id_src = -1;
	self->options.tsap_id_dst = -1;
    self->payload = payloadBuffer;

    /* default TPDU size is maximum size */
    CotpConnection_setTpduSize(self, COTP_MAX_TPDU_SIZE);

    self->writeBuffer = NULL;
    self->readBuffer = ByteBuffer_create(NULL, COTP_MAX_TPDU_SIZE + TPKT_RFC1006_HEADER_SIZE);
    self->packetSize = 0;
}

void
CotpConnection_destroy(CotpConnection* self)
{
    if (self->writeBuffer != NULL)
        ByteBuffer_destroy(self->writeBuffer);

    if (self->readBuffer != NULL)
        ByteBuffer_destroy(self->readBuffer);
}

int /* in byte */
CotpConnection_getTpduSize(CotpConnection* self)
{
    return (1 << self->options.tpdu_size);
}

void
CotpConnection_setTpduSize(CotpConnection* self, int tpduSize /* in byte */)
{
    int newTpduSize = 1;

    if (tpduSize > COTP_MAX_TPDU_SIZE)
        tpduSize = COTP_MAX_TPDU_SIZE;

    while ((1 << newTpduSize) < tpduSize)
        newTpduSize++;

    if ((1 << newTpduSize) > tpduSize)
        newTpduSize--;

    self->options.tpdu_size = newTpduSize;
}

ByteBuffer*
CotpConnection_getPayload(CotpConnection* self)
{
    return self->payload;
}

int
CotpConnection_getSrcRef(CotpConnection* self)
{
    return self->srcRef;
}

int
CotpConnection_getDstRef(CotpConnection* self)
{
    return self->dstRef;
}

/*
 CR TPDU (from RFC 0905):

 1    2        3        4       5   6    7    8    p  p+1...end
 +--+------+---------+---------+---+---+------+-------+---------+
 |LI|CR CDT|     DST - REF     |SRC-REF|CLASS |VARIAB.|USER     |
 |  |1110  |0000 0000|0000 0000|   |   |OPTION|PART   |DATA     |
 +--+------+---------+---------+---+---+------+-------+---------+
 */


static bool
parseConnectRequestTpdu(CotpConnection* self, uint8_t* buffer, uint8_t len)
{
    if (len < 6)
        return false;

    self->dstRef = getUint16(buffer);
    self->srcRef = getUint16(buffer + 2);
    self->protocolClass = getUint8(buffer + 4);

    return parseOptions(self, buffer + 5, len - 6);
}

static bool
parseConnectConfirmTpdu(CotpConnection* self, uint8_t* buffer, uint8_t len)
{
    if (len < 6)
        return false;

    self->srcRef = getUint16(buffer);
    self->dstRef = getUint16(buffer + 2);
    self->protocolClass = getUint8(buffer + 4);

    return parseOptions(self, buffer + 5, len - 6);
}

static bool
parseDataTpdu(CotpConnection* self, uint8_t* buffer, uint8_t len)
{
    if (len != 2)
        return false;

    uint8_t flowControl = getUint8(buffer);

    if (flowControl & 0x80)
        self->isLastDataUnit = true;
    else
        self->isLastDataUnit = false;

    return true;
}

static bool
addPayloadToBuffer(CotpConnection* self, uint8_t* buffer,  int payloadLength)
{
    if (DEBUG_COTP)
        printf("COTP: add to payload buffer (cur size: %i, len: %i)\n", self->payload->size, payloadLength);

    if ((self->payload->size + payloadLength) > self->payload->maxSize)
        return false;

    memcpy(self->payload->buffer + self->payload->size, buffer, payloadLength);

    self->payload->size += payloadLength;

    return true;
}

static CotpIndication
parseCotpMessage(CotpConnection* self)
{
    uint8_t* buffer = self->readBuffer->buffer + 4;
    int tpduLength = self->readBuffer->size - 4;

    uint8_t len;
    uint8_t tpduType;

    len = buffer[0];
    assert(len <= tpduLength);

    tpduType = buffer[1];

    switch (tpduType) {
    case 0xe0:
        if (parseConnectRequestTpdu(self, buffer + 2, len))
            return COTP_CONNECT_INDICATION;
        else
            return COTP_ERROR;
    case 0xd0:
        if (parseConnectConfirmTpdu(self, buffer + 2, len))
            return COTP_CONNECT_INDICATION;
        else
            return COTP_ERROR;
    case 0xf0:
        if (parseDataTpdu(self, buffer + 2, len)) {

            if (addPayloadToBuffer(self, buffer + 3, tpduLength - 3) != 1)
                return COTP_ERROR;

            if (self->isLastDataUnit)
                return COTP_DATA_INDICATION;
            else
                return COTP_MORE_FRAGMENTS_FOLLOW;
        }
        else
            return COTP_ERROR;
    default:
        return COTP_ERROR;
    }

}

CotpIndication
CotpConnection_parseIncomingMessage(CotpConnection* self)
{
    CotpIndication indication = parseCotpMessage(self);

    self->readBuffer->size = 0;
    self->packetSize = 0;

    return indication;
}

void
CotpConnection_resetPayload(CotpConnection* self)
{
    self->payload->size = 0;
}

TpktState
CotpConnection_readToTpktBuffer(CotpConnection* self)
{
    uint8_t* buffer = self->readBuffer->buffer;
    int bufferSize = self->readBuffer->maxSize;
    int bufPos = self->readBuffer->size;

    assert (bufferSize > 4);

    int readBytes;

    if (bufPos < 4) {

        readBytes = Socket_read(self->socket, buffer + bufPos, 4 - bufPos);

        if (readBytes < 0)
            goto exit_closed;

        if (DEBUG_COTP) {
            if (readBytes > 0)
                printf("TPKT: read %i bytes from socket\n", readBytes);
        }

        bufPos += readBytes;

        if (bufPos == 4) {
            if ((buffer[0] == 3) && (buffer[1] == 0)) {
                self->packetSize = (buffer[2] * 0x100) + buffer[3];

                if (DEBUG_COTP)
                    printf("TPKT: header complete (msg size = %i)\n", self->packetSize);

                if (self->packetSize > bufferSize) {
                    if (DEBUG_COTP) printf("TPKT: packet too large\n");
                    goto exit_error;
                }
            }
            else {
                if (DEBUG_COTP) printf("TPKT: failed to decode TPKT header.\n");
                goto exit_error;
            }
        }
        else
            goto exit_waiting;
    }

    readBytes = Socket_read(self->socket, buffer + bufPos, self->packetSize - bufPos);

    if (readBytes < 0)
        goto exit_closed;

    bufPos += readBytes;

    if (bufPos < self->packetSize)
       goto exit_waiting;

    if (DEBUG_COTP) printf("TPKT: message complete (size = %i)\n", self->packetSize);

    self->readBuffer->size = bufPos;
    return TPKT_PACKET_COMPLETE;

exit_closed:
    if (DEBUG_COTP) printf("TPKT: socket closed or socket error\n");
    return TPKT_ERROR;

exit_error:
    if (DEBUG_COTP) printf("TPKT: Error parsing message\n");
    return TPKT_ERROR;

exit_waiting:

    if (DEBUG_COTP)
        if (bufPos != 0)
            printf("TPKT: waiting (read %i of %i)\n", bufPos, self->packetSize);

    self->readBuffer->size = bufPos;
    return TPKT_WAITING;
}

