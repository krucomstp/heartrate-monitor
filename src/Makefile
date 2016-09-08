ccflags-y += ${PROJECTINCLUDE} \
		-I$(ZEPHYR_BASE)/include/drivers \
		-I$(ZEPHYR_BASE)/drivers

ifeq ($(ARCH), arc)
	obj-y = main_arc.o heartrate.o
else
	obj-y = main_x86.o
endif
