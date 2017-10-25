bin_PROGRAMS = nuTftpServer

nuTftpServer_SOURCES = config.h \
   nuTftpServer.cc \
   nuTftpUtil.cc \
   nuTrace.cc \
   nuSockTool.cc \
   nuAssert.h  \
   nuCriticalSection.h  \
   nuSockTool.h  \
   nuTerminal.h  \
   nuTftpServer.h  \
   nuTftpUtil.h  \
   nuTrace.h


AM_CXXFLAGS = $(INTI_CFLAGS) -pthread -std=c++11 
nuTftpServer_LDADD = -lpthread $(INTI_LIBS) 

