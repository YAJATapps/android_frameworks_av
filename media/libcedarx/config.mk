#############################################################################
############################## configuration. ###############################
#############################################################################

########## configure CONF_ANDROID_VERSION ##########
CONF_ANDROID_VERSION = $(shell echo $(PLATFORM_VERSION) | cut -c 1-3)
HAS_ANDROID_SELF_CUSTOMIZATION = 0

$(warning CONF_ANDROID_VERSION = $(CONF_ANDROID_VERSION))
ifeq ($(CONF_ANDROID_VERSION), 4.2)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=4
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=2
else ifeq ($(CONF_ANDROID_VERSION), 4.4)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=4
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=4
else ifeq ($(CONF_ANDROID_VERSION), 5.0)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=5
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
else ifeq ($(CONF_ANDROID_VERSION), 5.1)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=5
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=1
else ifeq ($(CONF_ANDROID_VERSION), 6.0)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=6
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
else ifeq ($(CONF_ANDROID_VERSION), 7.0)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=7
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
	LOCAL_32_BIT_ONLY := true
else ifeq ($(CONF_ANDROID_VERSION), 7.1)
        LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=7
        LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=1
        LOCAL_32_BIT_ONLY := true
else ifeq ($(CONF_ANDROID_VERSION), 8.0)
        LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=8
        LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
        LOCAL_32_BIT_ONLY := true
else ifeq ($(CONF_ANDROID_VERSION), 8.1)
        LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=8
        LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=1
        LOCAL_32_BIT_ONLY := true
else ifeq ($(CONF_ANDROID_VERSION), 9)
        LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=9
        LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
        LOCAL_32_BIT_ONLY := true
        #LOCAL_CFLAGS += -Wno-error=unused-variable -Wno-error=unused-function
else ifeq ($(CONF_ANDROID_VERSION), 12)
        LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=12
        LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
        LOCAL_32_BIT_ONLY := true
else
    $(warning "not support android version: "$(CONF_ANDROID_VERSION))
endif

ifeq ($(strip $(HAS_ANDROID_SELF_CUSTOMIZATION)), 1)
	LOCAL_CFLAGS += -DANDROID_SELF_CUSTOMIZATION=1
else
	LOCAL_CFLAGS += -DANDROID_SELF_CUSTOMIZATION=0
endif

############ config IOMMU  #############
ifeq ($(USE_IOMMU), true)
LOCAL_CFLAGS += -DCONF_USE_IOMMU
endif

########## configure SECURE OS ##########
#on semelis secure os, we transform phy addr to secure os to operate the buffer,
#but we adjust on optee secure os, just transform vir addr.
ifeq ($(BOARD_WIDEVINE_OEMCRYPTO_LEVEL), 1)
PLATFORM_SURPPORT_SECURE_OS = yes
LOCAL_CFLAGS +=-DPLATFORM_SURPPORT_SECURE_OS=1

    ifeq ($(SECURE_OS_OPTEE), yes)
        LOCAL_CFLAGS +=-DSECURE_OS_OPTEE=1
        LOCAL_CFLAGS +=-DADJUST_ADDRESS_FOR_SECURE_OS_OPTEE=1
    else
        LOCAL_CFLAGS +=-DSECURE_OS_OPTEE=0
        LOCAL_CFLAGS +=-DADJUST_ADDRESS_FOR_SECURE_OS_OPTEE=0
    endif

else
PLATFORM_SURPPORT_SECURE_OS = no
LOCAL_CFLAGS +=-DPLATFORM_SURPPORT_SECURE_OS=0
endif

########## configure CONFIG_TARGET_PRODUCT ##########
LIB_CEDARM_PATH := $(TOP)/frameworks/av/media/libcedarx

include $(LIB_CEDARM_PATH)/config/$(TARGET_BOARD_PLATFORM)_config.mk

###################################end define####################################

