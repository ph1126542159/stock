if(NOT DEFINED INPUT_FILE OR NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR "INPUT_FILE must point to an existing get_container_node_sizes.cmake file.")
endif()

file(READ "${INPUT_FILE}" _myiot_patch_contents)

set(_myiot_align_regex_old
    "string(REGEX MATCH \"align_of<.*,[ ]*([0-9]+)[ul ]*>\" align_of_matched"
)
set(_myiot_align_regex_new
    "string(REGEX MATCH \"[A-Za-z_]*ign_of<.*,[ ]*([0-9]+)[ul ]*>\" align_of_matched"
)

set(_myiot_node_regex_old
    "string(REGEX MATCH \"node_size_of<[ ]*([0-9]+)[ul ]*,[ ]*([0-9]+)[ul ]*,[ ]*true[ ]*>\" node_size_of_match"
)
set(_myiot_node_regex_old_intermediate
    "string(REGEX MATCH \"[A-Za-z_]*node_size_of<[ ]*([0-9]+)[ul ]*,[ ]*([0-9]+)[ul ]*,[ ]*true[ ]*>\" node_size_of_match"
)
set(_myiot_node_regex_new
    "string(REGEX MATCH \"[A-Za-z_]*size_of<[ ]*([0-9]+)[ul ]*,[ ]*([0-9]+)[ul ]*,[ ]*true[ ]*>\" node_size_of_match"
)

string(REPLACE
    "${_myiot_align_regex_old}"
    "${_myiot_align_regex_new}"
    _myiot_patch_contents
    "${_myiot_patch_contents}"
)
string(REPLACE
    "${_myiot_node_regex_old}"
    "${_myiot_node_regex_new}"
    _myiot_patch_contents
    "${_myiot_patch_contents}"
)
string(REPLACE
    "${_myiot_node_regex_old_intermediate}"
    "${_myiot_node_regex_new}"
    _myiot_patch_contents
    "${_myiot_patch_contents}"
)

file(WRITE "${INPUT_FILE}" "${_myiot_patch_contents}")
