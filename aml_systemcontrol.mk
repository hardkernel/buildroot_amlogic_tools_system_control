AML_SYSTEMCONTROL_VERSION = 0.1
AML_SYSTEMCONTROL_SITE = $(TOPDIR)/package/aml_systemcontrol/src
AML_SYSTEMCONTROL_SITE_METHOD = local
AML_SYSTEMCONTROL_HDCPTX22_DIRECTORY = board/amlogic/common/rootfs/rootfs-49/etc

define AML_SYSTEMCONTROL_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D) all
endef

define AML_SYSTEMCONTROL_INSTALL_TARGET_CMDS
    mkdir -p $(TARGET_DIR)/etc/firmware
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CC=$(TARGET_CC) -C $(@D) install
	cp -a $(TOPDIR)/$(AML_SYSTEMCONTROL_HDCPTX22_DIRECTORY)/firmware.le  $(TARGET_DIR)/etc/firmware
	cp -a $(TOPDIR)/$(AML_SYSTEMCONTROL_HDCPTX22_DIRECTORY)/mesondisplay.cfg   $(TARGET_DIR)/etc
	cp -a $(TOPDIR)/$(AML_SYSTEMCONTROL_HDCPTX22_DIRECTORY)/hdcp_tx22   $(TARGET_DIR)/usr/bin
	cp -a $(TOPDIR)/$(AML_SYSTEMCONTROL_HDCPTX22_DIRECTORY)/hdcpcontrol.sh   $(TARGET_DIR)/usr/bin
endef

define AML_SYSTEMCONTROL_INSTALL_INIT_SYSV
	rm -rf $(TARGET_DIR)/etc/init.d/S60systemcontrol
	$(INSTALL) -D -m 755 package/aml_systemcontrol/S60systemcontrol \
		$(TARGET_DIR)/etc/init.d/S60systemcontrol
endef

$(eval $(generic-package))
