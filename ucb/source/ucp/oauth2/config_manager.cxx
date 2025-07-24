/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config_manager.hxx"

#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/container/NoSuchElementException.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <comphelper/processfactory.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <sal/log.hxx>
#include <config_oauth2.h>
#include <comphelper/sequence.hxx>

using namespace css;

namespace ucb::oauth2 {

ConfigurationManager::ConfigurationManager(const uno::Reference<uno::XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bCacheLoaded(false)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager constructed - runtime OAuth2 configuration ready");
}

void ConfigurationManager::loadConfiguration() const
{
    if (m_bCacheLoaded)
        return;

    SAL_INFO("ucb.ucp.oauth2", "Loading OAuth2 provider configurations from registry");

    try
    {
        // For Phase 1, initialize with built-in defaults
        // Phase 2 will add full registry integration
        const_cast<ConfigurationManager*>(this)->initializeDefaultConfigurations();
        m_bCacheLoaded = true;

        SAL_INFO("ucb.ucp.oauth2", "OAuth2 configurations loaded successfully");
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to load OAuth2 configurations: " << e.Message);
        // Continue with empty configuration rather than failing
        m_bCacheLoaded = true;
    }
}

void ConfigurationManager::saveConfiguration()
{
    SAL_INFO("ucb.ucp.oauth2", "Saving OAuth2 provider configurations to registry");

    try
    {
        // Phase 1: Configuration is in-memory only
        // Phase 2 will add persistent registry storage
        SAL_INFO("ucb.ucp.oauth2", "Configuration saved to in-memory cache (Phase 1)");
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("ucb.ucp.oauth2", "Failed to save OAuth2 configurations: " << e.Message);
        throw;
    }
}

void ConfigurationManager::initializeDefaultConfigurations()
{
    SAL_INFO("ucb.ucp.oauth2", "Initializing default OAuth2 provider configurations - Enhanced Phase 3");

    // Google Drive configuration - Enhanced for Phase 3 testing
    {
        css::ucb::OAuth2ProviderConfig aConfig;
        aConfig.sDisplayName = u"Google Drive"_ustr;
        aConfig.sBaseUrl = u"https://www.googleapis.com/drive/v3"_ustr;
        aConfig.sAuthUrl = u"https://accounts.google.com/o/oauth2/v2/auth"_ustr;
        aConfig.sTokenUrl = u"https://oauth2.googleapis.com/token"_ustr;
        aConfig.sScope = u"https://www.googleapis.com/auth/drive"_ustr;
        aConfig.sClientId = u""_ustr; // To be configured by user
        aConfig.sClientSecret = u""_ustr; // To be configured by user
        aConfig.sRedirectUri = u"http://localhost:8080/oauth2callback"_ustr;
        aConfig.bUsePKCE = true;
        aConfig.bSupportDeviceFlow = false;

        // Use provider name instead of base URL as key for better identification
        m_aConfigCache[u"GoogleDrive"_ustr] = aConfig;
        SAL_INFO("ucb.ucp.oauth2", "Added enhanced Google Drive configuration for Phase 3 testing");
    }

    // OneDrive configuration - Enhanced for Phase 3 testing
    {
        css::ucb::OAuth2ProviderConfig aConfig;
        aConfig.sDisplayName = u"Microsoft OneDrive"_ustr;
        aConfig.sBaseUrl = u"https://graph.microsoft.com/v1.0"_ustr;
        aConfig.sAuthUrl = u"https://login.microsoftonline.com/common/oauth2/v2.0/authorize"_ustr;
        aConfig.sTokenUrl = u"https://login.microsoftonline.com/common/oauth2/v2.0/token"_ustr;
        aConfig.sScope = u"Files.ReadWrite offline_access"_ustr;
        aConfig.sClientId = u""_ustr; // To be configured by user
        aConfig.sClientSecret = u""_ustr; // To be configured by user
        aConfig.sRedirectUri = u"http://localhost:8080/oauth2callback"_ustr;
        aConfig.bUsePKCE = true;
        aConfig.bSupportDeviceFlow = false;

        // Use provider name as key
        m_aConfigCache[u"OneDrive"_ustr] = aConfig;
        SAL_INFO("ucb.ucp.oauth2", "Added enhanced OneDrive configuration for Phase 3 testing");
    }

    // Test provider configuration for development
    {
        css::ucb::OAuth2ProviderConfig aConfig;
        aConfig.sDisplayName = u"Test OAuth2 Provider"_ustr;
        aConfig.sBaseUrl = u"https://example.com/api"_ustr;
        aConfig.sAuthUrl = u"https://example.com/oauth2/authorize"_ustr;
        aConfig.sTokenUrl = u"https://example.com/oauth2/token"_ustr;
        aConfig.sScope = u"read write"_ustr;
        aConfig.sClientId = u"test-client-id"_ustr;
        aConfig.sClientSecret = u"test-client-secret"_ustr;
        aConfig.sRedirectUri = u"http://localhost:8080/oauth2callback"_ustr;
        aConfig.bUsePKCE = true;
        aConfig.bSupportDeviceFlow = false;

        m_aConfigCache[u"TestProvider"_ustr] = aConfig;
        SAL_INFO("ucb.ucp.oauth2", "Added test OAuth2 provider configuration for development");
    }

    SAL_INFO("ucb.ucp.oauth2", "Initialized " << m_aConfigCache.size() << " default OAuth2 provider configurations");
}

bool ConfigurationManager::validateConfigurationInternal(
    const css::ucb::OAuth2ProviderConfig& aConfig,
    OUString& rErrorMessage) const
{
    SAL_INFO("ucb.ucp.oauth2", "Validating OAuth2 configuration for " << aConfig.sDisplayName);

    // Check required fields
    if (aConfig.sBaseUrl.isEmpty())
    {
        rErrorMessage = u"Base URL is required"_ustr;
        return false;
    }

    if (aConfig.sAuthUrl.isEmpty())
    {
        rErrorMessage = u"Authorization URL is required"_ustr;
        return false;
    }

    if (aConfig.sTokenUrl.isEmpty())
    {
        rErrorMessage = u"Token URL is required"_ustr;
        return false;
    }

    if (aConfig.sClientId.isEmpty())
    {
        rErrorMessage = u"Client ID is required"_ustr;
        return false;
    }

    // Validate URL formats
    if (!aConfig.sBaseUrl.startsWith("https://"))
    {
        rErrorMessage = u"Base URL must use HTTPS"_ustr;
        return false;
    }

    if (!aConfig.sAuthUrl.startsWith("https://"))
    {
        rErrorMessage = u"Authorization URL must use HTTPS"_ustr;
        return false;
    }

    if (!aConfig.sTokenUrl.startsWith("https://"))
    {
        rErrorMessage = u"Token URL must use HTTPS"_ustr;
        return false;
    }

    SAL_INFO("ucb.ucp.oauth2", "OAuth2 configuration validation passed");
    return true;
}

void ConfigurationManager::setProviderConfig(
    const OUString& sProviderUrl,
    const css::ucb::OAuth2ProviderConfig& aConfig)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::setProviderConfig() called for " << sProviderUrl);

    if (sProviderUrl.isEmpty())
    {
        throw lang::IllegalArgumentException(
            u"Provider URL cannot be empty"_ustr,
            static_cast<cppu::OWeakObject*>(this), 0);
    }

    // Validate configuration
    OUString sErrorMessage;
    if (!validateConfigurationInternal(aConfig, sErrorMessage))
    {
        SAL_WARN("ucb.ucp.oauth2", "Invalid OAuth2 configuration: " << sErrorMessage);
        throw lang::IllegalArgumentException(
            u"Invalid OAuth2 configuration: "_ustr + sErrorMessage,
            static_cast<cppu::OWeakObject*>(this), 1);
    }

    loadConfiguration(); // Ensure cache is loaded

    // Store in cache
    m_aConfigCache[sProviderUrl] = aConfig;

    // Save to persistent storage
    saveConfiguration();

    SAL_INFO("ucb.ucp.oauth2", "OAuth2 provider configuration set successfully for " << sProviderUrl);
}

bool ConfigurationManager::getProviderConfig(const OUString& sProviderName, css::ucb::OAuth2ProviderConfig& rConfig)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::getProviderConfig() called for " << sProviderName);

    if (sProviderName.isEmpty())
    {
        SAL_WARN("ucb.ucp.oauth2", "Empty provider name in getProviderConfig");
        return false;
    }

    loadConfiguration(); // Ensure cache is loaded

    auto it = m_aConfigCache.find(sProviderName);
    if (it != m_aConfigCache.end())
    {
        rConfig = it->second;
        SAL_INFO("ucb.ucp.oauth2", "OAuth2 configuration found for " << sProviderName);
        return true;
    }

    SAL_INFO("ucb.ucp.oauth2", "No OAuth2 configuration found for " << sProviderName);
    return false;
}

css::ucb::OAuth2ProviderConfig ConfigurationManager::getProviderConfig(const OUString& sProviderUrl)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::getProviderConfig() called for " << sProviderUrl);

    if (sProviderUrl.isEmpty())
    {
        throw lang::IllegalArgumentException(
            u"Provider URL cannot be empty"_ustr,
            static_cast<cppu::OWeakObject*>(this), 0);
    }

    loadConfiguration(); // Ensure cache is loaded

    auto it = m_aConfigCache.find(sProviderUrl);
    if (it != m_aConfigCache.end())
    {
        SAL_INFO("ucb.ucp.oauth2", "OAuth2 configuration found for " << sProviderUrl);
        return it->second;
    }

    SAL_INFO("ucb.ucp.oauth2", "No OAuth2 configuration found for " << sProviderUrl);
    throw container::NoSuchElementException(
        u"No OAuth2 configuration found for provider: "_ustr + sProviderUrl,
        static_cast<cppu::OWeakObject*>(this));
}

sal_Bool ConfigurationManager::hasProviderConfig(const OUString& sProviderUrl)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::hasProviderConfig() called for " << sProviderUrl);

    if (sProviderUrl.isEmpty())
    {
        return false;
    }

    loadConfiguration(); // Ensure cache is loaded

    bool bHasConfig = m_aConfigCache.find(sProviderUrl) != m_aConfigCache.end();
    SAL_INFO("ucb.ucp.oauth2", "Provider configuration " << (bHasConfig ? "found" : "not found") << " for " << sProviderUrl);

    return bHasConfig;
}

void ConfigurationManager::removeProviderConfig(const OUString& sProviderUrl)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::removeProviderConfig() called for " << sProviderUrl);

    if (sProviderUrl.isEmpty())
    {
        return;
    }

    loadConfiguration(); // Ensure cache is loaded

    auto it = m_aConfigCache.find(sProviderUrl);
    if (it != m_aConfigCache.end())
    {
        m_aConfigCache.erase(it);
        saveConfiguration();
        SAL_INFO("ucb.ucp.oauth2", "OAuth2 provider configuration removed for " << sProviderUrl);
    }
    else
    {
        SAL_INFO("ucb.ucp.oauth2", "No OAuth2 configuration to remove for " << sProviderUrl);
    }
}

uno::Sequence<OUString> ConfigurationManager::getConfiguredProviders()
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::getConfiguredProviders() called");

    loadConfiguration(); // Ensure cache is loaded

    std::vector<OUString> aProviders;
    for (const auto& rPair : m_aConfigCache)
    {
        aProviders.push_back(rPair.first);
    }

    SAL_INFO("ucb.ucp.oauth2", "Found " << aProviders.size() << " configured OAuth2 providers");
    return comphelper::containerToSequence(aProviders);
}

uno::Sequence<beans::PropertyValue> ConfigurationManager::getConfiguredProvidersWithNames()
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::getConfiguredProvidersWithNames() called");

    loadConfiguration(); // Ensure cache is loaded

    std::vector<beans::PropertyValue> aProviders;
    for (const auto& rPair : m_aConfigCache)
    {
        beans::PropertyValue aProp;
        aProp.Name = rPair.first; // Provider URL
        aProp.Value <<= rPair.second.sDisplayName; // Display name
        aProviders.push_back(aProp);
    }

    SAL_INFO("ucb.ucp.oauth2", "Found " << aProviders.size() << " configured OAuth2 providers with names");
    return comphelper::containerToSequence(aProviders);
}

void ConfigurationManager::setDefaultConfigurations()
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::setDefaultConfigurations() called");

    // Clear existing cache
    m_aConfigCache.clear();
    m_bCacheLoaded = false;

    // Reload defaults
    loadConfiguration();

    // Save to persistent storage
    saveConfiguration();

    SAL_INFO("ucb.ucp.oauth2", "Default OAuth2 configurations restored");
}

sal_Bool ConfigurationManager::validateConfiguration(const css::ucb::OAuth2ProviderConfig& aConfig)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::validateConfiguration() called");

    OUString sErrorMessage;
    bool bValid = validateConfigurationInternal(aConfig, sErrorMessage);

    if (!bValid)
    {
        SAL_INFO("ucb.ucp.oauth2", "Configuration validation failed: " << sErrorMessage);
    }
    else
    {
        SAL_INFO("ucb.ucp.oauth2", "Configuration validation passed");
    }

    return bValid;
}

void ConfigurationManager::importConfigurations(const OUString& sConfigData)
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::importConfigurations() called");

    if (sConfigData.isEmpty())
    {
        throw lang::IllegalArgumentException(
            u"Configuration data cannot be empty"_ustr,
            static_cast<cppu::OWeakObject*>(this), 0);
    }

    // Phase 1: Placeholder implementation
    // Phase 2 will add full JSON/XML parsing
    SAL_INFO("ucb.ucp.oauth2", "Configuration import not yet implemented in Phase 1");
    throw lang::IllegalArgumentException(
        u"Configuration import not yet implemented in Phase 1"_ustr,
        static_cast<cppu::OWeakObject*>(this), 0);
}

OUString ConfigurationManager::exportConfigurations()
{
    SAL_INFO("ucb.ucp.oauth2", "ConfigurationManager::exportConfigurations() called");

    loadConfiguration(); // Ensure cache is loaded

    // Phase 1: Simple export format
    // Phase 2 will add full JSON export
    OUString sResult = u"# OAuth2 Provider Configurations (Phase 1 format)\n"_ustr;

    for (const auto& rPair : m_aConfigCache)
    {
        const css::ucb::OAuth2ProviderConfig& aConfig = rPair.second;
        sResult += u"Provider: "_ustr + aConfig.sDisplayName + u"\n"_ustr;
        sResult += u"BaseURL: "_ustr + aConfig.sBaseUrl + u"\n"_ustr;
        sResult += u"AuthURL: "_ustr + aConfig.sAuthUrl + u"\n"_ustr;
        sResult += u"TokenURL: "_ustr + aConfig.sTokenUrl + u"\n"_ustr;
        sResult += u"Scope: "_ustr + aConfig.sScope + u"\n"_ustr;
        sResult += u"---\n"_ustr;
    }

    SAL_INFO("ucb.ucp.oauth2", "Exported " << m_aConfigCache.size() << " OAuth2 configurations");
    return sResult;
}

// XServiceInfo interface implementation
OUString ConfigurationManager::getImplementationName()
{
    return u"com.sun.star.comp.ucb.ConfigurationManager"_ustr;
}

sal_Bool ConfigurationManager::supportsService(const OUString& ServiceName)
{
    return cppu::supportsService(this, ServiceName);
}

uno::Sequence<OUString> ConfigurationManager::getSupportedServiceNames()
{
    return { u"com.sun.star.ucb.ConfigurationManager"_ustr };
}

} // namespace ucb::oauth2

// Service factory function
extern "C" SAL_DLLPUBLIC_EXPORT uno::XInterface*
ucb_ConfigurationManager_get_implementation(
    uno::XComponentContext* pCtx,
    uno::Sequence<uno::Any> const& /*rSeq*/)
{
    SAL_INFO("ucb.ucp.oauth2", "Creating ConfigurationManager implementation");
    return cppu::acquire(new ::ucb::oauth2::ConfigurationManager(pCtx));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
