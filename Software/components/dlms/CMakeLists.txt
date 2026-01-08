idf_component_register(SRCS "dlms_parser.c"
                    INCLUDE_DIRS "include")


if(TEST_BUILD)
    add_subdirectory(test)
endif()