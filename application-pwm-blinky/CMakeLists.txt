# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)

project(zephyr-app-letitbeat)



# Alternatively, use a glob to include all C source files
# FILE(GLOB app_sources src/*.c)
# target_sources(app PRIVATE ${app_sources})
target_sources(app PRIVATE
	src/main.c
)

#zephyr_library_include_directories(.)
#zephyr_include_directories(src)

# add_subdirectory(drivers)

# target_include_directories(${PROJECT_NAME} PRIVATE drivers/IMU/mlx90393/src)
# target_link_directories(${PROJECT_NAME} PRIVATE drivers/IMU/mlx90393/src)
# target_link_libraries(${PROJECT_NAME} mlx90393)