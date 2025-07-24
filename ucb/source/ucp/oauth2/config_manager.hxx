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
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/ucb/XOAuth2Configuration.hpp>
#include <com/sun/star/util/XChangesBatch.hpp>
#include <cppuhelper/implbase.hxx>
#include <memory>
#include <map>

namespace ucb::oauth2 {

/**
 * ConfigurationManager Implementation
 *
 * Manages runtime configuration of OAuth2 providers, allowing dynamic
 * setup of cloud storage authentication without requiring LibreOffice rebuilds.
 *
 * Features:
 * - Runtime OAuth2 provider configuration
 * - Secure credential storage integration
 * - Built-in defaults for major providers (Google Drive, OneDrive)
 * - Configuration validation and error handling
 * - Import/export capabilities for enterprise deployment
 */
class ConfigurationManager : public cppu::WeakImplHelper<
    css::ucb::XOAuth2Configuration,
    css::lang::XServiceInfo>
{
private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;

    // Cache for configuration data
    mutable std::map<OUString, css::ucb::OAuth2ProviderConfig> m_aConfigCache;
    mutable bool m_bCacheLoaded;

    /**
     * Load configuration from LibreOffice registry
     */
    void loadConfiguration() const;

    /**
     * Save configuration to LibreOffice registry
     */
    void saveConfiguration();

    /**
     * Get configuration access for reading/writing
     */
    css::uno::Reference<css::uno::XInterface> getConfigurationAccess(bool bWritable = false);

    /**
     * Initialize default provider configurations
     */
    void initializeDefaultConfigurations();

    /**
     * Validate OAuth2 configuration parameters
     */
    bool validateConfigurationInternal(const css::ucb::OAuth2ProviderConfig& aConfig, OUString& rErrorMessage) const;

    /**
     * Convert PropertyValue sequence to OAuth2ProviderConfig
     */
    css::ucb::OAuth2ProviderConfig propertyValuesToConfig(
        const css::uno::Sequence<css::beans::PropertyValue>& aProperties) const;

    /**
     * Convert OAuth2ProviderConfig to PropertyValue sequence
     */
    css::uno::Sequence<css::beans::PropertyValue> configToPropertyValues(
        const css::ucb::OAuth2ProviderConfig& aConfig) const;

    /**
     * Get configuration key for provider
     */
    OUString getProviderConfigKey(const OUString& sProviderUrl) const;

public:
    explicit ConfigurationManager(const css::uno::Reference<css::uno::XComponentContext>& xContext);

    // XOAuth2Configuration interface
    virtual void SAL_CALL setProviderConfig(const OUString& sProviderUrl,
                                           const css::ucb::OAuth2ProviderConfig& aConfig) override;

    virtual css::ucb::OAuth2ProviderConfig SAL_CALL getProviderConfig(const OUString& sProviderUrl) override;

    // Enhanced method for OAuth2Service integration
    virtual bool getProviderConfig(const OUString& sProviderName, css::ucb::OAuth2ProviderConfig& rConfig);

    virtual sal_Bool SAL_CALL hasProviderConfig(const OUString& sProviderUrl) override;

    virtual void SAL_CALL removeProviderConfig(
        const OUString& sProviderUrl) override;

    virtual css::uno::Sequence<OUString> SAL_CALL getConfiguredProviders() override;

    virtual css::uno::Sequence<css::beans::PropertyValue> SAL_CALL getConfiguredProvidersWithNames() override;

    virtual void SAL_CALL setDefaultConfigurations() override;

    virtual sal_Bool SAL_CALL validateConfiguration(
        const css::ucb::OAuth2ProviderConfig& aConfig) override;

    virtual void SAL_CALL importConfigurations(
        const OUString& sConfigData) override;

    virtual OUString SAL_CALL exportConfigurations() override;

    // XServiceInfo interface
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
};

} // namespace ucb::oauth2

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
