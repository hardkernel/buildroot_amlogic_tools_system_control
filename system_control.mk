SYSTEM_CONTROL_VERSION = 0.1
SYSTEM_CONTROL_SITE = $(TOPDIR)/../vendor/amlogic/system_control/src
SYSTEM_CONTROL_SITE_METHOD = local
SYSTEM_CONTROL_HDCPTX22_DIRECTORY = $(SYSTEM_CONTROL_SITE)/firmware

define SYSTEM_CONTROL_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) all
endef

define SYSTEM_CONTROL_INSTALL_TARGET_CMDS
    mkdir -p $(TARGET_DIR)/etc/firmware
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CC=$(TARGET_CC) -C $(@D) install
	cp -a $(SYSTEM_CONTROL_HDCPTX22_DIRECTORY)/firmware.le  $(TARGET_DIR)/etc/firmware
	cp -a $(SYSTEM_CONTROL_HDCPTX22_DIRECTORY)/mesondisplay.cfg   $(TARGET_DIR)/etc
	cp -a $(SYSTEM_CONTROL_HDCPTX22_DIRECTORY)/hdcp_tx22   $(TARGET_DIR)/usr/bin
	cp -a $(SYSTEM_CONTROL_HDCPTX22_DIRECTORY)/hdcpcontrol.sh   $(TARGET_DIR)/usr/bin
endef

define SYSTEM_CONTROL_INSTALL_INIT_SYSV
	rm -rf $(TARGET_DIR)/etc/init.d/S60systemcontrol
	$(INSTALL) -D -m 755 $(SYSTEM_CONTROL_SITE)/S60systemcontrol \
		$(TARGET_DIR)/etc/init.d/S60systemcontrol
endef

$(eval $(generic-package))
