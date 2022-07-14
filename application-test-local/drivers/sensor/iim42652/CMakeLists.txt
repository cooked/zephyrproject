# Copyright (c) 2022 Anything Connected
# SPDX-License-Identifier: Apache-2.0

#if (CONFIG_IIM42652)
  # add .h to global include paths
  zephyr_include_directories(.)

  zephyr_library()
  zephyr_library_sources(iim42652.c)
  zephyr_library_sources(iim42652_setup.c)
  zephyr_library_sources(ixm42xxx_i2c.c)
  zephyr_library_sources_ifdef(CONFIG_I2C ixm42xxx_i2c.c)
  zephyr_library_sources_ifdef(CONFIG_SPI ixm42xxx_spi.c)
  #  zephyr_library_sources_ifdef(CONFIG_ICM42605_TRIGGER icm42605_trigger.c)
#endif()

# TODO add support for other sensors of the family in similar fashion
#if (CONFIG_IXM42XXX)
#  zephyr_library_sources(ixm42xxx.c)
#  zephyr_library_sources(ixm42xxx_setup.c)
#endif()
