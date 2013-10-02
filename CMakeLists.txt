project(libiec61850)

cmake_minimum_required(VERSION 2.6)

include_directories(
    inc
    src/common
    src/goose
    src/hal
    src/hal/ethernet
    src/hal/socket
    src/hal/thread
    src/iedclient
    src/iedcommon
    src/iedserver
    src/iedserver/mms_mapping
    src/iedserver/model
    src/mms/asn1
    src/mms/iso_acse
    src/mms/iso_client
    src/mms/iso_cotp
    src/mms/iso_mms/asn1c
    src/mms/iso_mms/client
    src/mms/iso_mms/common
    src/mms/iso_mms/server
    src/mms/iso_presentation
    src/mms/iso_server
    src/mms/iso_session
)

IF(WIN32)
include_directories(
    src/vs
)
ENDIF(WIN32)

add_subdirectory(examples)
add_subdirectory(src)
