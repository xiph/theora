; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

	AREA	|.text|, CODE, READONLY

	GET	ARMoptions.s

	EXPORT	oc_frag_recon_inter2_arm
	EXPORT	oc_frag_recon_inter_arm
	EXPORT	oc_frag_recon_intra_arm
	EXPORT	oc_frag_copy_list_arm

 [ ARM_HAS_NEON
  [ 1
oc_frag_copy_list_arm
	; r0 = dst_frame_data
	; r1 = src_frame_data
	; r2 = ystride
	; r3 = nfragis
	; <> = fragis
	; <> = frag_buf_offs
	STMFD	r13!,{r4-r6,r14}

	SUBS	r3, r3, #1
	LDRGE	r12,[r13,#4*4]		; r12= fragis
	BLT	ofcl_end
	LDR	r14,[r13,#4*5]		; r14= frag_buf_offs
	LDR	r6, [r12],#4		; r5 = fragis[fragii]
	; Stall (2 on Xscale)
	LDR	r6, [r14,r6, LSL #2]	; r5 = frag_buf_offs[fragis[fragii]]
	; Stall (on XScale)
ofcl_lp
	ADD	r4, r1, r6
	VLD1.32	{D0}, [r4], r2
	VLD1.32	{D1}, [r4], r2
	VLD1.32	{D2}, [r4], r2
	VLD1.32	{D3}, [r4], r2
	ADD	r5, r6, r0
	VLD1.32	{D4}, [r4], r2
	SUBS	r3, r3, #1
	VLD1.32	{D5}, [r4], r2
	VLD1.32	{D6}, [r4], r2
	VLD1.32	{D7}, [r4], r2
	VST1.32	{D0}, [r5], r2
	LDRGE	r6, [r12],#4		; r6 = fragis[fragii]
	VST1.32	{D1}, [r5], r2
	VST1.32	{D2}, [r5], r2
	VST1.32	{D3}, [r5], r2
	VST1.32	{D4}, [r5], r2
	LDRGE	r6, [r14,r6, LSL #2]	; r5 = frag_buf_offs[fragis[fragii]]
	VST1.32	{D5}, [r5], r2
	VST1.32	{D6}, [r5], r2
	VST1.32	{D7}, [r5], r2
	BGE	ofcl_lp
ofcl_end
	LDMFD	r13!,{r4-r6,PC}
  |
oc_frag_copy_list_arm
	; r0 = dst_frame_data
	; r1 = src_frame_data
	; r2 = ystride
	; r3 = nfragis
	; <> = fragis
	; <> = frag_buf_offs
	STMFD	r13!,{r4-r11,r14}

	SUBS	r3, r3, #1
	BLT	ofcl_end
	LDR	r12,[r13,#4*9]		; r12= fragis
	LDR	r14,[r13,#4*10]		; r14= frag_buf_offs
ofcl_lp
	LDR	r5, [r12],#4		; r5 = fragis[fragii]
	; Stall (2 on Xscale)
	LDR	r5, [r14,r5, LSL #2]	; r5 = frag_buf_offs[fragis[fragii]]
	SUBS	r3, r3, #1
	; Stall (on XScale)
	ADD	r4, r1, r5
	VLD1.32	{D0}, [r4], r2
	VLD1.32	{D1}, [r4], r2
	VLD1.32	{D2}, [r4], r2
	VLD1.32	{D3}, [r4], r2
	VLD1.32	{D4}, [r4], r2
	VLD1.32	{D5}, [r4], r2
	VLD1.32	{D6}, [r4], r2
	VLD1.32	{D7}, [r4], r2
	ADD	r5, r5, r0
	VST1.32	{D0}, [r5], r2
	VST1.32	{D1}, [r5], r2
	VST1.32	{D2}, [r5], r2
	VST1.32	{D3}, [r5], r2
	VST1.32	{D4}, [r5], r2
	VST1.32	{D5}, [r5], r2
	VST1.32	{D6}, [r5], r2
	VST1.32	{D7}, [r5], r2
	BGE	ofcl_lp
ofcl_end
	LDMFD	r13!,{r4-r11,PC}
  ]
 |
  [ ARM_HAS_LDRD
oc_frag_copy_list_arm
	; r0 = dst_frame_data
	; r1 = src_frame_data
	; r2 = ystride
	; r3 = nfragis
	; <> = fragis
	; <> = frag_buf_offs
	STMFD	r13!,{r4-r11,r14}

	SUBS	r3, r3, #1
	BLT	ofcl_end
	LDR	r12,[r13,#4*9]		; r12= fragis
	LDR	r14,[r13,#4*10]		; r14= frag_buf_offs
ofcl_lp
	LDR	r5, [r12],#4		; r5 = fragis[fragii]
	MOV	r4, r1
	; Stall (on Xscale)
	LDR	r5, [r14,r5, LSL #2]	; r5 = frag_buf_offs[fragis[fragii]]
	SUBS	r3, r3, #1
	; Stall (on XScale)
	LDRD	r6, [r4, r5]!		; r4 = src_frame_data+frag_buf_off
	LDRD	r8, [r4, r2]!
	; Stall
	STRD	r6, [r5, r0]!		; r5 = dst_frame_data+frag_buf_off
 	STRD	r8, [r5, r2]!
	; Stall
	LDRD	r6, [r4, r2]!	; On Xscale at least, doing 3 consecutive
	LDRD	r8, [r4, r2]!	; loads causes a stall, but that's no worse
	LDRD	r10,[r4, r2]!	; than us only doing 2, and having to do
				; another pair of LDRD/STRD later on.
	; Stall
	STRD	r6, [r5, r2]!
 	STRD	r8, [r5, r2]!
 	STRD	r10,[r5, r2]!
	; Stall
	LDRD	r6, [r4, r2]!
	LDRD	r8, [r4, r2]!
	LDRD	r10,[r4, r2]!
	; Stall
	STRD	r6, [r5, r2]!
 	STRD	r8, [r5, r2]!
 	STRD	r10,[r5, r2]!
	BGE	ofcl_lp
ofcl_end
	LDMFD	r13!,{r4-r11,PC}
  |
oc_frag_copy_list_arm
	; r0 = dst_frame_data
	; r1 = src_frame_data
	; r2 = ystride
	; r3 = nfragis
	; <> = fragis
	; <> = frag_buf_offs
	STMFD	r13!,{r4-r6,r11,r14}

	SUBS	r3, r3, #1
	BLT	ofcl_end
	LDR	r12,[r13,#4*5]		; r12 = fragis
	LDR	r14,[r13,#4*6]		; r14 = frag_buf_offs
	SUB	r2, r2, #4
ofcl_lp
	LDR	r11,[r12],#4		; r11 = fragis[fragii]
	; Stall (2 on Xscale)
	LDR	r11,[r14,r11,LSL #2]	; r11 = frag_buf_offs[fragis[fragii]]
	SUBS	r3, r3, #1
	; Stall (on XScale)
	ADD	r4, r1, r11		; r4 = src_frame_data+frag_buf_off

	LDR	r6, [r4], #4
	ADD	r11,r0, r11		; r11= dst_frame_data+frag_buf_off
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	STR	r5, [r11],r2
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	STR	r5, [r11],r2
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	STR	r5, [r11],r2
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	STR	r5, [r11],r2
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	STR	r5, [r11],r2
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	STR	r5, [r11],r2
	LDR	r5, [r4], r2
	STR	r6, [r11],#4
	LDR	r6, [r4], #4
	LDR	r4, [r4]
	STR	r5, [r11],r2
	STR	r6, [r11],#4
	STR	r4, [r11]
	BGE	ofcl_lp
ofcl_end
	LDMFD	r13!,{r4-r6,r11,PC}
  ]
 ]

 [ ARM_HAS_NEON
oc_frag_recon_intra_arm
	; r0 =       unsigned char *dst
	; r1 =       int            ystride
	; r2 = const ogg_int16_t    residue[64]
	MOV	r3, #128
	VLDMIA	r2,  {D0-D15}	; D0 = 3333222211110000 etc	; 9(8) cycles
	VDUP.S16	Q8, r3
	VQADD.S16	Q0, Q0, Q8
	VQADD.S16	Q1, Q1, Q8
	VQADD.S16	Q2, Q2, Q8
	VQADD.S16	Q3, Q3, Q8
	VQADD.S16	Q4, Q4, Q8
	VQADD.S16	Q5, Q5, Q8
	VQADD.S16	Q6, Q6, Q8
	VQADD.S16	Q7, Q7, Q8

	VQMOVUN.S16	D0, Q0	; D0 = 7766554433221100		; 1 cycle
	VQMOVUN.S16	D1, Q1	; D1 = FFEEDDCCBBAA9988		; 1 cycle
	VQMOVUN.S16	D2, Q2	; D2 = NNMMLLKKJJIIHHGG		; 1 cycle
	VQMOVUN.S16	D3, Q3	; D3 = VVUUTTSSRRQQPPOO		; 1 cycle
	VQMOVUN.S16	D4, Q4	; D4 = ddccbbaaZZYYXXWW		; 1 cycle
	VQMOVUN.S16	D5, Q5	; D5 = llkkjjiihhggffee		; 1 cycle
	VQMOVUN.S16	D6, Q6	; D6 = ttssrrqqppoonnmm		; 1 cycle
	VQMOVUN.S16	D7, Q7	; D7 = !!@@zzyyxxwwvvuu		; 1 cycle

	VST1.64	{D0}, [r0], r1
	VST1.64	{D1}, [r0], r1
	VST1.64	{D2}, [r0], r1
	VST1.64	{D3}, [r0], r1
	VST1.64	{D4}, [r0], r1
	VST1.64	{D5}, [r0], r1
	VST1.64	{D6}, [r0], r1
	VST1.64	{D7}, [r0], r1

	MOV	PC,R14
oc_frag_recon_inter_arm
	; r0 =       unsigned char *dst
	; r1 = const unsigned char *src
	; r2 =       int            ystride
	; r3 = const ogg_int16_t    residue[64]
	VLD1.64	{D24}, [r1], r2
	VLD1.64	{D25}, [r1], r2
	VLD1.64	{D26}, [r1], r2
	VLD1.64	{D27}, [r1], r2
	VLD1.64	{D28}, [r1], r2
	VLD1.64	{D29}, [r1], r2
	VLD1.64	{D30}, [r1], r2
	VLD1.64	{D31}, [r1], r2
	VLDMIA	r3, {D0-D15}	; D0 = 3333222211110000 etc	; 9(8) cycles
	VMOVL.U8	Q8, D24	; Q8 = __77__66__55__44__33__22__11__00
	VMOVL.U8	Q9, D25	; etc
	VMOVL.U8	Q10,D26
	VMOVL.U8	Q11,D27
	VMOVL.U8	Q12,D28
	VMOVL.U8	Q13,D29
	VMOVL.U8	Q14,D30
	VMOVL.U8	Q15,D31
	VQADD.S16	Q0, Q0, Q8
	VQADD.S16	Q1, Q1, Q9
	VQADD.S16	Q2, Q2, Q10
	VQADD.S16	Q3, Q3, Q11
	VQADD.S16	Q4, Q4, Q12
	VQADD.S16	Q5, Q5, Q13
	VQADD.S16	Q6, Q6, Q14
	VQADD.S16	Q7, Q7, Q15

	VQMOVUN.S16	D0, Q0
	VQMOVUN.S16	D1, Q1
	VQMOVUN.S16	D2, Q2
	VQMOVUN.S16	D3, Q3
	VQMOVUN.S16	D4, Q4
	VQMOVUN.S16	D5, Q5
	VQMOVUN.S16	D6, Q6
	VQMOVUN.S16	D7, Q7

	VST1.64	{D0}, [r0], r2
	VST1.64	{D1}, [r0], r2
	VST1.64	{D2}, [r0], r2
	VST1.64	{D3}, [r0], r2
	VST1.64	{D4}, [r0], r2
	VST1.64	{D5}, [r0], r2
	VST1.64	{D6}, [r0], r2
	VST1.64	{D7}, [r0], r2

	MOV	PC,R14
oc_frag_recon_inter2_arm
	; r0 =       unsigned char *dst
	; r1 = const unsigned char *src1
	; r2 = const unsigned char *src2
	; r3 =       int            ystride
	LDR	r12,[r13]
	; r12= const ogg_int16_t    residue[64]
	VLD1.64	{D16}, [r1], r3
	VLD1.64	{D17}, [r1], r3
	VLD1.64	{D18}, [r1], r3
	VLD1.64	{D19}, [r1], r3
	VLD1.64	{D20}, [r1], r3
	VLD1.64	{D21}, [r1], r3
	VLD1.64	{D22}, [r1], r3
	VLD1.64	{D23}, [r1], r3
	VLD1.64	{D24}, [r2], r3
	VLD1.64	{D25}, [r2], r3
	VLD1.64	{D26}, [r2], r3
	VLD1.64	{D27}, [r2], r3
	VLD1.64	{D28}, [r2], r3
	VLD1.64	{D29}, [r2], r3
	VLD1.64	{D30}, [r2], r3
	VLD1.64	{D31}, [r2], r3
	VLDMIA	r12,{D0-D15}
	VHADD.U8	Q12,Q8, Q12 ; Q12= FFEEDDCCBBAA99887766554433221100
	VHADD.U8	Q13,Q9, Q13
	VHADD.U8	Q14,Q10,Q14
	VHADD.U8	Q15,Q11,Q15
	VMOVL.U8	Q8, D24	; Q8 = __77__66__55__44__33__22__11__00
	VMOVL.U8	Q9, D25	; etc
	VMOVL.U8	Q10,D26
	VMOVL.U8	Q11,D27
	VMOVL.U8	Q12,D28
	VMOVL.U8	Q13,D29
	VMOVL.U8	Q14,D30
	VMOVL.U8	Q15,D31

	VQADD.S16	Q0, Q0, Q8
	VQADD.S16	Q1, Q1, Q9
	VQADD.S16	Q2, Q2, Q10
	VQADD.S16	Q3, Q3, Q11
	VQADD.S16	Q4, Q4, Q12
	VQADD.S16	Q5, Q5, Q13
	VQADD.S16	Q6, Q6, Q14
	VQADD.S16	Q7, Q7, Q15

	VQMOVUN.S16	D0, Q0
	VQMOVUN.S16	D1, Q1
	VQMOVUN.S16	D2, Q2
	VQMOVUN.S16	D3, Q3
	VQMOVUN.S16	D4, Q4
	VQMOVUN.S16	D5, Q5
	VQMOVUN.S16	D6, Q6
	VQMOVUN.S16	D7, Q7

	VST1.64	{D0}, [r0], r3
	VST1.64	{D1}, [r0], r3
	VST1.64	{D2}, [r0], r3
	VST1.64	{D3}, [r0], r3
	VST1.64	{D4}, [r0], r3
	VST1.64	{D5}, [r0], r3
	VST1.64	{D6}, [r0], r3
	VST1.64	{D7}, [r0], r3

	MOV	PC,R14
 |
  [ ARMV6
  [ ARM_HAS_LDRD
oc_frag_recon_intra_arm
	; r0 =       unsigned char *dst
	; r1 =       int            ystride
	; r2 = const ogg_int16_t    residue[64]
	STMFD	r13!,{r4-r6,r14}

	MOV	r14,#8
	MOV	r12,r2
	LDR	r6, =0x00800080
ofrintra_lp
	LDRD	r2, [r12],#8	; r2 = 11110000 r3 = 33332222
	LDRD	r4, [r12],#8	; r4 = 55554444 r5 = 77776666
	SUBS	r14,r14,#1
	QADD16	r2, r2, r6
	QADD16	r3, r3, r6
	QADD16	r4, r4, r6
	QADD16	r5, r5, r6
	USAT16	r2, #8, r2		; r2 = __11__00
	USAT16	r3, #8, r3		; r3 = __33__22
	USAT16	r4, #8, r4		; r4 = __55__44
	USAT16	r5, #8, r5		; r5 = __77__66
	ADD	r2, r2, r2, LSR #8	; r2 = __111100
	ADD	r3, r3, r3, LSR #8	; r3 = __333322
	BIC	r2, r2, #0x00FF0000	; r2 = ____1100
	ORR	r2, r2, r3, LSL #16	; r2 = 33221100
	ADD	r4, r4, r4, LSR #8	; r4 = __555544
	ADD	r5, r5, r5, LSR #8	; r5 = __777766
	BIC	r4, r4, #0x00FF0000	; r4 = ____5544
	ORR	r3, r4, r5, LSL #16	; r3 = 77665544
	STRD	r2, [r0], r1
	BGT	ofrintra_lp

	LDMFD	r13!,{r4-r6,PC}

oc_frag_recon_inter_arm
	; r0 =       unsigned char *dst
	; r1 = const unsigned char *src
	; r2 =       int            ystride
	; r3 = const ogg_int16_t    residue[64]
	STMFD	r13!,{r4-r11,r14}

	MOV	r14,#8
	LDR	r12,=0x00FF00FF
ofrinter_lp
 [ ARM_CAN_UNALIGN_LDRD
	LDRD	r4, [r1], r2	; Unaligned ; r4 = 33221100 r5 = 77665544
 |
	LDR	r5, [r1, #4]
	LDR	r4, [r1], r2
 ]
	LDRD	r6, [r3], #8		; r6 = 11110000 r7 = 33332222
	SUBS	r14,r14,#1
	PKHBT	r10,r4, r4, LSL #8	; r10= 22111100
	PKHTB	r4, r4, r4, ASR #8	; r4 = 33222211
	LDRD	r8, [r3], #8		; r8 = 55554444 r9 = 77776666
	PKHBT	r11,r5, r5, LSL #8	; r11= 66555544
	PKHTB	r5, r5, r5, ASR #8	; r5 = 77666655
	AND	r10,r12,r10		; r10= __11__00
	AND	r4, r12,r4, LSR #8	; r4 = __33__22
	AND	r11,r12,r11		; r11= __11__00
	AND	r5, r12,r5, LSR #8	; r5 = __33__22
	QADD16	r6, r6, r10		; r6 = xx11xx00
	QADD16	r7, r7, r4		; r7 = xx33xx22
	QADD16	r8, r8, r11		; r8 = xx55xx44
	QADD16	r9, r9, r5		; r9 = xx77xx66
	USAT16	r6, #8, r6		; r6 = __11__00
	USAT16	r7, #8, r7		; r7 = __33__22
	USAT16	r8, #8, r8		; r8 = __55__44
	USAT16	r9, #8, r9		; r9 = __77__66
	ADD	r6, r6, r6, LSR #8	; r6 = __111100
	ADD	r7, r7, r7, LSR #8	; r7 = __333322
	BIC	r6, r6, #0x00FF0000	; r6 = ____1100
	ORR	r6, r6, r7, LSL #16	; r6 = 33221100
	ADD	r8, r8, r8, LSR #8	; r8 = __555544
	ADD	r9, r9, r9, LSR #8	; r9 = __777766
	BIC	r8, r8, #0x00FF0000	; r8 = ____5544
	ORR	r7, r8, r9, LSL #16	; r9 = 77665544
	STRD	r6, [r0], r2
	BGT	ofrinter_lp

	LDMFD	r13!,{r4-r11,PC}

oc_frag_recon_inter2_arm
	; r0 =       unsigned char *dst
	; r1 = const unsigned char *src1
	; r2 = const unsigned char *src2
	; r3 =       int            ystride
	LDR	r12,[r13]
	; r12= const ogg_int16_t    residue[64]
	STMFD	r13!,{r4-r11,r14}

	MOV	r14,#8
	LDR	r7, =0x00FF00FF
ofrinter2_lp
	LDR	r5, [r1, #4]	; Unaligned	; r5 = src1[1] = 77665544
	LDR	r6, [r2, #4]	; Unaligned	; r6 = src2[1] = 77665544
	SUBS	r14,r14,#1
	LDRD	r8, [r12,#8]	; r8 = 55554444 r9 = 77776666
	UHADD8	r5, r5, r6	; r5 = (src1[7,6,5,4] + src2[7,6,5,4])>>1
	PKHBT	r6, r5, r5, LSL #8	; r6 = 66555544
	PKHTB	r5, r5, r5, ASR #8	; r5 = 77666655
	AND	r6, r7, r6		; r6 = __55__44
	AND	r5, r7, r5, LSR #8	; r5 = __77__66
	QADD16	r8, r8, r6		; r8 = xx55xx44
	QADD16	r9, r9, r5		; r9 = xx77xx66
	LDR	r5, [r1], r3	; Unaligned	; r5 = src1[0] = 33221100
	LDR	r6, [r2], r3	; Unaligned	; r6 = src2[0] = 33221100
	USAT16	r8, #8, r8		; r8 = __55__44
	USAT16	r9, #8, r9		; r9 = __77__66
	ADD	r8, r8, r8, LSR #8	; r8 = __555544
	ADD	r9, r9, r9, LSR #8	; r9 = __777766
	LDRD	r10,[r12],#16	; r10= 33332222 r11= 11110000
	BIC	r8, r8, #0x00FF0000	; r8 = ____5544

	UHADD8	r5, r5, r6	; r5 = (src1[3,2,1,0] + src2[3,2,1,0])>>1
	ORR	r9, r8, r9, LSL #16	; r9 = 77665544
	PKHBT	r6, r5, r5, LSL #8	; r6 = 22111100
	PKHTB	r5, r5, r5, ASR #8	; r5 = 33222211
	AND	r6, r7, r6		; r6 = __11__00
	AND	r5, r7, r5, LSR #8	; r5 = __33__22
	QADD16	r10,r10,r6		; r10= xx11xx00
	QADD16	r11,r11,r5		; r11= xx33xx22
	USAT16	r10,#8, r10		; r10= __11__00
	USAT16	r11,#8, r11		; r11= __33__22
	ADD	r10,r10,r10,LSR #8	; r10= __111100
	ADD	r11,r11,r11,LSR #8	; r11= __333322
	BIC	r10,r10,#0x00FF0000	; r10= ____1100
	ORR	r8, r10,r11,LSL #16	; r8 = 33221100
	STRD	r8, [r0], r3

	BGT	ofrinter2_lp

	LDMFD	r13!,{r4-r11,PC}
  |
  ; Vanilla ARM v4 version
oc_frag_recon_intra_arm
	; r0 =       unsigned char *dst
	; r1 =       int            ystride
	; r2 = const ogg_int16_t    residue[64]
	STMFD	r13!,{r4,r5,r14}

	MOV	r14,#8
	MOV	r5, #255
	SUB	r1, r1, #7
ofrintra_lp
	LDRSH	r3, [r2], #2
	LDRSH	r4, [r2], #2
	LDRSH	r12,[r2], #2
	ADDS	r3, r3, #128
	CMPGT	r5, r3
	EORLT	r3, r5, r3, ASR #32
	STRB	r3, [r0], #1
	ADDS	r4, r4, #128
	CMPGT	r5, r4
	EORLT	r4, r5, r4, ASR #32
	LDRSH	r3, [r2], #2
	STRB	r4, [r0], #1
	ADDS	r12,r12,#128
	CMPGT	r5, r12
	EORLT	r12,r5, r12,ASR #32
	LDRSH	r4, [r2], #2
	STRB	r12,[r0], #1
	ADDS	r3, r3, #128
	CMPGT	r5, r3
	EORLT	r3, r5, r3, ASR #32
	LDRSH	r12,[r2], #2
	STRB	r3, [r0], #1
	ADDS	r4, r4, #128
	CMPGT	r5, r4
	EORLT	r4, r5, r4, ASR #32
	LDRSH	r3, [r2], #2
	STRB	r4, [r0], #1
	ADDS	r12,r12,#128
	CMPGT	r5, r12
	EORLT	r12,r5, r12,ASR #32
	LDRSH	r4, [r2], #2
	STRB	r12,[r0], #1
	ADDS	r3, r3, #128
	CMPGT	r5, r3
	EORLT	r3, r5, r3, ASR #32
	STRB	r3, [r0], #1
	ADDS	r4, r4, #128
	CMPGT	r5, r4
	EORLT	r4, r5, r4, ASR #32
	STRB	r4, [r0], r1
	SUBS	r14,r14,#1
	BGT	ofrintra_lp

	LDMFD	r13!,{r4,r5,PC}

oc_frag_recon_inter_arm
	; r0 =       unsigned char *dst
	; r1 = const unsigned char *src
	; r2 =       int            ystride
	; r3 = const ogg_int16_t    residue[64]
	STMFD	r13!,{r5,r9-r11,r14}

	MOV	r9, #8
	MOV	r5, #255
	SUB	r2, r2, #7
ofrinter_lp
	LDRSH	r12,[r3], #2
	LDRB	r14,[r1], #1
	LDRSH	r11,[r3], #2
	LDRB	r10,[r1], #1
	ADDS	r12,r12,r14
	CMPGT	r5, r12
	EORLT	r12,r5, r12,ASR #32
	STRB	r12,[r0], #1
	ADDS	r11,r11,r10
	CMPGT	r5, r11
	LDRSH	r12,[r3], #2
	LDRB	r14,[r1], #1
	EORLT	r11,r5, r11,ASR #32
	STRB	r11,[r0], #1
	ADDS	r12,r12,r14
	CMPGT	r5, r12
	LDRSH	r11,[r3], #2
	LDRB	r10,[r1], #1
	EORLT	r12,r5, r12,ASR #32
	STRB	r12,[r0], #1
	ADDS	r11,r11,r10
	CMPGT	r5, r11
	LDRSH	r12,[r3], #2
	LDRB	r14,[r1], #1
	EORLT	r11,r5, r11,ASR #32
	STRB	r11,[r0], #1
	ADDS	r12,r12,r14
	CMPGT	r5, r12
	LDRSH	r11,[r3], #2
	LDRB	r10,[r1], #1
	EORLT	r12,r5, r12,ASR #32
	STRB	r12,[r0], #1
	ADDS	r11,r11,r10
	CMPGT	r5, r11
	LDRSH	r12,[r3], #2
	LDRB	r14,[r1], #1
	EORLT	r11,r5, r11,ASR #32
	STRB	r11,[r0], #1
	ADDS	r12,r12,r14
	CMPGT	r5, r12
	LDRSH	r11,[r3], #2
	LDRB	r10,[r1], r2
	EORLT	r12,r5, r12,ASR #32
	STRB	r12,[r0], #1
	ADDS	r11,r11,r10
	CMPGT	r5, r11
	EORLT	r11,r5, r11,ASR #32
	STRB	r11,[r0], r2
	SUBS	r9, r9, #1
	BGT	ofrinter_lp

	LDMFD	r13!,{r5,r9-r11,PC}

oc_frag_recon_inter2_arm
	; r0 =       unsigned char *dst
	; r1 = const unsigned char *src1
	; r2 = const unsigned char *src2
	; r3 =       int            ystride
	LDR	r12,[r13]
	; r12= const ogg_int16_t    residue[64]
	STMFD	r13!,{r4-r8,r14}

	MOV	r14,#8
	MOV	r8, #255
	SUB	r3, r3, #7
ofrinter2_lp
	LDRB	r5, [r1], #1
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	LDRB	r7, [r1], #1
	ADD	r5, r5, r6
	ADDS	r5, r4, r5, LSR #1
	CMPGT	r8, r5
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	EORLT	r5, r8, r5, ASR #32
	STRB	r5, [r0], #1

	ADD	r7, r7, r6
	ADDS	r7, r4, r7, LSR #1
	CMPGT	r8, r7
	LDRB	r5, [r1], #1
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	EORLT	r7, r8, r7, ASR #32
	STRB	r7, [r0], #1

	ADD	r5, r5, r6
	ADDS	r5, r4, r5, LSR #1
	CMPGT	r8, r5
	LDRB	r7, [r1], #1
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	EORLT	r5, r8, r5, ASR #32
	STRB	r5, [r0], #1

	ADD	r7, r7, r6
	ADDS	r7, r4, r7, LSR #1
	CMPGT	r8, r7
	LDRB	r5, [r1], #1
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	EORLT	r7, r8, r7, ASR #32
	STRB	r7, [r0], #1

	ADD	r5, r5, r6
	ADDS	r5, r4, r5, LSR #1
	CMPGT	r8, r5
	LDRB	r7, [r1], #1
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	EORLT	r5, r8, r5, ASR #32
	STRB	r5, [r0], #1

	ADD	r7, r7, r6
	ADDS	r7, r4, r7, LSR #1
	CMPGT	r8, r7
	LDRB	r5, [r1], #1
	LDRB	r6, [r2], #1
 	LDRSH	r4, [r12],#2
	EORLT	r7, r8, r7, ASR #32
	STRB	r7, [r0], #1

	ADD	r5, r5, r6
	ADDS	r5, r4, r5, LSR #1
	CMPGT	r8, r5
	LDRB	r7, [r1], r3
	LDRB	r6, [r2], r3
 	LDRSH	r4, [r12],#2
	EORLT	r5, r8, r5, ASR #32
	STRB	r5, [r0], #1

	ADD	r7, r7, r6
	ADDS	r7, r4, r7, LSR #1
	CMPGT	r8, r7
	EORLT	r7, r8, r7, ASR #32
	STRB	r7, [r0], r3

	SUBS	r14,r14,#1
	BGT	ofrinter2_lp

	LDMFD	r13!,{r4-r8,PC}
  ]
  ]
 ]
	END
