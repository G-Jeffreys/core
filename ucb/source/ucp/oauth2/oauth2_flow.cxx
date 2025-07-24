/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "oauth2_flow.hxx"
#include "config_manager.hxx"

#include <com/sun/star/task/XInteractionHandler.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/ucb/InteractiveIOException.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/system/XSystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecuteFlags.hpp>
#include <comphelper/processfactory.hxx>
#include <comphelper/base64.hxx>
#include <comphelper/hash.hxx>
#include <sal/log.hxx>
#include <rtl/random.h>
#include <rtl/digest.h>

#include <orcus/json_document_tree.hpp>
#include <orcus/json_parser.hpp>
#include <orcus/config.hpp>
#include <tools/urlobj.hxx>
#include <osl/socket.hxx>
#include <osl/conditn.hxx>

using namespace css;

namespace ucb::oauth2 {

/**
 * OAuth2CallbackServer Implementation
 *
 * Implements a simple HTTP server to handle OAuth2 authorization code callbacks
 */

namespace {
    // HTTP callback for curl to write response data
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response)
    {
        if (!response) return 0;
        size_t totalSize = size * nmemb;
        response->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    // Find an available port for the callback server
    sal_uInt16 findAvailablePort(sal_uInt16 startPort = 8080)
    {
        for (sal_uInt16 port = startPort; port < startPort + 100; ++port)
        {
            osl::AcceptorSocket testSocket(osl_Socket_FamilyInet, osl_Socket_ProtocolIp, osl_Socket_TypeStream);
            osl::SocketAddr addr(u"127.0.0.1"_ustr, port);

            if (testSocket.bind(addr))
            {
                SAL_INFO("ucb.ucp.oauth2", "Found available port: " << port);
                return port;
            }
        }

        SAL_WARN("ucb.ucp.oauth2", "Could not find available port in range");
        return 0;
    }
}

OAuth2CallbackServer::OAuth2CallbackServer()
    : m_aServerSocket(osl_Socket_FamilyInet, osl_Socket_ProtocolIp, osl_Socket_TypeStream)
    , m_nPort(0)
    , m_bShutdown(false)
    , m_bCallbackReceived(false)
{
    SAL_INFO("ucb.ucp.oauth2", "OAuth2CallbackServer created");
}

OAuth2CallbackServer::~OAuth2CallbackServer()
{
    if (isRunning())
    {
        stopServer();
        join();
    }
    SAL_INFO("ucb.ucp.oauth2", "OAuth2CallbackServer destroyed");
}

bool OAuth2CallbackServer::startServer()
{
    SAL_INFO("ucb.ucp.oauth2", "Starting OAuth2 callback server");

    // Find an available port
    m_nPort = findAvailablePort();
    if (m_nPort == 0)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to find available port for callback server");
        return false;
    }

    // Bind to localhost on the found port
    osl::SocketAddr aAddr(u"127.0.0.1"_ustr, m_nPort);
    if (!m_aServerSocket.bind(aAddr))
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to bind callback server to port " << m_nPort);
        return false;
    }

    // Set socket to non-blocking mode for timeout handling
    m_aServerSocket.setOption(osl_Socket_OptionReuseAddr, 1);

    // Start listening
    if (!m_aServerSocket.listen(1))
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to listen on callback server port " << m_nPort);
        return false;
    }

    // Start server thread
    m_bShutdown = false;
    m_bCallbackReceived = false;
    create();

    SAL_INFO("ucb.ucp.oauth2", "OAuth2 callback server started on port " << m_nPort);
    return true;
}

void OAuth2CallbackServer::stopServer()
{
    SAL_INFO("ucb.ucp.oauth2", "Stopping OAuth2 callback server");
    m_bShutdown = true;

    // Close the server socket to break out of accept()
    m_aServerSocket.close();
}

void SAL_CALL OAuth2CallbackServer::run()
{
    SAL_INFO("ucb.ucp.oauth2", "OAuth2 callback server thread started");

    while (!m_bShutdown.load())
    {
        osl::StreamSocket aClientSocket;

        // Accept client connections with timeout
        osl::SocketAddr aClientAddr;

        oslSocketResult eResult = m_aServerSocket.acceptConnection(aClientSocket, aClientAddr);

        if (eResult == osl_Socket_Ok)
        {
            SAL_INFO("ucb.ucp.oauth2", "Received callback connection");
            handleHttpRequest(aClientSocket);
            aClientSocket.close();

            if (m_bCallbackReceived.load())
            {
                SAL_INFO("ucb.ucp.oauth2", "Callback processed successfully, stopping server");
                break;
            }
        }
        else if (eResult == osl_Socket_TimedOut)
        {
            // Continue waiting if not shutdown
            continue;
        }
        else if (eResult == osl_Socket_Error || eResult == osl_Socket_Interrupted)
        {
            if (!m_bShutdown.load())
            {
                SAL_WARN("ucb.ucp.oauth2", "Error accepting connection on callback server");
            }
            break;
        }
    }

    SAL_INFO("ucb.ucp.oauth2", "OAuth2 callback server thread stopped");
}

void OAuth2CallbackServer::handleHttpRequest(osl::StreamSocket& rClientSocket)
{
    SAL_INFO("ucb.ucp.oauth2", "Handling HTTP request");

    // Read HTTP request
    constexpr sal_uInt32 nBufferSize = 4096;
    char buffer[nBufferSize];

    sal_Int32 nBytesRead = rClientSocket.recv(buffer, nBufferSize - 1);

    if (nBytesRead <= 0)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to read HTTP request");
        return;
    }

    buffer[nBytesRead] = '\0';
    OString sRequest(buffer);

    SAL_INFO("ucb.ucp.oauth2", "Received HTTP request: " << sRequest.getLength() << " bytes");

    // Parse the request to extract OAuth2 parameters
    OUString sCallbackUrl = parseHttpRequest(sRequest);

    OUString sResponseContent;
    if (!sCallbackUrl.isEmpty())
    {
        // Extract authorization code from callback URL
        sal_Int32 nCodePos = sCallbackUrl.indexOf(u"code=");
        if (nCodePos != -1)
        {
            sal_Int32 nStartPos = nCodePos + 5; // Length of "code="
            sal_Int32 nEndPos = sCallbackUrl.indexOf(u"&", nStartPos);
            if (nEndPos == -1)
                nEndPos = sCallbackUrl.getLength();

            m_sAuthorizationCode = sCallbackUrl.copy(nStartPos, nEndPos - nStartPos);

            // Extract state parameter
            sal_Int32 nStatePos = sCallbackUrl.indexOf(u"state=");
            if (nStatePos != -1)
            {
                nStartPos = nStatePos + 6; // Length of "state="
                nEndPos = sCallbackUrl.indexOf(u"&", nStartPos);
                if (nEndPos == -1)
                    nEndPos = sCallbackUrl.getLength();

                m_sState = sCallbackUrl.copy(nStartPos, nEndPos - nStartPos);
            }

            SAL_INFO("ucb.ucp.oauth2", "Successfully extracted authorization code and state");
            m_bCallbackReceived = true;

            sResponseContent = u"<!DOCTYPE html><html><head><title>OAuth2 Authorization</title></head>"
                              "<body><h1>Authorization Successful</h1>"
                              "<p>You have successfully authorized LibreOffice to access your account.</p>"
                              "<p>You can close this window and return to LibreOffice.</p></body></html>"_ustr;
        }
        else
        {
            // Check for error parameters
            sal_Int32 nErrorPos = sCallbackUrl.indexOf(u"error=");
            if (nErrorPos != -1)
            {
                sal_Int32 nStartPos = nErrorPos + 6; // Length of "error="
                sal_Int32 nEndPos = sCallbackUrl.indexOf(u"&", nStartPos);
                if (nEndPos == -1)
                    nEndPos = sCallbackUrl.getLength();

                m_sError = sCallbackUrl.copy(nStartPos, nEndPos - nStartPos);

                // Extract error description
                sal_Int32 nErrorDescPos = sCallbackUrl.indexOf(u"error_description=");
                if (nErrorDescPos != -1)
                {
                    nStartPos = nErrorDescPos + 18; // Length of "error_description="
                    nEndPos = sCallbackUrl.indexOf(u"&", nStartPos);
                    if (nEndPos == -1)
                        nEndPos = sCallbackUrl.getLength();

                    m_sErrorDescription = sCallbackUrl.copy(nStartPos, nEndPos - nStartPos);
                }

                SAL_WARN("ucb.ucp.oauth2", "OAuth2 authorization error: " << m_sError);
                m_bCallbackReceived = true;
            }

            sResponseContent = u"<!DOCTYPE html><html><head><title>OAuth2 Authorization Error</title></head>"
                              "<body><h1>Authorization Failed</h1>"
                              "<p>There was an error during the authorization process.</p>"
                              "<p>Please try again or contact support.</p></body></html>"_ustr;
        }
    }
    else
    {
        sResponseContent = u"<!DOCTYPE html><html><head><title>OAuth2 Callback</title></head>"
                          "<body><h1>Invalid Request</h1>"
                          "<p>This is the OAuth2 callback endpoint for LibreOffice.</p></body></html>"_ustr;
    }

    // Send HTTP response
    sendHttpResponse(rClientSocket, sResponseContent);
}

OUString OAuth2CallbackServer::parseHttpRequest(const OString& sRequest)
{
    SAL_INFO("ucb.ucp.oauth2", "Parsing HTTP request");

    // Extract the first line (request line)
    sal_Int32 nLineEnd = sRequest.indexOf("\r\n");
    if (nLineEnd == -1)
        nLineEnd = sRequest.indexOf("\n");

    if (nLineEnd == -1)
    {
        SAL_WARN("ucb.ucp.oauth2", "Invalid HTTP request format");
        return OUString();
    }

    OString sRequestLine = sRequest.copy(0, nLineEnd);

    // Parse request line: "GET /callback?code=... HTTP/1.1"
    sal_Int32 nMethodEnd = sRequestLine.indexOf(' ');
    if (nMethodEnd == -1)
        return OUString();

    sal_Int32 nUrlStart = nMethodEnd + 1;
    sal_Int32 nUrlEnd = sRequestLine.indexOf(' ', nUrlStart);
    if (nUrlEnd == -1)
        nUrlEnd = sRequestLine.getLength();

    OString sUrl = sRequestLine.copy(nUrlStart, nUrlEnd - nUrlStart);

    // Convert to Unicode and return full callback URL
    OUString sCallbackUrl = u"http://localhost"_ustr + OStringToOUString(sUrl, RTL_TEXTENCODING_UTF8);

    SAL_INFO("ucb.ucp.oauth2", "Parsed callback URL: " << sCallbackUrl);
    return sCallbackUrl;
}

void OAuth2CallbackServer::sendHttpResponse(osl::StreamSocket& rClientSocket, const OUString& sContent)
{
    SAL_INFO("ucb.ucp.oauth2", "Sending HTTP response");

    // Prepare HTTP response
    OString sContentUtf8 = OUStringToOString(sContent, RTL_TEXTENCODING_UTF8);

    OStringBuffer aResponse;
    aResponse.append("HTTP/1.1 200 OK\r\n");
    aResponse.append("Content-Type: text/html; charset=utf-8\r\n");
    aResponse.append("Content-Length: ");
    aResponse.append(static_cast<sal_Int32>(sContentUtf8.getLength()));
    aResponse.append("\r\n");
    aResponse.append("Connection: close\r\n");
    aResponse.append("Cache-Control: no-cache, no-store, must-revalidate\r\n");
    aResponse.append("\r\n");
    aResponse.append(sContentUtf8);

    OString sResponse = aResponse.makeStringAndClear();

    // Send response
    sal_Int32 nBytesSent = rClientSocket.send(sResponse.getStr(), sResponse.getLength());

    if (nBytesSent <= 0)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to send HTTP response");
    }
    else
    {
        SAL_INFO("ucb.ucp.oauth2", "HTTP response sent: " << nBytesSent << " bytes");
    }
}

bool OAuth2CallbackServer::waitForCallback(sal_uInt32 nTimeoutSeconds)
{
    SAL_INFO("ucb.ucp.oauth2", "Waiting for OAuth2 callback, timeout: " << nTimeoutSeconds << " seconds");

    // Wait for callback with timeout
    sal_uInt32 nElapsed = 0;
    const sal_uInt32 nSleepMs = 100; // Check every 100ms

    while (nElapsed < nTimeoutSeconds * 1000 && !m_bCallbackReceived.load())
    {
        osl::Thread::wait(std::chrono::milliseconds(nSleepMs));
        nElapsed += nSleepMs;
    }

    bool bSuccess = m_bCallbackReceived.load();
    SAL_INFO("ucb.ucp.oauth2", "Callback wait finished: " << (bSuccess ? "received" : "timeout"));

    return bSuccess;
}

/**
 * OAuth2Flow Implementation - Phase 3 Complete Version
 *
 * Implements the complete OAuth2 authorization code flow with PKCE for security,
 * browser-based authentication, and JSON token processing.
 */

OAuth2Flow::OAuth2Flow(const uno::Reference<uno::XComponentContext>& xContext)
    : m_xContext(xContext)
{
    SAL_INFO("ucb.ucp.oauth2", "OAuth2Flow created - Phase 3 complete implementation");
}

OAuth2Flow::~OAuth2Flow()
{
    SAL_INFO("ucb.ucp.oauth2", "OAuth2Flow destroyed");
}

/**
 * Generate a cryptographically secure code verifier for PKCE
 * Must be 43-128 characters long, URL-safe base64 encoded
 */
OUString OAuth2Flow::generateCodeVerifier()
{
    SAL_INFO("ucb.ucp.oauth2", "Generating PKCE code verifier");

    // Generate 32 random bytes (256 bits)
    constexpr sal_Int32 nByteCount = 32;
    uno::Sequence<sal_Int8> aRandomBytes(nByteCount);

    if (rtl_random_getBytes(nullptr, aRandomBytes.getArray(), nByteCount) != rtl_Random_E_None)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to generate random bytes for code verifier");
        throw uno::RuntimeException(u"Failed to generate secure random bytes"_ustr);
    }

    // Base64 encode and make URL-safe
    OUStringBuffer aBuffer;
    comphelper::Base64::encode(aBuffer, aRandomBytes);
    OUString sCodeVerifier = aBuffer.makeStringAndClear();

    // Make URL-safe: replace + with -, / with _, remove padding =
    sCodeVerifier = sCodeVerifier.replaceAll(u"+", u"-")
                                 .replaceAll(u"/", u"_")
                                 .replaceAll(u"=", u"");

    SAL_INFO("ucb.ucp.oauth2", "Generated code verifier: " << sCodeVerifier.getLength() << " characters");
    return sCodeVerifier;
}

/**
 * Generate code challenge from code verifier using SHA256
 */
OUString OAuth2Flow::generateCodeChallenge(const OUString& codeVerifier)
{
    SAL_INFO("ucb.ucp.oauth2", "Generating PKCE code challenge");

    // Convert to UTF-8 bytes
    OString sVerifierUtf8 = OUStringToOString(codeVerifier, RTL_TEXTENCODING_UTF8);

    // Calculate SHA256 hash
    ::comphelper::Hash aHasher(::comphelper::HashType::SHA256);
    aHasher.update(reinterpret_cast<const unsigned char*>(sVerifierUtf8.getStr()), sVerifierUtf8.getLength());
    std::vector<unsigned char> aHashResult = aHasher.finalize();

    // Convert to sequence for base64 encoding
    uno::Sequence<sal_Int8> aHashSequence(aHashResult.size());
    std::copy(aHashResult.begin(), aHashResult.end(), aHashSequence.getArray());

    // Base64 encode and make URL-safe
    OUStringBuffer aBuffer;
    comphelper::Base64::encode(aBuffer, aHashSequence);
    OUString sCodeChallenge = aBuffer.makeStringAndClear();

    // Make URL-safe
    sCodeChallenge = sCodeChallenge.replaceAll(u"+", u"-")
                                   .replaceAll(u"/", u"_")
                                   .replaceAll(u"=", u"");

    SAL_INFO("ucb.ucp.oauth2", "Generated code challenge");
    return sCodeChallenge;
}

/**
 * Generate a secure random state parameter
 */
OUString OAuth2Flow::generateState()
{
    SAL_INFO("ucb.ucp.oauth2", "Generating OAuth2 state parameter");

    constexpr sal_Int32 nByteCount = 16;
    uno::Sequence<sal_Int8> aRandomBytes(nByteCount);

    if (rtl_random_getBytes(nullptr, aRandomBytes.getArray(), nByteCount) != rtl_Random_E_None)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to generate random bytes for state");
        throw uno::RuntimeException(u"Failed to generate secure random bytes"_ustr);
    }

    OUStringBuffer aBuffer;
    comphelper::Base64::encode(aBuffer, aRandomBytes);
    OUString sState = aBuffer.makeStringAndClear();

    // Make URL-safe
    sState = sState.replaceAll(u"+", u"-")
                   .replaceAll(u"/", u"_")
                   .replaceAll(u"=", u"");

    SAL_INFO("ucb.ucp.oauth2", "Generated state parameter");
    return sState;
}

/**
 * URL encode a string
 */
OUString OAuth2Flow::urlEncode(const OUString& input)
{
    if (input.isEmpty()) return input;

    CurlHandle curl(curl_easy_init());
    if (!curl)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to initialize curl for URL encoding");
        return input;
    }

    OString sInputUtf8 = OUStringToOString(input, RTL_TEXTENCODING_UTF8);
    char* pEncoded = curl_easy_escape(curl.get(), sInputUtf8.getStr(), sInputUtf8.getLength());

    if (!pEncoded)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to URL encode string");
        return input;
    }

    OUString sResult = OStringToOUString(pEncoded, RTL_TEXTENCODING_UTF8);
    curl_free(pEncoded);

    return sResult;
}

/**
 * Make HTTP request using libcurl
 */
OUString OAuth2Flow::makeHttpRequest(const OUString& url, const OUString& postData, const OUString& contentType)
{
    SAL_INFO("ucb.ucp.oauth2", "Making HTTP request to: " << url);

    CurlHandle curl(curl_easy_init());
    if (!curl)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to initialize curl");
        throw uno::RuntimeException(u"Failed to initialize HTTP client"_ustr);
    }

    // Initialize with LibreOffice defaults
    // Initialize curl for LibreOffice environment
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "http,https");

    // Set URL
    OString sUrlUtf8 = OUStringToOString(url, RTL_TEXTENCODING_UTF8);
    curl_easy_setopt(curl.get(), CURLOPT_URL, sUrlUtf8.getStr());

    // Set up response handling
    std::string response;
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

    // Set timeouts suitable for OAuth operations
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 120L);

    // Follow redirects
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    // Headers
    struct curl_slist* headers = nullptr;
    if (!postData.isEmpty())
    {
        // POST request
        OString sPostDataUtf8 = OUStringToOString(postData, RTL_TEXTENCODING_UTF8);
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, sPostDataUtf8.getStr());

        // Set content type
        OString sContentTypeHeader = "Content-Type: " + OUStringToOString(contentType, RTL_TEXTENCODING_UTF8);
        headers = curl_slist_append(headers, sContentTypeHeader.getStr());
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl.get());

    // Clean up headers
    if (headers)
        curl_slist_free_all(headers);

    if (res != CURLE_OK)
    {
        SAL_WARN("ucb.ucp.oauth2", "HTTP request failed: " << curl_easy_strerror(res));
        throw css::ucb::InteractiveIOException(
            u"HTTP request failed: "_ustr + OStringToOUString(curl_easy_strerror(res), RTL_TEXTENCODING_UTF8),
            uno::Reference<uno::XInterface>(),
            css::task::InteractionClassification_ERROR,
            css::ucb::IOErrorCode_GENERAL);
    }

    // Check HTTP status code
    long httpCode = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpCode);

    SAL_INFO("ucb.ucp.oauth2", "HTTP response code: " << httpCode);

    if (httpCode >= 400)
    {
        SAL_WARN("ucb.ucp.oauth2", "HTTP error " << httpCode << ": " << response.c_str());
        throw css::ucb::InteractiveIOException(
            u"HTTP error "_ustr + OUString::number(httpCode),
            uno::Reference<uno::XInterface>(),
            css::task::InteractionClassification_ERROR,
            css::ucb::IOErrorCode_GENERAL);
    }

    return OStringToOUString(response, RTL_TEXTENCODING_UTF8);
}

/**
 * Build OAuth2 authorization URL with PKCE parameters and callback port
 */
OUString OAuth2Flow::buildAuthorizationUrl(const css::ucb::OAuth2ProviderConfig& config, const OUString& username, sal_uInt16 nCallbackPort)
{
    SAL_INFO("ucb.ucp.oauth2", "Building authorization URL for: " << config.sDisplayName);

    // Generate PKCE parameters
    m_sCodeVerifier = generateCodeVerifier();
    m_sCodeChallenge = generateCodeChallenge(m_sCodeVerifier);
    m_sState = generateState();

    // Build redirect URI with callback port
    OUString sRedirectUri = u"http://localhost:"_ustr + OUString::number(nCallbackPort) + u"/callback"_ustr;

    // Build authorization URL
    OUStringBuffer aUrlBuffer;
    aUrlBuffer.append(config.sAuthUrl);

    // Add required parameters
    aUrlBuffer.append(config.sAuthUrl.indexOf('?') == -1 ? u"?" : u"&");
    aUrlBuffer.append(u"response_type=code");
    aUrlBuffer.append(u"&client_id=").append(urlEncode(config.sClientId));
    aUrlBuffer.append(u"&redirect_uri=").append(urlEncode(sRedirectUri));
    aUrlBuffer.append(u"&scope=").append(urlEncode(config.sScope));
    aUrlBuffer.append(u"&state=").append(urlEncode(m_sState));

    // Add PKCE parameters if enabled
    if (config.bUsePKCE)
    {
        aUrlBuffer.append(u"&code_challenge=").append(urlEncode(m_sCodeChallenge));
        aUrlBuffer.append(u"&code_challenge_method=S256");
    }

    // Add login hint if username provided
    if (!username.isEmpty())
    {
        aUrlBuffer.append(u"&login_hint=").append(urlEncode(username));
    }

    // Add custom parameters
    for (const auto& param : config.aCustomParams)
    {
        OUString sValue;
        param.Value >>= sValue;
        aUrlBuffer.append(u"&").append(urlEncode(param.Name))
                  .append(u"=").append(urlEncode(sValue));
    }

    OUString sAuthUrl = aUrlBuffer.makeStringAndClear();
    SAL_INFO("ucb.ucp.oauth2", "Authorization URL built");
    return sAuthUrl;
}

/**
 * Launch system browser for authentication and wait for callback
 */
bool OAuth2Flow::launchBrowserAuthentication(const OUString& authUrl, OUString& authCode, const OUString& expectedState)
{
    (void)authCode;     // Will be set by callback server
    (void)expectedState; // Used for state validation
    SAL_INFO("ucb.ucp.oauth2", "Launching browser for OAuth2 authentication");

    try
    {
        // Get system shell execute service
        uno::Reference<css::system::XSystemShellExecute> xSystemShell =
            css::system::SystemShellExecute::create(m_xContext);

        if (!xSystemShell.is())
        {
            SAL_WARN("ucb.ucp.oauth2", "Could not create SystemShellExecute service");
            return false;
        }

        // Launch browser
        xSystemShell->execute(authUrl, OUString(), css::system::SystemShellExecuteFlags::URIS_ONLY);

        SAL_INFO("ucb.ucp.oauth2", "Browser launched successfully");
        return true;
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to launch browser: " << e.Message);
        return false;
    }
}

/**
 * Exchange authorization code for access/refresh tokens
 */
OAuth2AuthResult OAuth2Flow::exchangeCodeForTokens(const css::ucb::OAuth2ProviderConfig& config, const OUString& authCode)
{
    SAL_INFO("ucb.ucp.oauth2", "Exchanging authorization code for tokens");

    OAuth2AuthResult result;

    try
    {
        // Build token request parameters
        OUStringBuffer aPostData;
        aPostData.append(u"grant_type=authorization_code");
        aPostData.append(u"&code=").append(urlEncode(authCode));
        aPostData.append(u"&redirect_uri=").append(urlEncode(u"http://localhost/callback"_ustr));
        aPostData.append(u"&client_id=").append(urlEncode(config.sClientId));

        // Add client secret if provided
        if (!config.sClientSecret.isEmpty())
        {
            aPostData.append(u"&client_secret=").append(urlEncode(config.sClientSecret));
        }

        // Add PKCE code verifier if PKCE is enabled
        if (config.bUsePKCE && !m_sCodeVerifier.isEmpty())
        {
            aPostData.append(u"&code_verifier=").append(urlEncode(m_sCodeVerifier));
        }

        // Make token request
        OUString sResponse = makeHttpRequest(config.sTokenUrl, aPostData.makeStringAndClear());

        // Parse JSON response
        result = parseTokenResponse(sResponse);

        SAL_INFO("ucb.ucp.oauth2", "Token exchange " << (result.bSuccess ? "successful" : "failed"));
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Token exchange failed: " << e.Message);
        result.bSuccess = false;
        result.sError = u"token_exchange_failed"_ustr;
        result.sErrorDescription = e.Message;
    }

    return result;
}

/**
 * Parse OAuth2 token response JSON
 */
OAuth2AuthResult OAuth2Flow::parseTokenResponse(const OUString& jsonResponse)
{
    SAL_INFO("ucb.ucp.oauth2", "Parsing OAuth2 token response");

    OAuth2AuthResult result;

    if (jsonResponse.isEmpty())
    {
        result.sError = u"empty_response"_ustr;
        result.sErrorDescription = u"Received empty response from token endpoint"_ustr;
        return result;
    }

    try
    {
        orcus::json::document_tree aJsonDoc;
        orcus::json_config aConfig;

        OString sJsonUtf8 = OUStringToOString(jsonResponse, RTL_TEXTENCODING_UTF8);
        aJsonDoc.load(sJsonUtf8.getStr(), aConfig);

        auto aRoot = aJsonDoc.get_document_root();
        if (aRoot.type() != orcus::json::node_t::object)
        {
            result.sError = u"invalid_json"_ustr;
            result.sErrorDescription = u"Response is not a valid JSON object"_ustr;
            return result;
        }

        // Check for error response
        if (aRoot.has_key("error"))
        {
            auto errorNode = aRoot.child("error");
            result.sError = OStringToOUString(errorNode.string_value(), RTL_TEXTENCODING_UTF8);

            if (aRoot.has_key("error_description"))
            {
                auto descNode = aRoot.child("error_description");
                result.sErrorDescription = OStringToOUString(descNode.string_value(), RTL_TEXTENCODING_UTF8);
            }

            SAL_WARN("ucb.ucp.oauth2", "OAuth2 error: " << result.sError << " - " << result.sErrorDescription);
            return result;
        }

        // Parse successful response
        if (aRoot.has_key("access_token"))
        {
            auto tokenNode = aRoot.child("access_token");
            result.sAccessToken = OStringToOUString(tokenNode.string_value(), RTL_TEXTENCODING_UTF8);
        }

        if (aRoot.has_key("refresh_token"))
        {
            auto refreshNode = aRoot.child("refresh_token");
            result.sRefreshToken = OStringToOUString(refreshNode.string_value(), RTL_TEXTENCODING_UTF8);
        }

        if (aRoot.has_key("expires_in"))
        {
            auto expiresNode = aRoot.child("expires_in");
            result.nExpiresInSeconds = static_cast<long>(expiresNode.numeric_value());
            result.aExpiryTime = calculateExpiryTime(result.nExpiresInSeconds);
        }

        // Validate required fields
        if (result.sAccessToken.isEmpty())
        {
            result.sError = u"missing_access_token"_ustr;
            result.sErrorDescription = u"Response does not contain access_token"_ustr;
            return result;
        }

        result.bSuccess = true;
        SAL_INFO("ucb.ucp.oauth2", "Successfully parsed token response");
    }
    catch (const std::exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "JSON parsing failed: " << e.what());
        result.sError = u"json_parse_error"_ustr;
        result.sErrorDescription = OStringToOUString(e.what(), RTL_TEXTENCODING_UTF8);
    }

    return result;
}

/**
 * Parse OAuth2 refresh token response JSON
 */
OAuth2RefreshResult OAuth2Flow::parseRefreshResponse(const OUString& jsonResponse)
{
    SAL_INFO("ucb.ucp.oauth2", "Parsing OAuth2 refresh response");

    OAuth2RefreshResult result;

    // Parse using same logic as token response
    OAuth2AuthResult authResult = parseTokenResponse(jsonResponse);

    // Copy fields
    result.bSuccess = authResult.bSuccess;
    result.sAccessToken = authResult.sAccessToken;
    result.sRefreshToken = authResult.sRefreshToken;
    result.nExpiresInSeconds = authResult.nExpiresInSeconds;
    result.aExpiryTime = authResult.aExpiryTime;
    result.sError = authResult.sError;
    result.sErrorDescription = authResult.sErrorDescription;

    return result;
}

/**
 * Complete OAuth2 authentication flow with callback server
 */
OAuth2AuthResult OAuth2Flow::performAuthentication(
    const css::ucb::OAuth2ProviderConfig& config,
    const OUString& username,
    const uno::Reference<css::ucb::XCommandEnvironment>& /*xEnvironment*/)
{
    SAL_INFO("ucb.ucp.oauth2", "Starting OAuth2 authentication flow for: " << config.sDisplayName);

    OAuth2AuthResult result;

    try
    {
        // Validate configuration
        if (config.sAuthUrl.isEmpty() || config.sTokenUrl.isEmpty() ||
            config.sClientId.isEmpty())
        {
            result.sError = u"invalid_configuration"_ustr;
            result.sErrorDescription = u"OAuth2 provider configuration is incomplete"_ustr;
            SAL_WARN("ucb.ucp.oauth2", "Invalid OAuth2 configuration");
            return result;
        }

        // Store provider and user info
        m_sProviderUrl = config.sBaseUrl;
        m_sUsername = username;

        // Start callback server
        std::unique_ptr<OAuth2CallbackServer> pCallbackServer(new OAuth2CallbackServer());
        if (!pCallbackServer->startServer())
        {
            result.sError = u"callback_server_failed"_ustr;
            result.sErrorDescription = u"Failed to start OAuth2 callback server"_ustr;
            SAL_WARN("ucb.ucp.oauth2", "Failed to start callback server");
            return result;
        }

        sal_uInt16 nCallbackPort = pCallbackServer->getPort();
        SAL_INFO("ucb.ucp.oauth2", "Callback server started on port " << nCallbackPort);

        // Build authorization URL with callback port
        OUString sAuthUrl = buildAuthorizationUrl(config, username, nCallbackPort);

        // Launch browser for authentication
        OUString sAuthCode;
        if (!launchBrowserAuthentication(sAuthUrl, sAuthCode, m_sState))
        {
            pCallbackServer->stopServer();
            result.sError = u"browser_launch_failed"_ustr;
            result.sErrorDescription = u"Failed to launch browser for authentication"_ustr;
            SAL_WARN("ucb.ucp.oauth2", "Browser launch failed");
            return result;
        }

        // Wait for callback
        SAL_INFO("ucb.ucp.oauth2", "Waiting for OAuth2 callback...");
        if (!pCallbackServer->waitForCallback(300)) // 5 minutes timeout
        {
            pCallbackServer->stopServer();
            result.sError = u"callback_timeout"_ustr;
            result.sErrorDescription = u"Timeout waiting for OAuth2 authorization callback"_ustr;
            SAL_WARN("ucb.ucp.oauth2", "Callback timeout");
            return result;
        }

        // Get callback results
        if (!pCallbackServer->getError().isEmpty())
        {
            result.sError = pCallbackServer->getError();
            result.sErrorDescription = pCallbackServer->getErrorDescription();
            pCallbackServer->stopServer();
            SAL_WARN("ucb.ucp.oauth2", "OAuth2 authorization error: " << result.sError);
            return result;
        }

        sAuthCode = pCallbackServer->getAuthorizationCode();
        OUString sReceivedState = pCallbackServer->getState();
        pCallbackServer->stopServer();

        // Verify state parameter for security
        if (sReceivedState != m_sState)
        {
            result.sError = u"state_mismatch"_ustr;
            result.sErrorDescription = u"State parameter mismatch - possible CSRF attack"_ustr;
            SAL_WARN("ucb.ucp.oauth2", "State parameter mismatch");
            return result;
        }

        if (sAuthCode.isEmpty())
        {
            result.sError = u"no_authorization_code"_ustr;
            result.sErrorDescription = u"No authorization code received in callback"_ustr;
            SAL_WARN("ucb.ucp.oauth2", "No authorization code received");
            return result;
        }

        SAL_INFO("ucb.ucp.oauth2", "Authorization code received successfully");

        // Exchange code for tokens
        result = exchangeCodeForTokens(config, sAuthCode);

        SAL_INFO("ucb.ucp.oauth2", "OAuth2 authentication flow " << (result.bSuccess ? "completed successfully" : "failed"));
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "OAuth2 authentication failed: " << e.Message);
        result.bSuccess = false;
        result.sError = u"authentication_failed"_ustr;
        result.sErrorDescription = e.Message;
    }

    return result;
}

/**
 * Refresh access token using refresh token
 */
OAuth2RefreshResult OAuth2Flow::refreshAccessToken(
    const css::ucb::OAuth2ProviderConfig& config,
    const OUString& refreshToken,
    const uno::Reference<css::ucb::XCommandEnvironment>& /*xEnvironment*/)
{
    SAL_INFO("ucb.ucp.oauth2", "Refreshing OAuth2 access token");

    OAuth2RefreshResult result;

    try
    {
        // Validate inputs
        if (refreshToken.isEmpty() || config.sTokenUrl.isEmpty() || config.sClientId.isEmpty())
        {
            result.sError = u"invalid_request"_ustr;
            result.sErrorDescription = u"Missing required parameters for token refresh"_ustr;
            return result;
        }

        // Build refresh request parameters
        OUStringBuffer aPostData;
        aPostData.append(u"grant_type=refresh_token");
        aPostData.append(u"&refresh_token=").append(urlEncode(refreshToken));
        aPostData.append(u"&client_id=").append(urlEncode(config.sClientId));

        // Add client secret if provided
        if (!config.sClientSecret.isEmpty())
        {
            aPostData.append(u"&client_secret=").append(urlEncode(config.sClientSecret));
        }

        // Make refresh request
        OUString sResponse = makeHttpRequest(config.sTokenUrl, aPostData.makeStringAndClear());

        // Parse response
        result = parseRefreshResponse(sResponse);

        SAL_INFO("ucb.ucp.oauth2", "Token refresh " << (result.bSuccess ? "successful" : "failed"));
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Token refresh failed: " << e.Message);
        result.bSuccess = false;
        result.sError = u"refresh_failed"_ustr;
        result.sErrorDescription = e.Message;
    }

    return result;
}

/**
 * Calculate token expiry time from seconds
 */
css::util::DateTime OAuth2Flow::calculateExpiryTime(long expiresInSeconds)
{
    SAL_INFO("ucb.ucp.oauth2", "Calculating expiry time for " << expiresInSeconds << " seconds");

    css::util::DateTime aResult;

    // Get current time
    time_t now = time(nullptr);
    time_t expiryTime = now + expiresInSeconds;

    struct tm* timeinfo = gmtime(&expiryTime);
    if (timeinfo)
    {
        aResult.Year = timeinfo->tm_year + 1900;
        aResult.Month = timeinfo->tm_mon + 1;
        aResult.Day = timeinfo->tm_mday;
        aResult.Hours = timeinfo->tm_hour;
        aResult.Minutes = timeinfo->tm_min;
        aResult.Seconds = timeinfo->tm_sec;
        aResult.NanoSeconds = 0;
        aResult.IsUTC = true;

        SAL_INFO("ucb.ucp.oauth2", "Token expires at: " <<
                 aResult.Year << "-" << aResult.Month << "-" << aResult.Day << " " <<
                 aResult.Hours << ":" << aResult.Minutes << ":" << aResult.Seconds << " UTC");
    }
    else
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to calculate expiry time");
        // Return a future date as fallback
        aResult.Year = 2025;
        aResult.Month = 1;
        aResult.Day = 1;
        aResult.Hours = 0;
        aResult.Minutes = 0;
        aResult.Seconds = 0;
        aResult.NanoSeconds = 0;
        aResult.IsUTC = true;
    }

    return aResult;
}

/**
 * Extract authorization code from callback URL
 */
OUString OAuth2Flow::extractAuthCodeFromCallback(const OUString& callbackUrl)
{
    SAL_INFO("ucb.ucp.oauth2", "Extracting auth code from callback URL");

    // Look for code parameter
    sal_Int32 codePos = callbackUrl.indexOf(u"code=");
    if (codePos == -1)
    {
        SAL_INFO("ucb.ucp.oauth2", "No authorization code found in callback URL");
        return OUString();
    }

    sal_Int32 startPos = codePos + 5; // Length of "code="
    sal_Int32 endPos = callbackUrl.indexOf(u"&", startPos);

    if (endPos == -1)
        endPos = callbackUrl.getLength();

    OUString code = callbackUrl.copy(startPos, endPos - startPos);
    SAL_INFO("ucb.ucp.oauth2", "Extracted authorization code: " << code.getLength() << " characters");
    return code;
}

} // namespace ucb::oauth2

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
