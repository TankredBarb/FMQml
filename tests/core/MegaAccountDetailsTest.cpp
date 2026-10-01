#include "MegaClient.h"

#include <QCoreApplication>
#include <cstdio>

namespace {
class AccountDetails final : public mega::MegaAccountDetails {
public:
    long long used = 0;
    long long maximum = 0;
    long long getStorageUsed() override { return used; }
    long long getStorageMax() override { return maximum; }
    int getNumActiveFeatures() const override { return 0; }
    mega::MegaAccountFeature *getActiveFeature(int) const override { return nullptr; }
    int64_t getSubscriptionLevel() const override { return 0; }
    mega::MegaStringIntegerMap *getSubscriptionFeatures() const override { return nullptr; }
    int getNumSubscriptions() const override { return 0; }
    mega::MegaAccountSubscription *getSubscription(int) const override { return nullptr; }
    int getNumPlans() const override { return 0; }
    mega::MegaAccountPlan *getPlan(int) const override { return nullptr; }
};

class Error final : public mega::MegaError {
public:
    explicit Error(int code) : MegaError(code) {}
};

class Request final : public mega::MegaRequest {
public:
    int type = TYPE_ACCOUNT_DETAILS;
    AccountDetails *details = nullptr;
    int getType() const override { return type; }
    const char *getLink() const override { return nullptr; }
    mega::MegaAccountDetails *getMegaAccountDetails() const override { return details; }
};

int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    MegaClient &client = MegaClient::instance();
    int authorizationEvents = 0;
    QObject::connect(&client, &MegaClientInterface::accountAuthorizationChanged,
                     &app, [&](bool, const QString &, const QString &) { ++authorizationEvents; });

    AccountDetails details;
    details.used = 100;
    details.maximum = 1000;
    Request request;
    request.details = &details;
    Error success(mega::MegaError::API_OK);
    Error error(mega::MegaError::API_EACCESS);
    // No SDK session or network is needed to deliver account completion data.
    auto *listener = static_cast<mega::MegaListener *>(&client);
    listener->onRequestFinish(nullptr, &request, &success);
    if (client.accountStorageUsedBytes() != 100 || client.accountStorageMaxBytes() != 1000)
        return fail("account details did not update cached storage");
    if (authorizationEvents != 0)
        return fail("account details incorrectly notified credential persistence");

    listener->onRequestFinish(nullptr, &request, &success);
    details.used = 200;
    details.maximum = 2000;
    listener->onRequestFinish(nullptr, &request, &success);
    if (client.accountStorageUsedBytes() != 200 || client.accountStorageMaxBytes() != 2000
            || authorizationEvents != 0)
        return fail("repeated or changed storage details affected authorization");

    details.used = 300;
    listener->onRequestFinish(nullptr, &request, &error);
    request.details = nullptr;
    listener->onRequestFinish(nullptr, &request, &success);
    if (client.accountStorageUsedBytes() != 200 || client.accountStorageMaxBytes() != 2000
            || authorizationEvents != 0)
        return fail("missing or failed details altered cached storage or authorization");

    request.type = mega::MegaRequest::TYPE_LOGIN;
    listener->onRequestFinish(nullptr, &request, &error);
    if (authorizationEvents != 1)
        return fail("authorization failure stopped notifying consumers");
    client.logoutAccount();
    if (authorizationEvents != 2 || client.accountStorageUsedBytes() != -1
            || client.accountStorageMaxBytes() != -1)
        return fail("logout did not clear storage and notify authorization consumers");
    std::puts("account refresh preserves authorization; failure and logout still notify");
    return 0;
}
