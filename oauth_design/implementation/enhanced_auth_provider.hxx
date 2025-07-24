/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Enhanced AuthProvider for OAuth2 Modernization
 * Maintains 100% backward compatibility while adding modern OAuth2 features
 */

#pragma once

#include <libcmis/libcmis.hxx>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/XOAuth2Service.hpp>
#include <com/sun/star/ucb/XTokenManager.hpp>
#include <com/sun/star/ucb/XOAuth2Configuration.hpp>
#include <cppuhelper/weakref.hxx>
#include <memory>
#include <mutex>

namespace cmis
{
    /**
     * Enhanced AuthProvider with modern OAuth2 capabilities
     *
     * This class extends the existing AuthProvider to support:
     * - Automatic token refresh
     * - Runtime OAuth configuration
     * - Multi-account support
     * - PKCE and device flow
     * - Enhanced error handling
     *
     * Maintains 100% backward compatibility with existing CMIS integration.
     */
    class EnhancedAuthProvider : public libcmis::AuthProvider
    {
    private:
        // Existing compatibility fields
        const css::uno::Reference<css::ucb::XCommandEnvironment>& m_xEnv;
        static css::uno::WeakReference<css::ucb::XCommandEnvironment> sm_xEnv;
        OUString m_sUrl;
        OUString m_sBindingUrl;

        // New OAuth2 service references
        css::uno::Reference<css::ucb::XOAuth2Service> m_xOAuth2Service;
        css::uno::Reference<css::ucb::XTokenManager> m_xTokenManager;
        css::uno::Reference<css::ucb::XOAuth2Configuration> m_xConfigManager;

        // Thread safety for token operations
        mutable std::mutex m_aMutex;

        // Enhanced error tracking
        mutable sal_Int32 m_nConsecutiveFailures;
        mutable std::chrono::steady_clock::time_point m_aLastRefreshAttempt;

    public:
        /**
         * Constructor - maintains backward compatibility
         */
        EnhancedAuthProvider(
            const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnv,
            OUString sUrl,
            OUString sBindingUrl);

        /**
         * Existing interface - backward compatibility
         */
        bool authenticationQuery(std::string& username, std::string& password) override;

        /**
         * Enhanced token management with automatic refresh
         */
        std::string getRefreshToken(std::string& rUsername) override;
        bool storeRefreshToken(const std::string& username,
                              const std::string& password,
                              const std::string& refreshToken) override;

        /**
         * Enhanced web auth with PKCE support
         */
        static char* copyWebAuthCodeFallback(const char* url,
                                           const char* username,
                                           const char* password);

        /**
         * New methods for enhanced OAuth2 functionality
         */

        /**
         * Get valid access token with automatic refresh
         * This is the key method that prevents token expiry crashes
         */
        std::string getValidAccessToken(const std::string& username);

        /**
         * Check if user is authenticated
         */
        bool isAuthenticated(const std::string& username) const;

        /**
         * Force token refresh
         */
        std::string refreshAccessToken(const std::string& username);

        /**
         * Configure OAuth2 provider at runtime
         */
        void configureOAuth2Provider(const css::ucb::OAuth2ProviderConfig& config);

        /**
         * Get OAuth2 configuration for current provider
         */
        css::ucb::OAuth2ProviderConfig getOAuth2Configuration() const;

        /**
         * Revoke authentication
         */
        void revokeAuthentication(const std::string& username);

        /**
         * Handle OAuth2 errors gracefully
         */
        void handleOAuth2Error(const std::exception& error,
                              const std::string& context) const;

        /**
         * Background token refresh (called by timer)
         */
        void performBackgroundRefresh();

        // Static methods for compatibility
        static void setXEnv(const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnv);
        static css::uno::Reference<css::ucb::XCommandEnvironment> getXEnv();

    private:
        /**
         * Initialize OAuth2 services
         */
        void initializeOAuth2Services();

        /**
         * Determine if provider supports OAuth2
         */
        bool isOAuth2Provider() const;

        /**
         * Get provider configuration from runtime config or fallback to defaults
         */
        css::ucb::OAuth2ProviderConfig getProviderConfiguration() const;

        /**
         * Handle authentication failure with retry logic
         */
        void handleAuthenticationFailure(const std::string& error);

        /**
         * Check if token needs refresh (5-minute margin)
         */
        bool shouldRefreshToken(const std::string& username) const;

        /**
         * Perform PKCE-enhanced OAuth flow
         */
        std::string performPKCEFlow(const css::ucb::OAuth2ProviderConfig& config,
                                   const std::string& username);

        /**
         * Perform device flow for headless environments
         */
        std::string performDeviceFlow(const css::ucb::OAuth2ProviderConfig& config,
                                     const std::string& username);

        /**
         * Validate and sanitize OAuth2 tokens
         */
        bool validateTokenFormat(const std::string& token) const;

        /**
         * Log OAuth2 operations for debugging
         */
        void logOAuth2Operation(const std::string& operation,
                               const std::string& result) const;
    };

    /**
     * Factory function for creating enhanced auth provider
     * Maintains compatibility with existing CMIS code
     */
    std::unique_ptr<libcmis::AuthProvider> createEnhancedAuthProvider(
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnv,
        const OUString& sUrl,
        const OUString& sBindingUrl);

} // namespace cmis
