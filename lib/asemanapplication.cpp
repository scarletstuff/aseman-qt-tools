/*
    Copyright (C) 2017 Aseman Team
    http://aseman.co

    AsemanQtTools is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    AsemanQtTools is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "asemanapplication.h"
#include "asemandevices.h"
#include "asemannetworkproxy.h"
#include "asemantools.h"
#include "qtsingleapplication/qtlocalpeer.h"
#ifdef Q_OS_ANDROID
#include "asemanjavalayer.h"
#endif

#include <QDir>
#include <QFont>
#include <QPalette>
#include <QSettings>
#include <QThread>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QStandardPaths>
#include <QPointer>

#ifdef QT_WIDGETS_LIB
#define READ_DEFINITION(FUNCTION, DEFAULT_VALUE) \
    switch(aseman_app_singleton->p->appType) { \
    case GuiApplication: \
        return static_cast<QGuiApplication*>(QCoreApplication::instance())->FUNCTION(); \
        break; \
    case WidgetApplication: \
        return static_cast<QApplication*>(QCoreApplication::instance())->FUNCTION(); \
        break; \
    default: \
        return DEFAULT_VALUE; \
        break; \
    }
#else
#ifdef QT_GUI_LIB
#define READ_DEFINITION(FUNCTION, DEFAULT_VALUE) \
    switch(aseman_app_singleton->p->appType) { \
    case GuiApplication: \
        return static_cast<QGuiApplication*>(QCoreApplication::instance())->FUNCTION(); \
        break; \
    default: \
        return DEFAULT_VALUE; \
        break; \
    }
#else
#define READ_DEFINITION(FUNCTION, DEFAULT_VALUE) \
    return DEFAULT_VALUE;
#endif
#endif

#ifdef QT_WIDGETS_LIB
#define SET_DIFINITION(FUNCTION, VALUE) \
    switch(aseman_app_singleton->p->appType) { \
    case GuiApplication: \
        static_cast<QGuiApplication*>(QCoreApplication::instance())->FUNCTION(VALUE); \
        break; \
    case WidgetApplication: \
        static_cast<QApplication*>(QCoreApplication::instance())->FUNCTION(VALUE); \
        break; \
    }
#else
#ifdef QT_GUI_LIB
#define SET_DIFINITION(FUNCTION, VALUE) \
    switch(aseman_app_singleton->p->appType) { \
    case GuiApplication: \
        static_cast<QGuiApplication*>(QCoreApplication::instance())->FUNCTION(VALUE); \
        break; \
    }
#else
#define SET_DIFINITION(FUNCTION, VALUE)
    Q_UNUSED(VALUE)
#endif
#endif

#ifdef Q_OS_ANDROID
#include <QtAndroid>
#endif
#ifdef QT_QML_LIB
#include <QQmlEngine>
#endif

#ifdef QT_GUI_LIB
#include <QGuiApplication>
#endif
#ifdef QT_CORE_LIB
#include <QCoreApplication>
#endif
#ifdef QT_WIDGETS_LIB
#include <QApplication>
#endif

static QSettings *app_global_settings = 0;
static AsemanApplication *aseman_app_singleton = 0;
static QSet<AsemanApplication*> aseman_app_objects;
static QString *aseman_app_home_path = 0;
static QString *aseman_app_log_path = 0;
static QString *aseman_app_tmp_path = 0;

bool AsemanApplication::aseman_app_inited = AsemanApplication::aseman_app_init();

#if defined(Q_OS_MAC) && defined(Q_PROCESSOR_X86_32)
#include <objc/objc.h>
#include <objc/message.h>

bool dockClickHandler(id self,SEL _cmd,...)
{
    Q_UNUSED(self)
    Q_UNUSED(_cmd)
    if(aseman_app_singleton) aseman_app_singleton->clickedOnDock();
    return true;
}
#endif

class AsemanApplicationPrivate
{
public:
    QUrl windowIcon;
    QTimer *clickOnDock_timer;
    QFont globalFont;
    int appType;
    QCoreApplication *app;
    bool app_owner;
    QString appAbout;
#ifdef QT_QML_LIB
    QPointer<QQmlEngine> engine;
#endif
    static QtLocalPeer *peer;
    QPointer<AsemanNetworkProxy> proxy;
};

QtLocalPeer *AsemanApplicationPrivate::peer = 0;

AsemanApplication::AsemanApplication() :
    AsemanQuickObject()
{
    p = new AsemanApplicationPrivate;
    p->app = QCoreApplication::instance();
    p->appType = NoneApplication;
    p->app_owner = false;
    p->proxy = new AsemanNetworkProxy(this);

#ifdef QT_WIDGETS_LIB
    if( qobject_cast<QApplication*>(p->app) )
    {
        p->appType = WidgetApplication;
    }
    else
#endif
#ifdef QT_GUI_LIB
    if( qobject_cast<QGuiApplication*>(p->app) )
    {
        p->appType = GuiApplication;
    }
    else
#endif
#ifdef QT_CORE_LIB
    if( qobject_cast<QCoreApplication*>(p->app) )
        p->appType = CoreApplication;
#endif

    aseman_app_objects.insert(this);
    if(!aseman_app_singleton)
        aseman_app_singleton = this;

    p->app->installEventFilter(this);
    init();
}

#ifdef QT_QML_LIB
AsemanApplication::AsemanApplication(QQmlEngine *engine) :
    AsemanApplication()
{
    p->engine = engine;
}
#endif

AsemanApplication::AsemanApplication(int &argc, char **argv, ApplicationType appType) :
    AsemanQuickObject()
{
    if(!aseman_app_singleton)
        aseman_app_singleton = this;

    p = new AsemanApplicationPrivate;
    p->appType = appType;
    p->app_owner = true;

    switch(p->appType)
    {
#ifdef QT_CORE_LIB
    case CoreApplication:
        p->app = new QCoreApplication(argc, argv);
        break;
#endif
#ifdef QT_GUI_LIB
    case GuiApplication:
        p->app = new QGuiApplication(argc, argv);
        break;
#endif
#ifdef QT_WIDGETS_LIB
    case WidgetApplication:
        p->app = new QApplication(argc, argv);
        break;
#endif
    default:
        p->app = 0;
        break;
    }

    aseman_app_objects.insert(this);
    if(p->app)
        p->app->installEventFilter(this);

    init();
}

void AsemanApplication::init()
{
#ifndef FORCE_ASEMAN_DENSITY
    if(!qgetenv("ASEMAN_SCALE_FACTOR").isNull())
        AsemanDevices::setFlag(AsemanDevices::AsemanScaleFactorEnable);
    else
    if(!qgetenv("QT_SCALE_FACTOR").isNull())
        AsemanDevices::setFlag(AsemanDevices::DisableDensities);
#endif

    switch(p->appType)
    {
#ifdef QT_WIDGETS_LIB
    case WidgetApplication:
        p->globalFont = static_cast<QApplication*>(p->app)->font();
#endif
#ifdef QT_GUI_LIB
    case GuiApplication:
        connect(static_cast<QGuiApplication*>(p->app), &QGuiApplication::lastWindowClosed, this, &AsemanApplication::lastWindowClosed);
        connect(static_cast<QGuiApplication*>(p->app), &QGuiApplication::applicationStateChanged, this, &AsemanApplication::applicationStateChanged);
        p->globalFont = static_cast<QGuiApplication*>(p->app)->font();
#endif
#ifdef QT_CORE_LIB
    case CoreApplication:
        connect(p->app, &QCoreApplication::organizationNameChanged, this, &AsemanApplication::organizationNameChanged);
        connect(p->app, &QCoreApplication::organizationDomainChanged, this, &AsemanApplication::organizationDomainChanged);
        connect(p->app, &QCoreApplication::applicationNameChanged, this, &AsemanApplication::applicationNameChanged);
        connect(p->app, &QCoreApplication::applicationVersionChanged, this, &AsemanApplication::applicationVersionChanged);
        break;
#endif

    default:
        p->app = 0;
        break;
    }

    p->clickOnDock_timer = new QTimer(this);
    p->clickOnDock_timer->setSingleShot(true);
    p->clickOnDock_timer->setInterval(500);

#if defined(Q_OS_MAC) && defined(Q_PROCESSOR_X86_32)
#ifndef Q_OS_IOS
    objc_object* cls = objc_getClass("NSApplication");
    SEL sharedApplication = sel_registerName("sharedApplication");
    objc_object* appInst = objc_msgSend(cls,sharedApplication);

    if(appInst != NULL)
    {
        objc_object* delegate = objc_msgSend(appInst, sel_registerName("delegate"));
        objc_object* delClass = objc_msgSend(delegate,  sel_registerName("class"));
        const char* tst = class_getName(delClass->isa);
        bool test = class_addMethod((objc_class*)delClass, sel_registerName("applicationShouldHandleReopen:hasVisibleWindows:"), (IMP)dockClickHandler,"B@:");

        Q_UNUSED(tst)
        if (!test)
        {
            // failed to register handler...
        }
    }
#endif
#endif

    if(AsemanApplicationPrivate::peer)
        connect(AsemanApplicationPrivate::peer, &QtLocalPeer::messageReceived, this, &AsemanApplication::messageReceived);
}

bool AsemanApplication::aseman_app_init()
{
    if(aseman_app_inited)
        return true;

#ifndef FORCE_ASEMAN_DENSITY
#ifdef Q_OS_ANDROID
    const bool nexus5X = (AsemanJavaLayer::instance()->deviceName() == "LGE Nexus 5X");
    if(!nexus5X)
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    if(!QGuiApplication::testAttribute(Qt::AA_EnableHighDpiScaling) && qgetenv("QT_SCALE_FACTOR").isNull() && qgetenv("ASEMAN_SCALE_FACTOR").isNull())
    {
        qreal ratio = AsemanJavaLayer::instance()->density()*(AsemanDevices::isTablet()? 1.28 : 1);
        if(nexus5X)
        {
            qreal newRatio = qRound((ratio*20) + 0.01)/20.0;
            qputenv("QT_SCALE_FACTOR",QByteArray::number(newRatio));
            qputenv("ASEMAN_SCALE_FACTOR",QByteArray::number(ratio/newRatio));
        }
        else
            qputenv("QT_SCALE_FACTOR",QByteArray::number(ratio));
    }
#endif
#endif
    return true;
}

QString AsemanApplication::homePath()
{
    if(aseman_app_home_path)
        return *aseman_app_home_path;

    aseman_app_home_path = new QString();

    QString oldPath;
#ifdef Q_OS_WIN
    oldPath = QDir::homePath() + "/AppData/Local/" + QCoreApplication::applicationName();
#else
    oldPath = QDir::homePath() + "/.config/" + QCoreApplication::applicationName();
#endif

    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::QStandardPaths::AppDataLocation);
    if(paths.isEmpty())
        paths << oldPath;

    if( oldPath.count() && QFileInfo::exists(oldPath) )
        *aseman_app_home_path = oldPath;
    else
        *aseman_app_home_path = paths.first();

    QDir().mkpath(*aseman_app_home_path);
    return *aseman_app_home_path;
}

void AsemanApplication::setHomePath(const QString &path)
{
    homePath();
    *aseman_app_home_path = path;

    if(aseman_app_singleton)
    {
        Q_EMIT aseman_app_singleton->homePathChanged();
        Q_EMIT aseman_app_singleton->logPathChanged();
        Q_EMIT aseman_app_singleton->confsPathChanged();
        Q_EMIT aseman_app_singleton->backupsPathChanged();
    }
}

QString AsemanApplication::startPath()
{
#ifdef Q_OS_ANDROID
    return "/sdcard/";
#else
#ifdef Q_OS_IOS
    return homePath();
#else
    return QDir::homePath();
#endif
#endif
}

QString AsemanApplication::appPath()
{
    return QCoreApplication::applicationDirPath();
}

QString AsemanApplication::appFilePath()
{
    return QCoreApplication::applicationFilePath();
}

QString AsemanApplication::logPath()
{
    if(!aseman_app_log_path)
    {
        aseman_app_log_path = new QString();
        *aseman_app_log_path = homePath() + "/log";
        QDir().mkpath(*aseman_app_log_path);
    }

    return *aseman_app_log_path;
}

void AsemanApplication::setLogPath(const QString &path)
{
    if( logPath() == path )
        return;

    *aseman_app_log_path = path;
    if(aseman_app_singleton)
        Q_EMIT aseman_app_singleton->logPathChanged();
}

QString AsemanApplication::confsPath()
{
    return homePath() + "/config.ini";
}

QString AsemanApplication::tempPath()
{
    if(!aseman_app_tmp_path)
    {
        aseman_app_tmp_path = new QString();
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        *aseman_app_tmp_path = homePath() + "/tmp/";
#else
        *aseman_app_tmp_path = QDir::tempPath();
#endif
        QDir().mkpath(*aseman_app_tmp_path);
    }

    return *aseman_app_tmp_path;
}

QString AsemanApplication::backupsPath()
{
#ifdef Q_OS_ANDROID
    return "/sdcard/" + QCoreApplication::organizationDomain() + "/" + QCoreApplication::applicationName() + "/backups";
#else
#ifdef Q_OS_IOS
    return homePath() + "/backups/";
#else
    return homePath() + "/backups";
#endif
#endif
}

QString AsemanApplication::cameraPath()
{
#ifdef Q_OS_ANDROID
    return "/sdcard/DCIM";
#else
#ifdef Q_OS_IOS
    return QStandardPaths::standardLocations(QStandardPaths::QStandardPaths::AppDataLocation).first() + "/camera/";
#else
    return QDir::homePath() + "/Pictures/Camera";
#endif
#endif
}

QString AsemanApplication::applicationDirPath()
{
    return QCoreApplication::applicationDirPath();
}

QString AsemanApplication::applicationFilePath()
{
    return QCoreApplication::applicationFilePath();
}

qint64 AsemanApplication::applicationPid()
{
    return QCoreApplication::applicationPid();
}

void AsemanApplication::setOrganizationDomain(const QString &orgDomain)
{
    QCoreApplication::setOrganizationDomain(orgDomain);
}

QString AsemanApplication::organizationDomain()
{
    return QCoreApplication::organizationDomain();
}

void AsemanApplication::setOrganizationName(const QString &orgName)
{
    QCoreApplication::setOrganizationName(orgName);
}

QString AsemanApplication::organizationName()
{
    return QCoreApplication::organizationName();
}

void AsemanApplication::setApplicationName(const QString &application)
{
    QCoreApplication::setApplicationName(application);
}

QString AsemanApplication::applicationName()
{
    return QCoreApplication::applicationName();
}

void AsemanApplication::setApplicationVersion(const QString &version)
{
    QCoreApplication::setApplicationVersion(version);
}

QString AsemanApplication::applicationVersion()
{
    return QCoreApplication::applicationVersion();
}

void AsemanApplication::setApplicationDisplayName(const QString &name)
{
    SET_DIFINITION(setApplicationDisplayName, name)
}

QString AsemanApplication::applicationDisplayName()
{
    READ_DEFINITION(applicationDisplayName, QString())
}

void AsemanApplication::setApplicationAbout(const QString &desc)
{
    if(!aseman_app_singleton)
        return;
    if( aseman_app_singleton->p->appAbout == desc )
        return;

    aseman_app_singleton->p->appAbout = desc;
    Q_EMIT aseman_app_singleton->applicationAboutChanged();
}

QString AsemanApplication::applicationAbout()
{
    if(aseman_app_singleton)
        return tr(aseman_app_singleton->p->appAbout.toUtf8());
    else
        return QString();
}

void AsemanApplication::setApplicationId(const QString &applicationId)
{
    if(AsemanApplicationPrivate::peer && AsemanApplicationPrivate::peer->applicationId() == applicationId)
        return;
    if(AsemanApplicationPrivate::peer)
        delete AsemanApplicationPrivate::peer;

    AsemanApplicationPrivate::peer = 0;
    if(!applicationId.isEmpty())
    {
        AsemanApplicationPrivate::peer = new QtLocalPeer(0, applicationId);
        Q_FOREACH(AsemanApplication *app, aseman_app_objects)
            connect(AsemanApplicationPrivate::peer, &QtLocalPeer::messageReceived, app, &AsemanApplication::messageReceived);
    }

    Q_FOREACH(AsemanApplication *app, aseman_app_objects)
    {
        Q_EMIT app->applicationIdChanged();
        Q_EMIT app->isRunningChanged();
    }
}

QString AsemanApplication::applicationId()
{
    if(AsemanApplicationPrivate::peer)
        return AsemanApplicationPrivate::peer->applicationId();
    else
        return QString();
}

QString AsemanApplication::platformName()
{
    READ_DEFINITION(platformName, QString())
}

QStringList AsemanApplication::arguments()
{
    return QCoreApplication::arguments();
}

void AsemanApplication::setQuitOnLastWindowClosed(bool quit)
{
    SET_DIFINITION(setQuitOnLastWindowClosed, quit)
}

bool AsemanApplication::quitOnLastWindowClosed()
{
    READ_DEFINITION(quitOnLastWindowClosed, false)
}

QClipboard *AsemanApplication::clipboard()
{
    READ_DEFINITION(clipboard, 0)
}

void AsemanApplication::setWindowIcon(const QUrl &icon)
{
    if(p->windowIcon == icon)
        return;

    p->windowIcon = icon;
    QString file = icon.toString();
    if(file.left(6) == "qrc://")
        file = file.replace(0, 6, ":/");

#if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
    SET_DIFINITION(setWindowIcon, QIcon(file))
#else
    Q_UNUSED(icon)
#endif
    Q_EMIT windowIconChanged();
}

QUrl AsemanApplication::windowIcon() const
{
    return p->windowIcon;
}

bool AsemanApplication::isRunning()
{
    if(AsemanApplicationPrivate::peer)
        return AsemanApplicationPrivate::peer->isClient();

    return false;
}

int AsemanApplication::appType()
{
    return aseman_app_singleton->p->appType;
}

bool AsemanApplication::isDebug()
{
#ifdef QT_DEBUG
    return true;
#else
    return false;
#endif
}

void AsemanApplication::sendMessage(const QString &msg)
{
    if(AsemanApplicationPrivate::peer)
        AsemanApplicationPrivate::peer->sendMessage(msg, 5000);
}

#ifdef QT_QML_LIB
QVariantMap AsemanApplication::requestPermissions(QStringList persmissions, QJSValue callback)
{
    QVariantMap _res;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#ifdef Q_OS_ANDROID
    auto c_callback = [callback](const QtAndroid::PermissionResultMap &res) -> QVariantMap {
        QVariantMap map;

        QHashIterator<QString, QtAndroid::PermissionResult> i(res);
        while(i.hasNext())
        {
            i.next();
            map[i.key()] = (i.value() == QtAndroid::PermissionResult::Granted);
        }

        return map;
    };

    for(QString pr: persmissions)
        if(QtAndroid::checkPermission(pr) == QtAndroid::PermissionResult::Granted)
        {
            persmissions.removeAll(pr);
            _res[pr] = true;
        }

    if(persmissions.count())
    {
        if(callback.isCallable())
            QtAndroid::requestPermissions(persmissions, [callback, c_callback, _res, this](const QtAndroid::PermissionResultMap &res){
                QVariantMap map = c_callback(res);
                map.unite(_res);

                QJSValue callbackCopy = callback;
                if(p->engine)
                    callbackCopy.call(QJSValueList() << p->engine->toScriptValue<QVariant>(map));
            });
        else
            _res.unite(c_callback( QtAndroid::requestPermissionsSync(persmissions) ));
    }
    else
    {
        if(callback.isCallable())
        {
            if(p->engine)
                callback.call(QJSValueList() << p->engine->toScriptValue<QVariant>(_res));
        }
    }
#endif
#endif
    Q_UNUSED(persmissions)
    Q_UNUSED(callback)
    return _res;
}
#endif

AsemanApplication *AsemanApplication::instance()
{
    return aseman_app_singleton;
}

QCoreApplication *AsemanApplication::qapp()
{
    return QCoreApplication::instance();
}

void AsemanApplication::setGlobalFont(const QFont &font)
{
    if(p->globalFont == font)
        return;

    p->globalFont = font;
    Q_EMIT globalFontChanged();
}

QFont AsemanApplication::globalFont() const
{
    return p->globalFont;
}

QFont AsemanApplication::font()
{
    READ_DEFINITION(font, QFont())
}

void AsemanApplication::setFont(const QFont &f)
{
    SET_DIFINITION(setFont, f);
}

AsemanNetworkProxy *AsemanApplication::proxy() const
{
    return p->proxy;
}

void AsemanApplication::setProxy(AsemanNetworkProxy *proxy)
{
    if(p->proxy == proxy)
        return;

    if(p->proxy)
        delete p->proxy;

    p->proxy = proxy;
    Q_EMIT proxyChanged();
}

#ifdef QT_GUI_LIB
QPalette AsemanApplication::palette()
{
    READ_DEFINITION(palette, QPalette())
}

void AsemanApplication::setPalette(const QPalette &pal)
{
    SET_DIFINITION(setPalette, pal);
}

int AsemanApplication::applicationState()
{
    READ_DEFINITION(applicationState, Qt::ApplicationActive)
}
#endif

QSettings *AsemanApplication::settings()
{
    if( !app_global_settings )
    {
        QDir().mkpath(AsemanApplication::homePath());
        app_global_settings = new QSettings( AsemanApplication::homePath() + "/config.ini", QSettings::IniFormat );
    }

    return app_global_settings;
}

void AsemanApplication::refreshTranslations()
{
    Q_EMIT languageUpdated();
}

void AsemanApplication::back()
{
    Q_EMIT backRequest();
}

int AsemanApplication::exec()
{
    p->clickOnDock_timer->stop();
    p->clickOnDock_timer->start();

    return p->app->exec();
}

void AsemanApplication::exit(int retcode)
{
    aseman_app_singleton->p->app->exit(retcode);
}

void AsemanApplication::sleep(quint64 ms)
{
    QThread::msleep(ms);
}

void AsemanApplication::setSetting(const QString &key, const QVariant &value)
{
    settings()->setValue(key, value);
}

QVariant AsemanApplication::readSetting(const QString &key, const QVariant &defaultValue)
{
    return settings()->value(key, defaultValue);
}

bool AsemanApplication::eventFilter(QObject *o, QEvent *e)
{
    if(o == p->app)
    {
#ifdef Q_OS_MAC
        switch(e->type()) {
        case QEvent::ApplicationActivate:
            if(p->clickOnDock_timer->isActive())
            {
                p->clickOnDock_timer->stop();
                p->clickOnDock_timer->start();
            }
            else
                Q_EMIT clickedOnDock();
            break;

        default:
            break;
        }
#endif
    }

    return QObject::eventFilter(o,e);
}

AsemanApplication::~AsemanApplication()
{
    aseman_app_objects.remove(this);
    if(aseman_app_singleton == this)
    {
        if(AsemanApplicationPrivate::peer)
        {
            delete AsemanApplicationPrivate::peer;
            AsemanApplicationPrivate::peer = 0;
        }
        aseman_app_singleton = 0;
    }

    if(p->app && p->app_owner)
        delete p->app;

    delete p;
}
