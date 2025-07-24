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
#include <com/sun/star/ucb/XOAuth2Service.hpp>
#include <com/sun/star/ucb/XTokenManager.hpp>
#include <com/sun/star/ucb/XOAuth2Configuration.hpp>
#include <cppuhelper/implbase.hxx>
#include <memory>

namespace ucb::oauth2 {

/**
 * OAuth2Service Implementation
 *
 * Central OAuth2 authentication service that coordinates token management
 * and configuration for cloud storage providers.
 *
 * Features:
 * - Automatic token refresh before expiry
 * - Multi-account support per provider
 * - Runtime OAuth configuration
 * - Comprehensive error handling and logging
 */
class OAuth2Service : public cppu::WeakImplHelper<
    css::ucb::XOAuth2Service,
    css::lang::XServiceInfo>
{
private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    css::uno::Reference<css::ucb::XTokenManager> m_xTokenManager;
    css::uno::Reference<css::ucb::XOAuth2Configuration> m_xConfigManager;

    // Feature flag for gradual rollout - Phase 1 implementation
    bool m_bEnabled;

    /**
     * Get TokenManager service instance
     */
    css::uno::Reference<css::ucb::XTokenManager> getTokenManager();

    /**
     * Get ConfigurationManager service instance
     */
    css::uno::Reference<css::ucb::XOAuth2Configuration> getConfigManager();

    /**
     * Validate provider URL and username parameters
     */
    void validateParameters(const OUString& sProviderUrl, const OUString& sUsername);

    /**
     * Perform OAuth2 authentication flow
     */
    OUString performAuthentication(
        const OUString& sProviderUrl,
        const OUString& sUsername,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment);

    /**
     * Refresh expired OAuth tokens
     */
    OUString performTokenRefresh(
        const OUString& sProviderUrl,
        const OUString& sUsername,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment);

public:
    explicit OAuth2Service(const css::uno::Reference<css::uno::XComponentContext>& xContext);

    // XOAuth2Service interface
    virtual OUString SAL_CALL authenticate(
        const OUString& sProviderUrl,
        const OUString& sUsername,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment) override;

    virtual OUString SAL_CALL getValidAccessToken(
        const OUString& sProviderUrl,
        const OUString& sUsername,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment) override;

    virtual sal_Bool SAL_CALL isAuthenticated(
        const OUString& sProviderUrl,
        const OUString& sUsername) override;

    virtual OUString SAL_CALL refreshTokens(
        const OUString& sProviderUrl,
        const OUString& sUsername,
        const css::uno::Reference<css::ucb::XCommandEnvironment>& xEnvironment) override;

    virtual void SAL_CALL revokeAuthentication(
        const OUString& sProviderUrl,
        const OUString& sUsername) override;

    virtual css::uno::Sequence<OUString> SAL_CALL getAuthenticatedAccounts(
        const OUString& sProviderUrl) override;

    virtual void SAL_CALL configureProvider(
        const OUString& sProviderUrl,
        const css::uno::Sequence<css::beans::PropertyValue>& aConfiguration) override;

    // XServiceInfo interface
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
};

} // namespace ucb::oauth2

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
