# To build, copy this file to gcconfig.pri and then fill in the paths to your
# local installs of Boost and srmio in the copy.  If you don't want
# support for SRM downloads, just comment out the SRMIO_INSTALL line.

BOOST_INSTALL = /usr/local/boost
#SRMIO_INSTALL = /usr/local/srmio
#D2XX_INCLUDE = /usr/local/include/D2XX
D2XX_INCLUDE = ../../libftd2xx0.4.16_x86_64

# If you want 3D plotting, you need to install qwtplot3d
#
#  http://qwtplot3d.sourceforge.net/
#
# then set the following variable appropriately:

#QWT3D_INSTALL = /usr/local/qwtplot3d

# We recommend a debug build for development, and a static build for releases.
CONFIG += debug
#CONFIG += static

# Edit these paths only if you have a Boost/srmio install that uses
# a non-standard directory layout.

BOOST_INCLUDE = $${BOOST_INSTALL}/include

!isEmpty( SRMIO_INSTALL ) {
    SRMIO_INCLUDE = $${SRMIO_INSTALL}/include
    SRMIO_LIB = $${SRMIO_INSTALL}/lib/libsrmio.a
}

macx {
    # Uncomment this line to build with OS X Tiger support on a Leopard system:
    #QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk

    # Uncomment this line to build a OS X universal binary:
    #CONFIG+=x86 ppc
}

