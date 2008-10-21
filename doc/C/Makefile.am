SUBDIRS = \
  	 images

noinst_DATA = \
	xfce4-power-manager.xml

docdir=$(datadir)/xfce4/doc/C

doc_DATA= \
	xfce4-power-manager.html

EXTRA_DIST = $(noinst_DATA) $(doc_DATA)

html: $(srcdir)/xfce4-power-manager.xml
	xsltproc $(top_srcdir)/doc/xfce-nochunk.xsl \
		$(srcdir)/xfce4-power-manager.xml
