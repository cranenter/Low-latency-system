cmake_minimum_required(VERSION 3.16)

project(low_latency_trading_sim
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(trading_core
    src/md/market_data_replay.cpp
    src/md/order_book.cpp
    src/strategy/Strategy.cpp
    src/strategy/strategy_simulator.cpp
    src/risk/RiskEngine.cpp
    src/risk/pre_trade_risk_engine.cpp
    src/oms/OMS.cpp
    src/oms/order_manager.cpp
    src/oms/order_state_machine.cpp
    src/gateway/order_gateway.cpp
    src/exchange/exchange_simulator.cpp
    src/execution/execution_report_handler.cpp
    src/position/position_manager.cpp
    src/metrics/latency_recorder.cpp
)

target_include_directories(trading_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(trading_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(trading_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(trading_sim apps/trading_sim.cpp)
target_link_libraries(trading_sim PRIVATE trading_core)
set_target_properties(trading_sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
)

add_executable(trading_tests
    tests/test_main.cpp
    tests/test_core_types.cpp
    tests/test_market_data_replay.cpp
    tests/test_order_book.cpp
    tests/test_order_gateway.cpp
    tests/test_order_manager.cpp
    tests/test_order_state_machine.cpp
    tests/test_exchange_simulator.cpp
    tests/test_end_to_end_pipeline.cpp
    tests/test_execution_report_handler.cpp
    tests/test_object_pool.cpp
    tests/test_pre_trade_risk_engine.cpp
    tests/test_position_manager.cpp
    tests/test_latency_recorder.cpp
    tests/test_spsc_queue.cpp
    tests/test_strategy_simulator.cpp
    tests/test_threaded_pipeline.cpp
)
target_link_libraries(trading_tests PRIVATE trading_core)
set_target_properties(trading_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

add_executable(trading_benchmark benchmark/benchmark_main.cpp)
target_link_libraries(trading_benchmark PRIVATE trading_core)
set_target_properties(trading_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark
)

enable_testing()
add_test(NAME trading_tests COMMAND trading_tests)
