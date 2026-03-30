set(VXCORE_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/include/vxcore/vxcore.h
    ${PROJECT_SOURCE_DIR}/include/vxcore/vxcore_types.h
    ${PROJECT_SOURCE_DIR}/include/vxcore/vxcore_log.h
)

set(VXCORE_SOURCES
    api/vxcore_api.cpp
    api/vxcore_notebook_api.cpp
    api/vxcore_tag_api.cpp
    api/vxcore_folder_api.cpp
    api/vxcore_node_api.cpp
    api/vxcore_search_api.cpp
    api/vxcore_filetype_api.cpp
    api/vxcore_workspace_api.cpp
    api/vxcore_buffer_api.cpp
    api/vxcore_template_api.cpp
    api/vxcore_snippet_api.cpp
    api/handle_manager.cpp
    api/error_handler.cpp
    core/config_manager.cpp
    core/vxcore_config.cpp
    core/filetype_config.cpp
    core/vxcore_session_config.cpp
    core/workspace.cpp
    core/workspace_manager.cpp
    core/buffer.cpp
    core/buffer_manager.cpp
    core/buffer_provider.cpp
    core/standard_buffer_provider.cpp
    core/external_buffer_provider.cpp
    core/notebook.cpp
    core/bundled_notebook.cpp
    core/raw_notebook.cpp
    core/notebook_manager.cpp
    core/folder.cpp
    core/bundled_folder_manager.cpp
    core/folder_manager.cpp
    core/raw_folder_manager.cpp
    core/template_manager.cpp
    core/snippet_manager.cpp
    db/db_manager.cpp
    db/file_db.cpp
    db/tag_db.cpp
    db/notebook_db.cpp
    db/sqlite_metadata_store.cpp
    search/search_manager.cpp
    search/search_query.cpp
    search/search_file_info.cpp
    search/rg_search_backend.cpp
    search/simple_search_backend.cpp
    utils/utils.cpp
    utils/string_utils.cpp
    utils/base64.cpp
    utils/logger.cpp
    utils/file_utils.cpp
    platform/path_provider.cpp
    platform/process_utils.cpp
)

if(VXCORE_BUILD_SHARED)
    add_library(vxcore SHARED ${VXCORE_SOURCES} ${VXCORE_PUBLIC_HEADERS})
    target_compile_definitions(vxcore PRIVATE VXCORE_BUILD_DLL)
    target_compile_definitions(vxcore PUBLIC VXCORE_SHARED)
else()
    add_library(vxcore STATIC ${VXCORE_SOURCES} ${VXCORE_PUBLIC_HEADERS})
endif()

target_include_directories(vxcore
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(vxcore PRIVATE nlohmann_json sqlite3)

if(WIN32)
    target_link_libraries(vxcore PRIVATE shell32)
    target_compile_definitions(vxcore PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(vxcore PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${VXCORE_PUBLIC_HEADERS}"
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(MSVC)
    target_compile_options(vxcore PRIVATE /W4)
else()
    target_compile_options(vxcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(VXCORE_INSTALL)
    install(TARGETS vxcore
        EXPORT vxcore-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vxcore
    )

    install(EXPORT vxcore-targets
        FILE vxcore-targets.cmake
        NAMESPACE vxcore::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vxcore
    )
endif()
