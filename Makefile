CXX             ?= g++

CXXFLAGS        := -Werror -Wall -Wmissing-declarations -Wpointer-arith \
	                 -Wwrite-strings -Wcast-qual -Wcast-align -Wformat-security \
	                 -Wformat-nonliteral -Wmissing-format-attribute -Winline \
	                 -pedantic-errors -fstack-protector-all -D_FORTIFY_SOURCE=2 \
	                 -fPIC -std=c++11 -pthread -O0 -g

LMCP_CSI_DIR    ?= ./code-gen-3

GENERICAPI_DIR   = $(LMCP_CSI_DIR)/pkg_deps/genericapi
TRANSPORTS_DIR   = $(GENERICAPI_DIR)/pkg_deps/transports-cpp

INCLUDES         = -I$(LMCP_CSI_DIR)/include -I$(GENERICAPI_DIR)/include \
	            	 -I$(GENERICAPI_DIR)/serializers/include \
	                 -I$(GENERICAPI_DIR)/serializers/serializer_lmcp/include \
	                 -I$(TRANSPORT_DIR)/include

LIBPATHS         = -L$(LMCP_CSI_DIR)/build/libs -L$(GENERICAPI_DIR)/build/libs \
	                 -L$(GENERICAPI_DIR)/serializers/serializer_lmcp/build/libs

LIBS             = $(GENERICAPI_DIR)/serializers/serializer_lmcp/build/libs/libtangramlmcpserializer.a \
			 $(LMCP_CSI_DIR)/build/libs/libtangramgeneric.a \
			 $(GENERICAPI_DIR)/build/libs/libtangramgenericapi.a


all: lmcp_sender

lmcp_sender: lmcp_sender.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) lmcp_sender.cpp -o lmcp_sender $(LIBPATHS) $(LIBS)

clean:
	@rm -f lmcp_sender
