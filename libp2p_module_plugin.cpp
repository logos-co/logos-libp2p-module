#include "libp2p_module_plugin.h"
#include <QDebug>
#include <QCoreApplication>
#include <QVariantList>
#include <QDateTime>

WakuModulePlugin::WakuModulePlugin() : libp2pCtx(nullptr)
{
    qDebug() << "WakuModulePlugin: Initializing...";
    qDebug() << "WakuModulePlugin: Initialized successfully";
}

WakuModulePlugin::~WakuModulePlugin() 
{
    // Clean up resources
    if (logosAPI) {
        delete logosAPI;
        logosAPI = nullptr;
    }
    
    // Clean up Waku context if it exists
    if (libp2pCtx) {
        // TODO: Call libp2p_destroy when needed
        libp2pCtx = nullptr;
    }
}

bool WakuModulePlugin::foo(const QString &bar)
{
    qDebug() << "WakuModulePlugin::foo called with:" << bar;

    // Create event data with the bar parameter
    QVariantList eventData;
    eventData << bar; // Add the bar parameter to the event data
    eventData << QDateTime::currentDateTime().toString(Qt::ISODate); // Add timestamp

    // Trigger the event using LogosAPI client (like chat module does)
    if (logosAPI) {
        // print triggering signal
        qDebug() << "WakuModulePlugin: Triggering event 'fooTriggered' with data:" << eventData;
        logosAPI->getClient("core_manager")->onEventResponse(this, "fooTriggered", eventData);
        qDebug() << "WakuModulePlugin: Event 'fooTriggered' triggered with data:" << eventData;
    } else {
        qWarning() << "WakuModulePlugin: LogosAPI not available, cannot trigger event";
    }

    return true;
}

void WakuModulePlugin::init_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::init_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "WakuModulePlugin::init_callback message:" << message;
    }
}

void WakuModulePlugin::start_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::start_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "WakuModulePlugin::start_callback message:" << message;
    }
}

void WakuModulePlugin::event_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::event_callback called with ret:" << callerRet;

    WakuModulePlugin* plugin = static_cast<WakuModulePlugin*>(userData);
    if (!plugin) {
        qWarning() << "WakuModulePlugin::event_callback: Invalid userData";
        return;
    }

    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        // qDebug() << "WakuModulePlugin::event_callback message:" << message;

        // Create event data with the message
        QVariantList eventData;
        eventData << message;
        eventData << QDateTime::currentDateTime().toString(Qt::ISODate);

        // Trigger event using LogosAPI client
        if (plugin->logosAPI) {
            // qDebug() << "------------------------> WakuModulePlugin: Triggering event 'libp2pMessage' with data:" << eventData;
            plugin->logosAPI->getClient("core_manager")->onEventResponse(plugin, "libp2pMessage", eventData);
        } else {
            qWarning() << "WakuModulePlugin: LogosAPI not available, cannot trigger event";
        }
    }
}

void WakuModulePlugin::relay_subscribe_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::relay_subscribe_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "WakuModulePlugin::relay_subscribe_callback message:" << message;
    }
}

void WakuModulePlugin::relay_publish_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::relay_publish_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "WakuModulePlugin::relay_publish_callback message:" << message;
    }
}

void WakuModulePlugin::filter_subscribe_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::filter_subscribe_callback called with ret:" << callerRet;
    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        qDebug() << "WakuModulePlugin::filter_subscribe_callback message:" << message;
    }
}

void WakuModulePlugin::store_query_callback(int callerRet, const char* msg, size_t len, void* userData)
{
    qDebug() << "WakuModulePlugin::store_query_callback called with ret:" << callerRet;

    WakuModulePlugin* plugin = static_cast<WakuModulePlugin*>(userData);
    if (!plugin) {
        qWarning() << "WakuModulePlugin::store_query_callback: Invalid userData";
        exit(1);
        return;
    }

    if (msg && len > 0) {
        QString message = QString::fromUtf8(msg, len);
        // qDebug() << "WakuModulePlugin::store_query_callback message:" << message;

        // Create event data with the store query result
        QVariantList eventData;
        eventData << message;
        eventData << QDateTime::currentDateTime().toString(Qt::ISODate);

        // Trigger event using LogosAPI client (similar to event_callback)
        if (plugin->logosAPI) {
            // qDebug() << "WakuModulePlugin: Triggering event 'storeQueryResult' with data:" << eventData;
            plugin->logosAPI->getClient("core_manager")->onEventResponse(plugin, "storeQueryResponse", eventData);
            // exit(1);
        } else {
            qWarning() << "WakuModulePlugin: LogosAPI not available, cannot trigger event";
            exit(1);
        }
    }
}

void WakuModulePlugin::initLogos(LogosAPI* logosAPIInstance) {
    if (logosAPI) {
        delete logosAPI;
    }
    logosAPI = logosAPIInstance;
}

bool WakuModulePlugin::initWaku(const QString &cfg)
{
    qDebug() << "WakuModulePlugin::initWaku called with cfg:" << cfg;
    
    // Convert QString to UTF-8 byte array
    QByteArray cfgUtf8 = cfg.toUtf8();
    
    // Call libp2p_new with the configuration
    libp2pCtx = libp2p_new(cfgUtf8.constData(), init_callback, this);
    
    if (libp2pCtx) {
        qDebug() << "WakuModulePlugin: Waku context created successfully";
        return true;
    } else {
        qWarning() << "WakuModulePlugin: Failed to create Waku context";
        return false;
    }
}

bool WakuModulePlugin::startWaku()
{
    qDebug() << "WakuModulePlugin::startWaku called";
    
    if (!libp2pCtx) {
        qWarning() << "WakuModulePlugin: Cannot start Waku - context not initialized. Call initWaku first.";
        return false;
    }
    
    // Call libp2p_start with the saved context
    int result = libp2p_start(libp2pCtx, start_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "==================WakuModulePlugin: Waku start initiated successfully=======================";
        return true;
    } else {
        qWarning() << "WakuModulePlugin: Failed to start Waku, error code:" << result;
        return false;
    }
}

bool WakuModulePlugin::setEventCallback()
{
    qDebug() << "WakuModulePlugin::setEventCallback called";
    
    if (!libp2pCtx) {
        qWarning() << "WakuModulePlugin: Cannot set event callback - context not initialized. Call initWaku first.";
        return false;
    }
    
    // Set the event callback using libp2p_set_event_callback
    libp2p_set_event_callback(libp2pCtx, event_callback, this);
    
    qDebug() << "WakuModulePlugin: Event callback set successfully";
    return true;
}

bool WakuModulePlugin::relaySubscribe(const QString &pubSubTopic)
{
    qDebug() << "WakuModulePlugin::relaySubscribe called with pubSubTopic:" << pubSubTopic;
    
    if (!libp2pCtx) {
        qWarning() << "WakuModulePlugin: Cannot subscribe to relay - context not initialized. Call initWaku first.";
        return false;
    }
    
    // Convert QString to UTF-8 byte array
    QByteArray topicUtf8 = pubSubTopic.toUtf8();
    
    // Call libp2p_relay_subscribe with the pubsub topic
    int result = libp2p_relay_subscribe(libp2pCtx, topicUtf8.constData(), relay_subscribe_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "WakuModulePlugin: Relay subscribe initiated successfully for topic:" << pubSubTopic;
        return true;
    } else {
        qWarning() << "WakuModulePlugin: Failed to subscribe to relay topic:" << pubSubTopic << ", error code:" << result;
        return false;
    }
}

bool WakuModulePlugin::relayPublish(const QString &pubSubTopic, const QString &jsonWakuMessage)
{
    qDebug() << "WakuModulePlugin::relayPublish called with pubSubTopic:" << pubSubTopic;
    qDebug() << "WakuModulePlugin::relayPublish message:" << jsonWakuMessage;
    
    if (!libp2pCtx) {
        qWarning() << "WakuModulePlugin: Cannot publish to relay - context not initialized. Call initWaku first.";
        return false;
    }
    
    // Convert QStrings to UTF-8 byte arrays
    QByteArray topicUtf8 = pubSubTopic.toUtf8();
    QByteArray messageUtf8 = jsonWakuMessage.toUtf8();
    
    // Call libp2p_relay_publish with hardcoded timeout of 10000ms
    int result = libp2p_relay_publish(libp2pCtx, topicUtf8.constData(), messageUtf8.constData(), 10000, relay_publish_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "WakuModulePlugin: Relay publish initiated successfully for topic:" << pubSubTopic;
        return true;
    } else {
        qWarning() << "WakuModulePlugin: Failed to publish to relay topic:" << pubSubTopic << ", error code:" << result;
        return false;
    }
}

bool WakuModulePlugin::filterSubscribe(const QString &pubSubTopic, const QString &contentTopics)
{
    qDebug() << "WakuModulePlugin::filterSubscribe called with pubSubTopic:" << pubSubTopic;
    qDebug() << "WakuModulePlugin::filterSubscribe contentTopics:" << contentTopics;
    
    if (!libp2pCtx) {
        qWarning() << "WakuModulePlugin: Cannot subscribe to filter - context not initialized. Call initWaku first.";
        return false;
    }
    
    // Convert QStrings to UTF-8 byte arrays
    QByteArray topicUtf8 = pubSubTopic.toUtf8();
    QByteArray contentTopicsUtf8 = contentTopics.toUtf8();
    
    // Call libp2p_filter_subscribe
    int result = libp2p_filter_subscribe(libp2pCtx, topicUtf8.constData(), contentTopicsUtf8.constData(), filter_subscribe_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "WakuModulePlugin: Filter subscribe initiated successfully for topic:" << pubSubTopic;
        return true;
    } else {
        qWarning() << "WakuModulePlugin: Failed to subscribe to filter topic:" << pubSubTopic << ", error code:" << result;
        return false;
    }
}

bool WakuModulePlugin::storeQuery(const QString &jsonQuery, const QString &peerAddr)
{
    qDebug() << "WakuModulePlugin::storeQuery called with jsonQuery:" << jsonQuery;
    qDebug() << "WakuModulePlugin::storeQuery peerAddr:" << peerAddr;
    
    if (!libp2pCtx) {
        qWarning() << "WakuModulePlugin: Cannot execute store query - context not initialized. Call initWaku first.";
        return false;
    }
    
    // Convert QStrings to UTF-8 byte arrays
    QByteArray queryUtf8 = jsonQuery.toUtf8();
    QByteArray peerAddrUtf8 = peerAddr.toUtf8();
    
    // Call libp2p_store_query with hardcoded timeout of 30000ms
    int result = libp2p_store_query(libp2pCtx, queryUtf8.constData(), peerAddrUtf8.constData(), 30000, store_query_callback, this);
    
    if (result == RET_OK) {
        qDebug() << "WakuModulePlugin: Store query initiated successfully for peer:" << peerAddr;
        return true;
    } else {
        qWarning() << "WakuModulePlugin: Failed to execute store query for peer:" << peerAddr << ", error code:" << result;
        return false;
    }
} 

