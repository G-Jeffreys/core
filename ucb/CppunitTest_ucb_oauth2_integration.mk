# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CppunitTest_CppunitTest,ucb_oauth2_integration))

$(eval $(call gb_CppunitTest_add_exception_objects,ucb_oauth2_integration, \
    ucb/source/ucp/oauth2/test_oauth2_integration \
))

$(eval $(call gb_CppunitTest_use_libraries,ucb_oauth2_integration, \
    comphelper \
    cppu \
    cppuhelper \
    sal \
    test \
    ucpoauth2 \
    unotest \
))

$(eval $(call gb_CppunitTest_use_sdk_api,ucb_oauth2_integration))

$(eval $(call gb_CppunitTest_use_ure,ucb_oauth2_integration))

$(eval $(call gb_CppunitTest_use_externals,ucb_oauth2_integration,\
    boost_headers \
    curl \
    orcus-parser \
))

$(eval $(call gb_CppunitTest_use_components,ucb_oauth2_integration,\
    configmgr/source/configmgr \
    ucb/source/core/ucb1 \
    ucb/source/ucp/file/ucpfile1 \
    ucb/source/ucp/oauth2/oauth2 \
))

$(eval $(call gb_CppunitTest_use_configuration,ucb_oauth2_integration))

# vim: set noet sw=4 ts=4:
