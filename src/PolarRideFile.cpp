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

#include "PolarRideFile.h"
#include <QRegExp>
#include <QTextStream>
#include <algorithm> // for std::sort
#include <assert.h>
#include "math.h"


static int polarFileReaderRegistered =
    RideFileFactory::instance().registerReader(
        "hrm", "Polar Precision", new PolarFileReader());

RideFile *PolarFileReader::openRideFile(QFile &file, QStringList &errors) const
{
    QRegExp metricUnits("(km|kph|km/h)", Qt::CaseInsensitive);
    QRegExp englishUnits("(miles|mph|mp/h)", Qt::CaseInsensitive);
//    bool metric;

    QDate date;
    QString note("");

    double version=0;

    double seconds=0;
    double distance=0;
    int interval = 0;

    bool speed = false;
    bool cadence = false;
    bool altitude = false;
    bool power = false;
    bool balance = false;
    bool pedaling_index = false;


    int recInterval = 1;

    if (!file.open(QFile::ReadOnly)) {
        errors << ("Could not open ride file: \""
                   + file.fileName() + "\"");
        return NULL;
    }

    int lineno = 1;


    double next_interval=0;
    QList<double> intervals;

    QTextStream is(&file);
    RideFile *rideFile = new RideFile();
    QString section = NULL;

    while (!is.atEnd()) {
        // the readLine() method doesn't handle old Macintosh CR line endings
        // this workaround will load the the entire file if it has CR endings
        // then split and loop through each line
        // otherwise, there will be nothing to split and it will read each line as expected.
        QString linesIn = is.readLine();
        QStringList lines = linesIn.split('\r');
        // workaround for empty lines
        if(lines.size() == 0) {
            lineno++;
            continue;
        }
        for (int li = 0; li < lines.size(); ++li) {
            QString line = lines[li];

            if (line == "") {

            }
            else if (line.startsWith("[")) {
                //fprintf(stderr, "section : %s\n", line.toAscii().constData());
                section=line;
                if (section == "[HRData]")
                   next_interval = intervals.at(0);
            }
            else if (section == "[Params]"){
                if (line.contains("Version=")) {
                    QString versionString = QString(line);
                    versionString.remove(0,8).insert(1, ".");
                    version = versionString.toFloat();
                    rideFile->setDeviceType("Polar HRM (v"+versionString+")");

                } else if (line.contains("SMode=")) {
                    line.remove(0,6);
                    QString smode = QString(line);
                    if (smode.at(0)=='1')
                        speed = true;
                    if (smode.length()>0 && smode.at(1)=='1')
                        cadence = true;
                    if (smode.length()>1 && smode.at(2)=='1')
                        altitude = true;
                    if (smode.length()>2 && smode.at(3)=='1')
                        power = true;
                    if (smode.length()>3 && smode.at(4)=='1')
                        balance = true;
                    if (smode.length()>4 && smode.at(5)=='1')
                        pedaling_index = true;

/*
It appears that the Polar CS600 exports its data alays in metric when downloaded from the 
polar software even when English units are displayed on the unit..  It also never sets 
this bit low in the .hrm file.  This will have to get changed if other software downloads 
this differently
*/

//                    if (smode.length()>6 && smode.at(7)=='1')
//                        metric = true;

                } else if (line.contains("Interval=")) {
                    recInterval = line.remove(0,9).toInt();
                    rideFile->setRecIntSecs(recInterval);
                } else if (line.contains("Date=")) {
                    line.remove(0,5);
                    date= QDate(line.left(4).toInt(),
                                line.mid(4,2).toInt(),
                                line.mid(6,2).toInt());
                } else if (line.contains("StartTime=")) {
                    line.remove(0,10);
                    QDateTime datetime(date,
                                      QTime(line.left(2).toInt(),
                                            line.mid(3,2).toInt(),
                                            line.mid(6,2).toInt()));
                    rideFile->setStartTime(datetime);
                 }


            }
            else if (section == "[Note]"){
                note.append(line);
            }
            else if (section == "[IntTimes]"){
                double int_seconds = line.left(2).toInt()*60*60+line.mid(3,2).toInt()*60+line.mid(6,3).toFloat();
                intervals.append(int_seconds);

                if (lines.size()==1) {
                   is.readLine();
                   is.readLine();
                   if (version>1.05) {
                       is.readLine();
                       is.readLine();
                   }
                } else {
                   li+=2;
                   if (version>1.05)
                      li+=2;
                }
            }
            else if (section == "[HRData]"){
                double nm=0,kph=0,watts=0,km=0,cad=0,hr=0,alt=0;

                seconds += recInterval;

                int i=0;
                hr = line.section('\t', i, i).toDouble();
                i++;

                if (speed) {
                    kph = line.section('\t', i, i).toDouble()/10;
                    i++;
                }
                if (cadence) {
                    cad = line.section('\t', i, i).toDouble();
                    i++;
                }
                if (altitude) {
                    alt = line.section('\t', i, i).toDouble();
                    i++;
                }
                if (power) {
                    watts = line.section('\t', i, i).toDouble();
                }

                distance = distance + kph/60/60*recInterval;
                km = distance;

                if (next_interval < seconds) {
                    interval = intervals.indexOf(next_interval);
                    if (intervals.count()>interval+1){
                        interval++;
                        next_interval = intervals.at(interval);
                    }
                }
	            rideFile->appendPoint(seconds, cad, hr, km, kph, nm, watts, alt, 0.0, 0.0, 0.0, interval);
	            //fprintf(stderr, " %f, %f, %f, %f, %f, %f, %f, %d\n", seconds, cad, hr, km, kph, nm, watts, alt, interval);
            }

        ++lineno;
        }
    }

    QRegExp rideTime("^.*/(\\d\\d\\d\\d)_(\\d\\d)_(\\d\\d)_"
                     "(\\d\\d)_(\\d\\d)_(\\d\\d)\\.hrm$");
    if (rideTime.indexIn(file.fileName()) >= 0) {
        QDateTime datetime(QDate(rideTime.cap(1).toInt(),
                                 rideTime.cap(2).toInt(),
                                 rideTime.cap(3).toInt()),
                           QTime(rideTime.cap(4).toInt(),
                                 rideTime.cap(5).toInt(),
                                 rideTime.cap(6).toInt()));


        rideFile->setStartTime(datetime);
    }
    file.close();

    return rideFile;
}

