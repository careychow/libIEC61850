LIBIEC_HOME=.

include make/target_system.mk

LIB_SOURCE_DIRS = src/mms/iso_acse
LIB_SOURCE_DIRS += src/mms/iso_acse/asn1c
LIB_SOURCE_DIRS += src/mms/iso_presentation/asn1c 
LIB_SOURCE_DIRS += src/mms/iso_presentation
LIB_SOURCE_DIRS += src/mms/iso_session
LIB_SOURCE_DIRS += src/common
LIB_SOURCE_DIRS += src/mms/asn1
LIB_SOURCE_DIRS += src/mms/iso_cotp
LIB_SOURCE_DIRS += src/mms/iso_mms/server
LIB_SOURCE_DIRS += src/mms/iso_mms/client
LIB_SOURCE_DIRS += src/mms/iso_client/impl
LIB_SOURCE_DIRS += src/mms/iso_mms/common
LIB_SOURCE_DIRS += src/mms/iso_mms/asn1c
LIB_SOURCE_DIRS += src/mms/iso_server
LIB_SOURCE_DIRS += src/goose
LIB_SOURCE_DIRS += src/iedclient/impl
LIB_SOURCE_DIRS += src/iedcommon
LIB_SOURCE_DIRS += src/iedserver
LIB_SOURCE_DIRS += src/iedserver/model
LIB_SOURCE_DIRS += src/iedserver/mms_mapping
LIB_SOURCE_DIRS += src/hal
ifeq ($(HAL_IMPL), WIN32)
LIB_SOURCE_DIRS += src/hal/socket/win32
LIB_SOURCE_DIRS += src/hal/thread/win32
LIB_SOURCE_DIRS += src/hal/ethernet/win32
else ifeq ($(HAL_IMPL), POSIX)
LIB_SOURCE_DIRS += src/hal/socket/linux
LIB_SOURCE_DIRS += src/hal/thread/linux
LIB_SOURCE_DIRS += src/hal/ethernet/linux
endif

LIB_INCLUDES = -Isrc/mms/iso_presentation/asn1c
LIB_INCLUDES +=	-Isrc/mms/iso_presentation
LIB_INCLUDES +=	-Isrc/mms/iso_session
LIB_INCLUDES +=	-Isrc/mms/iso_cotp
LIB_INCLUDES +=	-Isrc/mms/iso_acse
LIB_INCLUDES +=	-Isrc/mms/iso_acse/asn1c
LIB_INCLUDES +=	-Iinc
LIB_INCLUDES += -Isrc/mms/asn1
LIB_INCLUDES += -Isrc/mms/iso_client
LIB_INCLUDES +=	-Isrc/mms/iso_mms/server
LIB_INCLUDES +=	-Isrc/mms/iso_mms/common
LIB_INCLUDES += -Isrc/mms/iso_mms/client
LIB_INCLUDES +=	-Isrc/mms/iso_mms/asn1c
LIB_INCLUDES +=	-Isrc/common
LIB_INCLUDES +=	-Isrc/hal/socket
LIB_INCLUDES +=	-Isrc/hal/thread
LIB_INCLUDES +=	-Isrc/hal/ethernet
LIB_INCLUDES +=	-Isrc/hal
LIB_INCLUDES +=	-Isrc/goose
LIB_INCLUDES +=	-Isrc/mms/iso_server
LIB_INCLUDES += -Isrc/iedclient
LIB_INCLUDES += -Isrc/iedcommon
LIB_INCLUDES += -Isrc/iedserver
LIB_INCLUDES += -Isrc/iedserver/model
LIB_INCLUDES += -Isrc/iedserver/mms_mapping
ifeq ($(HAL_IMPL), WIN32)
LIB_INCLUDES += -Ithird_party/winpcap/Include
endif


get_sources_from_directory  = $(wildcard $1/*.c)
get_sources = $(foreach dir, $1, $(call get_sources_from_directory,$(dir)))
src_to = $(addprefix $(LIB_OBJS_DIR)/,$(subst .c,$1,$2))

LIB_SOURCES = $(call get_sources,$(LIB_SOURCE_DIRS))

LIB_OBJS = $(call src_to,.o,$(LIB_SOURCES))


all:	lib

lib:	$(LIB_NAME)

dynlib: CFLAGS += -fPIC

dynlib:	$(DYN_LIB_NAME)

.PHONY:	examples

examples:
	cd examples; $(MAKE)

$(LIB_NAME):	$(LIB_OBJS)
	$(AR) r $(LIB_NAME) $(LIB_OBJS)
	$(RANLIB) $(LIB_NAME)
	
$(DYN_LIB_NAME):	$(LIB_OBJS)
	$(CC) $(LDFLAGS) $(DYNLIB_LDFLAGS) -shared -o $(DYN_LIB_NAME) $(LIB_OBJS) $(LDLIBS)

$(LIB_OBJS_DIR)/%.o: %.c
	@echo compiling $(notdir $<)
	$(SILENCE)mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(LIB_INCLUDES) $(OUTPUT_OPTION) $<

clean:
	rm -f $(EXAMPLES)
	rm -rf $(LIB_OBJS_DIR)

