/*
 * Copyright (c) 2007 Sean C. Rhea (srhea@srhea.net)
 *               2009 Justin F. Knotzke (jknotzke@shampoo.ca)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "RideFile.h"
#include "Settings.h"
#include "Units.h"
#include <QtXml/QtXml>
#include <algorithm> // for std::lower_bound
#include <assert.h>

#define mark() \
{ \
    addInterval(start, previous->secs + recIntSecs_, \
                QString("%1").arg(interval)); \
    interval = point->interval; \
    start = point->secs; \
}
void
RideFile::clearIntervals()
{
    intervals_.clear();
}

void
RideFile::fillInIntervals()
{
    if (dataPoints_.empty())
        return;
    intervals_.clear();
    double start = 0.0;
    int interval = dataPoints().first()->interval;
    const RideFilePoint *point, *previous;
    foreach (point, dataPoints()) {
        if (point->interval != interval)
            mark();
        previous = point;
    }
    if (interval > 0)
        mark();
}

struct ComparePointKm {
    bool operator()(const RideFilePoint *p1, const RideFilePoint *p2) {
        return p1->km < p2->km;
    }
};

struct ComparePointSecs {
    bool operator()(const RideFilePoint *p1, const RideFilePoint *p2) {
        return p1->secs < p2->secs;
    }
};

int
RideFile::intervalBegin(const RideFileInterval &interval) const
{
    RideFilePoint p;
    p.secs = interval.start;
    QVector<RideFilePoint*>::const_iterator i = std::lower_bound(
        dataPoints_.begin(), dataPoints_.end(), &p, ComparePointSecs());
    if (i == dataPoints_.end())
        return dataPoints_.size();
    return i - dataPoints_.begin();
}

double
RideFile::timeToDistance(double secs) const
{
    RideFilePoint p;
    p.secs = secs;

    // Check we have some data and the secs is in bounds
    if (dataPoints_.isEmpty()) return 0;
    if (secs < dataPoints_.first()->secs) return dataPoints_.first()->km;
    if (secs > dataPoints_.last()->secs) return dataPoints_.last()->km;

    QVector<RideFilePoint*>::const_iterator i = std::lower_bound(dataPoints_.begin(), dataPoints_.end(), &p, ComparePointSecs());
    return (*i)->km;
}

int
RideFile::timeIndex(double secs) const
{
    // return index offset for specified time
    RideFilePoint p;
    p.secs = secs;

    QVector<RideFilePoint*>::const_iterator i = std::lower_bound(
        dataPoints_.begin(), dataPoints_.end(), &p, ComparePointSecs());
    if (i == dataPoints_.end())
        return dataPoints_.size();
    return i - dataPoints_.begin();
}

int
RideFile::distanceIndex(double km) const
{
    // return index offset for specified distance in km
    RideFilePoint p;
    p.km = km;

    QVector<RideFilePoint*>::const_iterator i = std::lower_bound(
        dataPoints_.begin(), dataPoints_.end(), &p, ComparePointKm());
    if (i == dataPoints_.end())
        return dataPoints_.size();
    return i - dataPoints_.begin();
}

void RideFile::writeAsCsv(QFile &file, bool bIsMetric) const
{

    // Use the column headers that make WKO+ happy.
    double convertUnit;
    QTextStream out(&file);
    if (!bIsMetric)
    {
        out << "Minutes,Torq (N-m),MPH,Watts,Miles,Cadence,Hrate,ID,Altitude (feet)\n";
        convertUnit = MILES_PER_KM;
    }
    else {
        out << "Minutes,Torq (N-m),Km/h,Watts,Km,Cadence,Hrate,ID,Altitude (m)\n";
        convertUnit = 1.0;
    }

    foreach (const RideFilePoint *point, dataPoints()) {
        if (point->secs == 0.0)
            continue;
        out << point->secs/60.0;
        out << ",";
        out << ((point->nm >= 0) ? point->nm : 0.0);
        out << ",";
        out << ((point->kph >= 0) ? (point->kph * convertUnit) : 0.0);
        out << ",";
        out << ((point->watts >= 0) ? point->watts : 0.0);
        out << ",";
        out << point->km * convertUnit;
        out << ",";
        out << point->cad;
        out << ",";
        out << point->hr;
        out << ",";
        out << point->interval;
        out << ",";
        out << point->alt;
        out << "\n";
    }

    file.close();
}

RideFileFactory *RideFileFactory::instance_;

RideFileFactory &RideFileFactory::instance()
{
    if (!instance_)
        instance_ = new RideFileFactory();
    return *instance_;
}

int RideFileFactory::registerReader(const QString &suffix,
                                    const QString &description,
                                       RideFileReader *reader)
{
    assert(!readFuncs_.contains(suffix));
    readFuncs_.insert(suffix, reader);
    descriptions_.insert(suffix, description);
    return 1;
}

QStringList RideFileFactory::suffixes() const
{
    return readFuncs_.keys();
}

QRegExp
RideFileFactory::rideFileRegExp() const
{
    QStringList suffixList = RideFileFactory::instance().suffixes();
    QString s("^(\\d\\d\\d\\d)_(\\d\\d)_(\\d\\d)_(\\d\\d)_(\\d\\d)_(\\d\\d)\\.(%1)$");
    return QRegExp(s.arg(suffixList.join("|")));
}

RideFile *RideFileFactory::openRideFile(QFile &file,
                                           QStringList &errors) const
{
    QString suffix = file.fileName();
    int dot = suffix.lastIndexOf(".");
    assert(dot >= 0);
    suffix.remove(0, dot + 1);
    RideFileReader *reader = readFuncs_.value(suffix.toLower());
    assert(reader);
    RideFile *result = reader->openRideFile(file, errors);
    if (result && result->intervals().empty())
        result->fillInIntervals();
    return result;
}

QStringList RideFileFactory::listRideFiles(const QDir &dir) const
{
    QStringList filters;
    QMapIterator<QString,RideFileReader*> i(readFuncs_);
    while (i.hasNext()) {
        i.next();
        filters << ("*." + i.key());
    }
    // This will read the user preferences and change the file list order as necessary:
    boost::shared_ptr<QSettings> settings = GetApplicationSettings();;

    QVariant isAscending = settings->value(GC_ALLRIDES_ASCENDING,Qt::Checked);
    if(isAscending.toInt()>0){
        return dir.entryList(filters, QDir::Files, QDir::Name);
    }
    return dir.entryList(filters, QDir::Files, QDir::Name|QDir::Reversed);
}

void RideFile::appendPoint(double secs, double cad, double hr, double km,
                           double kph, double nm, double watts, double alt,
                           double lon, double lat, double headwind, int interval)
{
    dataPoints_.append(new RideFilePoint(secs, cad, hr, km, kph,
                                         nm, watts, alt, lon, lat, headwind, interval,
                                         nmAdjust_));
    dataPresent.secs  |= (secs != 0);
    dataPresent.cad   |= (cad != 0);
    dataPresent.hr    |= (hr != 0);
    dataPresent.km    |= (km != 0);
    dataPresent.kph   |= (kph != 0);
    dataPresent.nm    |= (nm != 0);
    dataPresent.watts |= (watts != 0);
    dataPresent.alt   |= (alt != 0);
    dataPresent.lon   |= (lon != 0);
    dataPresent.lat   |= (lat != 0);
    dataPresent.headwind |= (headwind != 0);
    dataPresent.interval |= (interval != 0);
}

void RideFile::setTorqueAdjust(const QString &value)
{
    torqueAdjust_ = value.trimmed();
    // Allow the entered value to end with either "pi"
    // for Pound Inch or with "nm" for Newtom Meter.
    bool pi = torqueAdjust_.endsWith("pi", Qt::CaseInsensitive);
    if (pi || torqueAdjust_.endsWith("nm", Qt::CaseInsensitive)) {
        nmAdjust_ = torqueAdjust_.left(value.length() - 2).toDouble();
        if (pi) {
            nmAdjust_ *= 0.11298482933;
        }
    } else {
        nmAdjust_ = torqueAdjust_.toDouble();
    }
    foreach (RideFilePoint *point, dataPoints_) {
        point->setNmAdjust(nmAdjust_);
    }
}
