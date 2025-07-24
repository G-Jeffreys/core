/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#pragma once

#include <libcmis/libcmis.hxx>

#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/XOAuth2Service.hpp>
#include <cppuhelper/weakref.hxx>

namespace cmis
{
    class AuthProvider : public libcmis::AuthProvider
    {
        const css::uno::Reference< css::ucb::XCommandEnvironment>& m_xEnv;
        static css::uno::WeakReference< css::ucb::XCommandEnvironment> sm_xEnv;
        OUString m_sUrl;
        OUString m_sBindingUrl;

        // OAuth2 modernization support (Enhanced Phase 3)
        mutable css::uno::Reference< css::ucb::XOAuth2Service > m_xOAuth2Service;

        public:
            AuthProvider ( const css::uno::Reference< css::ucb::XCommandEnvironment> & xEnv,
                           OUString sUrl,
                           OUString sBindingUrl ):
                m_xEnv( xEnv ), m_sUrl( std::move(sUrl) ), m_sBindingUrl( std::move(sBindingUrl) ) { }

            bool authenticationQuery( std::string& username, std::string& password ) override;

            std::string getRefreshToken( std::string& username );
            bool storeRefreshToken(const std::string& username, const std::string& password,
                                   const std::string& refreshToken);

            // OAuth2 modernization methods (Enhanced Phase 3)
            /**
             * Get OAuth2Service instance (lazy initialization)
             */
            css::uno::Reference< css::ucb::XOAuth2Service > getOAuth2Service() const;

            /**
             * Check if OAuth2 modernization is enabled for this provider
             */
            bool shouldUseOAuth2Service() const;

            /**
             * Get provider name from binding URL for OAuth2 service
             */
            OUString getProviderNameFromUrl() const;

            /**
             * Configure Google Drive OAuth2 provider settings
             */
            void configureGoogleDriveProvider() const;

            /**
             * Get valid access token using OAuth2 service with automatic refresh
             */
            std::string getValidOAuthToken(const std::string& username) const;

            /**
             * Perform complete OAuth2 authentication flow
             */
            std::string performOAuth2Authentication(const std::string& username) const;

            static char* copyWebAuthCodeFallback( const char* url,
                    const char* /*username*/,
                    const char* /*password*/ );

            static void setXEnv( const css::uno::Reference< css::ucb::XCommandEnvironment>& xEnv );
            static css::uno::Reference< css::ucb::XCommandEnvironment> getXEnv();

    };
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
