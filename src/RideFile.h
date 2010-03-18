/*
 * Copyright (c) 2007 Sean C. Rhea (srhea@srhea.net)
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

#ifndef _RideFile_h
#define _RideFile_h

#include <QDate>
#include <QDir>
#include <QFile>
#include <QList>
#include <QMap>
#include <QVector>

// This file defines four classes:
//
// RideFile, as the name suggests, represents the data stored in a ride file,
// regardless of what type of file it is (.raw, .srm, .csv).
//
// RideFilePoint represents the data for a single sample in a RideFile.
//
// RideFileReader is an abstract base class for function-objects that take a
// filename and return a RideFile object representing the ride stored in the
// corresponding file.
//
// RideFileFactory is a singleton that maintains a mapping from ride file
// suffixes to the RideFileReader objects capable of converting those files
// into RideFile objects.

struct RideFilePoint
{
    double secs, cad, hr, km, kph, nm, watts, alt, lon, lat, headwind;;
    int interval;
    double nm_org, watts_org;
    RideFilePoint() : secs(0.0), cad(0.0), hr(0.0), km(0.0), kph(0.0),
        nm(0.0), watts(0.0), alt(0.0), lon(0.0), lat(0.0), headwind(0.0), interval(0),
        nm_org(0.0), watts_org(0.0) {}
    RideFilePoint(double secs, double cad, double hr, double km, double kph,
                  double nm, double watts, double alt, double lon, double lat, double headwind, int interval,
                  double nmAdjust) :
        secs(secs), cad(cad), hr(hr), km(km), kph(kph), nm(nm),
        watts(watts), alt(alt), lon(lon), lat(lat), headwind(headwind), interval(interval),
        nm_org(nm), watts_org(watts) {
            setNmAdjust(nmAdjust);
    }
    void setNmAdjust(double nmAdjust) {
        if (nm_org != 0) {
            nm = nm_org + nmAdjust;
            watts = watts_org * (nm / nm_org);
        }
    }
};

struct RideFileDataPresent
{
    bool secs, cad, hr, km, kph, nm, watts, alt, lon, lat, headwind, interval;
    // whether non-zero data of each field is present
    RideFileDataPresent():
        secs(false), cad(false), hr(false), km(false),
        kph(false), nm(false), watts(false), alt(false), lon(false), lat(false), headwind(false), interval(false) {}
};

struct RideFileInterval
{
    double start, stop;
    QString name;
    RideFileInterval() : start(0.0), stop(0.0) {}
    RideFileInterval(double start, double stop, QString name) :
        start(start), stop(stop), name(name) {}
};

class RideFile
{
    private:

        QDateTime startTime_;  // time of day that the ride started
        double recIntSecs_;    // recording interval in seconds
        QVector<RideFilePoint*> dataPoints_;
        RideFileDataPresent dataPresent;
        QString deviceType_;
        QList<RideFileInterval> intervals_;
        double nmAdjust_;      // adjustment of torque newton meters

    public:

 RideFile() : recIntSecs_(0.0), deviceType_("unknown"), nmAdjust_(0.0) {}
        RideFile(const QDateTime &startTime, double recIntSecs) :
            startTime_(startTime), recIntSecs_(recIntSecs),
            deviceType_("unknown"), nmAdjust_(0.0) {}

        virtual ~RideFile() {
            foreach(RideFilePoint *point, dataPoints_)
                delete point;
        }

        const QDateTime &startTime() const { return startTime_; }
        double recIntSecs() const { return recIntSecs_; }
        const QVector<RideFilePoint*> dataPoints() const { return dataPoints_; }
        inline const RideFileDataPresent *areDataPresent() const { return &dataPresent; }
        const QString &deviceType() const { return deviceType_; }
        double nmAdjust() const { return nmAdjust_; }

        void setStartTime(const QDateTime &value) { startTime_ = value; }
        void setRecIntSecs(double value) { recIntSecs_ = value; }
        void setDeviceType(const QString &value) { deviceType_ = value; }
        void setNmAdjust(double nmAdjust) {
            nmAdjust_ = nmAdjust;
            foreach (RideFilePoint *point, dataPoints_) {
                point->setNmAdjust(nmAdjust);
            }
        }

        void appendPoint(double secs, double cad, double hr, double km,
                         double kph, double nm, double watts, double alt,
                         double lon, double lat, double headwind, int interval);

        const QList<RideFileInterval> &intervals() const { return intervals_; }
        void addInterval(double start, double stop, const QString &name) {
            intervals_.append(RideFileInterval(start, stop, name));
        }
        void clearIntervals();
        void fillInIntervals();
        int intervalBegin(const RideFileInterval &interval) const;

        void writeAsCsv(QFile &file, bool bIsMetric) const;

        void resetDataPresent();

        double timeToDistance(double) const;  // get distance in km at time in secs
        int timeIndex(double) const;          // get index offset for time in secs
        int distanceIndex(double) const;      // get index offset for distance in KM

        QMap<QString,QMap<QString,QString> > metricOverrides;
};

struct RideFileReader {
    virtual ~RideFileReader() {}
    virtual RideFile *openRideFile(QFile &file, QStringList &errors) const = 0;
};

class RideFileFactory {

    private:

        static RideFileFactory *instance_;
        QMap<QString,RideFileReader*> readFuncs_;
        QMap<QString,QString> descriptions_;

        RideFileFactory() {}

    public:

        static RideFileFactory &instance();

        int registerReader(const QString &suffix, const QString &description,
                           RideFileReader *reader);
        RideFile *openRideFile(QFile &file, QStringList &errors) const;
        QStringList listRideFiles(const QDir &dir) const;
        QStringList suffixes() const;
        QString description(const QString &suffix) const {
            return descriptions_[suffix];
        }
        QRegExp rideFileRegExp() const;
};

#endif // _RideFile_h

