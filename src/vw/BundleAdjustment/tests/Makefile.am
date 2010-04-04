# __BEGIN_LICENSE__
# Copyright (C) 2006-2010 United States Government as represented by
# the Administrator of the National Aeronautics and Space Administration.
# All Rights Reserved.
# __END_LICENSE__


########################################################################
# sources
########################################################################

if MAKE_MODULE_BUNDLEADJUSTMENT

TestBundleAdjustment_SOURCES      = TestBundleAdjustment.cxx
TestControlNetwork_SOURCES        = TestControlNetwork.cxx
TestCameraRelation_SOURCES        = TestCameraRelation.cxx

TESTS = TestBundleAdjustment TestControlNetwork TestCameraRelation

include $(top_srcdir)/config/instantiate.am

endif

########################################################################
# general
########################################################################

AM_CPPFLAGS = @VW_CPPFLAGS@ $(TEST_CPPFLAGS)
AM_LDFLAGS  = @VW_LDFLAGS@ @PKG_BUNDLEADJUSTMENT_LIBS@

check_PROGRAMS = $(TESTS)

include $(top_srcdir)/config/rules.mak
include $(top_srcdir)/config/tests.am