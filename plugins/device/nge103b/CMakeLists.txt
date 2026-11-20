iwp_add_plugin(iwp_nge103b
    SOURCES
        nge103bplugin.h
        nge103bplugin.cpp
        nge103bsession.h
        nge103bsession.cpp
        nge103bpage.h
        nge103bpage.cpp
        nge103bpage.ui          # header and master controls, drawn in Qt Designer
        channelcard.h
        channelcard.cpp
        channelcard.ui)         # one channel (used three times by the page)
