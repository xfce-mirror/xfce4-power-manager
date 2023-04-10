#
# .appdata
#
%.appdata.xml: %.appdata.xml.in
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

appdatadir = $(datadir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = xfce4-power-manager.appdata.xml.in

CLEANFILES		= $(appdata_DATA)
EXTRA_DIST		= $(appdata_in_files)
