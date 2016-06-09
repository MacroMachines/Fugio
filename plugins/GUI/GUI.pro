#-------------------------------------------------
#
# Project created by QtCreator 2013-04-16T09:48:22
#
#-------------------------------------------------

QT += gui widgets

TARGET = $$qtLibraryTarget(fugio-gui)
TEMPLATE = lib
CONFIG += plugin
CONFIG += c++11

CONFIG(debug,debug|release) {
    DESTDIR = $$OUT_PWD/../../../deploy-debug/plugins
} else {
    DESTDIR = $$OUT_PWD/../../../deploy-release/plugins
}

DEFINES += BASENODES_LIBRARY

SOURCES += \
    floatnode.cpp \
    buttonnode.cpp \
    integernode.cpp \
    lcdnumbernode.cpp \
    keyboardnode.cpp \
    lednode.cpp \
    ledwidget.cpp \
    keyboarddialog.cpp \
    guiplugin.cpp \
    slidernode.cpp \
    keyboardpin.cpp \
    mainwindownode.cpp \
    choicenode.cpp \
    numbermonitorform.cpp \
    numbermonitornode.cpp

HEADERS += \
    floatnode.h \
    ../../include/fugio/gui/uuid.h \
    ../../include/fugio/playhead_interface.h \
    buttonnode.h \
    ../../include/fugio/nodecontrolbase.h \
    ../../include/fugio/pincontrolbase.h \
    integernode.h \
    lcdnumbernode.h \
    numbermonitorform.h \
    numbermonitornode.h \
    keyboardnode.h \
    lednode.h \
    ledwidget.h \
    keyboarddialog.h \
    guiplugin.h \
    choicenode.h \
    ../../include/fugio/gui/keyboard_interface.h \
    keyboardpin.h \
    slidernode.h \
    mainwindownode.h

FORMS += \
    numbermonitorform.ui \
    keyboarddialog.ui

#------------------------------------------------------------------------------
# OSX plugin bundle

defineReplace( libChange ) {
    return( && install_name_tool -change @executable_path/../Frameworks/$$1 @loader_path/../Frameworks/$$1 $$LIBCHANGEDEST )
}

defineReplace( qtLibChange ) {
    return( && install_name_tool -change @rpath/$$1".framework"/Versions/5/$$1 @executable_path/../Frameworks/$$1".framework"/Versions/5/$$1 $$LIBCHANGEDEST )
}

macx {
    DEFINES += TARGET_OS_MAC
    CONFIG -= x86
    CONFIG += lib_bundle

    BUNDLEDIR    = $$DESTDIR/$$TARGET".bundle"
    INSTALLBASE  = $$OUT_PWD/../../../deploy-installer
    INSTALLDIR   = $$INSTALLBASE/packages/com.bigfug.fugio
    INSTALLDEST  = $$INSTALLDIR/data/plugins
    INCLUDEDEST  = $$INSTALLDIR/data/include/fugio

    DESTDIR = $$BUNDLEDIR/Contents/MacOS
    DESTLIB = $$DESTDIR/"lib"$$TARGET".dylib"

    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7

    CONFIG(release,debug|release) {
        QMAKE_POST_LINK += echo

        LIBCHANGEDEST = $$DESTLIB

        QMAKE_POST_LINK += $$qtLibChange( QtWidgets )
        QMAKE_POST_LINK += $$qtLibChange( QtGui )
        QMAKE_POST_LINK += $$qtLibChange( QtCore )

        QMAKE_POST_LINK += && defaults write $$absolute_path( "Contents/Info", $$BUNDLEDIR ) CFBundleExecutable "lib"$$TARGET".dylib"

        QMAKE_POST_LINK += && macdeployqt $$BUNDLEDIR -always-overwrite

        QMAKE_POST_LINK += && mkdir -pv $$INSTALLDIR/meta
        QMAKE_POST_LINK += && mkdir -pv $$INSTALLDEST
        QMAKE_POST_LINK += && mkdir -pv $$INCLUDEDEST

        QMAKE_POST_LINK += && rm -rf $$INSTALLDEST/$$TARGET".bundle"

        QMAKE_POST_LINK += && cp -a $$BUNDLEDIR $$INSTALLDEST
    }
}

windows {
	INSTALLBASE  = $$OUT_PWD/../../../deploy-installer
	INSTALLDIR   = $$INSTALLBASE/packages/com.bigfug.fugio

	CONFIG(release,debug|release) {
		QMAKE_POST_LINK += echo

		QMAKE_POST_LINK += & mkdir $$shell_path( $$INSTALLDIR/data/plugins )

		QMAKE_POST_LINK += & copy /V /Y $$shell_path( $$DESTDIR/$$TARGET".dll" ) $$shell_path( $$INSTALLDIR/data/plugins )
	}
}

#------------------------------------------------------------------------------
# API

INCLUDEPATH += $$PWD/../../include
