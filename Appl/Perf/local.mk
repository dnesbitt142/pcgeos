# PM-BRINGUP [PM-PERF] CHANGED 2026-09-16; ChatGPT-assisted project changes.
# PM-BRINGUP [PM-PERF] Base archive commit: 30df506fc64720fd82e528acbcb192adbaa48fce.
# PM-BRINGUP [PM-PERF] Pass the opt-in PM_PERF_MINIMAL symbol to both assembler and UI compiler.
# PM-BRINGUP [PM-PERF] See PM-BRINGUP.md and TechDocs/Markdown/pm-bringup/CHANGES.md; original notices retained.
#
# Local Makefile for Perf
#
#	$Id: local.mk,v 1.1 97/04/04 16:27:02 newdeal Exp $
#

ASMFLAGS += -Wall
LINKFLAGS += -Wunref
# PM-BRINGUP [PM-PERF] ADDED one flag for both code and UI so meter availability cannot diverge.
#if defined(PM_PERF_MINIMAL)
ASMFLAGS += -DPM_PERF_MINIMAL
UICFLAGS += -DPM_PERF_MINIMAL
#endif
#include <$(SYSMAKEFILE)>
