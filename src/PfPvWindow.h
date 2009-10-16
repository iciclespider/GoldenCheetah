/*
 * Copyright (c) 2009 Sean C. Rhea (srhea@srhea.net)
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

#ifndef _GC_PfPvWindow_h
#define _GC_PfPvWindow_h 1

#include <QWidget>

class PfPvPlot;
class QCheckBox;
class QLineEdit;
class RideItem;

class PfPvWindow : public QWidget
{
    Q_OBJECT

    public:

        PfPvWindow(QWidget *parent);
        void setData(RideItem *item);
        void zonesChanged();

    protected slots:

        void setQaCPFromLineEdit();
        void setQaCADFromLineEdit();
        void setQaCLFromLineEdit();
        void setShadeZonesPfPvFromCheckBox();

    protected:

        PfPvPlot *pfPvPlot;
        QCheckBox *shadeZonesPfPvCheckBox;
        QLineEdit *qaCPValue;
        QLineEdit *qaCadValue;
        QLineEdit *qaClValue;
};

#endif // _GC_PfPvWindow_h
