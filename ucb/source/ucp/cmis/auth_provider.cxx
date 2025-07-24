/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define OUSTR_TO_STDSTR(s) std::string( OUStringToOString( s, RTL_TEXTENCODING_UTF8 ) )
#define STD_TO_OUSTR( str ) OUString( str.c_str(), str.length( ), RTL_TEXTENCODING_UTF8 )

#include <com/sun/star/task/XInteractionHandler.hpp>
#include <com/sun/star/task/PasswordContainer.hpp>
#include <com/sun/star/task/XPasswordContainer2.hpp>
#include <com/sun/star/ucb/XOAuth2Service.hpp>
#include <com/sun/star/ucb/OAuth2ProviderConfig.hpp>

#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <ucbhelper/simpleauthenticationrequest.hxx>
#include <ucbhelper/authenticationfallback.hxx>
#include <sal/log.hxx>

#include "auth_provider.hxx"

using namespace com::sun::star;

namespace cmis
{
    // Google Drive and OneDrive provider URLs for OAuth2 detection
    constexpr OUStringLiteral GDRIVE_BASE_URL = u"https://www.googleapis.com/drive/v3";
    constexpr OUStringLiteral ONEDRIVE_BASE_URL = u"https://graph.microsoft.com/v1.0";
    constexpr OUStringLiteral GDRIVE_PROVIDER_NAME = u"GoogleDrive";
    constexpr OUStringLiteral ONEDRIVE_PROVIDER_NAME = u"OneDrive";

    bool AuthProvider::authenticationQuery( std::string& username, std::string& password )
    {
        SAL_INFO("ucb.ucp.cmis", "AuthProvider::authenticationQuery - checking OAuth2 modernization first");

        // ENHANCED PHASE 3: Try OAuth2 authentication first for supported providers
        if (shouldUseOAuth2Service())
        {
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Attempting OAuth2 authentication");
            std::string oauthToken = performOAuth2Authentication(username);
            if (!oauthToken.empty())
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication successful");
                password = oauthToken; // Use OAuth token as password for libcmis
                return true;
            }
            else
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication failed, falling back to legacy method");
                // Continue with legacy method below
            }
        }

        // LEGACY METHOD: Traditional username/password authentication
        SAL_INFO("ucb.ucp.cmis", "AuthProvider: Using legacy authentication method");

        if ( m_xEnv.is() )
        {
            uno::Reference< task::XInteractionHandler > xIH
                = m_xEnv->getInteractionHandler();

            if ( xIH.is() )
            {
                rtl::Reference< ucbhelper::SimpleAuthenticationRequest > xRequest
                    = new ucbhelper::SimpleAuthenticationRequest(
                        m_sUrl, m_sBindingUrl, OUString(),
                        STD_TO_OUSTR( username ),
                        STD_TO_OUSTR( password ),
                        false, false );
                xIH->handle( xRequest );

                rtl::Reference< ucbhelper::InteractionContinuation > xSelection
                    = xRequest->getSelection();

                if ( xSelection.is() )
                {
                    // Handler handled the request.
                    uno::Reference< task::XInteractionAbort > xAbort(
                        xSelection->getXWeak(), uno::UNO_QUERY );
                    if ( !xAbort.is() )
                    {
                        const rtl::Reference<
                            ucbhelper::InteractionSupplyAuthentication > & xSupp
                            = xRequest->getAuthenticationSupplier();

                        username = OUSTR_TO_STDSTR( xSupp->getUserName() );
                        password = OUSTR_TO_STDSTR( xSupp->getPassword() );

                        return true;
                    }
                }
            }
        }
        return false;
    }

    std::string AuthProvider::getRefreshToken(std::string& rUsername)
    {
        SAL_INFO("ucb.ucp.cmis", "AuthProvider::getRefreshToken() called - enhanced OAuth2 modernization");

        // ENHANCED PHASE 3: Try OAuth2 service first if available
        if (shouldUseOAuth2Service())
        {
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Using OAuth2Service for token retrieval");
            std::string oauthToken = getValidOAuthToken(rUsername);
            if (!oauthToken.empty())
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: Successfully retrieved OAuth token via OAuth2Service");
                return oauthToken;
            }
            else
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2Service token retrieval failed");
                // For OAuth providers, try to authenticate if no token available
                OUString sProviderName = getProviderNameFromUrl();
                if (!sProviderName.isEmpty())
                {
                    SAL_INFO("ucb.ucp.cmis", "AuthProvider: Attempting OAuth2 authentication for provider: " << sProviderName);
                    std::string newToken = performOAuth2Authentication(rUsername);
                    if (!newToken.empty())
                    {
                        SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication successful, returning new token");
                        return newToken;
                    }
                }
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication failed, falling back to legacy method");
            }
        }

        // LEGACY METHOD: Continue with existing implementation
        SAL_INFO("ucb.ucp.cmis", "AuthProvider: Using legacy authentication method");
        std::string refreshToken;
        const css::uno::Reference<css::ucb::XCommandEnvironment> xEnv = getXEnv();
        if (xEnv.is())
        {
            uno::Reference<task::XInteractionHandler> xIH = xEnv->getInteractionHandler();

            if (rUsername.empty())
            {
                rtl::Reference<ucbhelper::SimpleAuthenticationRequest> xRequest
                    = new ucbhelper::SimpleAuthenticationRequest(
                        m_sUrl, m_sBindingUrl,
                        ucbhelper::SimpleAuthenticationRequest::EntityType::ENTITY_NA, OUString(),
                        ucbhelper::SimpleAuthenticationRequest::EntityType::ENTITY_MODIFY,
                        STD_TO_OUSTR(rUsername),
                        ucbhelper::SimpleAuthenticationRequest::EntityType::ENTITY_NA, OUString());
                xIH->handle(xRequest);

                rtl::Reference<ucbhelper::InteractionContinuation> xSelection
                    = xRequest->getSelection();

                if (xSelection.is())
                {
                    // Handler handled the request.
                    uno::Reference<task::XInteractionAbort> xAbort(xSelection->getXWeak(),
                                                                   uno::UNO_QUERY);
                    if (!xAbort.is())
                    {
                        const rtl::Reference<ucbhelper::InteractionSupplyAuthentication>& xSupp
                            = xRequest->getAuthenticationSupplier();

                        rUsername = OUSTR_TO_STDSTR(xSupp->getUserName());
                    }
                }
            }

            const uno::Reference<uno::XComponentContext>& xContext
                = ::comphelper::getProcessComponentContext();
            uno::Reference<task::XPasswordContainer2> xMasterPasswd
                = task::PasswordContainer::create(xContext);
            if (xMasterPasswd->hasMasterPassword())
            {
                xMasterPasswd->authorizateWithMasterPassword(xIH);
            }
            if (xMasterPasswd->isPersistentStoringAllowed())
            {
                task::UrlRecord aRec
                    = xMasterPasswd->findForName(m_sBindingUrl, STD_TO_OUSTR(rUsername), xIH);
                if (aRec.UserList.hasElements() && aRec.UserList[0].Passwords.hasElements())
                    refreshToken = OUSTR_TO_STDSTR(aRec.UserList[0].Passwords[0]);
            }
        }
        return refreshToken;
    }

    bool AuthProvider::storeRefreshToken(const std::string& username, const std::string& password,
                                         const std::string& refreshToken)
    {
        SAL_INFO("ucb.ucp.cmis", "AuthProvider::storeRefreshToken - enhanced with OAuth2 support");

        // ENHANCED PHASE 3: If using OAuth2, delegate to OAuth2Service
        if (shouldUseOAuth2Service() && !refreshToken.empty())
        {
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Storing OAuth2 tokens via OAuth2Service");
            try
            {
                css::uno::Reference<css::ucb::XOAuth2Service> xService = getOAuth2Service();
                if (xService.is())
                {
                    OUString sProviderName = getProviderNameFromUrl();
                    OUString sUsername = STD_TO_OUSTR(username);

                    if (!sProviderName.isEmpty())
                    {
                        // Check if user is already authenticated to avoid duplicate storage
                        if (!xService->isAuthenticated(sProviderName, sUsername))
                        {
                            SAL_INFO("ucb.ucp.cmis", "AuthProvider: User not yet authenticated via OAuth2Service - authentication will handle token storage");
                        }
                        else
                        {
                            SAL_INFO("ucb.ucp.cmis", "AuthProvider: User already authenticated via OAuth2Service");
                        }
                        return true;
                    }
                }
            }
            catch (const css::uno::Exception& e)
            {
                SAL_WARN("ucb.ucp.cmis", "AuthProvider: Error storing OAuth2 tokens: " << e.Message);
                // Fall back to legacy storage
            }
        }

        // LEGACY METHOD: Continue with existing implementation
        if (refreshToken.empty())
            return false;
        if (password == refreshToken)
            return true;
        const css::uno::Reference<css::ucb::XCommandEnvironment> xEnv = getXEnv();
        if (xEnv.is())
        {
            uno::Reference<task::XInteractionHandler> xIH = xEnv->getInteractionHandler();
            const uno::Reference<uno::XComponentContext>& xContext
                = ::comphelper::getProcessComponentContext();
            uno::Reference<task::XPasswordContainer2> xMasterPasswd
                = task::PasswordContainer::create(xContext);
            uno::Sequence<OUString> aPasswd{ STD_TO_OUSTR(refreshToken) };
            if (xMasterPasswd->isPersistentStoringAllowed())
            {
                if (xMasterPasswd->hasMasterPassword())
                {
                    xMasterPasswd->authorizateWithMasterPassword(xIH);
                }
                xMasterPasswd->addPersistent(m_sBindingUrl, STD_TO_OUSTR(username), aPasswd, xIH);
                return true;
            }
        }
        return false;
    }

    css::uno::WeakReference< css::ucb::XCommandEnvironment> AuthProvider::sm_xEnv;

    void AuthProvider::setXEnv(const css::uno::Reference< css::ucb::XCommandEnvironment>& xEnv )
    {
        sm_xEnv = xEnv;
    }

    css::uno::Reference< css::ucb::XCommandEnvironment> AuthProvider::getXEnv()
    {
        return sm_xEnv;
    }

    char* AuthProvider::copyWebAuthCodeFallback( const char* url,
            const char* /*username*/,
            const char* /*password*/ )
    {
        OUString url_oustr( url, strlen( url ), RTL_TEXTENCODING_UTF8 );
        const css::uno::Reference<
            css::ucb::XCommandEnvironment> xEnv = getXEnv( );

        if ( xEnv.is() )
        {
            uno::Reference< task::XInteractionHandler > xIH
                = xEnv->getInteractionHandler();

            if ( xIH.is() )
            {
                rtl::Reference< ucbhelper::AuthenticationFallbackRequest > xRequest
                    = new ucbhelper::AuthenticationFallbackRequest (
                            u"Open the following link in your browser and "
                            "paste the code from the URL you have been redirected to in the "
                            "box below. For example:\n"
                            "http://localhost/LibreOffice?code=YOUR_CODE"_ustr,
                            url_oustr );

                xIH->handle( xRequest );

                rtl::Reference< ucbhelper::InteractionContinuation > xSelection
                    = xRequest->getSelection();

                if ( xSelection.is() )
                {
                    // Handler handled the request.
                    const rtl::Reference< ucbhelper::InteractionAuthFallback >&
                        xAuthFallback = xRequest->getAuthFallbackInter( );
                    if ( xAuthFallback.is() )
                    {
                        OUString code = xAuthFallback->getCode( );
                        return strdup( OUSTR_TO_STDSTR( code ).c_str( ) );
                    }
                }
            }
        }

        return strdup( "" );
    }

    css::uno::Reference<css::ucb::XOAuth2Service> AuthProvider::getOAuth2Service() const
    {
        if (!m_xOAuth2Service.is())
        {
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Creating OAuth2Service instance for enhanced authentication");
            try
            {
                css::uno::Reference<css::uno::XComponentContext> xContext =
                    ::comphelper::getProcessComponentContext();

                if (xContext.is())
                {
                    // Try to create the OAuth2Service
                    m_xOAuth2Service = css::uno::Reference<css::ucb::XOAuth2Service>(
                        xContext->getServiceManager()->createInstanceWithContext(
                            u"com.sun.star.ucb.OAuth2Service"_ustr, xContext),
                        css::uno::UNO_QUERY);

                    if (m_xOAuth2Service.is())
                    {
                        SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2Service created successfully - enhanced authentication available");

                        // Configure Google Drive provider if needed
                        configureGoogleDriveProvider();
                    }
                    else
                    {
                        SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2Service not available - using legacy authentication");
                    }
                }
            }
            catch (const css::uno::Exception& e)
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: Failed to create OAuth2Service: " << e.Message << " - falling back to legacy authentication");
                // Continue with legacy authentication
            }
        }
        return m_xOAuth2Service;
    }

    bool AuthProvider::shouldUseOAuth2Service() const
    {
        // Enhanced Phase 3: Support OAuth2 for Google Drive and OneDrive
        OUString sProviderName = getProviderNameFromUrl();

        if (sProviderName.isEmpty())
        {
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Not a recognized OAuth provider - using legacy authentication");
            return false;
        }

        // Check if the OAuth2Service is available
        css::uno::Reference<css::ucb::XOAuth2Service> xService = getOAuth2Service();
        bool bShouldUse = xService.is();

        SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2 modernization " <<
                (bShouldUse ? "enabled" : "disabled") << " for " << sProviderName);

        return bShouldUse;
    }

    OUString AuthProvider::getProviderNameFromUrl() const
    {
        OUString sBindingUrl = m_sBindingUrl;

        // Google Drive detection
        if (sBindingUrl.indexOf(GDRIVE_BASE_URL) != -1 ||
            sBindingUrl.indexOf(u"googleapis.com") != -1 ||
            sBindingUrl.indexOf(u"google.com") != -1)
        {
            return GDRIVE_PROVIDER_NAME;
        }

        // OneDrive detection
        if (sBindingUrl.indexOf(ONEDRIVE_BASE_URL) != -1 ||
            sBindingUrl.indexOf(u"graph.microsoft.com") != -1 ||
            sBindingUrl.indexOf(u"sharepoint.com") != -1)
        {
            return ONEDRIVE_PROVIDER_NAME;
        }

        return OUString(); // Not a recognized OAuth provider
    }

    void AuthProvider::configureGoogleDriveProvider() const
    {
        if (!m_xOAuth2Service.is())
            return;

        try
        {
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Configuring Google Drive OAuth2 provider");

            // Configure Google Drive OAuth2 settings as PropertyValues
            css::uno::Sequence<css::beans::PropertyValue> aConfiguration{
                comphelper::makePropertyValue(u"DisplayName"_ustr, u"Google Drive"_ustr),
                comphelper::makePropertyValue(u"BaseUrl"_ustr, OUString(GDRIVE_BASE_URL)),
                comphelper::makePropertyValue(u"AuthUrl"_ustr, u"https://accounts.google.com/o/oauth2/v2/auth"_ustr),
                comphelper::makePropertyValue(u"TokenUrl"_ustr, u"https://oauth2.googleapis.com/token"_ustr),
                comphelper::makePropertyValue(u"Scope"_ustr, u"https://www.googleapis.com/auth/drive"_ustr),
                comphelper::makePropertyValue(u"ClientId"_ustr, u""_ustr), // Will be set from configuration
                comphelper::makePropertyValue(u"ClientSecret"_ustr, u""_ustr), // Will be set from configuration
                comphelper::makePropertyValue(u"RedirectUri"_ustr, u"http://localhost:8080/oauth2callback"_ustr)
            };

            m_xOAuth2Service->configureProvider(GDRIVE_PROVIDER_NAME, aConfiguration);
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Google Drive provider configured successfully");
        }
        catch (const css::uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.cmis", "AuthProvider: Failed to configure Google Drive provider: " << e.Message);
        }
    }

    std::string AuthProvider::getValidOAuthToken(const std::string& username) const
    {
        SAL_INFO("ucb.ucp.cmis", "AuthProvider: Getting valid OAuth token using enhanced OAuth2Service");

        try
        {
            css::uno::Reference<css::ucb::XOAuth2Service> xService = getOAuth2Service();
            if (!xService.is())
            {
                SAL_WARN("ucb.ucp.cmis", "AuthProvider: OAuth2Service not available for token retrieval");
                return "";
            }

            OUString sProviderName = getProviderNameFromUrl();
            OUString sUsername = STD_TO_OUSTR(username);

            if (sProviderName.isEmpty())
            {
                SAL_WARN("ucb.ucp.cmis", "AuthProvider: Cannot determine provider name from URL: " << m_sBindingUrl);
                return "";
            }

            // Check if user is already authenticated
            if (xService->isAuthenticated(sProviderName, sUsername))
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: User already authenticated, getting valid access token");

                // Get valid access token (will auto-refresh if needed)
                OUString sAccessToken = xService->getValidAccessToken(sProviderName, sUsername, nullptr);

                std::string sToken = OUSTR_TO_STDSTR(sAccessToken);
                if (!sToken.empty())
                {
                    SAL_INFO("ucb.ucp.cmis", "AuthProvider: Retrieved valid OAuth access token via OAuth2Service");
                }
                return sToken;
            }
            else
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: User not authenticated via OAuth2Service - authentication needed");
                return "";
            }
        }
        catch (const css::uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.cmis", "AuthProvider: Error getting OAuth token via OAuth2Service: " << e.Message);
            return "";
        }
    }

    std::string AuthProvider::performOAuth2Authentication(const std::string& username) const
    {
        SAL_INFO("ucb.ucp.cmis", "AuthProvider: Performing complete OAuth2 authentication flow");

        try
        {
            css::uno::Reference<css::ucb::XOAuth2Service> xService = getOAuth2Service();
            if (!xService.is())
            {
                SAL_WARN("ucb.ucp.cmis", "AuthProvider: OAuth2Service not available for authentication");
                return "";
            }

            OUString sProviderName = getProviderNameFromUrl();
            OUString sUsername = STD_TO_OUSTR(username);

            if (sProviderName.isEmpty())
            {
                SAL_WARN("ucb.ucp.cmis", "AuthProvider: Cannot determine provider name for OAuth2 authentication");
                return "";
            }

            // Perform OAuth2 authentication
            SAL_INFO("ucb.ucp.cmis", "AuthProvider: Starting OAuth2 authentication for " << sProviderName << ":" << sUsername);
            xService->authenticate(sProviderName, sUsername, m_xEnv);

            // Get the access token after successful authentication
            OUString sAccessToken = xService->getValidAccessToken(sProviderName, sUsername, m_xEnv);
            std::string sToken = OUSTR_TO_STDSTR(sAccessToken);

            if (!sToken.empty())
            {
                SAL_INFO("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication completed successfully");
            }
            else
            {
                SAL_WARN("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication completed but no token received");
            }

            return sToken;
        }
        catch (const css::uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.cmis", "AuthProvider: OAuth2 authentication failed: " << e.Message);
            return "";
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
