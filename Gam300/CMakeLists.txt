cmake_minimum_required(VERSION 3.20)
project(Gam300Deps LANGUAGES NONE)

include(ExternalProject)

# ----------------------------------------------------------------------------
# 1) Python embeddable
# ----------------------------------------------------------------------------
set(PYTHON_VERSION  "3.11.8")
set(PYTHON_ZIP_NAME "python-${PYTHON_VERSION}-embed-amd64.zip")
set(PYTHON_ZIP_URL  "https://www.python.org/ftp/python/${PYTHON_VERSION}/${PYTHON_ZIP_NAME}")
set(PYTHON_ZIP_PATH "${CMAKE_BINARY_DIR}/${PYTHON_ZIP_NAME}")
set(PYTHON_DIR      "${CMAKE_BINARY_DIR}/external/python")

ExternalProject_Add(
    PythonEmbed
    URL            ${PYTHON_ZIP_URL}
    DOWNLOAD_DIR   ${CMAKE_BINARY_DIR}
    DOWNLOAD_NAME  ${PYTHON_ZIP_NAME}

    CONFIGURE_COMMAND ""
    BUILD_COMMAND    ""

    # clear default INSTALL_COMMAND...
    INSTALL_COMMAND  ""

    # 1) delete any old embed
    COMMAND          ${CMAKE_COMMAND} -E rm -rf "${PYTHON_DIR}"
    # 2) recreate the folder
    COMMAND          ${CMAKE_COMMAND} -E make_directory "${PYTHON_DIR}"
    # 3) unpack the ZIP into it
    COMMAND          ${CMAKE_COMMAND} -E chdir "${PYTHON_DIR}"
                          ${CMAKE_COMMAND} -E tar xvf "${PYTHON_ZIP_PATH}" --format=zip
    # 4) **debug**: print out the embedded python version
    COMMAND          "${PYTHON_DIR}/python.exe" --version

    LOG_DOWNLOAD ON
    LOG_INSTALL  ON
)

# ----------------------------------------------------------------------------
# 2) meta-target
# ----------------------------------------------------------------------------
add_custom_target(InstallDependencies ALL
    DEPENDS PythonEmbed
    COMMENT "✓ Python embedded unpacked (and version printed above)."
)
