CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(udisks-automount-agent)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}")
SET(BINDIR "/usr/bin")
SET(SYSTEMDDIR "${SYSTEMD_SERVICE_DIR}")
SET(POLKITRULESDIR "/etc/polkit-1/localauthority/10-vendor.d")

SET(VERSION 0.0.1)

# Verbose mode on/off
SET(CMAKE_VERBOSE_MAKEFILE off)

SET(SRCS src/udisk-agent.c)

SET(SYSTEMD_SERVICE	data/udisks-automount-agent.service.in)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 gio-2.0 gio-unix-2.0 udisks2 )

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

# Build udisks-automount-agent.service file
EXECUTE_PROCESS( COMMAND /bin/sed -e "s|\@udisksagentbindir\@|${BINDIR}|" INPUT_FILE ${SYSTEMD_SERVICE} OUTPUT_FILE udisks-automount-agent.service ) 

# Executables
ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS} )
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS})

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed ${pkgs_LIBS}")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")

# Install 
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BINDIR})
INSTALL(FILES udisks-automount-agent.service DESTINATION ${SYSTEMDDIR})
INSTALL(FILES data/10-udisks.pkla DESTINATION ${POLKITRULESDIR})
