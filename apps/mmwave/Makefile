############################################################################
# apps/mmwave/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.
#
# SPDX-License-Identifier: Apache-2.0
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = mmwave
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 2048
MODULE    = $(CONFIG_MMWAVE_CMD)

MAINSRC = mmwave_cmd.c

include $(APPDIR)/Application.mk
