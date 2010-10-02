noinst_PROGRAMS = embed_python
embed_python_SOURCES = embed_python.cc
embed_python_CXXFLAGS = @python_cflags@
embed_python_LDFLAGS = @python_ldflags@

lib_LTLIBRARIES = ecap_python_processor.la

ecap_python_processor_la_SOURCES= python_processor.cc
ecap_python_processor_la_CXXFLAGS = $(AM_MAKEFLAGS) @python_cflags@ -shared
ecap_python_processor_la_LDFLAGS = $(AM_LDFLAGS) @python_ldflags@ -module -avoid-version -lecap
