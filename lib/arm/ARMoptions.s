; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

; Set the following to 1 if we have LDRD/STRD
ARM_HAS_LDRD		*	1

; Set the following to 1 if we have ARMV6 or higher
ARMV6			*	1

; Set the following to 1 if we have NEON
ARM_HAS_NEON		*	1

; Set the following to 1 if LDR/STR can work on unaligned addresses
ARM_CAN_UNALIGN		*	0

; Set the following to 1 if LDRD/STRD can work on unaligned addresses
ARM_CAN_UNALIGN_LDRD	*	0

QEMU			*	0

	END

