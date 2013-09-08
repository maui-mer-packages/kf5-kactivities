/*
 * Copyright (c) 2010, 2011, 2012 Ivan Cukic <ivan.cukic(at)kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "info.h"
#include "info_p.h"
#include "manager_p.h"

#include <QFileSystemWatcher>

namespace KActivities {

// Private common

InfoPrivateCommon *InfoPrivateCommon::s_instance = Q_NULLPTR;

InfoPrivateCommon *InfoPrivateCommon::self()
{
    if (!s_instance) {
        s_instance = new InfoPrivateCommon();
    }

    return s_instance;
}

InfoPrivateCommon::InfoPrivateCommon()
{
}

InfoPrivateCommon::~InfoPrivateCommon()
{
}

// Private

InfoPrivate::InfoPrivate(Info *info, const QString &activity)
    : q(info)
    , state(Info::Invalid)
    , nameCallWatcher(Q_NULLPTR)
    , iconCallWatcher(Q_NULLPTR)
    , id(activity)
{
    if (Manager::isServicePresent()) {
        initializeCachedData();
    }
}

// Filters out signals for only this activity
#define IMPLEMENT_SIGNAL_HANDLER(ORIGINAL, INTERNAL)     \
    void InfoPrivate::INTERNAL(const QString &_id) const \
    {                                                    \
        if (id == _id)                                   \
            emit q->INTERNAL();                          \
    }

IMPLEMENT_SIGNAL_HANDLER(ActivityAdded, added)
IMPLEMENT_SIGNAL_HANDLER(ActivityRemoved, removed)
IMPLEMENT_SIGNAL_HANDLER(ActivityStarted, started)
IMPLEMENT_SIGNAL_HANDLER(ActivityStopped, stopped)
IMPLEMENT_SIGNAL_HANDLER(ActivityChanged, infoChanged)

#undef IMPLEMENT_SIGNAL_HANDLER

KAMD_RETRIEVE_REMOTE_VALUE_HANDLER(QString, InfoPrivate, name, QString())
KAMD_RETRIEVE_REMOTE_VALUE_HANDLER(QString, InfoPrivate, icon, QString())

void InfoPrivate::nameChanged(const QString &_id, const QString &_name) const
{
    if (id == _id) {
        name = _name;
        emit q->nameChanged(name);
    }
}

void InfoPrivate::iconChanged(const QString &_id, const QString &_icon) const
{
    if (id == _id) {
        icon = _icon;
        emit q->iconChanged(icon);
    }
}

void InfoPrivate::setServicePresent(bool present)
{
    if (present) {
        initializeCachedData();
    }
}

void InfoPrivate::activityStateChanged(const QString &idChanged, int newState)
{
    if (idChanged == id) {
        state = static_cast<Info::State>(newState);
        emit q->stateChanged(state);
    }
}

void InfoPrivate::initializeCachedData()
{
    KAMD_RETRIEVE_REMOTE_VALUE(name, ActivityName(id), q);
    KAMD_RETRIEVE_REMOTE_VALUE(icon, ActivityIcon(id), q);
}

// Info
Info::Info(const QString &activity, QObject *parent)
    : QObject(parent)
    , d(new InfoPrivate(this, activity))
{
    connect(Manager::activities(), SIGNAL(ActivityStateChanged(QString, int)),
            this, SLOT(activityStateChanged(QString, int)));

    connect(Manager::activities(), SIGNAL(ActivityChanged(QString)),
            this, SLOT(infoChanged(QString)));

    connect(Manager::activities(), SIGNAL(ActivityNameChanged(QString, QString)),
            this, SLOT(nameChanged(QString, QString)));

    connect(Manager::activities(), SIGNAL(ActivityIconChanged(QString, QString)),
            this, SLOT(iconChanged(QString, QString)));

    connect(Manager::activities(), SIGNAL(ActivityAdded(QString)),
            this, SLOT(added(QString)));

    connect(Manager::activities(), SIGNAL(ActivityRemoved(QString)),
            this, SLOT(removed(QString)));

    connect(Manager::activities(), SIGNAL(ActivityStarted(QString)),
            this, SLOT(started(QString)));

    connect(Manager::activities(), SIGNAL(ActivityStopped(QString)),
            this, SLOT(stopped(QString)));
}

Info::~Info()
{
}

bool Info::isValid() const
{
    return (state() != Invalid);
}

QString Info::uri() const
{
    return QStringLiteral("activities://") + d->id;
}

QString Info::id() const
{
    return d->id;
}

KAMD_REMOTE_VALUE_GETTER(QString, Info, name, QString());
//        i18nc("The name of the main activity - when the activity manager is not running", "Main"))

KAMD_REMOTE_VALUE_GETTER(QString, Info, icon, QString::fromLatin1("preferences-activities"))

Info::State Info::state() const
{
    if (d->state == Invalid) {
        QDBusReply<int> dbusReply = Manager::activities()->ActivityState(d->id);

        if (dbusReply.isValid()) {
            d->state = (State)(dbusReply.value());
        }
    }

    return d->state;
}

QString Info::name(const QString &id)
{
    KAMD_RETRIEVE_REMOTE_VALUE_SYNC(
        QString, activities, ActivityName(id), QString()
        // i18nc("The name of the main activity - when the activity manager is not running", "Main")
        );
}

Info::Availability Info::availability() const
{
    Availability result = Nothing;

    if (!Manager::isServicePresent()) {
        return result;
    }

    if (Manager::activities()->ListActivities().value().contains(d->id)) {
        result = BasicInfo;

        if (Manager::features()->IsFeatureOperational(QString::fromLatin1("org.kde.ActivityManager.Nepomuk/linking"))) {
            result = Everything;
        }
    }

    return result;
}

void Info::linkResource(const QString &resourceUri)
{
    Manager::resourcesLinking()->LinkResourceToActivity(resourceUri, d->id);
}

void Info::unlinkResource(const QString &resourceUri)
{
    Manager::resourcesLinking()->UnlinkResourceFromActivity(resourceUri, d->id);
}

bool Info::isResourceLinked(const QString &resourceUri)
{
    return Manager::resourcesLinking()->IsResourceLinkedToActivity(resourceUri, d->id);
}

} // namespace KActivities

#include "moc_info.cpp"
#include "moc_info_p.cpp"
