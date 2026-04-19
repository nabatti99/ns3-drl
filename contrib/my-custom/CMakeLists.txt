# # Find libcurl
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(CURL REQUIRED libcurl)

# Find nlohmann/json
find_package(nlohmann_json REQUIRED)

build_lib(
    LIBNAME my-custom
    SOURCE_FILES
        tags/application-id-tag.cc
        tags/flow-type-tag.cc
        tags/send-time-tag.cc
        tags/enqueue-time-tag.cc
        # helper/http-request.cc
        # queues/bd-fairq2s-queue-disc.cc
        applications/realtime-application.cc
        applications/non-realtime-application.cc
        applications/other-application.cc
        applications/my-application.cc
    HEADER_FILES
        tags/application-id-tag.h
        tags/flow-type-tag.h
        tags/send-time-tag.h
        tags/enqueue-time-tag.h
        helper/my-mapping.h
        # helper/http-request.h
        # queues/bd-fairq2s-queue-disc.h
        applications/realtime-application.h
        applications/non-realtime-application.h
        applications/other-application.h
        applications/my-application.h
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libinternet}
        ${libapplications}
        ${libflow-monitor}
        # ${CURL_LIBRARIES}
        nlohmann_json::nlohmann_json
)

# # Add curl include directories and compile flags
# target_include_directories(my-custom PRIVATE ${CURL_INCLUDE_DIRS})
# target_compile_options(my-custom PRIVATE ${CURL_CFLAGS_OTHER})
