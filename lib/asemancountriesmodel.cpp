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

#include "asemancountriesmodel.h"

#include <QFile>
#include <QStringList>
#include <QHash>
#include <QLocale>
#include <QTimeZone>
#include <QDebug>

class AsemanCountriesModelPrivate
{
public:
    QMap<QString, QHash<QString,QString> > data;
    QStringList fullList;
    QList<QString> list;
    QString filter;
    QString systemCountry;
};

AsemanCountriesModel::AsemanCountriesModel(QObject *parent) :
    AsemanAbstractListModel(parent)
{
    p = new AsemanCountriesModelPrivate;
    Q_INIT_RESOURCE(asemanresource_lib);
    init_buff();
}

QString AsemanCountriesModel::id(const QModelIndex &index) const
{
    int row = index.row();
    return p->list.at(row);
}

int AsemanCountriesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return p->list.count();
}

QVariant AsemanCountriesModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    const QString & key = id(index);
    switch( role )
    {
    case Qt::DisplayRole:
    case NameRole:
        res = p->data[key]["name"];
        break;

    case NativeNameRole:
        res = p->data[key]["nativeName"];
        break;

    case TldRole:
        res = p->data[key]["tld"];
        break;

    case Cca2Role:
        res = p->data[key]["cca2"];
        break;

    case Ccn3Role:
        res = p->data[key]["ccn3"];
        break;

    case Cca3Role:
        res = p->data[key]["cca3"];
        break;

    case CurrencyRole:
        res = p->data[key]["currency"];
        break;

    case CallingCodeRole:
        res = p->data[key]["callingCode"];
        break;

    case CapitalRole:
        res = p->data[key]["capital"];
        break;

    case AltSpellingsRole:
        res = p->data[key]["altSpellings"];
        break;

    case RelevanceRole:
        res = p->data[key]["relevance"];
        break;

    case RegionRole:
        res = p->data[key]["region"];
        break;

    case SubregionRole:
        res = p->data[key]["subregion"];
        break;

    case LanguageRole:
        res = p->data[key]["language"];
        break;

    case LanguageCodesRole:
        res = p->data[key]["languageCodes"];
        break;

    case TranslationsRole:
        res = p->data[key]["translations"];
        break;

    case LatlngRole:
        res = p->data[key]["latlng"];
        break;

    case DemonymRole:
        res = p->data[key]["demonym"];
        break;

    case BordersRole:
        res = p->data[key]["borders"];
        break;

    case AreaRole:
        res = p->data[key]["area"];
        break;

    case KeyRole:
        res = key;
        break;
    }

    return res;
}

QHash<qint32, QByteArray> AsemanCountriesModel::roleNames() const
{
    static QHash<qint32, QByteArray> *res = 0;
    if( res )
        return *res;

    res = new QHash<qint32, QByteArray>();
    res->insert( NameRole, "name");
    res->insert( NativeNameRole, "nativeName");
    res->insert( TldRole, "tld");
    res->insert( Cca2Role, "cca2");
    res->insert( Ccn3Role, "ccn3");
    res->insert( Cca3Role, "cca3");
    res->insert( CurrencyRole, "currency");
    res->insert( CallingCodeRole, "callingCode");
    res->insert( CapitalRole, "capital");
    res->insert( AltSpellingsRole, "altSpellings");
    res->insert( RelevanceRole, "relevance");
    res->insert( RegionRole, "region");
    res->insert( SubregionRole, "subregion");
    res->insert( LanguageRole, "language");
    res->insert( LanguageCodesRole, "languageCodes");
    res->insert( TranslationsRole, "translations");
    res->insert( LatlngRole, "latlng");
    res->insert( DemonymRole, "demonym");
    res->insert( BordersRole, "borders");
    res->insert( AreaRole, "area");
    res->insert( KeyRole, "key");

    return *res;
}

int AsemanCountriesModel::count() const
{
    return p->list.count();
}

int AsemanCountriesModel::indexOf(const QString &name)
{
    return p->list.indexOf(name.toLower());
}

void AsemanCountriesModel::setFilter(const QString &filter)
{
    if(p->filter == filter)
        return;

    p->filter = filter;

    QStringList list = p->fullList;
    for(int i=0; i<list.count(); i++)
        if(!list.at(i).contains(filter.toLower()))
        {
            list.removeAt(i);
            i--;
        }

    changed(list);

    Q_EMIT filterChanged();
}

QString AsemanCountriesModel::filter() const
{
    return p->filter;
}

QString AsemanCountriesModel::systemCountry() const
{
    return p->systemCountry;
}

void AsemanCountriesModel::init_buff()
{
    QFile file(":/asemantools/files/countries.csv");
    if( !file.open(QFile::ReadOnly) )
    {
        qDebug() << __FUNCTION__ << "Can't load countries.csv file";
        return;
    }

    QString data = file.readAll();
    QStringList splits = data.split("\n",QString::SkipEmptyParts);
    if( splits.isEmpty() )
        return;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    QString country = QLocale::countryToString(QTimeZone::systemTimeZone().country()).toLower().trimmed().remove(" ");
#else
    QString country;
#endif
    QStringList heads = splits.takeFirst().split(";");

    for( const QString & s: splits )
    {
        const QStringList & parts = s.split(";");
        for( int i=0; i<parts.count(); i++ )
        {
            const QString & prt = parts.at(i);
            const QString & countryName = parts.first().toLower();
            if(countryName.toLower().trimmed().remove(" ") == country)
                p->systemCountry = countryName;

            p->data[countryName][heads.at(i)] = prt.split(",").first();
        }
    }

    p->fullList = p->data.keys();
    changed(p->fullList);
    Q_EMIT systemCountryChanged();
}

void AsemanCountriesModel::changed(const QStringList &list)
{
    bool count_changed = (list.count()!=p->list.count());

    for( int i=0 ; i<p->list.count() ; i++ )
    {
        const QString &item = p->list.at(i);
        if( list.contains(item) )
            continue;

        beginRemoveRows(QModelIndex(), i, i);
        p->list.removeAt(i);
        i--;
        endRemoveRows();
    }

    QList<QString> temp_list = list;
    for( int i=0 ; i<temp_list.count() ; i++ )
    {
        const QString &item = temp_list.at(i);
        if( p->list.contains(item) )
            continue;

        temp_list.removeAt(i);
        i--;
    }
    while( p->list != temp_list )
        for( int i=0 ; i<p->list.count() ; i++ )
        {
            const QString &item = p->list.at(i);
            int nw = temp_list.indexOf(item);
            if( i == nw )
                continue;

            beginMoveRows( QModelIndex(), i, i, QModelIndex(), nw>i?nw+1:nw );
            p->list.move( i, nw );
            endMoveRows();
        }

    for( int i=0 ; i<list.count() ; i++ )
    {
        const QString &item = list.at(i);
        if( p->list.contains(item) )
            continue;

        beginInsertRows(QModelIndex(), i, i );
        p->list.insert( i, item );
        endInsertRows();
    }

    if(count_changed)
        Q_EMIT countChanged();
}

AsemanCountriesModel::~AsemanCountriesModel()
{
    delete p;
}
