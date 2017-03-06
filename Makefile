#
# Copyright (C) 2014-2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
    
PKG_NAME:=presto-proxy
PKG_VERSION:=1.1.1
PKG_RELEASE:=1.1.1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/uclibc++.mk
include $(INCLUDE_DIR)/nls.mk
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/presto-proxy
	SECTION:=net
	CATEGORY:=Network
#  DEPENDS+=+polarssl +libcurl +libmbedtls +libm +libpthread
	DEPENDS+=$(CXX_DEPENDS) +libpthread +libm +polarssl +libcurl +libstdcpp


	TITLE:=presto-proxy
	MAINTAINER:=PeoplePower <mikhail@peoplepowerco.com>
endef

PLATFORM=host

define Package/map/description
Presto private SDK kit.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/presto-proxy/install

	$(INSTALL_DIR) $(1)/root/presto/Proxy
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/Proxy/Proxy $(1)/root/presto/Proxy
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/Proxy/proxyJSON.conf.mips $(1)/root/presto/Proxy/proxyJSON.conf

	$(INSTALL_DIR) $(1)/root/presto/WUD
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/WUD/WUD $(1)/root/presto/WUD
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/WUD/wud.conf.mips $(1)/root/presto/WUD/wud.conf
endef

$(eval $(call BuildPackage,presto-proxy,+libpthread))
