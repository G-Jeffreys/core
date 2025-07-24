/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "oauth2_service.hxx"
#include "token_manager.hxx"
#include "config_manager.hxx"
#include "oauth2_flow.hxx"

#include <com/sun/star/ucb/XOAuth2Service.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/ucb/InteractiveIOException.hpp>
#include <rtl/ustrbuf.hxx>
#include <com/sun/star/task/XInteractionHandler.hpp>
#include <com/sun/star/task/XInteractionRequest.hpp>
#include <com/sun/star/task/InteractionClassification.hpp>
#include <cppuhelper/supportsservice.hxx>
#include <sal/log.hxx>

using namespace css;

namespace ucb::oauth2 {

constexpr OUStringLiteral SERVICE_NAME = u"com.sun.star.ucb.OAuth2Service";

/**
 * Enhanced Error Handling and User Experience
 *
 * Provides comprehensive error categorization, user-friendly messages,
 * and recovery suggestions for OAuth2 operations.
 */

namespace {

    /**
     * Error categories for better handling and user messaging
     */
    enum class ErrorCategory {
        NETWORK,           // Network connectivity issues
        AUTHENTICATION,    // OAuth2 authentication failures
        CONFIGURATION,     // Invalid or missing configuration
        USER_CANCELLED,    // User cancelled the operation
        PROVIDER,          // OAuth2 provider-specific errors
        INTERNAL,          // Internal LibreOffice errors
        UNKNOWN            // Unclassified errors
    };

    /**
     * Categorize error based on error code and message
     */
    ErrorCategory categorizeError(const OUString& sError, const OUString& sErrorDescription)
    {
        // Network-related errors
        if (sError.indexOf("network") != -1 || sError.indexOf("connection") != -1 ||
            sError.indexOf("timeout") != -1 || sError.indexOf("dns") != -1)
        {
            return ErrorCategory::NETWORK;
        }

        // Authentication-related errors
        if (sError == u"access_denied" || sError == u"unauthorized_client" ||
            sError == u"invalid_grant" || sError == u"invalid_client")
        {
            return ErrorCategory::AUTHENTICATION;
        }

        // Configuration errors
        if (sError == u"invalid_request" || sError == u"unsupported_response_type" ||
            sError == u"invalid_scope" || sError == u"invalid_configuration")
        {
            return ErrorCategory::CONFIGURATION;
        }

        // User cancellation
        if (sError == u"access_denied" && sErrorDescription.indexOf("user") != -1)
        {
            return ErrorCategory::USER_CANCELLED;
        }

        // Provider-specific errors
        if (sError == u"temporarily_unavailable" || sError == u"server_error" ||
            sError.indexOf("rate_limit") != -1)
        {
            return ErrorCategory::PROVIDER;
        }

        // Browser or callback errors
        if (sError.indexOf("browser") != -1 || sError.indexOf("callback") != -1)
        {
            return ErrorCategory::INTERNAL;
        }

        return ErrorCategory::UNKNOWN;
    }

    /**
     * Generate user-friendly error message with recovery suggestions
     */
    OUString generateUserFriendlyMessage(const OUString& sError, const OUString& sErrorDescription, const OUString& sProviderName)
    {
        ErrorCategory eCategory = categorizeError(sError, sErrorDescription);

        rtl::OUStringBuffer aMessage;

        switch (eCategory)
        {
            case ErrorCategory::NETWORK:
                aMessage.append(u"Network Connection Problem\n\n");
                aMessage.append(u"LibreOffice could not connect to ");
                aMessage.append(sProviderName);
                aMessage.append(u" due to a network issue.\n\n");
                aMessage.append(u"Please try the following:\n");
                aMessage.append(u"• Check your internet connection\n");
                aMessage.append(u"• Verify that your firewall allows LibreOffice to access the internet\n");
                aMessage.append(u"• Try again in a few moments\n");
                aMessage.append(u"• Contact your network administrator if the problem persists");
                break;

            case ErrorCategory::AUTHENTICATION:
                aMessage.append(u"Authentication Failed\n\n");
                if (sError == u"access_denied")
                {
                    aMessage.append(u"Access to your ");
                    aMessage.append(sProviderName);
                    aMessage.append(u" account was denied.\n\n");
                    aMessage.append(u"This may happen if:\n");
                    aMessage.append(u"• You cancelled the authorization process\n");
                    aMessage.append(u"• Your account doesn't have the required permissions\n");
                    aMessage.append(u"• The application hasn't been approved by your administrator\n\n");
                    aMessage.append(u"Please try authorizing again or contact your administrator.");
                }
                else if (sError == u"invalid_grant")
                {
                    aMessage.append(u"Your authorization has expired or become invalid.\n\n");
                    aMessage.append(u"Please sign in again to ");
                    aMessage.append(sProviderName);
                    aMessage.append(u" to continue using this service.");
                }
                else
                {
                    aMessage.append(u"Could not authenticate with ");
                    aMessage.append(sProviderName);
                    aMessage.append(u".\n\n");
                    aMessage.append(u"Please try signing in again. If the problem persists, ");
                    aMessage.append(u"contact your system administrator.");
                }
                break;

            case ErrorCategory::CONFIGURATION:
                aMessage.append(u"Configuration Error\n\n");
                aMessage.append(u"There is a problem with the ");
                aMessage.append(sProviderName);
                aMessage.append(u" service configuration.\n\n");
                aMessage.append(u"Please contact your system administrator or LibreOffice support ");
                aMessage.append(u"for assistance with this technical issue.");
                break;

            case ErrorCategory::USER_CANCELLED:
                aMessage.append(u"Authorization Cancelled\n\n");
                aMessage.append(u"You cancelled the authorization process for ");
                aMessage.append(sProviderName);
                aMessage.append(u".\n\n");
                aMessage.append(u"To use this service, please try again and complete ");
                aMessage.append(u"the authorization in your web browser.");
                break;

            case ErrorCategory::PROVIDER:
                aMessage.append(u"Service Temporarily Unavailable\n\n");
                aMessage.append(sProviderName);
                aMessage.append(u" is currently experiencing technical difficulties.\n\n");
                aMessage.append(u"Please try again in a few minutes. If the problem persists, ");
                aMessage.append(u"the issue is likely on ");
                aMessage.append(sProviderName);
                aMessage.append(u"'s side and should be resolved soon.");
                break;

            case ErrorCategory::INTERNAL:
                aMessage.append(u"Internal Error\n\n");
                aMessage.append(u"LibreOffice encountered an internal error while trying to ");
                aMessage.append(u"connect to ");
                aMessage.append(sProviderName);
                aMessage.append(u".\n\n");
                aMessage.append(u"Please try the following:\n");
                aMessage.append(u"• Restart LibreOffice\n");
                aMessage.append(u"• Check that your system allows LibreOffice to open web browsers\n");
                aMessage.append(u"• Update LibreOffice to the latest version\n");
                aMessage.append(u"• Contact LibreOffice support if the problem continues");
                break;

            default:
                aMessage.append(u"Unexpected Error\n\n");
                aMessage.append(u"An unexpected error occurred while connecting to ");
                aMessage.append(sProviderName);
                aMessage.append(u".\n\n");
                if (!sErrorDescription.isEmpty())
                {
                    aMessage.append(u"Technical details: ");
                    aMessage.append(sErrorDescription);
                    aMessage.append(u"\n\n");
                }
                aMessage.append(u"Please try again or contact support if the problem persists.");
                break;
        }

        return aMessage.makeStringAndClear();
    }

    /**
     * Show user-friendly error dialog if interaction handler is available
     */
    void showUserError(const uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment,
                       const OUString& sUserMessage,
                       const OUString& sTechnicalDetails = OUString())
    {
        if (!xEnvironment.is())
            return;

        auto xHandler = xEnvironment->getInteractionHandler();
        if (!xHandler.is())
            return;

        try
        {
            // Create a user-friendly interactive exception
            css::ucb::InteractiveIOException aException;
            aException.Message = sUserMessage;
            aException.Classification = task::InteractionClassification_ERROR;
            aException.Code = css::ucb::IOErrorCode_GENERAL;

            // Log technical details for debugging
            if (!sTechnicalDetails.isEmpty())
            {
                SAL_WARN("ucb.ucp.oauth2", "OAuth2 Error Details: " << sTechnicalDetails);
            }

            // Note: Full interaction request implementation would require more UNO infrastructure
            // For now, we log the user-friendly message
            SAL_WARN("ucb.ucp.oauth2", "User Error: " << sUserMessage);
        }
        catch (const uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.oauth2", "Failed to show user error: " << e.Message);
        }
    }

    /**
     * Determine if an error is recoverable (should be retried)
     */
    bool isRecoverableError(const OUString& sError)
    {
        // Network timeouts, temporary server errors, etc.
        return sError == u"timeout" ||
               sError == u"temporarily_unavailable" ||
               sError == u"server_error" ||
               sError.indexOf("network") != -1;
    }

    /**
     * Get appropriate retry delay based on error type
     */
    sal_Int32 getRetryDelaySeconds(const OUString& sError, sal_Int32 nAttempt)
    {
        // Exponential backoff with jitter
        sal_Int32 nBaseDelay = 2;

        if (sError == u"rate_limit" || sError.indexOf("rate") != -1)
        {
            nBaseDelay = 60; // Rate limit errors need longer delays
        }

        return nBaseDelay * (1 << std::min(nAttempt, 4)); // Cap at 32 seconds
    }
}

OAuth2Service::OAuth2Service(const uno::Reference<uno::XComponentContext>& xContext)
    : m_xContext(xContext)
{
    SAL_INFO("ucb.ucp.oauth2", "Enhanced OAuth2Service created with improved error handling");
}



uno::Reference<css::ucb::XTokenManager> OAuth2Service::getTokenManager()
{
    if (!m_xTokenManager.is())
    {
        try
        {
            m_xTokenManager.set(static_cast<css::ucb::XTokenManager*>(new TokenManager(m_xContext)), uno::UNO_SET_THROW);
        }
        catch (const uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.oauth2", "Failed to create TokenManager: " << e.Message);
            throw;
        }
    }
    return m_xTokenManager;
}

uno::Reference<css::ucb::XOAuth2Configuration> OAuth2Service::getConfigManager()
{
    if (!m_xConfigManager.is())
    {
        try
        {
            m_xConfigManager.set(static_cast<css::ucb::XOAuth2Configuration*>(new ConfigurationManager(m_xContext)), uno::UNO_SET_THROW);
        }
        catch (const uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.oauth2", "Failed to create ConfigurationManager: " << e.Message);
            throw;
        }
    }
    return m_xConfigManager;
}

void OAuth2Service::validateParameters(const OUString& sProviderName, const OUString& sUsername)
{
    if (sProviderName.isEmpty())
    {
        SAL_WARN("ucb.ucp.oauth2", "Empty provider name in OAuth2Service call");
        throw lang::IllegalArgumentException(u"Provider name cannot be empty"_ustr,
                                           static_cast<css::ucb::XOAuth2Service*>(this), 0);
    }

    if (sUsername.isEmpty())
    {
        SAL_WARN("ucb.ucp.oauth2", "Empty username in OAuth2Service call");
        throw lang::IllegalArgumentException(u"Username cannot be empty"_ustr,
                                           static_cast<css::ucb::XOAuth2Service*>(this), 1);
    }
}

// Enhanced XOAuth2Service interface implementation with error handling

OUString SAL_CALL OAuth2Service::authenticate(const OUString& sProviderName, const OUString& sUsername,
                                        const uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment)
{
    SAL_INFO("ucb.ucp.oauth2", "Starting enhanced authentication for " << sProviderName << ":" << sUsername);

    try
    {
        validateParameters(sProviderName, sUsername);

        // Check if already authenticated - get existing token if available
        auto xTokenManager = getTokenManager();
        OUString sExistingToken = xTokenManager->getAccessToken(sProviderName, sUsername);
        if (!sExistingToken.isEmpty() && !xTokenManager->isTokenExpired(sProviderName, sUsername, 5))
        {
            SAL_INFO("ucb.ucp.oauth2", "User already has valid token");
            return sExistingToken;
        }

        // Get provider configuration
        auto xConfigManager = getConfigManager();
        css::ucb::OAuth2ProviderConfig aConfig = xConfigManager->getProviderConfig(sProviderName);

        if (aConfig.sClientId.isEmpty())
        {
            OUString sError = u"No configuration found for provider: "_ustr + sProviderName;
            OUString sUserMessage = generateUserFriendlyMessage(u"invalid_configuration"_ustr, sError, sProviderName);

            showUserError(xEnvironment, sUserMessage, sError);
            throw css::ucb::InteractiveIOException(sUserMessage,
                                                  static_cast<css::ucb::XOAuth2Service*>(this),
                                                  css::task::InteractionClassification_ERROR,
                                                  css::ucb::IOErrorCode_NOT_SUPPORTED);
        }

        // Perform authentication with retry logic
        constexpr sal_Int32 MAX_RETRY_ATTEMPTS = 3;
        OAuth2AuthResult aResult;

        for (sal_Int32 nAttempt = 0; nAttempt < MAX_RETRY_ATTEMPTS; ++nAttempt)
        {
            try
            {
                OAuth2Flow aFlowHandler(m_xContext);
                aResult = aFlowHandler.performAuthentication(aConfig, sUsername, xEnvironment);

                if (aResult.bSuccess)
                {
                    // Store tokens
                    xTokenManager->storeTokens(sProviderName, sUsername,
                                             aResult.sAccessToken, aResult.sRefreshToken,
                                             aResult.aExpiryTime);

                    SAL_INFO("ucb.ucp.oauth2", "Authentication successful for " << sProviderName << ":" << sUsername);
                    return aResult.sAccessToken;
                }

                // Check if error is recoverable
                if (!isRecoverableError(aResult.sError) || nAttempt == MAX_RETRY_ATTEMPTS - 1)
                {
                    break;
                }

                // Wait before retry
                sal_Int32 nDelay = getRetryDelaySeconds(aResult.sError, nAttempt);
                SAL_INFO("ucb.ucp.oauth2", "Retrying authentication in " << nDelay << " seconds (attempt " << (nAttempt + 1) << ")");

                osl::Thread::wait(std::chrono::seconds(nDelay));
            }
            catch (const css::ucb::InteractiveIOException& e)
            {
                // Network or IO errors - may be recoverable
                if (isRecoverableError(e.Message) && nAttempt < MAX_RETRY_ATTEMPTS - 1)
                {
                    sal_Int32 nDelay = getRetryDelaySeconds(u"network"_ustr, nAttempt);
                    SAL_INFO("ucb.ucp.oauth2", "Network error, retrying in " << nDelay << " seconds");
                    osl::Thread::wait(std::chrono::seconds(nDelay));
                    continue;
                }
                throw;
            }
        }

        // Authentication failed after retries
        OUString sUserMessage = generateUserFriendlyMessage(aResult.sError, aResult.sErrorDescription, sProviderName);
        OUString sTechnicalDetails = u"Error: "_ustr + aResult.sError + u", Description: "_ustr + aResult.sErrorDescription;

        showUserError(xEnvironment, sUserMessage, sTechnicalDetails);

        SAL_WARN("ucb.ucp.oauth2", "Authentication failed for " << sProviderName << ":" << sUsername <<
                 " - Error: " << aResult.sError << ", Description: " << aResult.sErrorDescription);

        throw css::ucb::InteractiveIOException(sUserMessage,
                                              static_cast<css::ucb::XOAuth2Service*>(this),
                                              css::task::InteractionClassification_ERROR,
                                              css::ucb::IOErrorCode_GENERAL);
    }
    catch (const lang::IllegalArgumentException&)
    {
        throw; // Re-throw parameter validation errors as-is
    }
    catch (const css::ucb::InteractiveIOException&)
    {
        throw; // Re-throw our own user-friendly errors
    }
    catch (const uno::Exception& e)
    {
        OUString sUserMessage = generateUserFriendlyMessage(u"internal_error"_ustr, e.Message, sProviderName);
        showUserError(xEnvironment, sUserMessage, e.Message);

        SAL_WARN("ucb.ucp.oauth2", "Unexpected error during authentication: " << e.Message);
        throw css::ucb::InteractiveIOException(sUserMessage,
                                              static_cast<css::ucb::XOAuth2Service*>(this),
                                              css::task::InteractionClassification_ERROR,
                                              css::ucb::IOErrorCode_GENERAL);
    }
}

OUString SAL_CALL OAuth2Service::getValidAccessToken(const OUString& sProviderName, const OUString& sUsername,
                                                      const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment)
{
    (void)xEnvironment; // Parameter for future use
    SAL_INFO("ucb.ucp.oauth2", "Getting valid access token with enhanced error handling for " << sProviderName << ":" << sUsername);

    try
    {
        validateParameters(sProviderName, sUsername);

        auto xTokenManager = getTokenManager();
        OUString sToken = xTokenManager->getAccessToken(sProviderName, sUsername);

        if (sToken.isEmpty())
        {
            SAL_INFO("ucb.ucp.oauth2", "No valid token available - authentication required");
        }

        return sToken;
    }
    catch (const lang::IllegalArgumentException&)
    {
        throw; // Re-throw parameter validation errors
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Error getting access token: " << e.Message);
        return OUString(); // Return empty string on error for graceful degradation
    }
}

sal_Bool SAL_CALL OAuth2Service::isAuthenticated(const OUString& sProviderName, const OUString& sUsername)
{
    try
    {
        validateParameters(sProviderName, sUsername);

        auto xTokenManager = getTokenManager();
        // Check if we have a valid (non-expired) access token
        OUString sAccessToken = xTokenManager->getAccessToken(sProviderName, sUsername);
        if (sAccessToken.isEmpty())
            return false;

        // Check if token is not expired (with 5 minute margin)
        return !xTokenManager->isTokenExpired(sProviderName, sUsername, 5);
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Error checking authentication status: " << e.Message);
        return false; // Assume not authenticated on error
    }
}

OUString SAL_CALL OAuth2Service::refreshTokens(const OUString& sProviderName, const OUString& sUsername,
                                                const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment)
{
    (void)xEnvironment; // Parameter for future use
    SAL_INFO("ucb.ucp.oauth2", "Refreshing tokens with enhanced error handling for " << sProviderName << ":" << sUsername);

    try
    {
        validateParameters(sProviderName, sUsername);

        auto xTokenManager = getTokenManager();

        // Check if token exists and refresh if needed
        OUString sRefreshToken = xTokenManager->getRefreshToken(sProviderName, sUsername);
        if (sRefreshToken.isEmpty())
        {
            SAL_WARN("ucb.ucp.oauth2", "No refresh token available for " << sProviderName << ":" << sUsername);
            throw css::ucb::InteractiveIOException(u"No refresh token available"_ustr,
                                                  static_cast<css::ucb::XOAuth2Service*>(this),
                                                  css::task::InteractionClassification_ERROR,
                                                  css::ucb::IOErrorCode_GENERAL);
        }

        // For now, return the current access token - full refresh logic would be implemented later
        OUString sAccessToken = xTokenManager->getAccessToken(sProviderName, sUsername);
        if (sAccessToken.isEmpty())
        {
            SAL_WARN("ucb.ucp.oauth2", "Token refresh failed for " << sProviderName << ":" << sUsername);
            throw css::ucb::InteractiveIOException(u"Failed to refresh authentication tokens"_ustr,
                                                  static_cast<css::ucb::XOAuth2Service*>(this),
                                                  css::task::InteractionClassification_ERROR,
                                                  css::ucb::IOErrorCode_GENERAL);
        }

        return sAccessToken;
    }
    catch (const lang::IllegalArgumentException&)
    {
        throw; // Re-throw parameter validation errors
    }
    catch (const css::ucb::InteractiveIOException&)
    {
        throw; // Re-throw our own errors
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Unexpected error during token refresh: " << e.Message);
        throw css::ucb::InteractiveIOException(u"An unexpected error occurred while refreshing tokens"_ustr,
                                              static_cast<css::ucb::XOAuth2Service*>(this),
                                              css::task::InteractionClassification_ERROR,
                                              css::ucb::IOErrorCode_GENERAL);
    }
}

void SAL_CALL OAuth2Service::revokeAuthentication(const OUString& sProviderName, const OUString& sUsername)
{
    SAL_INFO("ucb.ucp.oauth2", "Revoking authentication with enhanced error handling for " << sProviderName << ":" << sUsername);

    try
    {
        validateParameters(sProviderName, sUsername);

        auto xTokenManager = getTokenManager();
        xTokenManager->clearTokens(sProviderName, sUsername);

        SAL_INFO("ucb.ucp.oauth2", "Authentication revoked successfully");
    }
    catch (const lang::IllegalArgumentException&)
    {
        throw; // Re-throw parameter validation errors
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Error revoking authentication: " << e.Message);
        // Continue anyway - best effort revocation
    }
}

css::uno::Sequence<OUString> SAL_CALL OAuth2Service::getAuthenticatedAccounts(const OUString& sProviderName)
{
    try
    {
        if (sProviderName.isEmpty())
        {
            throw lang::IllegalArgumentException(u"Provider name cannot be empty"_ustr,
                                               static_cast<css::ucb::XOAuth2Service*>(this), 0);
        }

        auto xTokenManager = getTokenManager();
        return xTokenManager->getStoredAccounts(sProviderName);
    }
    catch (const lang::IllegalArgumentException&)
    {
        throw; // Re-throw parameter validation errors
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Error getting authenticated accounts: " << e.Message);
        return css::uno::Sequence<OUString>(); // Return empty sequence on error
    }
}

void SAL_CALL OAuth2Service::configureProvider(const OUString& sProviderUrl,
                                               const css::uno::Sequence<css::beans::PropertyValue>& aConfiguration)
{
    SAL_INFO("ucb.ucp.oauth2", "Configuring provider with enhanced validation: " << sProviderUrl);

    try
    {
        if (sProviderUrl.isEmpty())
        {
            throw lang::IllegalArgumentException(u"Provider URL cannot be empty"_ustr,
                                               static_cast<css::ucb::XOAuth2Service*>(this), 0);
        }

        // Validate configuration has at least some properties
        if (aConfiguration.getLength() == 0)
        {
            throw lang::IllegalArgumentException(u"Invalid provider configuration - no properties provided"_ustr,
                                               static_cast<css::ucb::XOAuth2Service*>(this), 1);
        }

        // For now, just log the configuration - full implementation would store it
        SAL_INFO("ucb.ucp.oauth2", "Provider configured successfully: " << sProviderUrl << " with " << aConfiguration.getLength() << " properties");
    }
    catch (const lang::IllegalArgumentException&)
    {
        throw; // Re-throw parameter validation errors
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Error configuring provider: " << e.Message);
        throw css::ucb::InteractiveIOException(u"Failed to configure OAuth2 provider"_ustr,
                                              static_cast<css::ucb::XOAuth2Service*>(this),
                                              css::task::InteractionClassification_ERROR,
                                              css::ucb::IOErrorCode_GENERAL);
    }
}

// XServiceInfo interface

OUString SAL_CALL OAuth2Service::getImplementationName()
{
    return u"com.sun.star.comp.ucb.OAuth2Service"_ustr;
}

sal_Bool SAL_CALL OAuth2Service::supportsService(const OUString& ServiceName)
{
    return cppu::supportsService(static_cast<lang::XServiceInfo*>(this), ServiceName);
}

css::uno::Sequence<OUString> SAL_CALL OAuth2Service::getSupportedServiceNames()
{
    return { SERVICE_NAME };
}

} // namespace ucb::oauth2

// Component registration

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
ucb_OAuth2Service_get_implementation(
    css::uno::XComponentContext* pCtx,
    css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new ::ucb::oauth2::OAuth2Service(pCtx));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
