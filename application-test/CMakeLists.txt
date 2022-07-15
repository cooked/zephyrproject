# SPDX-License-Identifier: Apache-2.0

# Add the driver directory as a zephyr module by hand. If your driver is a
# project that's managed by west, you can remove this line.

#list(APPEND ZEPHYR_EXTRA_MODULES
#	$ENV{BASE_DIR}/drivers/iim42652
#)

#set(ZEPHYR_EXTRA_MODULES $ENV{BASE_DIR}/drivers/iim42652)


cmake_minimum_required(VERSION 3.20.0)

# required for apps
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(application-test-local)

#zephyr_library_include_directories(.)

zephyr_include_directories(src)



# Alternatively, use a glob to include all C source files
# FILE(GLOB app_sources src/*.c)
# target_sources(app PRIVATE ${app_sources})
target_sources(app PRIVATE
	src/main.c
)
target_sources(app PRIVATE src/ixm42xxx_i2c.c)
target_sources(app PRIVATE src/ixm42xxx_util.c)
target_sources(app PRIVATE src/iim42652.c)
target_sources(app PRIVATE src/iim42652_setup.c)


# directly include drivers
#zephyr_library_include_directories(../drivers/iim42652)
#zephyr_include_directories(../drivers/iim42652)
