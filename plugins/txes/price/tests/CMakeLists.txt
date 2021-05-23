cmake_minimum_required(VERSION 3.14)

set(TARGET_NAME tests.catapult.plugins.price)

catapult_tx_plugin_tests(${TARGET_NAME})
target_link_libraries(tests.catapult.plugins.price tests.catapult.test.net)
