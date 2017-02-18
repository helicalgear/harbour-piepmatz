#include "accountmodel.h"
#include "o1twitterglobals.h"
#include "o0globals.h"
#include "o0settingsstore.h"

#include <QFile>
#include <QUuid>

AccountModel::AccountModel()
{
    networkConfigurationManager = new QNetworkConfigurationManager(this);
    obtainEncryptionKey();

    o1 = new O1Twitter(this);
    O0SettingsStore *settings = new O0SettingsStore(encryptionKey);
    o1->setStore(settings);
    o1->setClientId(TWITTER_CLIENT_ID);
    o1->setClientSecret(TWITTER_CLIENT_SECRET);
    connect(o1, SIGNAL(pinRequestError(QString)), this, SLOT(handlePinRequestError(QString)));
    connect(o1, SIGNAL(pinRequestSuccessful(QUrl)), this, SLOT(handlePinRequestSuccessful(QUrl)));
    connect(o1, SIGNAL(linkingFailed()), this, SLOT(handleLinkingFailed()));
    connect(o1, SIGNAL(linkingSucceeded()), this, SLOT(handleLinkingSucceeded()));

    manager = new QNetworkAccessManager(this);
    requestor = new O1Requestor(manager, o1, this);
}
QVariant AccountModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid()) {
        return QVariant();
    }
    if(role == Qt::DisplayRole) {
        QMap<QString,QVariant> resultMap;
        Account* account = availableAccounts.value(index.row());
        //resultMap.insert("id", QVariant(availableAccounts->getId()));
        return QVariant(resultMap);
    }
    return QVariant();
}

void AccountModel::obtainPinUrl()
{
    if (networkConfigurationManager->isOnline()) {
        o1->obtainPinUrl();
    } else {
        emit pinRequestError("I'm sorry, your device is offline!");
    }
}

void AccountModel::enterPin(const QString &pin)
{
    qDebug() << "PIN entered: " + pin;
    if (networkConfigurationManager->isOnline()) {
        o1->verifyPin(pin);
    } else {
        emit linkingFailed("I'm sorry, your device is offline!");
    }
}

bool AccountModel::isLinked()
{
    return o1->linked();
}

void AccountModel::verifyCredentials()
{
    qDebug() << "AccountModel::verifyCredentials";
    QUrl url = QUrl("https://api.twitter.com/1.1/account/verify_credentials.json");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, O2_MIME_TYPE_XFORM);
    QList<O0RequestParameter> requestParameters = QList<O0RequestParameter>();
    QNetworkReply *reply = requestor->get(request, requestParameters);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onVerificationError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(onVerificationFinished()));
}

void AccountModel::unlink()
{
    o1->unlink();
}

void AccountModel::handlePinRequestError(const QString &errorMessage)
{
    emit pinRequestError("I'm sorry, there was an error: " + errorMessage);
}

void AccountModel::handlePinRequestSuccessful(const QUrl &url)
{
    emit pinRequestSuccessful(url.toString());
}

void AccountModel::handleLinkingFailed()
{
    qDebug() << "Linking failed! :(";
    emit linkingFailed("Linking error");
}

void AccountModel::handleLinkingSucceeded()
{
    qDebug() << "Linking successful! :)";
    emit linkingSuccessful();
}

void AccountModel::obtainEncryptionKey()
{
    // We try to use the unique device ID as encryption key. If we can't determine this ID, a default key is used...
    // Unique device ID determination copied from the QtSystems module of the Qt Toolkit
    if (encryptionKey.isEmpty()) {
        QFile file(QStringLiteral("/sys/devices/virtual/dmi/id/product_uuid"));
        if (file.open(QIODevice::ReadOnly)) {
            QString id = QString::fromLocal8Bit(file.readAll().simplified().data());
            if (id.length() == 36) {
                encryptionKey = id;
            }
            file.close();
        }
    }
    if (encryptionKey.isEmpty()) {
        QFile file(QStringLiteral("/etc/machine-id"));
        if (file.open(QIODevice::ReadOnly)) {
            QString id = QString::fromLocal8Bit(file.readAll().simplified().data());
            if (id.length() == 32) {
                encryptionKey = id.insert(8,'-').insert(13,'-').insert(18,'-').insert(23,'-');
            }
            file.close();
        }
    }
    if (encryptionKey.isEmpty()) {
        QFile file(QStringLiteral("/etc/unique-id"));
        if (file.open(QIODevice::ReadOnly)) {
            QString id = QString::fromLocal8Bit(file.readAll().simplified().data());
            if (id.length() == 32) {
                encryptionKey = id.insert(8,'-').insert(13,'-').insert(18,'-').insert(23,'-');
            }
            file.close();
        }
    }
    if (encryptionKey.isEmpty()) {
        QFile file(QStringLiteral("/var/lib/dbus/machine-id"));
        if (file.open(QIODevice::ReadOnly)) {
            QString id = QString::fromLocal8Bit(file.readAll().simplified().data());
            if (id.length() == 32) {
                encryptionKey = id.insert(8,'-').insert(13,'-').insert(18,'-').insert(23,'-');
            }
            file.close();
        }
    }
    QUuid uid(encryptionKey); //make sure this can be made into a valid QUUid
    if (uid.isNull()) {
         encryptionKey = QString(TWITTER_STORE_DEFAULT_ENCRYPTION_KEY);
    }
    qDebug() << "Using encryption key: " + encryptionKey;
}

void AccountModel::onVerificationError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    qWarning() << "AccountModel::onVerificationError:" << (int)error << reply->errorString() << reply->readAll();
    emit verificationError(reply->errorString());
}

void AccountModel::onVerificationFinished()
{
    qDebug() << "AccountModel::onVerificationFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "AccountModel::onVerificationFinished: " << reply->errorString();
        return;
    }

    QJsonDocument jsonDocument = QJsonDocument::fromJson(reply->readAll());
    if (jsonDocument.isObject()) {
        QJsonObject responseObject = jsonDocument.object();
        qDebug() << "Full Name: " << responseObject.value("name").toString();
        qDebug() << "Twitter Handle: " << responseObject.value("screen_name").toString();
        emit credentialsVerified();
    } else {
        emit verificationError("Piepmatz couldn't understand Twitter's response!");
    }
}

int AccountModel::rowCount(const QModelIndex&) const {
    return availableAccounts.size();
}