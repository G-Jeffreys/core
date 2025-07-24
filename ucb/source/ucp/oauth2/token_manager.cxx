/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "token_manager.hxx"
#include "oauth2_flow.hxx"
#include "config_manager.hxx"

#include <com/sun/star/task/PasswordContainer.hpp>
#include <com/sun/star/task/XInteractionHandler.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/beans/NamedValue.hpp>
#include <comphelper/processfactory.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <sal/log.hxx>
#include <rtl/ustrbuf.hxx>
#include <tools/datetime.hxx>
#include <chrono>

using namespace css;

namespace ucb::oauth2 {

constexpr OUStringLiteral SERVICE_NAME = u"com.sun.star.ucb.TokenManager";

TokenManager::TokenManager(const uno::Reference<uno::XComponentContext>& xContext)
    : m_xContext(xContext)
{
    SAL_INFO("ucb.ucp.oauth2", "Enhanced TokenManager created with automatic refresh support");

    try
    {
        // Initialize password container for secure storage
        m_xPasswordContainer = task::PasswordContainer::create(m_xContext);

        // Initialize configuration manager for OAuth2 settings
        m_xConfigManager.set(static_cast<css::ucb::XOAuth2Configuration*>(new ConfigurationManager(m_xContext)), uno::UNO_SET_THROW);
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to initialize TokenManager: " << e.Message);
    }
}

TokenManager::~TokenManager()
{
    SAL_INFO("ucb.ucp.oauth2", "Enhanced TokenManager destroyed");
}

OUString TokenManager::generateTokenKey(const OUString& sProviderUrl, const OUString& sUsername) const
{
    return sProviderUrl + u":" + sUsername;
}

bool TokenManager::isTokenExpiredHelper(const css::util::DateTime& aExpiryTime) const
{
    css::util::DateTime aCurrentTime = getCurrentDateTime();
    return getTimeDifferenceMinutes(aCurrentTime, aExpiryTime) <= 0;
}

bool TokenManager::shouldRefreshToken(const css::util::DateTime& aExpiryTime) const
{
    css::util::DateTime aCurrentTime = getCurrentDateTime();
    sal_Int64 nMinutesUntilExpiry = getTimeDifferenceMinutes(aCurrentTime, aExpiryTime);
    return nMinutesUntilExpiry <= REFRESH_THRESHOLD_MINUTES;
}

css::util::DateTime TokenManager::getCurrentDateTime() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::gmtime(&time_t);

    css::util::DateTime aDateTime;
    aDateTime.Year = tm->tm_year + 1900;
    aDateTime.Month = tm->tm_mon + 1;
    aDateTime.Day = tm->tm_mday;
    aDateTime.Hours = tm->tm_hour;
    aDateTime.Minutes = tm->tm_min;
    aDateTime.Seconds = tm->tm_sec;
    aDateTime.NanoSeconds = 0;
    aDateTime.IsUTC = true;

    return aDateTime;
}

sal_Int64 TokenManager::getTimeDifferenceMinutes(const css::util::DateTime& aTime1, const css::util::DateTime& aTime2) const
{
    // Convert DateTime to minutes since epoch for comparison
    auto toMinutes = [](const css::util::DateTime& dt) -> sal_Int64 {
        // Simple approximation - good enough for token expiry calculations
        sal_Int64 years = dt.Year - 1970;
        sal_Int64 days = years * 365 + years / 4;  // Rough leap year adjustment
        days += (dt.Month - 1) * 30;  // Rough month approximation
        days += dt.Day - 1;

        sal_Int64 minutes = days * 24 * 60;
        minutes += dt.Hours * 60;
        minutes += dt.Minutes;

        return minutes;
    };

    return toMinutes(aTime2) - toMinutes(aTime1);
}

OUString TokenManager::getPasswordContainerKey(const OUString& sProviderName, const OUString& sUsername) const
{
    return u"LibreOffice.OAuth2:" + sProviderName + u":" + sUsername;
}

void TokenManager::storeInPasswordContainer(const OUString& sKey, const OUString& sAccessToken, const OUString& sRefreshToken, const css::util::DateTime& aExpiryTime)
{
    if (!m_xPasswordContainer.is())
        return;

    try
    {
        // Convert expiry time to string
        OUStringBuffer aExpiryBuffer;
        aExpiryBuffer.append(static_cast<sal_Int32>(aExpiryTime.Year));
        aExpiryBuffer.append(u"-");
        aExpiryBuffer.append(static_cast<sal_Int32>(aExpiryTime.Month));
        aExpiryBuffer.append(u"-");
        aExpiryBuffer.append(static_cast<sal_Int32>(aExpiryTime.Day));
        aExpiryBuffer.append(u"T");
        aExpiryBuffer.append(static_cast<sal_Int32>(aExpiryTime.Hours));
        aExpiryBuffer.append(u":");
        aExpiryBuffer.append(static_cast<sal_Int32>(aExpiryTime.Minutes));
        aExpiryBuffer.append(u":");
        aExpiryBuffer.append(static_cast<sal_Int32>(aExpiryTime.Seconds));
        aExpiryBuffer.append(u"Z");

        OUString sExpiryTime = aExpiryBuffer.makeStringAndClear();

        // Encode token data as JSON-like string for storage
        OUStringBuffer aTokenDataBuffer;
        aTokenDataBuffer.append(u"{\"access_token\":\"");
        aTokenDataBuffer.append(sAccessToken);
        aTokenDataBuffer.append(u"\",\"refresh_token\":\"");
        aTokenDataBuffer.append(sRefreshToken);
        aTokenDataBuffer.append(u"\",\"expiry_time\":\"");
        aTokenDataBuffer.append(sExpiryTime);
        aTokenDataBuffer.append(u"\"}");

        OUString sTokenData = aTokenDataBuffer.makeStringAndClear();

        // Store as password list (password container expects sequence of strings)
        uno::Sequence<OUString> aPasswords{ sTokenData };

        // Store in password container - use a dummy username for OAuth2 tokens
        m_xPasswordContainer->addPersistent(sKey, u"oauth2_token"_ustr, aPasswords, uno::Reference<task::XInteractionHandler>());

        SAL_INFO("ucb.ucp.oauth2", "Token stored in password container: " << sKey);
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to store token in password container: " << e.Message);
    }
}

bool TokenManager::loadFromPasswordContainer(const OUString& sKey, TokenInfo& rTokenInfo)
{
    if (!m_xPasswordContainer.is())
        return false;

    try
    {
        // Try to find the stored token using findForName with dummy username
        auto aUrlRecord = m_xPasswordContainer->findForName(sKey, u"oauth2_token"_ustr, uno::Reference<task::XInteractionHandler>());

        if (!aUrlRecord.UserList.hasElements())
            return false;

        // Get the first (and only) user record
        const auto& aUserRecord = aUrlRecord.UserList[0];
        if (aUserRecord.Passwords.hasElements())
        {
            OUString sTokenData = aUserRecord.Passwords[0];

            // Parse JSON-like token data (simple parsing)
            sal_Int32 nAccessStart = sTokenData.indexOf(u"\"access_token\":\"") + 16;
            sal_Int32 nAccessEnd = sTokenData.indexOf(u"\"", nAccessStart);
            if (nAccessStart > 15 && nAccessEnd > nAccessStart)
            {
                rTokenInfo.sAccessToken = sTokenData.copy(nAccessStart, nAccessEnd - nAccessStart);
            }

            sal_Int32 nRefreshStart = sTokenData.indexOf(u"\"refresh_token\":\"") + 17;
            sal_Int32 nRefreshEnd = sTokenData.indexOf(u"\"", nRefreshStart);
            if (nRefreshStart > 16 && nRefreshEnd > nRefreshStart)
            {
                rTokenInfo.sRefreshToken = sTokenData.copy(nRefreshStart, nRefreshEnd - nRefreshStart);
            }

            sal_Int32 nExpiryStart = sTokenData.indexOf(u"\"expiry_time\":\"") + 15;
            sal_Int32 nExpiryEnd = sTokenData.indexOf(u"\"", nExpiryStart);
            if (nExpiryStart > 14 && nExpiryEnd > nExpiryStart)
            {
                OUString sExpiryTime = sTokenData.copy(nExpiryStart, nExpiryEnd - nExpiryStart);

                // Parse expiry time (simple ISO format)
                if (sExpiryTime.getLength() >= 19)  // YYYY-MM-DDTHH:MM:SS
                {
                    rTokenInfo.aExpiryTime.Year = OUString(sExpiryTime.subView(0, 4)).toInt32();
                    rTokenInfo.aExpiryTime.Month = OUString(sExpiryTime.subView(5, 2)).toInt32();
                    rTokenInfo.aExpiryTime.Day = OUString(sExpiryTime.subView(8, 2)).toInt32();
                    rTokenInfo.aExpiryTime.Hours = OUString(sExpiryTime.subView(11, 2)).toInt32();
                    rTokenInfo.aExpiryTime.Minutes = OUString(sExpiryTime.subView(14, 2)).toInt32();
                    rTokenInfo.aExpiryTime.Seconds = OUString(sExpiryTime.subView(17, 2)).toInt32();
                    rTokenInfo.aExpiryTime.NanoSeconds = 0;
                    rTokenInfo.aExpiryTime.IsUTC = true;
                }
            }

            // Update token status
            rTokenInfo.bExpiresSoon = shouldRefreshToken(rTokenInfo.aExpiryTime);
            rTokenInfo.bRefreshInProgress = false;
            rTokenInfo.nRefreshAttempts = 0;

            SAL_INFO("ucb.ucp.oauth2", "Token loaded from password container: " << sKey);
            return true;
        }
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to load token from password container: " << e.Message);
    }

    return false;
}

std::unique_ptr<OAuth2Flow> TokenManager::createOAuth2Flow()
{
    return std::make_unique<OAuth2Flow>(m_xContext);
}

bool TokenManager::performTokenRefresh(const OUString& sProviderUrl, const OUString& sUsername, TokenInfo& rTokenInfo)
{
    SAL_INFO("ucb.ucp.oauth2", "Performing token refresh for " << sProviderUrl << ":" << sUsername);

    if (rTokenInfo.bRefreshInProgress)
    {
        SAL_INFO("ucb.ucp.oauth2", "Token refresh already in progress");
        return false;
    }

    if (rTokenInfo.nRefreshAttempts >= MAX_REFRESH_ATTEMPTS)
    {
        SAL_WARN("ucb.ucp.oauth2", "Maximum refresh attempts exceeded");
        return false;
    }

    if (rTokenInfo.sRefreshToken.isEmpty())
    {
        SAL_WARN("ucb.ucp.oauth2", "No refresh token available");
        return false;
    }

    rTokenInfo.bRefreshInProgress = true;
    rTokenInfo.nRefreshAttempts++;

    try
    {
        // Get provider configuration
        css::ucb::OAuth2ProviderConfig aConfig;
        try
        {
            aConfig = m_xConfigManager->getProviderConfig(sProviderUrl);
        }
        catch (const uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.oauth2", "No configuration found for provider: " << sProviderUrl << " - " << e.Message);
            rTokenInfo.bRefreshInProgress = false;
            return false;
        }

        // Create OAuth2 flow and refresh token
        auto pFlow = createOAuth2Flow();
        auto aResult = pFlow->refreshAccessToken(aConfig, rTokenInfo.sRefreshToken, uno::Reference<css::ucb::XCommandEnvironment>());

        rTokenInfo.bRefreshInProgress = false;

        if (aResult.bSuccess)
        {
            // Update token information
            rTokenInfo.sAccessToken = aResult.sAccessToken;
            if (!aResult.sRefreshToken.isEmpty())
            {
                // Handle refresh token rotation
                rTokenInfo.sRefreshToken = aResult.sRefreshToken;
            }
            rTokenInfo.aExpiryTime = aResult.aExpiryTime;
            rTokenInfo.bExpiresSoon = false;
            rTokenInfo.nRefreshAttempts = 0;

            // Store updated tokens
            OUString sKey = getPasswordContainerKey(sProviderUrl, sUsername);
            storeInPasswordContainer(sKey, rTokenInfo.sAccessToken, rTokenInfo.sRefreshToken, rTokenInfo.aExpiryTime);

            SAL_INFO("ucb.ucp.oauth2", "Token refresh successful");
            return true;
        }
        else
        {
            SAL_WARN("ucb.ucp.oauth2", "Token refresh failed: " << aResult.sError);
            return false;
        }
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Token refresh exception: " << e.Message);
        rTokenInfo.bRefreshInProgress = false;
        return false;
    }
}

TokenInfo* TokenManager::getCachedTokenInfo(const OUString& sTokenKey) const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    auto it = m_aTokenCache.find(sTokenKey);
    return (it != m_aTokenCache.end()) ? it->second.get() : nullptr;
}

void TokenManager::setCachedTokenInfo(const OUString& sTokenKey, std::unique_ptr<TokenInfo> pTokenInfo)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aTokenCache[sTokenKey] = std::move(pTokenInfo);
}

void TokenManager::removeCachedTokenInfo(const OUString& sTokenKey)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aTokenCache.erase(sTokenKey);
}

// XTokenManager interface implementation

void SAL_CALL TokenManager::storeTokens(const OUString& sProviderUrl, const OUString& sUsername,
                                      const OUString& sAccessToken, const OUString& sRefreshToken,
                                      const css::util::DateTime& aExpiryTime)
{
    SAL_INFO("ucb.ucp.oauth2", "Storing token for " << sProviderUrl << ":" << sUsername);

    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);
    OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);

    // Store in password container
    storeInPasswordContainer(sPasswordKey, sAccessToken, sRefreshToken, aExpiryTime);

    // Update cache
    auto pTokenInfo = std::make_unique<TokenInfo>();
    pTokenInfo->sAccessToken = sAccessToken;
    pTokenInfo->sRefreshToken = sRefreshToken;
    pTokenInfo->aExpiryTime = aExpiryTime;
    pTokenInfo->bExpiresSoon = shouldRefreshToken(aExpiryTime);

    setCachedTokenInfo(sTokenKey, std::move(pTokenInfo));
}

OUString SAL_CALL TokenManager::getAccessToken(const OUString& sProviderUrl, const OUString& sUsername)
{
    SAL_INFO("ucb.ucp.oauth2", "Getting valid access token for " << sProviderUrl << ":" << sUsername);

    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);

    // Check cache first
    TokenInfo* pTokenInfo = getCachedTokenInfo(sTokenKey);
    if (!pTokenInfo)
    {
        // Load from password container
        auto pNewTokenInfo = std::make_unique<TokenInfo>();
        OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);

        if (!loadFromPasswordContainer(sPasswordKey, *pNewTokenInfo))
        {
            SAL_INFO("ucb.ucp.oauth2", "No token found for " << sProviderUrl << ":" << sUsername);
            return OUString();
        }

        pTokenInfo = pNewTokenInfo.get();
        setCachedTokenInfo(sTokenKey, std::move(pNewTokenInfo));
    }

    // Check if token is expired
    if (isTokenExpiredHelper(pTokenInfo->aExpiryTime))
    {
        SAL_INFO("ucb.ucp.oauth2", "Token expired, attempting refresh");
        if (!performTokenRefresh(sProviderUrl, sUsername, *pTokenInfo))
        {
            SAL_WARN("ucb.ucp.oauth2", "Token refresh failed");
            return OUString();
        }
    }
    // Check if token should be refreshed proactively
    else if (pTokenInfo->bExpiresSoon && !pTokenInfo->bRefreshInProgress)
    {
        SAL_INFO("ucb.ucp.oauth2", "Token expires soon, attempting proactive refresh");
        performTokenRefresh(sProviderUrl, sUsername, *pTokenInfo);
        // Continue with current token even if refresh fails
    }

    return pTokenInfo->sAccessToken;
}

OUString SAL_CALL TokenManager::getRefreshToken(const OUString& sProviderUrl, const OUString& sUsername)
{
    SAL_INFO("ucb.ucp.oauth2", "Getting refresh token for " << sProviderUrl << ":" << sUsername);

    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);

    // Check cache first
    TokenInfo* pTokenInfo = getCachedTokenInfo(sTokenKey);
    if (!pTokenInfo)
    {
        // Load from password container
        auto pNewTokenInfo = std::make_unique<TokenInfo>();
        OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);

        if (!loadFromPasswordContainer(sPasswordKey, *pNewTokenInfo))
        {
            return OUString();
        }

        pTokenInfo = pNewTokenInfo.get();
        setCachedTokenInfo(sTokenKey, std::move(pNewTokenInfo));
    }

    return pTokenInfo->sRefreshToken;
}

// Note: hasValidToken method removed as it's not in the IDL interface

sal_Bool SAL_CALL TokenManager::isTokenExpired(const OUString& sProviderUrl, const OUString& sUsername, sal_Int32 nMarginMinutes)
{
    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);

    // Check cache first
    TokenInfo* pTokenInfo = getCachedTokenInfo(sTokenKey);
    if (!pTokenInfo)
    {
        // Load from password container
        auto pNewTokenInfo = std::make_unique<TokenInfo>();
        OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);

        if (!loadFromPasswordContainer(sPasswordKey, *pNewTokenInfo))
        {
            return true; // No token = expired
        }

        pTokenInfo = pNewTokenInfo.get();
        setCachedTokenInfo(sTokenKey, std::move(pNewTokenInfo));
    }

    // Calculate expiry with margin
    css::util::DateTime aCurrentTime = getCurrentDateTime();
    sal_Int64 nMinutesUntilExpiry = getTimeDifferenceMinutes(aCurrentTime, pTokenInfo->aExpiryTime);
    return nMinutesUntilExpiry <= nMarginMinutes;
}

void SAL_CALL TokenManager::clearTokens(const OUString& sProviderUrl, const OUString& sUsername)
{
    SAL_INFO("ucb.ucp.oauth2", "Clearing token for " << sProviderUrl << ":" << sUsername);

    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);
    OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);

    // Remove from cache
    removeCachedTokenInfo(sTokenKey);

    // Remove from password container
    if (m_xPasswordContainer.is())
    {
        try
        {
            m_xPasswordContainer->removePersistent(sPasswordKey, u"oauth2_token"_ustr);
        }
        catch (const uno::Exception& e)
        {
            SAL_WARN("ucb.ucp.oauth2", "Failed to remove token from password container: " << e.Message);
        }
    }
}

// Note: clearAllTokens method removed as it's not in the IDL interface

css::uno::Sequence<OUString> SAL_CALL TokenManager::getStoredAccounts(const OUString& sProviderUrl)
{
    SAL_INFO("ucb.ucp.oauth2", "Getting authenticated accounts for " << sProviderUrl);

    std::vector<OUString> aAccounts;

    // Search through cache for matching provider
    {
        std::lock_guard<std::mutex> aGuard(m_aMutex);
        for (const auto& pair : m_aTokenCache)
        {
            const OUString& sKey = pair.first;
            sal_Int32 nColonPos = sKey.indexOf(':');
            if (nColonPos != -1)
            {
                OUString sProvider = sKey.copy(0, nColonPos);
                if (sProvider == sProviderUrl)
                {
                    OUString sUsername = sKey.copy(nColonPos + 1);
                    aAccounts.push_back(sUsername);
                }
            }
        }
    }

    // Convert to sequence
    uno::Sequence<OUString> aResult(aAccounts.size());
    std::copy(aAccounts.begin(), aAccounts.end(), aResult.getArray());

    return aResult;
}

void SAL_CALL TokenManager::updateAccessToken(const OUString& sProviderUrl, const OUString& sUsername,
                                             const OUString& sNewAccessToken, const css::util::DateTime& aNewExpiryTime)
{
    SAL_INFO("ucb.ucp.oauth2", "Updating access token for " << sProviderUrl << ":" << sUsername);

    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);

    TokenInfo* pTokenInfo = getCachedTokenInfo(sTokenKey);
    if (pTokenInfo)
    {
        pTokenInfo->sAccessToken = sNewAccessToken;
        pTokenInfo->aExpiryTime = aNewExpiryTime;
        pTokenInfo->bExpiresSoon = shouldRefreshToken(aNewExpiryTime);

        // Update in password container
        OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);
        storeInPasswordContainer(sPasswordKey, pTokenInfo->sAccessToken, pTokenInfo->sRefreshToken, aNewExpiryTime);
    }
}

// Note: refreshTokenIfNeeded and getTokenExpiryMinutes methods removed as they're not in the IDL interface

css::util::DateTime SAL_CALL TokenManager::getTokenExpiry(const OUString& sProviderUrl, const OUString& sUsername)
{
    OUString sTokenKey = generateTokenKey(sProviderUrl, sUsername);

    TokenInfo* pTokenInfo = getCachedTokenInfo(sTokenKey);
    if (!pTokenInfo)
    {
        // Load from password container
        auto pNewTokenInfo = std::make_unique<TokenInfo>();
        OUString sPasswordKey = getPasswordContainerKey(sProviderUrl, sUsername);

        if (!loadFromPasswordContainer(sPasswordKey, *pNewTokenInfo))
        {
            // Return invalid/empty datetime
            css::util::DateTime aEmpty;
            return aEmpty;
        }

        pTokenInfo = pNewTokenInfo.get();
        setCachedTokenInfo(sTokenKey, std::move(pNewTokenInfo));
    }

    return pTokenInfo->aExpiryTime;
}

// XServiceInfo interface

OUString SAL_CALL TokenManager::getImplementationName()
{
    return u"com.sun.star.comp.ucb.TokenManager"_ustr;
}

sal_Bool SAL_CALL TokenManager::supportsService(const OUString& ServiceName)
{
    return cppu::supportsService(static_cast<lang::XServiceInfo*>(this), ServiceName);
}

css::uno::Sequence<OUString> SAL_CALL TokenManager::getSupportedServiceNames()
{
    return { SERVICE_NAME };
}

// Factory method

css::uno::Reference<css::uno::XInterface> TokenManager::create(const css::uno::Reference<css::uno::XComponentContext>& xContext)
{
    return static_cast<css::ucb::XTokenManager*>(new TokenManager(xContext));
}

} // namespace ucb::oauth2

// Component registration

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
ucb_TokenManager_get_implementation(
    css::uno::XComponentContext* pCtx,
    css::uno::Sequence<css::uno::Any> const&)
{
    return cppu::acquire(new ::ucb::oauth2::TokenManager(pCtx));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
