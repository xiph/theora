; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

	AREA	|.text|, CODE, READONLY

	EXPORT	oc_idct8x8_arm

oc_idct8x8_arm
	; r0 = ogg_int16_t *y
	; r1 = int          last_zzi
	CMP	r1, #3
	BLT	oc_idct8x8_3
	CMP	r1, #6
	BLT	oc_idct8x8_6
	CMP	r1, #10
	BLT	oc_idct8x8_10
oc_idct8x8_slow
	STMFD	r13!,{r4-r11,r14}
	SUB	r13,r13,#64*2

	MOV	r1, r0		; read from r1
	MOV	r0, r13		; write to temp storage
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core
	ADD	r1, r1, #16
	BL	idct8core

	SUB	r0, r1, #7*16	; Now src becomes dst
	MOV	r1, r13		; and dst becomes src
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down
	ADD	r1, r1, #16
	BL	idct8core_down

	ADD	r13,r13,#64*2
	LDMFD	r13!,{r4-r11,PC}

oc_idct8x8_10
	STMFD	r13!,{r4-r11,r14}
	SUB	r13,r13,#64*2

	MOV	r1, r0		; read from r1
	MOV	r0, r13		; write to temp storage
	BL	idct4core
	BL	idct3core
	BL	idct2core
	BL	idct1core

	SUB	r0, r1, #4*16	; Now src becomes dst
	MOV	r1, r13		; and dst becomes src
	BL	idct4core_down
	BL	idct4core_down
	BL	idct4core_down
	BL	idct4core_down
	BL	idct4core_down
	BL	idct4core_down
	BL	idct4core_down
	BL	idct4core_down

	ADD	r13,r13,#64*2
	LDMFD	r13!,{r4-r11,PC}
oc_idct8x8_6
	STMFD	r13!,{r4-r11,r14}
	SUB	r13,r13,#64*2

	MOV	r1, r0		; read from r1
	MOV	r0, r13		; write to temp storage
	BL	idct3core
	BL	idct2core
	BL	idct1core

	SUB	r0, r1, #3*16	; Now src becomes dst
	MOV	r1, r13		; and dst becomes src
	BL	idct3core_down
	BL	idct3core_down
	BL	idct3core_down
	BL	idct3core_down
	BL	idct3core_down
	BL	idct3core_down
	BL	idct3core_down
	BL	idct3core_down

	ADD	r13,r13,#64*2
	LDMFD	r13!,{r4-r11,PC}
oc_idct8x8_3
	STMFD	r13!,{r4-r11,r14}
	SUB	r13,r13,#64*2

	MOV	r1, r0		; read from r1
	MOV	r0, r13		; write to temp storage
	BL	idct2core
	BL	idct1core

	SUB	r0, r1, #2*16	; Now src becomes dst
	MOV	r1, r13		; and dst becomes src
	BL	idct2core_down
	BL	idct2core_down
	BL	idct2core_down
	BL	idct2core_down
	BL	idct2core_down
	BL	idct2core_down
	BL	idct2core_down
	BL	idct2core_down

	ADD	r13,r13,#64*2
	LDMFD	r13!,{r4-r11,PC}

 [ 0 = 1
	EXPORT	idct8_1
	EXPORT	idct8_2
	EXPORT	idct8_3
	EXPORT	idct8_4
	EXPORT	idct8
	EXPORT	oc_idct8x8_slow
	EXPORT	oc_idct8x8_10
	EXPORT	oc_idct8x8_3
idct8_2
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	STMFD	r13!,{r4-r11,r14}

	BL	idct2core

	LDMFD	r13!,{r4-r11,PC}
idct8_3
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	STMFD	r13!,{r4-r11,r14}

	BL	idct3core

	LDMFD	r13!,{r4-r11,PC}
idct8_4
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	STMFD	r13!,{r4-r11,r14}

	BL	idct4core

	LDMFD	r13!,{r4-r11,PC}
idct8_1
 ]
idct1core
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	LDRSH	r3, [r1], #16
	MOV	r12,#0x05
	ORR	r12,r12,#0xB500
	MUL	r3, r12, r3
	; Stall ?
	MOV	r3, r3, ASR #16
	STRH	r3, [r0], #2
	STRH	r3, [r0, #14]
	STRH	r3, [r0, #30]
	STRH	r3, [r0, #46]
	STRH	r3, [r0, #62]
	STRH	r3, [r0, #78]
	STRH	r3, [r0, #94]
	STRH	r3, [r0, #110]
	MOV	PC,R14

idct2core
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	LDRSH	r2, [r1], #16		; r2 = x[0]
	LDR	r12,OC_C4S4
	LDRSH	r11,[r1, #-14]		; r11= x[1]
	LDR	r3, OC_C7S1
	MUL	r2, r12,r2		; r2 = t[0]<<16 = OC_C4S4*x[0]
	LDR	r10,OC_C1S7
	MUL	r3, r11,r3		; r3 = t[4]<<16 = OC_C7S1*x[1]
	MOV	r2, r2, ASR #16		; r2 = t[0]
	MUL	r11,r10,r11		; r11= t[7]<<16 = OC_C1S7*x[1]
	MOV	r3, r3, ASR #16		; r3 = t[4]
	MUL	r10,r12,r3		; r10= t[5]<<16 = OC_C4S4*t[4]
	MOV	r11,r11,ASR #16		; r11= t[7]
	MUL	r12,r11,r12		; r12= t[6]<<16 = OC_C4S4*t[7]
	MOV	r10,r10,ASR #16		; r10= t[5]
	ADD	r12,r2,r12,ASR #16	; r12= t[0]+t[6]
	ADD	r12,r12,r10		; r12= t[0]+t2[6] = t[0]+t[6]+t[5]
	SUB	r10,r12,r10,LSL #1	; r10= t[0]+t2[5] = t[0]+t[6]-t[5]
	ADD	r3, r3, r2		; r3 = t[0]+t[4]
	ADD	r11,r11,r2		; r11= t[0]+t[7]
	STRH	r11,[r0], #2		; y[0] = t[0]+t[7]
	STRH	r12,[r0, #14]		; y[1] = t[0]+t[6]
	STRH	r10,[r0, #30]		; y[2] = t[0]+t[5]
	STRH	r3, [r0, #46]		; y[3] = t[0]+t[4]
	RSB	r3, r3, r2, LSL #1	; r3 = t[0]*2-(t[0]+t[4])=t[0]-t[4]
	RSB	r10,r10,r2, LSL #1	; r10= t[0]*2-(t[0]+t[5])=t[0]-t[5]
	RSB	r12,r12,r2, LSL #1	; r12= t[0]*2-(t[0]+t[6])=t[0]-t[6]
	RSB	r11,r11,r2, LSL #1	; r1 = t[0]*2-(t[0]+t[7])=t[0]-t[7]
	STRH	r3, [r0, #62]		; y[4] = t[0]-t[4]
	STRH	r10,[r0, #78]		; y[5] = t[0]-t[5]
	STRH	r12,[r0, #94]		; y[6] = t[0]-t[6]
	STRH	r11,[r0, #110]		; y[7] = t[0]-t[7]

	MOV	PC,r14
idct2core_down
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	LDRSH	r2, [r1], #16		; r2 = x[0]
	LDR	r12,OC_C4S4
	LDRSH	r11,[r1, #-14]		; r11= x[1]
	LDR	r3, OC_C7S1
	MUL	r2, r12,r2		; r2 = t[0]<<16 = OC_C4S4*x[0]
	LDR	r10,OC_C1S7
	MUL	r3, r11,r3		; r3 = t[4]<<16 = OC_C7S1*x[1]
	MOV	r2, r2, ASR #16		; r2 = t[0]
	MUL	r11,r10,r11		; r11= t[7]<<16 = OC_C1S7*x[1]
	ADD	r2, r2, #8		; r2 = t[0]+8
	MOV	r3, r3, ASR #16		; r3 = t[4]
	MUL	r10,r12,r3		; r10= t[5]<<16 = OC_C4S4*t[4]
	MOV	r11,r11,ASR #16		; r11= t[7]
	MUL	r12,r11,r12		; r12= t[6]<<16 = OC_C4S4*t[7]
	MOV	r10,r10,ASR #16		; r10= t[5]
	ADD	r12,r2,r12,ASR #16	; r12= t[0]+t[6]+8
	ADD	r12,r12,r10		; r12= t[0]+t2[6] = t[0]+t[6]+t[5]+8
	SUB	r10,r12,r10,LSL #1	; r10= t[0]+t2[5] = t[0]+t[6]-t[5]+8
	ADD	r3, r3, r2		; r3 = t[0]+t[4]+8
	ADD	r11,r11,r2		; r11= t[0]+t[7]+8
	MOV	r4, r11,ASR #4
	MOV	r5, r12,ASR #4
	MOV	r6, r10,ASR #4
	MOV	r7, r3, ASR #4
	RSB	r3, r3, r2, LSL #1	;r3 =t[0]*2+8-(t[0]+t[4])=t[0]-t[4]+8
	RSB	r10,r10,r2, LSL #1	;r10=t[0]*2+8-(t[0]+t[5])=t[0]-t[5]+8
	RSB	r12,r12,r2, LSL #1	;r12=t[0]*2+8-(t[0]+t[6])=t[0]-t[6]+8
	RSB	r11,r11,r2, LSL #1	;r11=t[0]*2+8-(t[0]+t[7])=t[0]-t[7]+8
	MOV	r3, r3, ASR #4
	MOV	r10,r10,ASR #4
	MOV	r12,r12,ASR #4
	MOV	r11,r11,ASR #4
	STRH	r4, [r0], #2		; y[0] = t[0]+t[7]
	STRH	r5, [r0, #14]		; y[1] = t[0]+t[6]
	STRH	r6, [r0, #30]		; y[2] = t[0]+t[5]
	STRH	r7, [r0, #46]		; y[3] = t[0]+t[4]
	STRH	r3, [r0, #62]		; y[4] = t[0]-t[4]
	STRH	r10,[r0, #78]		; y[5] = t[0]-t[5]
	STRH	r12,[r0, #94]		; y[6] = t[0]-t[6]
	STRH	r11,[r0, #110]		; y[7] = t[0]-t[7]

	MOV	PC,r14
idct3core
	LDRSH	r2, [r1], #16		; r2 = x[0]
	LDR	r12,OC_C4S4		; r12= OC_C4S4
	LDRSH	r3, [r1, #-12]		; r3 = x[2]
	LDR	r10,OC_C6S2		; r10= OC_C6S2
	MUL	r2, r12,r2		; r2 = t[0]<<16 = OC_C4S4*x[0]
	LDR	r4, OC_C2S6		; r4 = OC_C2S6
	MUL	r10,r3, r10		; r10= t[2]<<16 = OC_C6S2*x[2]
	LDRSH	r11,[r1, #-14]		; r11= x[1]
	MUL	r3, r4, r3		; r3 = t[3]<<16 = OC_C2S6*x[2]
	LDR	r4, OC_C7S1		; r4 = OC_C7S1
	LDR	r5, OC_C1S7		; r5 = OC_C1S7
	MOV	r2, r2, ASR #16		; r2 = t[0]
	MUL	r4, r11,r4		; r4 = t[4]<<16 = OC_C7S1*x[1]
	ADD	r3, r2, r3, ASR #16	; r3 = t[0]+t[3]
	MUL	r11,r5, r11		; r11= t[7]<<16 = OC_C1S7*x[1]
	MOV	r4, r4, ASR #16		; r4 = t[4]
	MUL	r5, r12,r4		; r5 = t[5]<<16 = OC_C4S4*t[4]
	MOV	r11,r11,ASR #16		; r11= t[7]
	MUL	r12,r11,r12		; r12= t[6]<<16 = OC_C4S4*t[7]

	ADD	r10,r2, r10,ASR #16	; r10= t[1] = t[0]+t[2]
	RSB	r6, r10,r2, LSL #1	; r6 = t[2] = t[0]-t[2]
					; r3 = t2[0] = t[0]+t[3]
	RSB	r2, r3, r2, LSL #1	; r2 = t2[3] = t[0]-t[3]
	MOV	r12,r12,ASR #16		; r12= t[6]
	ADD	r5, r12,r5, ASR #16	; r5 = t2[6] = t[6]+t[5]
	RSB	r12,r5, r12,LSL #1	; r12= t2[5] = t[6]-t[5]

	ADD	r11,r3, r11		; r11= t2[0]+t[7]
	ADD	r5, r10,r5		; r5 = t[1]+t2[6]
	ADD	r12,r6, r12		; r12= t[2]+t2[5]
	ADD	r4, r2, r4		; r4 = t2[3]+t[4]
	STRH	r11,[r0], #2		; y[0] = t[0]+t[7]
	STRH	r5, [r0, #14]		; y[1] = t[1]+t2[6]
	STRH	r12,[r0, #30]		; y[2] = t[2]+t2[5]
	STRH	r4, [r0, #46]		; y[3] = t2[3]+t[4]

	RSB	r11,r11,r3, LSL #1	; r11= t2[0] - t[7]
	RSB	r5, r5, r10,LSL #1	; r5 = t[1]  - t2[6]
	RSB	r12,r12,r6, LSL #1	; r6 = t[2]  - t2[5]
	RSB	r4, r4, r2, LSL #1	; r4 = t2[3] - t[4]
	STRH	r4, [r0, #62]		; y[4] = t2[3]-t[4]
	STRH	r12,[r0, #78]		; y[5] = t[2]-t2[5]
	STRH	r5, [r0, #94]		; y[6] = t[1]-t2[6]
	STRH	r11,[r0, #110]		; y[7] = t2[0]-t[7]

	MOV	PC,R14
idct3core_down
	LDRSH	r2, [r1], #16		; r2 = x[0]
	LDR	r12,OC_C4S4		; r12= OC_C4S4
	LDRSH	r3, [r1, #-12]		; r3 = x[2]
	LDR	r10,OC_C6S2		; r10= OC_C6S2
	MUL	r2, r12,r2		; r2 = t[0]<<16 = OC_C4S4*x[0]
	LDR	r4, OC_C2S6		; r4 = OC_C2S6
	MUL	r10,r3, r10		; r10= t[2]<<16 = OC_C6S2*x[2]
	LDRSH	r11,[r1, #-14]		; r11= x[1]
	MUL	r3, r4, r3		; r3 = t[3]<<16 = OC_C2S6*x[2]
	LDR	r4, OC_C7S1		; r4 = OC_C7S1
	LDR	r5, OC_C1S7		; r5 = OC_C1S7
	MOV	r2, r2, ASR #16		; r2 = t[0]
	ADD	r2, r2, #8		; r2 = t[0]+8
	MUL	r4, r11,r4		; r4 = t[4]<<16 = OC_C7S1*x[1]
	ADD	r3, r2, r3, ASR #16	; r3 = t[0]+t[3]+8
	MUL	r11,r5, r11		; r11= t[7]<<16 = OC_C1S7*x[1]
	MOV	r4, r4, ASR #16		; r4 = t[4]
	MUL	r5, r12,r4		; r5 = t[5]<<16 = OC_C4S4*t[4]
	MOV	r11,r11,ASR #16		; r11= t[7]
	MUL	r12,r11,r12		; r12= t[6]<<16 = OC_C4S4*t[7]

	ADD	r10,r2, r10,ASR #16	; r10= t[1] = t[0]+t[2]+8
	RSB	r6, r10,r2, LSL #1	; r6 = t[2] = t[0]-t[2]+8
					; r3 = t2[0] = t[0]+t[3]+8
	RSB	r2, r3, r2, LSL #1	; r2 = t2[3] = t[0]-t[3]+8
	MOV	r12,r12,ASR #16		; r12= t[6]
	ADD	r5, r12,r5, ASR #16	; r5 = t2[6] = t[6]+t[5]
	RSB	r12,r5, r12,LSL #1	; r12= t2[5] = t[6]-t[5]

	ADD	r11,r3, r11		; r11= t2[0]+t[7]
	ADD	r5, r10,r5		; r5 = t[1] +t2[6]
	ADD	r12,r6, r12		; r12= t[2] +t2[5]
	ADD	r4, r2, r4		; r4 = t2[3]+t[4]
	RSB	r3, r11,r3, LSL #1	; r11= t2[0] - t[7]
	RSB	r10,r5, r10,LSL #1	; r5 = t[1]  - t2[6]
	RSB	r6, r12,r6, LSL #1	; r6 = t[2]  - t2[5]
	RSB	r2, r4, r2, LSL #1	; r4 = t2[3] - t[4]
	MOV	r11,r11,ASR #4
	MOV	r5, r5, ASR #4
	MOV	r12,r12,ASR #4
	MOV	r4, r4, ASR #4
	MOV	r2, r2, ASR #4
	MOV	r6, r6, ASR #4
	MOV	r10,r10,ASR #4
	MOV	r3, r3, ASR #4
	STRH	r11,[r0], #2		; y[0] = t[0]+t[7]
	STRH	r5, [r0, #14]		; y[1] = t[1]+t2[6]
	STRH	r12,[r0, #30]		; y[2] = t[2]+t2[5]
	STRH	r4, [r0, #46]		; y[3] = t2[3]+t[4]
	STRH	r2, [r0, #62]		; y[4] = t2[3]-t[4]
	STRH	r6, [r0, #78]		; y[5] = t[2]-t2[5]
	STRH	r10,[r0, #94]		; y[6] = t[1]-t2[6]
	STRH	r3, [r0, #110]		; y[7] = t2[0]-t[7]

	MOV	PC,R14

idct4core
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	LDRSH	r2, [r1], #16		; r2 = x[0]
	LDR	r10,OC_C4S4		; r10= OC_C4S4
	LDRSH	r12,[r1, #-12]		; r12= x[2]
	LDR	r4, OC_C6S2		; r4 = OC_C6S2
	MUL	r2, r10,r2		; r2 = t[0]<<16 = OC_C4S4*x[0]
	LDR	r5, OC_C2S6		; r5 = OC_C2S6
	MUL	r4, r12,r4		; r4 = t[2]<<16 = OC_C6S2*x[2]
	LDRSH	r3, [r1, #-14]		; r3 = x[1]
	MUL	r5, r12,r5		; r5 = t[3]<<16 = OC_C2S6*x[2]
	LDR	r6, OC_C7S1		; r6 = OC_C7S1
	LDR	r12,OC_C1S7		; r12= OC_C1S7
	LDRSH	r11,[r1, #-10]		; r11= x[3]
	MUL	r6, r3, r6		; r6 = t[4]<<16 = OC_C7S1*x[1]
	LDR	r7, OC_C5S3		; r7 = OC_C5S3
	MUL	r3, r12,r3		; r3 = t[7]<<16 = OC_C1S7*x[1]
	LDR	r8, OC_C3S5		; r8 = OC_C3S5
	MUL	r7, r11,r7		; r7 = -t[5]<<16 = OC_C5S3*x[3]
	MOV	r2, r2, ASR #16		; r2 = t[0]
	MUL	r11,r8, r11		; r11= t[6]<<16 = OC_C3S5*x[3]

	MOV	r6, r6, ASR #16		; r6 = t[4]
	SUB	r7, r6, r7, ASR #16	; r7 = t2[4]=t[4]+t[5] (as r7=-t[5])
	RSB	r6, r7, r6, LSL #1	; r6 = t[4]-t[5]
	MUL	r6, r10,r6		; r6 = t2[5]<<16 =OC_C4S4*(t[4]-t[5])

	MOV	r3, r3, ASR #16		; r3 = t[7]
	ADD	r11,r3, r11,ASR #16	; r11= t2[7]=t[7]+t[6]
	RSB	r3, r11,r3, LSL #1	; r3 = t[7]-t[6]
	MUL	r3, r10,r3		; r3 = t2[6]<<16 =OC_C4S4*(t[7]-t[6])

	ADD	r4, r2, r4, ASR #16	; r4 = t[1] = t[0] + t[2]
	RSB	r10,r4, r2, LSL #1	; r10= t[2] = t[0] - t[2]

	ADD	r5, r2, r5, ASR #16	; r5 = t[0] = t[0] + t[3]
	RSB	r2, r5, r2, LSL #1	; r2 = t[3] = t[0] - t[3]

	MOV	r3, r3, ASR #16		; r3 = t2[6]
	ADD	r6, r3, r6, ASR #16	; r6 = t3[6] = t2[6]+t2[5]
	RSB	r3, r6, r3, LSL #1	; r3 = t3[5] = t2[6]-t2[5]

	ADD	r11,r5, r11		; r11= t[0]+t2[7]
	ADD	r6, r4, r6		; r6 = t[1]+t3[6]
	ADD	r3, r10,r3		; r3 = t[2]+t3[5]
	ADD	r7, r2, r7		; r7 = t[3]+t2[4]
	STRH	r11,[r0], #2		; y[0] = t[0]+t[7]
	STRH	r6, [r0, #14]		; y[1] = t[1]+t2[6]
	STRH	r3, [r0, #30]		; y[2] = t[2]+t2[5]
	STRH	r7, [r0, #46]		; y[3] = t2[3]+t[4]

	RSB	r11,r11,r5, LSL #1	; r11= t[0]-t2[7]
	RSB	r6, r6, r4, LSL #1	; r6 = t[1]-t3[6]
	RSB	r3, r3, r10,LSL #1	; r3 = t[2]-t3[5]
	RSB	r7, r7, r2, LSL #1	; r7 = t[3]-t2[4]
	STRH	r7, [r0, #62]		; y[4] = t2[3]-t[4]
	STRH	r3, [r0, #78]		; y[5] = t[2]-t2[5]
	STRH	r6, [r0, #94]		; y[6] = t[1]-t2[6]
	STRH	r11, [r0, #110]		; y[7] = t2[0]-t[7]

	MOV	PC,r14
idct4core_down
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	LDRSH	r2, [r1], #16		; r2 = x[0]
	LDR	r10,OC_C4S4		; r10= OC_C4S4
	LDRSH	r12,[r1, #-12]		; r12= x[2]
	LDR	r4, OC_C6S2		; r4 = OC_C6S2
	MUL	r2, r10,r2		; r2 = t[0]<<16 = OC_C4S4*x[0]
	LDR	r5, OC_C2S6		; r5 = OC_C2S6
	MUL	r4, r12,r4		; r4 = t[2]<<16 = OC_C6S2*x[2]
	LDRSH	r3, [r1, #-14]		; r3 = x[1]
	MUL	r5, r12,r5		; r5 = t[3]<<16 = OC_C2S6*x[2]
	LDR	r6, OC_C7S1		; r6 = OC_C7S1
	LDR	r12,OC_C1S7		; r12= OC_C1S7
	LDRSH	r11,[r1, #-10]		; r11= x[3]
	MUL	r6, r3, r6		; r6 = t[4]<<16 = OC_C7S1*x[1]
	LDR	r7, OC_C5S3		; r7 = OC_C5S3
	MUL	r3, r12,r3		; r3 = t[7]<<16 = OC_C1S7*x[1]
	LDR	r8, OC_C3S5		; r8 = OC_C3S5
	MUL	r7, r11,r7		; r7 = -t[5]<<16 = OC_C5S3*x[3]
	MOV	r2, r2, ASR #16		; r2 = t[0]
	MUL	r11,r8, r11		; r11= t[6]<<16 = OC_C3S5*x[3]

	MOV	r6, r6, ASR #16		; r6 = t[4]
	SUB	r7, r6, r7, ASR #16	; r7 = t2[4]=t[4]+t[5] (as r7=-t[5])
	RSB	r6, r7, r6, LSL #1	; r6 = t[4]-t[5]
	MUL	r6, r10,r6		; r6 = t2[5]<<16 =OC_C4S4*(t[4]-t[5])

	MOV	r3, r3, ASR #16		; r3 = t[7]
	ADD	r11,r3, r11,ASR #16	; r11= t2[7]=t[7]+t[6]
	RSB	r3, r11,r3, LSL #1	; r3 = t[7]-t[6]
	MUL	r3, r10,r3		; r3 = t2[6]<<16 =OC_C4S4*(t[7]-t[6])

	ADD	r4, r2, r4, ASR #16	; r4 = t[1] = t[0] + t[2]
	RSB	r10,r4, r2, LSL #1	; r10= t[2] = t[0] - t[2]

	ADD	r5, r2, r5, ASR #16	; r5 = t[0] = t[0] + t[3]
	RSB	r2, r5, r2, LSL #1	; r2 = t[3] = t[0] - t[3]

	MOV	r3, r3, ASR #16		; r3 = t2[6]
	ADD	r6, r3, r6, ASR #16	; r6 = t3[6] = t2[6]+t2[5]
	RSB	r3, r6, r3, LSL #1	; r3 = t3[5] = t2[6]-t2[5]

	ADD	r5, r5, r11		; r5 = t[0]+t2[7]
	ADD	r4, r4, r6		; r4 = t[1]+t3[6]
	ADD	r10,r10,r3		; r10= t[2]+t3[5]
	ADD	r2, r2, r7		; r2 = t[3]+t2[4]
	ADD	r2, r2, #8
	ADD	r10,r10,#8
	ADD	r4, r4, #8
	ADD	r5, r5, #8
	SUB	r11,r5, r11,LSL #1	; r11= t[0]-t2[7]
	SUB	r6, r4, r6, LSL #1	; r6 = t[1]-t3[6]
	SUB	r3, r10,r3, LSL #1	; r3 = t[2]-t3[5]
	SUB	r7, r2, r7, LSL #1	; r7 = t[3]-t2[4]
	MOV	r11,r11,ASR #4
	MOV	r6, r6, ASR #4
	MOV	r3, r3, ASR #4
	MOV	r7, r7, ASR #4
	MOV	r2, r2, ASR #4
	MOV	r10,r10,ASR #4
	MOV	r4, r4, ASR #4
	MOV	r5, r5, ASR #4
	STRH	r5,[r0], #2		; y[0] = t[0]+t[7]
	STRH	r4, [r0, #14]		; y[1] = t[1]+t2[6]
	STRH	r10,[r0, #30]		; y[2] = t[2]+t2[5]
	STRH	r2, [r0, #46]		; y[3] = t2[3]+t[4]
	STRH	r7, [r0, #62]		; y[4] = t2[3]-t[4]
	STRH	r3, [r0, #78]		; y[5] = t[2]-t2[5]
	STRH	r6, [r0, #94]		; y[6] = t[1]-t2[6]
	STRH	r11,[r0, #110]		; y[7] = t2[0]-t[7]

	MOV	PC,r14

OC_C1S7
	DCD	64277 ; FB15
OC_C2S6
	DCD	60547 ; EC83
OC_C4S4
	DCD	46341 ; B505
OC_C6S2
	DCD	25080 ; 61F8
OC_C7S1
	DCD	12785 ; 31F1
OC_C3S5
	DCD	54491 ; D4DB
OC_C5S3
	DCD	36410 ; 8E3A

	ALIGN
idct8
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	STMFD	r13!,{r4-r11,r14}

	LDRSH	r2, [r1]		; r2 = x[0]
	LDRSH	r6, [r1, #8]		; r6 = x[4]
	LDR	r12,OC_C4S4		; r12= C4S4
	LDRSH	r4, [r1, #4]		; r4 = x[2]
	ADD	r2, r2, r6		; r2 = x[0] + x[4]
	SUB	r6, r2, r6, LSL #1	; r6 = x[0] - x[4]
	MUL	r2, r12,r2		; r2 = t[0]<<16 = C4S4*(x[0]+x[4])
	LDRSH	r8, [r1, #12]		; r8 = x[6]
	LDR	r7, OC_C6S2		; r7 = OC_C6S2
	MUL	r6, r12,r6		; r6 = t[1]<<16 = C4S4*(x[0]-x[4])
	LDR	r14,OC_C2S6		; r14= OC_C2S6
	MUL	r3, r4, r7		; r3 = OC_C6S2*x[2]
	LDR	r5, OC_C7S1		; r5 = OC_C7S1
	MUL	r4, r14,r4		; r4 = OC_C2S6*x[2]
	MOV	r3, r3, ASR #16		; r3 = OC_C6S2*x[2]>>16
	MUL	r14,r8, r14		; r14= OC_C2S6*x[6]
	MOV	r4, r4, ASR #16		; r4 = OC_C2S6*x[2]>>16
	MUL	r8, r7, r8		; r8 = OC_C6S2*x[6]
	LDR	r7, OC_C1S7		; r7 = OC_C1S7
	SUB	r3, r3, r14,ASR #16	; r3=t[2]=C6S2*x[2]>>16-C2S6*x[6]>>16
	LDRSH	r14,[r1, #2]		; r14= x[1]
	ADD	r4, r4, r8, ASR #16	; r4=t[3]=C2S6*x[2]>>16+C6S2*x[6]>>16
	LDRSH	r8, [r1, #14]		; r8 = x[7]
	MUL	r9, r5, r14		; r9 = OC_C7S1*x[1]
	LDRSH	r10,[r1, #10]		; r10= x[5]
	MUL	r14,r7, r14		; r14= OC_C1S7*x[1]
	MOV	r9, r9, ASR #16		; r9 = OC_C7S1*x[1]>>16
	MUL	r7, r8, r7		; r7 = OC_C1S7*x[7]
	MOV	r14,r14,ASR #16		; r14= OC_C1S7*x[1]>>16
	MUL	r8, r5, r8		; r8 = OC_C7S1*x[7]
	LDRSH	r1, [r1, #6]		; r1 = x[3]
	LDR	r5, OC_C3S5		; r5 = OC_C3S5
	LDR	r11,OC_C5S3		; r11= OC_C5S3
	ADD	r8, r14,r8, ASR #16	; r8=t[7]=C1S7*x[1]>>16+C7S1*x[7]>>16
	MUL	r14,r5, r10		; r14= OC_C3S5*x[5]
	SUB	r9, r9, r7, ASR #16	; r9=t[4]=C7S1*x[1]>>16-C1S7*x[7]>>16
	MUL	r10,r11,r10		; r10= OC_C5S3*x[5]
	MOV	r14,r14,ASR #16		; r14= OC_C3S5*x[5]>>16
	MUL	r11,r1, r11		; r11= OC_C5S3*x[3]
	MOV	r10,r10,ASR #16		; r10= OC_C5S3*x[5]>>16
	MUL	r1, r5, r1		; r1 = OC_C3S5*x[3]
	SUB	r14,r14,r11,ASR #16	;r14=t[5]=C3S5*x[5]>>16-C5S3*x[3]>>16
	ADD	r10,r10,r1, ASR #16	;r10=t[6]=C5S3*x[5]>>16+C3S5*x[3]>>16

	; r2=t[0]<<16 r3=t[2] r4=t[3] r6=t[1]<<16 r8=t[7] r9=t[4]
	; r10=t[6] r12=C4S4 r14=t[5]

	; Stage 2
	; 4-5 butterfly
	ADD	r9, r9, r14		; r9 = t2[4]     =       t[4]+t[5]
	SUB	r14,r9, r14, LSL #1	; r14=                   t[4]-t[5]
	MUL	r14,r12,r14		; r14= t2[5]<<16 = C4S4*(t[4]-t[5])

	; 7-6 butterfly
	ADD	r8, r8, r10		; r8 = t2[7]     =       t[7]+t[6]
	SUB	r10,r8, r10, LSL #1	; r10=                   t[7]-t[6]
	MUL	r10,r12,r10		; r10= t2[6]<<16 = C4S4*(t[7]+t[6])

	; r2=t[0]<<16 r3=t[2] r4=t[3] r6=t[1]<<16 r8=t2[7] r9=t2[4]
	; r10=t2[6]<<16 r12=C4S4 r14=t2[5]<<16

	; Stage 3
	; 0-3 butterfly
	ADD	r2, r4, r2, ASR #16	; r2 = t2[0] = t[0] + t[3]
	SUB	r4, r2, r4, LSL #1	; r4 = t2[3] = t[0] - t[3]

	; 1-2 butterfly
	ADD	r6, r3, r6, ASR #16	; r6 = t2[1] = t[1] + t[2]
	SUB	r3, r6, r3, LSL #1	; r3 = t2[2] = t[1] - t[2]

	; 6-5 butterfly
	MOV	r14,r14,ASR #16		; r14= t2[5]
	ADD	r10,r14,r10,ASR #16	; r10= t3[6] = t[6] + t[5]
	SUB	r14,r10,r14,LSL #1	; r14= t3[5] = t[6] - t[5]

	; r2=t2[0] r3=t2[2] r4=t2[3] r6=t2[1] r8=t2[7] r9=t2[4]
	; r10=t3[6] r14=t3[5]

	; Stage 4
	ADD	r2, r2, r8		; r2 = t[0] + t[7]
	ADD	r6, r6, r10		; r6 = t[1] + t[6]
	ADD	r3, r3, r14		; r3 = t[2] + t[5]
	ADD	r4, r4, r9		; r4 = t[3] + t[4]
	SUB	r8, r2, r8, LSL #1	; r8 = t[0] - t[7]
	SUB	r10,r6, r10,LSL #1	; r10= t[1] - t[6]
	SUB	r14,r3, r14,LSL #1	; r14= t[2] - t[5]
	SUB	r9, r4, r9, LSL #1	; r9 = t[3] - t[4]
	STRH	r2, [r0], #2		; y[0] = t[0]+t[7]
	STRH	r6, [r0, #14]		; y[1] = t[1]+t[6]
	STRH	r3, [r0, #30]		; y[2] = t[2]+t[5]
	STRH	r4, [r0, #46]		; y[3] = t[3]+t[4]
	STRH	r9, [r0, #62]		; y[4] = t[3]-t[4]
	STRH	r14,[r0, #78]		; y[5] = t[2]-t[5]
	STRH	r10,[r0, #94]		; y[6] = t[1]-t[6]
	STRH	r8, [r0, #110]		; y[7] = t[0]-t[7]

	LDMFD	r13!,{r4-r11,PC}
idct8core
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	STMFD	r13!,{r1,r14}

	LDRSH	r2, [r1]		; r2 = x[0]
	LDRSH	r6, [r1, #8]		; r6 = x[4]
	LDR	r12,OC_C4S4		; r12= C4S4
	LDRSH	r4, [r1, #4]		; r4 = x[2]
	ADD	r2, r2, r6		; r2 = x[0] + x[4]
	SUB	r6, r2, r6, LSL #1	; r6 = x[0] - x[4]
	MUL	r2, r12,r2		; r2 = t[0]<<16 = C4S4*(x[0]+x[4])
	LDRSH	r8, [r1, #12]		; r8 = x[6]
	LDR	r7, OC_C6S2		; r7 = OC_C6S2
	MUL	r6, r12,r6		; r6 = t[1]<<16 = C4S4*(x[0]-x[4])
	LDR	r14,OC_C2S6		; r14= OC_C2S6
	MUL	r3, r4, r7		; r3 = OC_C6S2*x[2]
	LDR	r5, OC_C7S1		; r5 = OC_C7S1
	MUL	r4, r14,r4		; r4 = OC_C2S6*x[2]
	MOV	r3, r3, ASR #16		; r3 = OC_C6S2*x[2]>>16
	MUL	r14,r8, r14		; r14= OC_C2S6*x[6]
	MOV	r4, r4, ASR #16		; r4 = OC_C2S6*x[2]>>16
	MUL	r8, r7, r8		; r8 = OC_C6S2*x[6]
	LDR	r7, OC_C1S7		; r7 = OC_C1S7
	SUB	r3, r3, r14,ASR #16	; r3=t[2]=C6S2*x[2]>>16-C2S6*x[6]>>16
	LDRSH	r14,[r1, #2]		; r14= x[1]
	ADD	r4, r4, r8, ASR #16	; r4=t[3]=C2S6*x[2]>>16+C6S2*x[6]>>16
	LDRSH	r8, [r1, #14]		; r8 = x[7]
	MUL	r9, r5, r14		; r9 = OC_C7S1*x[1]
	LDRSH	r10,[r1, #10]		; r10= x[5]
	MUL	r14,r7, r14		; r14= OC_C1S7*x[1]
	MOV	r9, r9, ASR #16		; r9 = OC_C7S1*x[1]>>16
	MUL	r7, r8, r7		; r7 = OC_C1S7*x[7]
	MOV	r14,r14,ASR #16		; r14= OC_C1S7*x[1]>>16
	MUL	r8, r5, r8		; r8 = OC_C7S1*x[7]
	LDRSH	r1, [r1, #6]		; r1 = x[3]
	LDR	r5, OC_C3S5		; r5 = OC_C3S5
	LDR	r11,OC_C5S3		; r11= OC_C5S3
	ADD	r8, r14,r8, ASR #16	; r8=t[7]=C1S7*x[1]>>16+C7S1*x[7]>>16
	MUL	r14,r5, r10		; r14= OC_C3S5*x[5]
	SUB	r9, r9, r7, ASR #16	; r9=t[4]=C7S1*x[1]>>16-C1S7*x[7]>>16
	MUL	r10,r11,r10		; r10= OC_C5S3*x[5]
	MOV	r14,r14,ASR #16		; r14= OC_C3S5*x[5]>>16
	MUL	r11,r1, r11		; r11= OC_C5S3*x[3]
	MOV	r10,r10,ASR #16		; r10= OC_C5S3*x[5]>>16
	MUL	r1, r5, r1		; r1 = OC_C3S5*x[3]
	SUB	r14,r14,r11,ASR #16	;r14=t[5]=C3S5*x[5]>>16-C5S3*x[3]>>16
	ADD	r10,r10,r1, ASR #16	;r10=t[6]=C5S3*x[5]>>16+C3S5*x[3]>>16

	; r2=t[0]<<16 r3=t[2] r4=t[3] r6=t[1]<<16 r8=t[7] r9=t[4]
	; r10=t[6] r12=C4S4 r14=t[5]

	; Stage 2
	; 4-5 butterfly
	ADD	r9, r9, r14		; r9 = t2[4]     =       t[4]+t[5]
	SUB	r14,r9, r14, LSL #1	; r14=                   t[4]-t[5]
	MUL	r14,r12,r14		; r14= t2[5]<<16 = C4S4*(t[4]-t[5])

	; 7-6 butterfly
	ADD	r8, r8, r10		; r8 = t2[7]     =       t[7]+t[6]
	SUB	r10,r8, r10, LSL #1	; r10=                   t[7]-t[6]
	MUL	r10,r12,r10		; r10= t2[6]<<16 = C4S4*(t[7]+t[6])

	; r2=t[0]<<16 r3=t[2] r4=t[3] r6=t[1]<<16 r8=t2[7] r9=t2[4]
	; r10=t2[6]<<16 r12=C4S4 r14=t2[5]<<16

	; Stage 3
	; 0-3 butterfly
	ADD	r2, r4, r2, ASR #16	; r2 = t2[0] = t[0] + t[3]
	SUB	r4, r2, r4, LSL #1	; r4 = t2[3] = t[0] - t[3]

	; 1-2 butterfly
	ADD	r6, r3, r6, ASR #16	; r6 = t2[1] = t[1] + t[2]
	SUB	r3, r6, r3, LSL #1	; r3 = t2[2] = t[1] - t[2]

	; 6-5 butterfly
	MOV	r14,r14,ASR #16		; r14= t2[5]
	ADD	r10,r14,r10,ASR #16	; r10= t3[6] = t[6] + t[5]
	SUB	r14,r10,r14,LSL #1	; r14= t3[5] = t[6] - t[5]

	; r2=t2[0] r3=t2[2] r4=t2[3] r6=t2[1] r8=t2[7] r9=t2[4]
	; r10=t3[6] r14=t3[5]

	; Stage 4
	ADD	r2, r2, r8		; r2 = t[0] + t[7]
	ADD	r6, r6, r10		; r6 = t[1] + t[6]
	ADD	r3, r3, r14		; r3 = t[2] + t[5]
	ADD	r4, r4, r9		; r4 = t[3] + t[4]
	SUB	r8, r2, r8, LSL #1	; r8 = t[0] - t[7]
	SUB	r10,r6, r10,LSL #1	; r10= t[1] - t[6]
	SUB	r14,r3, r14,LSL #1	; r14= t[2] - t[5]
	SUB	r9, r4, r9, LSL #1	; r9 = t[3] - t[4]
	STRH	r2, [r0], #2		; y[0] = t[0]+t[7]
	STRH	r6, [r0, #14]		; y[1] = t[1]+t[6]
	STRH	r3, [r0, #30]		; y[2] = t[2]+t[5]
	STRH	r4, [r0, #46]		; y[3] = t[3]+t[4]
	STRH	r9, [r0, #62]		; y[4] = t[3]-t[4]
	STRH	r14,[r0, #78]		; y[5] = t[2]-t[5]
	STRH	r10,[r0, #94]		; y[6] = t[1]-t[6]
	STRH	r8, [r0, #110]		; y[7] = t[0]-t[7]

	LDMFD	r13!,{r1,PC}
idct8core_down
	; r0 =       ogg_int16_t *y      (destination)
	; r1 = const ogg_int16_t *x      (source)
	STMFD	r13!,{r1,r14}

	LDRSH	r2, [r1]		; r2 = x[0]
	LDRSH	r6, [r1, #8]		; r6 = x[4]
	LDR	r12,OC_C4S4		; r12= C4S4
	LDRSH	r4, [r1, #4]		; r4 = x[2]
	ADD	r2, r2, r6		; r2 = x[0] + x[4]
	SUB	r6, r2, r6, LSL #1	; r6 = x[0] - x[4]
	MUL	r2, r12,r2		; r2 = t[0]<<16 = C4S4*(x[0]+x[4])
	LDRSH	r8, [r1, #12]		; r8 = x[6]
	LDR	r7, OC_C6S2		; r7 = OC_C6S2
	MUL	r6, r12,r6		; r6 = t[1]<<16 = C4S4*(x[0]-x[4])
	LDR	r14,OC_C2S6		; r14= OC_C2S6
	MUL	r3, r4, r7		; r3 = OC_C6S2*x[2]
	LDR	r5, OC_C7S1		; r5 = OC_C7S1
	MUL	r4, r14,r4		; r4 = OC_C2S6*x[2]
	MOV	r3, r3, ASR #16		; r3 = OC_C6S2*x[2]>>16
	MUL	r14,r8, r14		; r14= OC_C2S6*x[6]
	MOV	r4, r4, ASR #16		; r4 = OC_C2S6*x[2]>>16
	MUL	r8, r7, r8		; r8 = OC_C6S2*x[6]
	LDR	r7, OC_C1S7		; r7 = OC_C1S7
	SUB	r3, r3, r14,ASR #16	; r3=t[2]=C6S2*x[2]>>16-C2S6*x[6]>>16
	LDRSH	r14,[r1, #2]		; r14= x[1]
	ADD	r4, r4, r8, ASR #16	; r4=t[3]=C2S6*x[2]>>16+C6S2*x[6]>>16
	LDRSH	r8, [r1, #14]		; r8 = x[7]
	MUL	r9, r5, r14		; r9 = OC_C7S1*x[1]
	LDRSH	r10,[r1, #10]		; r10= x[5]
	MUL	r14,r7, r14		; r14= OC_C1S7*x[1]
	MOV	r9, r9, ASR #16		; r9 = OC_C7S1*x[1]>>16
	MUL	r7, r8, r7		; r7 = OC_C1S7*x[7]
	MOV	r14,r14,ASR #16		; r14= OC_C1S7*x[1]>>16
	MUL	r8, r5, r8		; r8 = OC_C7S1*x[7]
	LDRSH	r1, [r1, #6]		; r1 = x[3]
	LDR	r5, OC_C3S5		; r5 = OC_C3S5
	LDR	r11,OC_C5S3		; r11= OC_C5S3
	ADD	r8, r14,r8, ASR #16	; r8=t[7]=C1S7*x[1]>>16+C7S1*x[7]>>16
	MUL	r14,r5, r10		; r14= OC_C3S5*x[5]
	SUB	r9, r9, r7, ASR #16	; r9=t[4]=C7S1*x[1]>>16-C1S7*x[7]>>16
	MUL	r10,r11,r10		; r10= OC_C5S3*x[5]
	MOV	r14,r14,ASR #16		; r14= OC_C3S5*x[5]>>16
	MUL	r11,r1, r11		; r11= OC_C5S3*x[3]
	MOV	r10,r10,ASR #16		; r10= OC_C5S3*x[5]>>16
	MUL	r1, r5, r1		; r1 = OC_C3S5*x[3]
	SUB	r14,r14,r11,ASR #16	;r14=t[5]=C3S5*x[5]>>16-C5S3*x[3]>>16
	ADD	r10,r10,r1, ASR #16	;r10=t[6]=C5S3*x[5]>>16+C3S5*x[3]>>16

	; r2=t[0]<<16 r3=t[2] r4=t[3] r6=t[1]<<16 r8=t[7] r9=t[4]
	; r10=t[6] r12=C4S4 r14=t[5]

	; Stage 2
	; 4-5 butterfly
	ADD	r9, r9, r14		; r9 = t2[4]     =       t[4]+t[5]
	SUB	r14,r9, r14, LSL #1	; r14=                   t[4]-t[5]
	MUL	r14,r12,r14		; r14= t2[5]<<16 = C4S4*(t[4]-t[5])

	; 7-6 butterfly
	ADD	r8, r8, r10		; r8 = t2[7]     =       t[7]+t[6]
	SUB	r10,r8, r10, LSL #1	; r10=                   t[7]-t[6]
	MUL	r10,r12,r10		; r10= t2[6]<<16 = C4S4*(t[7]+t[6])

	; r2=t[0]<<16 r3=t[2] r4=t[3] r6=t[1]<<16 r8=t2[7] r9=t2[4]
	; r10=t2[6]<<16 r12=C4S4 r14=t2[5]<<16

	; Stage 3
	; 0-3 butterfly
	ADD	r2, r4, r2, ASR #16	; r2 = t2[0] = t[0] + t[3]
	SUB	r4, r2, r4, LSL #1	; r4 = t2[3] = t[0] - t[3]

	; 1-2 butterfly
	ADD	r6, r3, r6, ASR #16	; r6 = t2[1] = t[1] + t[2]
	SUB	r3, r6, r3, LSL #1	; r3 = t2[2] = t[1] - t[2]

	; 6-5 butterfly
	MOV	r14,r14,ASR #16		; r14= t2[5]
	ADD	r10,r14,r10,ASR #16	; r10= t3[6] = t[6] + t[5]
	SUB	r14,r10,r14,LSL #1	; r14= t3[5] = t[6] - t[5]

	; r2=t2[0] r3=t2[2] r4=t2[3] r6=t2[1] r8=t2[7] r9=t2[4]
	; r10=t3[6] r14=t3[5]

	; Stage 4
	ADD	r2, r2, r8		; r2 = t[0] + t[7]
	ADD	r6, r6, r10		; r6 = t[1] + t[6]
	ADD	r3, r3, r14		; r3 = t[2] + t[5]
	ADD	r4, r4, r9		; r4 = t[3] + t[4]
	ADD	r2, r2, #8
	ADD	r6, r6, #8
	ADD	r3, r3, #8
	ADD	r4, r4, #8
	SUB	r8, r2, r8, LSL #1	; r8 = t[0] - t[7]
	SUB	r10,r6, r10,LSL #1	; r10= t[1] - t[6]
	SUB	r14,r3, r14,LSL #1	; r14= t[2] - t[5]
	SUB	r9, r4, r9, LSL #1	; r9 = t[3] - t[4]
	MOV	r2, r2, ASR #4
	MOV	r6, r6, ASR #4
	MOV	r3, r3, ASR #4
	MOV	r4, r4, ASR #4
	MOV	r8, r8, ASR #4
	MOV	r10,r10,ASR #4
	MOV	r14,r14,ASR #4
	MOV	r9, r9, ASR #4
	STRH	r2, [r0], #2		; y[0] = t[0]+t[7]
	STRH	r6, [r0, #14]		; y[1] = t[1]+t[6]
	STRH	r3, [r0, #30]		; y[2] = t[2]+t[5]
	STRH	r4, [r0, #46]		; y[3] = t[3]+t[4]
	STRH	r9, [r0, #62]		; y[4] = t[3]-t[4]
	STRH	r14,[r0, #78]		; y[5] = t[2]-t[5]
	STRH	r10,[r0, #94]		; y[6] = t[1]-t[6]
	STRH	r8, [r0, #110]		; y[7] = t[0]-t[7]

	LDMFD	r13!,{r1,PC}

	END
