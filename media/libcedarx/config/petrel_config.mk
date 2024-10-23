#############################################################################
############################# H6 configuration. #############################
#############################################################################

##---: new bdmv stream
LOCAL_CFLAGS += -DCONF_NEW_BDMV_STREAM
##---------------------------------------------------------------------------##

##---: Deinterlace V2X Support // new just support in H6
LOCAL_CFLAGS += -DCONF_DI_V2X_SUPPORT
##---------------------------------------------------------------------------##

##---: new display
CONF_NEW_DISPLAY = yes
LOCAL_CFLAGS += -DCONF_NEW_DISPLAY
##---------------------------------------------------------------------------##

##---: STB // Set Top Box
CONF_PRODUCT_STB = yes
LOCAL_CFLAGS += -DCONF_PRODUCT_STB
##---------------------------------------------------------------------------##

##---: CMCC-OTT // must be STB
CONF_CMCC = no
ifeq ($(CONF_PRODUCT_STB), yes)
    ifeq (cmccwasu, $(TARGET_BUSINESS_PLATFORM))
        CONF_CMCC = yes
    endif

    ifeq ($(CONF_CMCC), yes)
        LOCAL_CFLAGS += -DCONF_CMCC
    endif
endif
##---------------------------------------------------------------------------##

##---: YUNOS-OTT // must be STB
CONF_YUNOS = no
ifeq ($(CONF_PRODUCT_STB), yes)
    ifeq (aliyunos , $(TARGET_BUSINESS_PLATFORM))
        CONF_YUNOS = yes
    endif

    ifeq ($(CONF_YUNOS), yes)
        LOCAL_CFLAGS += -DCONF_YUNOS
    endif
endif
##---------------------------------------------------------------------------##

##---: dtv
CONF_DTV = no
#LOCAL_CFLAGS += -DCONF_DTV
##---------------------------------------------------------------------------##

##---: fec-rtp
CONF_RTP = yes
#LOCAL_CFLAGS += -DCONF_RTP
##---------------------------------------------------------------------------##

##---: detail track info
#LOCAL_CFLAGS += -DCONF_DETAIL_TRACK_INFO
##---------------------------------------------------------------------------##

##---: send pts to surface flinger
LOCAL_CFLAGS += -DCONF_PTS_TOSF
##---------------------------------------------------------------------------##

##---: H.264@4K point to point
LOCAL_CFLAGS += -DCONF_H264_4K_P2P
##---------------------------------------------------------------------------##

##---: H.265@4K
LOCAL_CFLAGS += -DCONF_H265_4K
##---------------------------------------------------------------------------##

##---: H.265@4K point to point
LOCAL_CFLAGS += -DCONF_H265_4K_P2P
##---------------------------------------------------------------------------##

##---: ion_handle type
#LOCAL_CFLAGS += -DCONF_ION_HANDLE_POINTER
LOCAL_CFLAGS += -DCONF_ION_HANDLE_INT
##---------------------------------------------------------------------------##

##---: enable media boost memory
#LOCAL_CFLAGS += -DCONF_MEDIA_BOOST_MEM
##---------------------------------------------------------------------------##

##---: enable lock cpu num/freq, h5/h6
LOCAL_CFLAGS += -DCONF_MEDIA_BOOST_CPU
LOCAL_CFLAGS += -DCONF_MEDIA_BOOST_CPU_MODE=2
##---------------------------------------------------------------------------##

##---: GPU type IMG/MALI
LOCAL_CFLAGS += -DCONF_GPU_MALI
#LOCAL_CFLAGS += -DCONF_GPU_IMG
##---------------------------------------------------------------------------##

##---: define kernel bitwide, values 32/64
LOCAL_CFLAGS += -DCONF_KERN_BITWIDE=64
##---------------------------------------------------------------------------##

##---: ve phy offset
LOCAL_CFLAGS += -DCONF_VE_PHY_OFFSET=0x40000000
##---------------------------------------------------------------------------##

##---: if support 3D
LOCAL_CFLAGS += -DCONF_3D_ENABLE
##---------------------------------------------------------------------------##

##---: if need send black frame to gpu for avoid GPU bug
LOCAL_CFLAGS += -DCONF_SEND_BLACK_FRAME_TO_GPU
##---------------------------------------------------------------------------##

##---: play rate config for android 6.0 and later android version
ifeq ($(CONF_ANDROID_VERSION), 6.0)
LOCAL_CFLAGS += -DCONF_PLAY_RATE
endif
##---------------------------------------------------------------------------##

##--: afbc enable
LOCAL_CFLAGS += -DCONF_AFBC_ENABLE
##---------------------------------------------------------------------------##

##---: if support HDR
LOCAL_CFLAGS += -DCONF_HIGH_DYNAMIC_RANGE_ENABLE
##---------------------------------------------------------------------------##

##---: if support scale down
LOCAL_CFLAGS += -DCONF_SCALE_DOWN
##---------------------------------------------------------------------------##

###################################end define####################################
