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
#include <QXmlStreamReader>
#include <QVector>
#include <QDomDocument>
#include <assert.h>

#define DATETIME_FORMAT "yyyy/MM/dd hh:mm:ss' UTC'"

static int gcFileReaderRegistered =
    RideFileFactory::instance().registerReader(
        "gc", "GoldenCheetah Native Format", new GcFileReader());


RideFile *
GcFileReader::openRideFile(QFile &file, QStringList &errors) const
{
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors << "Could not open file.";
        return NULL;
    }
    QXmlStreamReader reader(&file);
    
    RideFile *rideFile = new RideFile();
    QStringRef group;
    QVector<double> intervalStops; // used to set the interval number for each point
    RideFileInterval add;          // used to add each named interval to RideFile
    int interval;
    bool recIntSet;

    while (!reader.atEnd()) {
        reader.readNext();
	if (!reader.isStartElement()) {
	  continue;
	}
	QStringRef name = reader.name();
	if (name == "attributes") {
	  group = name;
	  continue;
	}
	if (name == "intervals") {
	  group = name;
	  continue;
	}
	if (name == "samples") {
	  group = name;
	  std::sort(intervalStops.begin(), intervalStops.end()); // just in case
	  interval = 0;
	  recIntSet = false;
	  continue;
	}

	if (group == "attributes" && name == "attribute") {
	  QStringRef key = reader.attributes().value("key");
	  QStringRef value = reader.attributes().value("value");
	  if (key == "Device type") {
            rideFile->setDeviceType(value.toString());
	  } else if (key == "Start time") {
            // by default QDateTime is localtime - the source however is UTC
            QDateTime aslocal = QDateTime::fromString(value.toString(), DATETIME_FORMAT);
            // construct in UTC so we can honour the conversion to localtime
            QDateTime asUTC = QDateTime(aslocal.date(), aslocal.time(), Qt::UTC);
            // now set in localtime
            rideFile->setStartTime(asUTC.toLocalTime());
	  } else if (key == "NM adjust") {
            rideFile->setNmAdjust(value.toString().toDouble());
	  } else if (key == "PI adjust") {
            rideFile->setNmAdjust(value.toString().toDouble() * 0.11298482933);
	  }
	  continue;
	}

	if (group == "intervals" && name == "interval") {
	  // record the stops for old-style datapoint interval numbering
	  double stop = reader.attributes().value("stop").toString().toDouble();
	  intervalStops.append(stop);
	  rideFile->addInterval(reader.attributes().value("start").toString().toDouble(), stop, reader.attributes().value("name").toString());
	  continue;
	}

	if (group == "samples" && name == "sample") {
	  double secs = reader.attributes().value("secs").toString().toDouble();
	  double cad = reader.attributes().value("cad").toString().toDouble();
	  double hr = reader.attributes().value("hr").toString().toDouble();
	  double km = reader.attributes().value("km").toString().toDouble();
	  double kph = reader.attributes().value("kph").toString().toDouble();
	  double nm = reader.attributes().value("nm").toString().toDouble();
	  double watts = reader.attributes().value("watts").toString().toDouble();
	  double alt = reader.attributes().value("alt").toString().toDouble();
	  double lon = reader.attributes().value("lon").toString().toDouble();
	  double lat = reader.attributes().value("lat").toString().toDouble();
	  double headwind = 0.0;
	  while ((interval < intervalStops.size()) && (secs >= intervalStops[interval])) {
            ++interval;
	  }
	  rideFile->appendPoint(secs, cad, hr, km, kph, nm, watts, alt, lon, lat, headwind, interval);
	  if (!recIntSet) {
            rideFile->setRecIntSecs(reader.attributes().value("len").toString().toDouble());
            recIntSet = true;
	  }
	  continue;
	}
    }
    if (reader.hasError()) {
        delete rideFile;
        errors << "Could not parse file: " << reader.errorString();
        return NULL;
    }
    if (!recIntSet) {
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

