set(STRESS_TARGETS
    event_compression_stress
    event_loss_stress
    post_event_migration_stress
    stale_posted_event_stress
)

foreach(target IN LISTS STRESS_TARGETS)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE icore)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        LABELS stress
        TIMEOUT 120
    )
endforeach()
