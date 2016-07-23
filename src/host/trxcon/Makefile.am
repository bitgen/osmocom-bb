AUTOMAKE_OPTIONS = foreign dist-bzip2 1.6

# versioning magic
BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

AM_CPPFLAGS = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS = -Wall $(LIBOSMOCORE_CFLAGS) $(LIBOSMOGSM_CFLAGS)

sbin_PROGRAMS = trxcon

trxcon_SOURCES = logging.c l1ctl_link.c trx_if.c trxcon.c
trxcon_LDADD = $(LIBOSMOCORE_LIBS) $(LIBOSMOGSM_LIBS)
