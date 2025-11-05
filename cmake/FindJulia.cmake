# FindJulia.cmake - Locate Julia installation and libraries
#
# This module defines:
#  Julia_FOUND - System has Julia
#  Julia_EXECUTABLE - The Julia executable
#  Julia_VERSION - Julia version
#  Julia_INCLUDE_DIRS - Julia include directories
#  Julia_LIBRARY - Julia library path
#
# User can provide hints via:
#  Julia_ROOT - Installation prefix to search first

# Find Julia executable
find_program(Julia_EXECUTABLE
    NAMES julia
    HINTS ${Julia_ROOT}
    PATH_SUFFIXES bin
    DOC "Julia executable"
)

if(Julia_EXECUTABLE)
    # Get Julia version
    execute_process(
        COMMAND ${Julia_EXECUTABLE} --version
        OUTPUT_VARIABLE Julia_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Extract version number (format: "julia version X.Y.Z")
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" Julia_VERSION "${Julia_VERSION_STRING}")

    # Get include directory
    execute_process(
        COMMAND ${Julia_EXECUTABLE} -e "print(joinpath(Sys.BINDIR, Base.INCLUDEDIR, \"julia\"))"
        OUTPUT_VARIABLE Julia_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get library directory
    execute_process(
        COMMAND ${Julia_EXECUTABLE} -e "print(joinpath(Sys.BINDIR, Base.LIBDIR))"
        OUTPUT_VARIABLE Julia_LIBRARY_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Find libjulia
    find_library(Julia_LIBRARY
        NAMES julia libjulia
        HINTS ${Julia_LIBRARY_DIR}
        NO_DEFAULT_PATH
    )

    set(Julia_INCLUDE_DIRS ${Julia_INCLUDE_DIR})

    # Check if we found everything
    if(Julia_INCLUDE_DIR AND Julia_LIBRARY AND Julia_VERSION)
        set(Julia_FOUND TRUE)
    endif()

    # Check version requirement
    if(Julia_FIND_VERSION AND Julia_VERSION VERSION_LESS Julia_FIND_VERSION)
        set(Julia_FOUND FALSE)
        if(Julia_FIND_REQUIRED)
            message(FATAL_ERROR "Julia version ${Julia_VERSION} found, but ${Julia_FIND_VERSION} required")
        endif()
    endif()
endif()

# Handle REQUIRED and QUIET arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Julia
    REQUIRED_VARS Julia_EXECUTABLE Julia_INCLUDE_DIRS Julia_LIBRARY
    VERSION_VAR Julia_VERSION
)

# Create imported target
if(Julia_FOUND AND NOT TARGET Julia::Julia)
    add_library(Julia::Julia UNKNOWN IMPORTED)
    set_target_properties(Julia::Julia PROPERTIES
        IMPORTED_LOCATION "${Julia_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Julia_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(
    Julia_EXECUTABLE
    Julia_INCLUDE_DIR
    Julia_INCLUDE_DIRS
    Julia_LIBRARY
    Julia_LIBRARY_DIR
)
