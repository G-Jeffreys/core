/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/XOAuth2Configuration.hpp>
#include <com/sun/star/util/DateTime.hpp>
#include <rtl/ustring.hxx>
#include <osl/socket.hxx>
#include <osl/thread.hxx>
#include <curl/curl.h>
#include <memory>
#include <atomic>

namespace ucb::oauth2 {

/**
 * OAuth2 Authentication Result
 * Contains the result of OAuth2 authentication flow
 */
struct OAuth2AuthResult
{
    bool bSuccess = false;
    OUString sAccessToken;
    OUString sRefreshToken;
    long nExpiresInSeconds = 0;
    css::util::DateTime aExpiryTime;
    OUString sError;
    OUString sErrorDescription;
};

/**
 * OAuth2 Token Refresh Result
 * Contains the result of OAuth2 token refresh operation
 */
struct OAuth2RefreshResult
{
    bool bSuccess = false;
    OUString sAccessToken;
    OUString sRefreshToken;
    long nExpiresInSeconds = 0;
    css::util::DateTime aExpiryTime;
    OUString sError;
    OUString sErrorDescription;
};

/**
 * Local HTTP server for OAuth2 callback handling
 * Implements a simple HTTP server to receive authorization codes
 */
class OAuth2CallbackServer : public osl::Thread
{
private:
    osl::AcceptorSocket m_aServerSocket;
    sal_uInt16 m_nPort;
    std::atomic<bool> m_bShutdown;
    std::atomic<bool> m_bCallbackReceived;
    OUString m_sAuthorizationCode;
    OUString m_sState;
    OUString m_sError;
    OUString m_sErrorDescription;

    // HTTP response handling
    void handleHttpRequest(osl::StreamSocket& rClientSocket);
    OUString parseHttpRequest(const OString& sRequest);
    void sendHttpResponse(osl::StreamSocket& rClientSocket, const OUString& sContent);

protected:
    virtual void SAL_CALL run() override;

public:
    OAuth2CallbackServer();
    virtual ~OAuth2CallbackServer() override;

    // Server management
    bool startServer();
    void stopServer();
    sal_uInt16 getPort() const { return m_nPort; }

    // Callback results
    bool waitForCallback(sal_uInt32 nTimeoutSeconds = 300); // 5 minutes default
    bool hasCallbackBeenReceived() const { return m_bCallbackReceived.load(); }
    OUString getAuthorizationCode() const { return m_sAuthorizationCode; }
    OUString getState() const { return m_sState; }
    OUString getError() const { return m_sError; }
    OUString getErrorDescription() const { return m_sErrorDescription; }
};

/**
 * OAuth2 Flow Implementation - Phase 3 Complete Version
 *
 * Implements complete OAuth2 authorization code flow with PKCE,
 * browser-based authentication, and JSON token processing.
 */
class OAuth2Flow
{
private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;

    // OAuth2 flow state
    OUString m_sProviderUrl;
    OUString m_sUsername;
    OUString m_sCodeVerifier;
    OUString m_sCodeChallenge;
    OUString m_sState;

    // HTTP client management
    struct CurlDeleter {
        void operator()(CURL* curl) const { curl_easy_cleanup(curl); }
    };
    using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;

    // PKCE (Proof Key for Code Exchange) implementation
    OUString generateCodeVerifier();
    OUString generateCodeChallenge(const OUString& codeVerifier);
    OUString generateState();

    // HTTP utilities
    OUString makeHttpRequest(const OUString& url, const OUString& postData = OUString(),
                            const OUString& contentType = u"application/x-www-form-urlencoded"_ustr);
    OUString urlEncode(const OUString& input);

    // OAuth2 flow methods
    OUString buildAuthorizationUrl(const css::ucb::OAuth2ProviderConfig& config, const OUString& username, sal_uInt16 nCallbackPort);
    OAuth2AuthResult exchangeCodeForTokens(const css::ucb::OAuth2ProviderConfig& config, const OUString& authCode);
    bool launchBrowserAuthentication(const OUString& authUrl, OUString& authCode, const OUString& expectedState);

    // JSON processing
    OAuth2AuthResult parseTokenResponse(const OUString& jsonResponse);
    OAuth2RefreshResult parseRefreshResponse(const OUString& jsonResponse);

    // Utility methods
    css::util::DateTime calculateExpiryTime(long expiresInSeconds);
    OUString extractAuthCodeFromCallback(const OUString& callbackUrl);
    OUString base64UrlEncode(const OUString& input);

public:
    explicit OAuth2Flow(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    ~OAuth2Flow();

    /**
     * Performs complete OAuth2 authentication flow
     * 1. Generates PKCE parameters (code verifier, challenge, state)
     * 2. Starts local callback server
     * 3. Builds authorization URL with callback port
     * 4. Launches browser for user authentication
     * 5. Waits for callback with authorization code
     * 6. Exchanges code for access/refresh tokens
     */
    OAuth2AuthResult performAuthentication(
        const css::ucb::OAuth2ProviderConfig& config,
        const OUString& username,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment);

    /**
     * Refreshes access token using refresh token
     * Implements OAuth2 token refresh with automatic rotation support
     */
    OAuth2RefreshResult refreshAccessToken(
        const css::ucb::OAuth2ProviderConfig& config,
        const OUString& refreshToken,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment);
};

} // namespace ucb::oauth2

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
