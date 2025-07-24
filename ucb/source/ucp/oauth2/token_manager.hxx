/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/ucb/XTokenManager.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/task/XPasswordContainer2.hpp>
#include <com/sun/star/ucb/XOAuth2Configuration.hpp>
#include <cppuhelper/implbase.hxx>
#include <rtl/ustring.hxx>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace ucb::oauth2 {

// Forward declaration
class OAuth2Flow;

/**
 * Token information structure with enhanced metadata
 */
struct TokenInfo
{
    OUString sAccessToken;
    OUString sRefreshToken;
    css::util::DateTime aExpiryTime;
    bool bExpiresSoon;          // Token expires within refresh threshold
    bool bRefreshInProgress;    // Refresh operation in progress
    sal_Int32 nRefreshAttempts; // Number of refresh attempts made

    TokenInfo()
        : bExpiresSoon(false)
        , bRefreshInProgress(false)
        , nRefreshAttempts(0)
    {}
};

/**
 * TokenManager Implementation - Enhanced with automatic refresh
 *
 * Manages OAuth2 tokens with automatic refresh, rotation support,
 * and intelligent caching with expiry detection.
 */
class TokenManager : public cppu::WeakImplHelper<css::ucb::XTokenManager, css::lang::XServiceInfo>
{
private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    css::uno::Reference<css::task::XPasswordContainer2> m_xPasswordContainer;
    css::uno::Reference<css::ucb::XOAuth2Configuration> m_xConfigManager;

    // Token cache with thread safety
    mutable std::mutex m_aMutex;
    std::unordered_map<OUString, std::unique_ptr<TokenInfo>> m_aTokenCache;

    // Configuration
    static constexpr sal_Int32 REFRESH_THRESHOLD_MINUTES = 5;  // Refresh if expires within 5 minutes
    static constexpr sal_Int32 MAX_REFRESH_ATTEMPTS = 3;       // Maximum retry attempts

    // Internal helper methods
    OUString generateTokenKey(const OUString& sProviderUrl, const OUString& sUsername) const;
    bool isTokenExpiredHelper(const css::util::DateTime& aExpiryTime) const;
    bool shouldRefreshToken(const css::util::DateTime& aExpiryTime) const;
    css::util::DateTime getCurrentDateTime() const;
    sal_Int64 getTimeDifferenceMinutes(const css::util::DateTime& aTime1, const css::util::DateTime& aTime2) const;

    // Password container helpers
    OUString getPasswordContainerKey(const OUString& sProviderName, const OUString& sUsername) const;
    void storeInPasswordContainer(const OUString& sKey, const OUString& sAccessToken, const OUString& sRefreshToken, const css::util::DateTime& aExpiryTime);
    bool loadFromPasswordContainer(const OUString& sKey, TokenInfo& rTokenInfo);

    // Token refresh with OAuth2Flow
    bool performTokenRefresh(const OUString& sProviderUrl, const OUString& sUsername, TokenInfo& rTokenInfo);
    std::unique_ptr<OAuth2Flow> createOAuth2Flow();

    // Thread-safe cache operations
    TokenInfo* getCachedTokenInfo(const OUString& sTokenKey) const;
    void setCachedTokenInfo(const OUString& sTokenKey, std::unique_ptr<TokenInfo> pTokenInfo);
    void removeCachedTokenInfo(const OUString& sTokenKey);

public:
    explicit TokenManager(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    virtual ~TokenManager() override;

    // XTokenManager interface implementation
    virtual void SAL_CALL storeTokens(const OUString& sProviderUrl, const OUString& sUsername,
                                      const OUString& sAccessToken, const OUString& sRefreshToken,
                                      const css::util::DateTime& aExpiryTime) override;

    virtual OUString SAL_CALL getAccessToken(const OUString& sProviderUrl, const OUString& sUsername) override;
    virtual OUString SAL_CALL getRefreshToken(const OUString& sProviderUrl, const OUString& sUsername) override;
    virtual sal_Bool SAL_CALL isTokenExpired(const OUString& sProviderUrl, const OUString& sUsername, sal_Int32 nMarginMinutes) override;
    virtual css::util::DateTime SAL_CALL getTokenExpiry(const OUString& sProviderUrl, const OUString& sUsername) override;
    virtual void SAL_CALL clearTokens(const OUString& sProviderUrl, const OUString& sUsername) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getStoredAccounts(const OUString& sProviderUrl) override;
    virtual void SAL_CALL updateAccessToken(const OUString& sProviderUrl, const OUString& sUsername,
                                           const OUString& sNewAccessToken, const css::util::DateTime& aNewExpiryTime) override;

    // XServiceInfo interface
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;

    // Factory method
    static css::uno::Reference<css::uno::XInterface> create(const css::uno::Reference<css::uno::XComponentContext>& xContext);
};

} // namespace ucb::oauth2

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
