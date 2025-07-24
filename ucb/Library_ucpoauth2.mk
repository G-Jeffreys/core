# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Library_Library,ucpoauth2))

$(eval $(call gb_Library_set_componentfile,ucpoauth2,ucb/source/ucp/oauth2/oauth2))

$(eval $(call gb_Library_use_sdk_api,ucpoauth2))

$(eval $(call gb_Library_use_libraries,ucpoauth2,\
	comphelper \
	cppu \
	cppuhelper \
	sal \
	salhelper \
	tl \
	ucbhelper \
))

$(eval $(call gb_Library_use_externals,ucpoauth2,\
	curl \
	orcus \
	orcus-parser \
))

$(eval $(call gb_Library_add_exception_objects,ucpoauth2,\
	ucb/source/ucp/oauth2/oauth2_service \
	ucb/source/ucp/oauth2/token_manager \
	ucb/source/ucp/oauth2/config_manager \
	ucb/source/ucp/oauth2/oauth2_flow \
))

# vim: set noet sw=4 ts=4:
