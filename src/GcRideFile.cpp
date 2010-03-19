/*
 * Copyright (c) 2009 Sean C. Rhea (srhea@srhea.net),
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

#include "GcRideFile.h"
#include <algorithm> // for std::sort
#include <QXmlSimpleReader>
#include <QXmlInputSource>
#include <QXmlDefaultHandler>
#include <QDomDocument>
#include <QVector>
#include <assert.h>

#define DATETIME_FORMAT "yyyy/MM/dd hh:mm:ss' UTC'"

static int gcFileReaderRegistered =
    RideFileFactory::instance().registerReader(
        "gc", "GoldenCheetah Native Format", new GcFileReader());

class GcXmlHandler: public QXmlDefaultHandler
{
    private:
        RideFile *rideFile;
        QStringList &errors;
        QString group;
        QVector<double> intervalStops; // used to set the interval number for each point
        int interval;
        bool recIntSet;
    public:
        GcXmlHandler(RideFile *rideFile, QStringList &errors) : rideFile(rideFile), errors(errors) {}
        bool startElement(const QString&, const QString&, const QString&, const QXmlAttributes&);
        bool hasSamples() { return recIntSet; }
};

bool
GcXmlHandler::startElement(const QString &, const QString &localName, const QString &, const QXmlAttributes &atts)
{
    if (localName == "attributes") {
        group = localName;
        return TRUE;
    }
    if (localName == "intervals") {
        group = localName;
        return TRUE;
    }
    if (localName == "samples") {
        group = localName;
        std::sort(intervalStops.begin(), intervalStops.end()); // just in case
        interval = 0;
        recIntSet = false;
        return TRUE;
    }

    if (group == "attributes" && localName == "attribute") {
        QString key = atts.value("key");
        QString value = atts.value("value");
        if (key == "Device type") {
            rideFile->setDeviceType(value);
        } else if (key == "Start time") {
            // by default QDateTime is localtime - the source however is UTC
            QDateTime aslocal = QDateTime::fromString(value, DATETIME_FORMAT);
            // construct in UTC so we can honour the conversion to localtime
            QDateTime asUTC = QDateTime(aslocal.date(), aslocal.time(), Qt::UTC);
            // now set in localtime
            rideFile->setStartTime(asUTC.toLocalTime());
        } else if (key == "NM adjust") {
            rideFile->setNmAdjust(value.toDouble());
        } else if (key == "PI adjust") {
            rideFile->setNmAdjust(value.toDouble() * 0.11298482933);
        }
        return TRUE;
    }

    if (group == "intervals" && localName == "interval") {
        // record the stops for old-style datapoint interval numbering
        double stop = atts.value("stop").toDouble();
        intervalStops.append(stop);
        rideFile->addInterval(atts.value("start").toDouble(), stop, atts.value("name"));
        return TRUE;
    }

    if (group == "samples" && localName == "sample") {
        double secs = atts.value("secs").toDouble();
        double cad = atts.value("cad").toDouble();
	double hr = atts.value("hr").toDouble();
	double km = atts.value("km").toDouble();
	double kph = atts.value("kph").toDouble();
	double nm = atts.value("nm").toDouble();
	double watts = atts.value("watts").toDouble();
	double alt = atts.value("alt").toDouble();
	double lon = atts.value("lon").toDouble();
	double lat = atts.value("lat").toDouble();
	double headwind = 0.0;
	while ((interval < intervalStops.size()) && (secs >= intervalStops[interval])) {
	    ++interval;
	}
	rideFile->appendPoint(secs, cad, hr, km, kph, nm, watts, alt, lon, lat, headwind, interval);
	if (!recIntSet) {
	    rideFile->setRecIntSecs(atts.value("len").toDouble());
	    recIntSet = true;
	}
	return TRUE;
    }

    return TRUE;
}


RideFile *
GcFileReader::openRideFile(QFile &file, QStringList &errors) const
{
    if (!file.open(QIODevice::ReadOnly)) {
        errors << "Could not open file.";
        return NULL;
    }
    RideFile *rideFile = new RideFile();
    GcXmlHandler handler(rideFile, errors);
    QXmlSimpleReader reader;
    reader.setContentHandler(&handler);
    QXmlInputSource source(&file);
    bool parsed = reader.parse(source);
    file.close();
    if (!parsed) {
        delete rideFile;
	errors << "Could not parse file.";
	return NULL;
    }
    if (!handler.hasSamples()) {
        delete rideFile;
	errors << "No samples in ride file.";
	return NULL;
    }
    return rideFile;
}


#define add_sample(name) \
    if (present->name) \
        sample.setAttribute(#name, QString("%1").arg(point->name));
#define add_sample_org(name, name_org)			\
    if (present->name) \
        sample.setAttribute(#name, QString("%1").arg(point->name_org));

void
GcFileReader::writeRideFile(const RideFile *ride, QFile &file) const
{
    QDomDocument doc("GoldenCheetah");
    QDomElement root = doc.createElement("ride");
    doc.appendChild(root);

    QDomElement attributes = doc.createElement("attributes");
    root.appendChild(attributes);

    QDomElement attribute = doc.createElement("attribute");
    attributes.appendChild(attribute);
    attribute.setAttribute("key", "Start time");
    attribute.setAttribute(
        "value", ride->startTime().toUTC().toString(DATETIME_FORMAT));
    attribute = doc.createElement("attribute");
    attributes.appendChild(attribute);
    attribute.setAttribute("key", "Device type");
    attribute.setAttribute("value", ride->deviceType());
    if (ride->nmAdjust() != 0.0) {
        attribute = doc.createElement("attribute");
        attributes.appendChild(attribute);
        attribute.setAttribute("key", "NM adjust");
        attribute.setAttribute("value", ride->nmAdjust());
    }

    if (!ride->intervals().empty()) {
        QDomElement intervals = doc.createElement("intervals");
        root.appendChild(intervals);
        foreach (RideFileInterval i, ride->intervals()) {
            QDomElement interval = doc.createElement("interval");
            intervals.appendChild(interval);
            interval.setAttribute("name", i.name);
            interval.setAttribute("start", QString("%1").arg(i.start));
            interval.setAttribute("stop", QString("%1").arg(i.stop));
        }
    }

    if (!ride->dataPoints().empty()) {
        QDomElement samples = doc.createElement("samples");
        root.appendChild(samples);
        const RideFileDataPresent *present = ride->areDataPresent();
        assert(present->secs);
        foreach (const RideFilePoint *point, ride->dataPoints()) {
            QDomElement sample = doc.createElement("sample");
            samples.appendChild(sample);
            assert(present->secs);
            add_sample(secs);
            add_sample(cad);
            add_sample(hr);
            add_sample(km);
            add_sample(kph);
            add_sample_org(nm, nm_org);
            add_sample_org(watts, watts_org);
            add_sample(alt);
            add_sample(lon);
            add_sample(lat);
            sample.setAttribute("len", QString("%1").arg(ride->recIntSecs()));
        }
    }

    QByteArray xml = doc.toByteArray(4);
    if (!file.open(QIODevice::WriteOnly))
        assert(false);
    if (file.write(xml) != xml.size())
        assert(false);
    file.close();
}

