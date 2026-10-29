# Remove staged runtime asset files whose source no longer exists.
#
# elite_copy_asset_tree copies file by file and never deletes, so a
# regenerated catalog (renamed or removed system directories, body files,
# interstellar blocks) would leave stale copies that the StarAtlas loads as
# duplicates. Only the generated / regenerated trees are pruned.
#
# cmake -DSOURCE_DIR=<src/assets> -DDEST_DIR=<runtime assets> -P PruneStaleAssets.cmake
foreach(SUBTREE IN ITEMS data/galaxy_details localization/world/star_systems)
    # Only the tree that owns the subtree prunes it (the external asset tree
    # is copied into the same runtime directory and does not own the catalog).
    if(NOT IS_DIRECTORY "${DEST_DIR}/${SUBTREE}" OR NOT IS_DIRECTORY "${SOURCE_DIR}/${SUBTREE}")
        continue()
    endif()
    file(GLOB_RECURSE STAGED RELATIVE "${DEST_DIR}/${SUBTREE}" "${DEST_DIR}/${SUBTREE}/*")
    foreach(RELATIVE_FILE IN LISTS STAGED)
        if(NOT EXISTS "${SOURCE_DIR}/${SUBTREE}/${RELATIVE_FILE}")
            file(REMOVE "${DEST_DIR}/${SUBTREE}/${RELATIVE_FILE}")
            message(STATUS "Removed stale asset: ${SUBTREE}/${RELATIVE_FILE}")
        endif()
    endforeach()
endforeach()
