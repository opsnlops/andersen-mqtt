# Debian packaging for andersen-mqtt, built with CPack.
#
# This follows April's fleet packaging policy (agent-memory, "How April
# distributes software"): raw CPack, usr-merged paths, a generated man page,
# a one-entry changelog, dh_strip-style stripping, and a systemd unit.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

set(ANDERSEN_PACKAGE "andersen-mqtt")

#
# Install rules. The packaging prefix is /, so every DESTINATION is relative
# to /. Debian Policy 10.1 forbids shipping to /bin, so the binary goes to
# usr/bin.
#
install(TARGETS andersen_mqtt RUNTIME DESTINATION usr/bin COMPONENT ${ANDERSEN_PACKAGE})

# Strip the way dh_strip does. CPACK_STRIP_FILES runs plain strip, which leaves
# the .comment section that lintian reports.
install(CODE "execute_process(COMMAND \"${CMAKE_STRIP}\" --remove-section=.comment --remove-section=.note
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/bin/andersen-mqtt\" COMMAND_ERROR_IS_FATAL ANY)"
        COMPONENT ${ANDERSEN_PACKAGE})

install(FILES packaging/andersen-mqtt.json DESTINATION etc/andersen-mqtt COMPONENT ${ANDERSEN_PACKAGE})
install(FILES packaging/andersen-mqtt.service DESTINATION usr/lib/systemd/system COMPONENT ${ANDERSEN_PACKAGE})
install(FILES README.md DESTINATION usr/share/doc/${ANDERSEN_PACKAGE} COMPONENT ${ANDERSEN_PACKAGE})
install(FILES LICENSE DESTINATION usr/share/doc/${ANDERSEN_PACKAGE} RENAME copyright COMPONENT ${ANDERSEN_PACKAGE})

find_program(ANDERSEN_GZIP gzip REQUIRED)

# Section-8 manual: help2man takes the options from --help so they can't drift
# from the binary, and the include file adds the service, configuration and
# file layout. The program reads no environment variables, so nothing from the
# build machine can leak into the page.
find_program(ANDERSEN_HELP2MAN help2man REQUIRED)
set(ANDERSEN_MAN "${CMAKE_CURRENT_BINARY_DIR}/andersen-mqtt.8")
set(ANDERSEN_MAN_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/docs/man/andersen-mqtt.h2m")
add_custom_command(
        OUTPUT "${ANDERSEN_MAN}.gz"
        COMMAND "${ANDERSEN_HELP2MAN}" --section=8 --no-info
                "--name=bridge Andersen window controllers to MQTT"
                "--version-string=${PROJECT_VERSION}" "--include=${ANDERSEN_MAN_INCLUDE}"
                "--output=${ANDERSEN_MAN}" "$<TARGET_FILE:andersen_mqtt>"
        # argparse heads its list "Optional arguments:", which help2man doesn't file as OPTIONS
        COMMAND sed -i -E "s/^\\.SS \"?Optional arguments:?\"?$/.SH OPTIONS/" "${ANDERSEN_MAN}"
        COMMAND "${ANDERSEN_GZIP}" -9 -n -f "${ANDERSEN_MAN}"
        DEPENDS andersen_mqtt "${ANDERSEN_MAN_INCLUDE}"
        COMMENT "Generating the andersen-mqtt manual"
        VERBATIM)
add_custom_target(andersen-mqtt-manual ALL DEPENDS "${ANDERSEN_MAN}.gz")
install(FILES "${ANDERSEN_MAN}.gz" DESTINATION usr/share/man/man8 COMPONENT ${ANDERSEN_PACKAGE})


#
# CPack
#
set(CPACK_GENERATOR "DEB")
# Package only our own component. The FetchContent dependencies (fmt, spdlog,
# json, argparse) carry install rules of their own, which would otherwise put
# their headers and static libraries under /include and /lib in the package.
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};${ANDERSEN_PACKAGE};/")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_PACKAGE_NAME ${ANDERSEN_PACKAGE})
set(CPACK_DEBIAN_PACKAGE_NAME ${ANDERSEN_PACKAGE})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

# A timestamp release keeps rebuilds of one version distinct. The version itself
# still has to move between releases or apt won't see a new package.
string(TIMESTAMP NOW "%s")
set(CPACK_DEBIAN_PACKAGE_RELEASE ${NOW})

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "April White <april@opsnlops.io>")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")

# The summary line always comes from CPACK_PACKAGE_DESCRIPTION_SUMMARY; the body
# from CPACK_DEBIAN_PACKAGE_DESCRIPTION.
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bridge Andersen window controllers to MQTT")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Talks to an Andersen window controller through a TCP serial gateway,
polls the status of its four windows, and publishes each window's state
to an MQTT broker as retained topics. Commands published to a window's
command topic open, close or stop that window.")

# shlibdeps finds the linked Boost and C++ libraries. locales-all provides the
# en_US.UTF-8 locale the program sets at startup, which shlibdeps can't see.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "locales-all")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

include(CPack)

# Debian requires a changelog (Policy 12.7). The history lives in git, so each
# build gets a single entry for its version. The package carries a Debian
# revision, so the file is changelog.Debian.gz.
string(TIMESTAMP ANDERSEN_CHANGELOG_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)
set(ANDERSEN_CHANGELOG "${CMAKE_CURRENT_BINARY_DIR}/changelog/changelog.Debian")
file(WRITE "${ANDERSEN_CHANGELOG}"
     "${ANDERSEN_PACKAGE} (${PROJECT_VERSION}-${NOW}) unstable; urgency=medium\n\n"
     "  * Automated build. The change history is in the project's git log:\n"
     "    ${PROJECT_HOMEPAGE_URL}\n\n"
     " -- ${CPACK_DEBIAN_PACKAGE_MAINTAINER}  ${ANDERSEN_CHANGELOG_DATE}\n")
execute_process(COMMAND "${ANDERSEN_GZIP}" -9 -n -f "${ANDERSEN_CHANGELOG}" COMMAND_ERROR_IS_FATAL ANY)
install(FILES "${ANDERSEN_CHANGELOG}.gz" DESTINATION usr/share/doc/${ANDERSEN_PACKAGE} COMPONENT ${ANDERSEN_PACKAGE})
