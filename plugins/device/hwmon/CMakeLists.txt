iwp_add_plugin(iwp_hwmon
    SOURCES
        hwmonplugin.h
        hwmonplugin.cpp
        hwmonsession.h
        hwmonsession.cpp
        hwmonpage.h
        hwmonpage.cpp
        hwmonpage.ui   # layout drawn in Qt Designer; AUTOUIC makes ui_hwmonpage.h
        hwmonpreset.h
        hwmonpreset.cpp
        hwmonsettings.h
        hwmonsettings.cpp
        sensortile.h
        sensortile.cpp
        hwmoncore.h)
