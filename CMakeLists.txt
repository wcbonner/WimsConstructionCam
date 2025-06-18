# CMakeList.txt : CMake project for WimsConstructionCam, include source and define
# project specific logic here.
#

# This set of commands should build the package
# git clone https://github.com/wcbonner/WimsConstructionCam.git
# cmake -S WimsConstructionCam -B WimsConstructionCam/build
# cmake --build WimsConstructionCam/build
# pushd WimsConstructionCam/build && cpack . && popd

cmake_minimum_required(VERSION 3.12.0)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (WimsConstructionCam
    VERSION 1.20250618.0
    DESCRIPTION "Records TimeLapse Pictures at one minute intervals and turns them into a daily video"
    HOMEPAGE_URL https://github.com/wcbonner/WimsConstructionCam
)

configure_file(wimsconstructioncam-version.h.in wimsconstructioncam-version.h)

# Add source to this project's executable.
add_executable (wimsconstructioncam wimsconstructioncam.cpp wimsconstructioncam-version.h)
target_link_libraries(wimsconstructioncam -lgps -lgd -lexif -lstdc++fs)
target_compile_definitions(wimsconstructioncam PRIVATE _USE_GPSD)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET wimsconstructioncam PROPERTY CXX_STANDARD 17)
endif()

target_include_directories(wimsconstructioncam PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           ${EXTRA_INCLUDES}
                           )

# TODO: Add tests and install targets if needed.
include(CTest)
add_test(NAME wimsconstructioncam COMMAND wimsconstructioncam --help)

install(TARGETS wimsconstructioncam
    DESTINATION bin
    RUNTIME DESTINATION "/usr/local/bin/"
    LIBRARY DESTINATION "/usr/local/lib/"
)

install(DIRECTORY etc-wimsconstructioncam
    DESTINATION /usr/local/etc/wimsconstructioncam)

install(FILES wimsconstructioncam.service
    DESTINATION "/usr/local/lib/systemd/system"
    COMPONENT "wimsconstructioncam"
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "wcbonner@users.noreply.github.com")
include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE ${CMAKE_PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_SECTION custom)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst" "${CMAKE_CURRENT_SOURCE_DIR}/prerm" "${CMAKE_CURRENT_SOURCE_DIR}/postrm")
set(CPACK_STRIP_FILES YES)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
set(CPACK_DEBIAN_PACKAGE_DEPENDS ffmpeg)
include(CPack)
