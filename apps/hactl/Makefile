############################################################################
# apps/hactl/Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = hactl
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 4096
MODULE    = $(CONFIG_HACTL_CMD)

MAINSRC = hactl_cmd.c

include $(APPDIR)/Application.mk
