# PM-BRINGUP [PM-VBE] CHANGED 2026-09-16; ChatGPT-assisted project changes.
# PM-BRINGUP [PM-VBE] Base archive commit: 30df506fc64720fd82e528acbcb192adbaa48fce.
# PM-BRINGUP [PM-VBE] Pass the opt-in PM_PERF_MINIMAL symbol to the VGA16 assembler; this symbol also selects the restricted display path.
# PM-BRINGUP [PM-VBE] See PM-BRINGUP.md and TechDocs/Markdown/pm-bringup/CHANGES.md; original notices retained.
##############################################################################
#
# 	Copyright (c) Global PC 1998 -- All Rights Reserved
#
# PROJECT:	GEOS
# MODULE:	VGA16 Driver -- special definitions
# FILE: 	local.mk
# AUTHOR: 	Jim DeFrisco, 10/92
#
# REVISION HISTORY:
#	Name	Date		Description
#	----	----		-----------
#	jim	10/92		Initial Revision
#
# DESCRIPTION:
#	Special definitions required for the VGA16 driver for the 
#	VESA Compatible SVGA 64K-color modes
#
#	$Id: local.mk,v 1.2$
#
###############################################################################
ASMFLAGS	+= -i

# Explicit narrow protected-mode Perf/VGA bring-up configuration.
# PM-BRINGUP [PM-VBE] ADDED opt-in define; omit it to compile the original generic video path.
#if defined(PM_PERF_MINIMAL)
ASMFLAGS += -DPM_PERF_MINIMAL
#endif

.PATH.asm .PATH.def: ../../VidCom $(INSTALL_DIR:H)/../VidCom

#include	<$(SYSMAKEFILE)>
