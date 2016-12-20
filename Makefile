BOARD ?= arduino_101_sss
CONF_FILE = prj_$(ARCH).conf

include ${ZEPHYR_BASE}/Makefile.inc
