#include "libp2p_module_plugin.h"
#include <QDebug>
#include <QCoreApplication>
#include <QVariantList>
#include <QDateTime>

Libp2pModulePlugin::Libp2pModulePlugin() : libp2pCtx(nullptr)
{
    qDebug() << "Libp2pModulePlugin: Initializing...";
    qDebug() << "Libp2pModulePlugin: Initialized successfully";
}

Libp2pModulePlugin::~Libp2pModulePlugin() 
{
    // Clean up resources
    if (logosAPI) {
        delete logosAPI;
        logosAPI = nullptr;
    }
    
    // Clean up Libp2p context if it exists
    if (libp2pCtx) {
        // TODO: Call libp2p_destroy when needed
        libp2pCtx = nullptr;
    }
}

bool Libp2pModulePlugin::start()
{
    qDebug() << "Libp2pModulePlugin::start";

    // Create event data with the bar parameter
    QVariantList eventData;

    // Trigger the event using LogosAPI client (like chat module does)
    if (logosAPI) {
        // print triggering signal
        qDebug() << "Libp2pModulePlugin: Triggering event 'fooTriggered' with data:" << eventData;
        logosAPI->getClient("core_manager")->onEventResponse(this, "fooTriggered", eventData);
        qDebug() << "Libp2pModulePlugin: Event 'fooTriggered' triggered with data:" << eventData;
    } else {
        qWarning() << "Libp2pModulePlugin: LogosAPI not available, cannot trigger event";
    }

    return true;
}

void Libp2pModulePlugin::init_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::init_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "Libp2pModulePlugin::init_callback message:" << message;
    }
}

void Libp2pModulePlugin::start_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::start_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "Libp2pModulePlugin::start_callback message:" << message;
    }
}

void Libp2pModulePlugin::event_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::event_callback called with ret:" << callerRet;

    Libp2pModulePlugin* plugin = static_cast<Libp2pModulePlugin*>(userData);
    if (!plugin) {
        qWarning() << "Libp2pModulePlugin::event_callback: Invalid userData";
        return;
    }

    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        // qDebug() << "Libp2pModulePlugin::event_callback message:" << message;

        // Create event data with the message
        QVariantList eventData;
        eventData << message;
        eventData << QDateTime::currentDateTime().toString(Qt::ISODate);

        // Trigger event using LogosAPI client
        if (plugin->logosAPI) {
            // qDebug() << "------------------------> Libp2pModulePlugin: Triggering event 'libp2pMessage' with data:" << eventData;
            plugin->logosAPI->getClient("core_manager")->onEventResponse(plugin, "libp2pMessage", eventData);
        } else {
            qWarning() << "Libp2pModulePlugin: LogosAPI not available, cannot trigger event";
        }
    }
}

void Libp2pModulePlugin::relay_subscribe_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::relay_subscribe_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "Libp2pModulePlugin::relay_subscribe_callback message:" << message;
    }
}

void Libp2pModulePlugin::relay_publish_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::relay_publish_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "Libp2pModulePlugin::relay_publish_callback message:" << message;
    }
}

void Libp2pModulePlugin::filter_subscribe_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::filter_subscribe_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "Libp2pModulePlugin::filter_subscribe_callback message:" << message;
    }
}

void Libp2pModulePlugin::store_query_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "Libp2pModulePlugin::store_query_callback called with ret:" << callerRet;

    Libp2pModulePlugin* plugin = static_cast<Libp2pModulePlugin*>(userData);
    if (!plugin) {
        qWarning() << "Libp2pModulePlugin::store_query_callback: Invalid userData";
        exit(1);
        return;
    }

    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        // qDebug() << "Libp2pModulePlugin::store_query_callback message:" << message;

        // Create event data with the store query result
        QVariantList eventData;
        eventData << message;
        eventData << QDateTime::currentDateTime().toString(Qt::ISODate);

        // Trigger event using LogosAPI client (similar to event_callback)
        if (plugin->logosAPI) {
            // qDebug() << "Libp2pModulePlugin: Triggering event 'storeQueryResult' with data:" << eventData;
            plugin->logosAPI->getClient("core_manager")->onEventResponse(plugin, "storeQueryResponse", eventData);
            // exit(1);
        } else {
            qWarning() << "Libp2pModulePlugin: LogosAPI not available, cannot trigger event";
            exit(1);
        }
    }
}

void Libp2pModulePlugin::initLogos(LogosAPI* logosAPIInstance) {
    if (logosAPI) {
        delete logosAPI;
    }
    logosAPI = logosAPIInstance;
}

bool Libp2pModulePlugin::initLibp2p(const QString &cfg)
{
    qDebug() << "Libp2pModulePlugin::initLibp2p called with cfg:" << cfg;
    
    // Convert QString to UTF-8 byte array
    QByteArray cfgUtf8 = cfg.toUtf8();
    
    // Call libp2p_new with the configuration
    libp2pCtx = libp2p_new(cfgUtf8.constData(), init_callback, this);
    
    if (libp2pCtx) {
        qDebug() << "Libp2pModulePlugin: Libp2p context created successfully";
        return true;
    } else {
        qWarning() << "Libp2pModulePlugin: Failed to create Libp2p context";
        return false;
    }
}

bool Libp2pModulePlugin::startLibp2p()
{
    qDebug() << "Libp2pModulePlugin::startLibp2p called";
    
    if (!libp2pCtx) {
        qWarning() << "Libp2pModulePlugin: Cannot start Libp2p - context not initialized. Call initLibp2p first.";
        return false;
    }
    
    // Call libp2p_start with the saved context
    int result = libp2p_start(libp2pCtx, start_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "==================Libp2pModulePlugin: Libp2p start initiated successfully=======================";
        return true;
    } else {
        qWarning() << "Libp2pModulePlugin: Failed to start Libp2p, error code:" << result;
        return false;
    }
}

bool Libp2pModulePlugin::setEventCallback()
{
    qDebug() << "Libp2pModulePlugin::setEventCallback called";
    
    if (!libp2pCtx) {
        qWarning() << "Libp2pModulePlugin: Cannot set event callback - context not initialized. Call initLibp2p first.";
        return false;
    }
    
    // Set the event callback using libp2p_set_event_callback
    libp2p_set_event_callback(libp2pCtx, event_callback, this);
    
    qDebug() << "Libp2pModulePlugin: Event callback set successfully";
    return true;
}

bool Libp2pModulePlugin::relaySubscribe(const QString &pubSubTopic)
{
    qDebug() << "Libp2pModulePlugin::relaySubscribe called with pubSubTopic:" << pubSubTopic;
    
    if (!libp2pCtx) {
        qWarning() << "Libp2pModulePlugin: Cannot subscribe to relay - context not initialized. Call initLibp2p first.";
        return false;
    }
    
    // Convert QString to UTF-8 byte array
    QByteArray topicUtf8 = pubSubTopic.toUtf8();
    
    // Call libp2p_relay_subscribe with the pubsub topic
    int result = libp2p_relay_subscribe(libp2pCtx, topicUtf8.constData(), relay_subscribe_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "Libp2pModulePlugin: Relay subscribe initiated successfully for topic:" << pubSubTopic;
        return true;
    } else {
        qWarning() << "Libp2pModulePlugin: Failed to subscribe to relay topic:" << pubSubTopic << ", error code:" << result;
        return false;
    }
}

bool Libp2pModulePlugin::relayPublish(const QString &pubSubTopic, const QString &jsonLibp2pMessage)
{
    qDebug() << "Libp2pModulePlugin::relayPublish called with pubSubTopic:" << pubSubTopic;
    qDebug() << "Libp2pModulePlugin::relayPublish message:" << jsonLibp2pMessage;
    
    if (!libp2pCtx) {
        qWarning() << "Libp2pModulePlugin: Cannot publish to relay - context not initialized. Call initLibp2p first.";
        return false;
    }
    
    // Convert QStrings to UTF-8 byte arrays
    QByteArray topicUtf8 = pubSubTopic.toUtf8();
    QByteArray messageUtf8 = jsonLibp2pMessage.toUtf8();
    
    // Call libp2p_relay_publish with hardcoded timeout of 10000ms
    int result = libp2p_relay_publish(libp2pCtx, topicUtf8.constData(), messageUtf8.constData(), 10000, relay_publish_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "Libp2pModulePlugin: Relay publish initiated successfully for topic:" << pubSubTopic;
        return true;
    } else {
        qWarning() << "Libp2pModulePlugin: Failed to publish to relay topic:" << pubSubTopic << ", error code:" << result;
        return false;
    }
}

bool Libp2pModulePlugin::filterSubscribe(const QString &pubSubTopic, const QString &contentTopics)
{
    qDebug() << "Libp2pModulePlugin::filterSubscribe called with pubSubTopic:" << pubSubTopic;
    qDebug() << "Libp2pModulePlugin::filterSubscribe contentTopics:" << contentTopics;
    
    if (!libp2pCtx) {
        qWarning() << "Libp2pModulePlugin: Cannot subscribe to filter - context not initialized. Call initLibp2p first.";
        return false;
    }
    
    // Convert QStrings to UTF-8 byte arrays
    QByteArray topicUtf8 = pubSubTopic.toUtf8();
    QByteArray contentTopicsUtf8 = contentTopics.toUtf8();
    
    // Call libp2p_filter_subscribe
    int result = libp2p_filter_subscribe(libp2pCtx, topicUtf8.constData(), contentTopicsUtf8.constData(), filter_subscribe_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "Libp2pModulePlugin: Filter subscribe initiated successfully for topic:" << pubSubTopic;
        return true;
    } else {
        qWarning() << "Libp2pModulePlugin: Failed to subscribe to filter topic:" << pubSubTopic << ", error code:" << result;
        return false;
    }
}

bool Libp2pModulePlugin::storeQuery(const QString &jsonQuery, const QString &peerAddr)
{
    qDebug() << "Libp2pModulePlugin::storeQuery called with jsonQuery:" << jsonQuery;
    qDebug() << "Libp2pModulePlugin::storeQuery peerAddr:" << peerAddr;
    
    if (!libp2pCtx) {
        qWarning() << "Libp2pModulePlugin: Cannot execute store query - context not initialized. Call initLibp2p first.";
        return false;
    }
    
    // Convert QStrings to UTF-8 byte arrays
    QByteArray queryUtf8 = jsonQuery.toUtf8();
    QByteArray peerAddrUtf8 = peerAddr.toUtf8();
    
    // Call libp2p_store_query with hardcoded timeout of 30000ms
    int result = libp2p_store_query(libp2pCtx, queryUtf8.constData(), peerAddrUtf8.constData(), 30000, store_query_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "Libp2pModulePlugin: Store query initiated successfully for peer:" << peerAddr;
        return true;
    } else {
        qWarning() << "Libp2pModulePlugin: Failed to execute store query for peer:" << peerAddr << ", error code:" << result;
        return false;
    }
} 

