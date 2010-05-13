; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

	AREA	|.text|, CODE, READONLY

	GET	common.s

	EXPORT	loop_filter_h_arm
	EXPORT	loop_filter_v_arm
	EXPORT	oc_state_loop_filter_frag_rows_inner
	EXPORT	oc_state_border_fill_row_arm
	EXPORT	oc_memzero_16_64arm
	EXPORT	oc_memzero_ptrdiff_64arm
	EXPORT	oc_memset_al_mult8arm

        EXPORT	loop_filter_h_PROFILE
loop_filter_h_PROFILE
	DCD	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

; loop_filter_h
;
; I've tried to use the ARMV6 SIMD stuff here, but have failed to find a way to
; be any faster.
;
; [ ARM_CAN_UNALIGN = 1
;	LDR	r12,[r0, #-2]		; r12=       [3] | [2] | [1] | [0]
;	; Stall (2 on Xscale)
; |
;	LDRH	r12,[r0, #-2]		; r12= pix[1]<<8 | pix[0]
;	LDRH	r5, [r0]		; r5 = pix[3]<<8 | pix[2]
;	; Stall (2 on Xscale)
;	ORR	r12,r12,r5, LSL #16	; r12=       [3] | [2] | [1] | [0]
; ]
;	AND	r5, r6, r12,ROR #8	; r5 =        0  | [3] |  0  | [1]
;	AND	r12,r6, r12,ROR #16	; r12=        0  | [0] |  0  | [2]
;	USUB16	r12,r12,r5		; r12=       [0-3]     |   [2-1]
;	ADD	r12,r12,#4<<16		; r12=       [0-3]+4   |   [2-1]
;	ADD	r12,r12,r12,LSL #16	; r12= [0-3]+  [2-1]+4 |   [2-1]
;	ADD	r12,r12,r12,LSL #17	; r12= [0-3]+3*[2-1]+4 |   [2-1]
;
; Slower than the v4 stuff :(

 [ ARM_HAS_NEON
loop_filter_h_arm
	; r0 = unsigned char *pix
	; r1 = int            ystride
	; r2 = int           *bv
	; preserves r0-r3
	; We assume Q7 = 2*flimit in U16s

	;                    My best guesses at cycle counts (and latency)--vvv
	SUB	r12,r0, #2
	VLD1.32	{D0[0]}, [r12], r1		; D0 = ________33221100     2,1
	VLD1.32	{D2[0]}, [r12], r1		; D2 = ________77665544     2,1
	VLD1.32	{D4[0]}, [r12], r1		; D4 = ________BBAA9988     2,1
	VLD1.32	{D6[0]}, [r12], r1		; D6 = ________FFEEDDCC     2,1
	VLD1.32	{D0[1]}, [r12], r1		; D0 = JJIIHHGG33221100     2,1
	VLD1.32	{D2[1]}, [r12], r1		; D2 = NNMMLLKK77665544     2,1
	VLD1.32	{D4[1]}, [r12], r1		; D4 = RRQQPPOOBBAA9988     2,1
	VLD1.32	{D6[1]}, [r12], r1		; D6 = VVUUTTSSFFEEDDCC     2,1

	VTRN.8	D0, D2	; D0 = MMIIKKGG66224400 D2 = NNJJLLHH77335511       1,1
	VTRN.8	D4, D6	; D4 = UUQQSSOOEEAACC88 D6 = VVRRTTPPFFBBDD99       1,1
	VTRN.16	D0, D4	; D0 = SSOOKKGGCC884400 D4 = UUQQMMIIEEAA6622       1,1
	VTRN.16	D2, D6	; D2 = TTPPLLHHDD995511 D6 = VVRRNNJJFFBB7733       1,1

	VSUBL.U8	Q0, D0, D6	; Q0 = 00 - 33 in S16s              1,3
	VSUBL.U8	Q4, D4, D2	; Q4 = 22 - 11 in S16s              1,3
	; Stall
	VADD.S16	Q0, Q0, Q4	;                                   1,3
	SUB	r12, r0, #1
	; Stall
	VADD.S16	Q0, Q0, Q4	;                                   1,3

	; Stall x2
	VADD.S16	Q0, Q0, Q4	; Q0 = [0-3]+3*[2-1]                1,3
	; Stall x2
	VRSHR.S16	Q0, Q0, #3	; Q0 = f = ([0-3]+3*[2-1]+4)>>3     1,4

	;  We want to do
	; f =             CLAMP(MIN(-2F-f,0), f, MAX(2F-f,0))
	;   = ((f >= 0) ? MIN( f ,MAX(2F- f ,0)) : MAX(  f , MIN(-2F- f ,0)))
	;   = ((f >= 0) ? MIN(|f|,MAX(2F-|f|,0)) : MAX(-|f|, MIN(-2F+|f|,0)))
	;   = ((f >= 0) ? MIN(|f|,MAX(2F-|f|,0)) :-MIN( |f|,-MIN(-2F+|f|,0)))
	;   = ((f >= 0) ? MIN(|f|,MAX(2F-|f|,0)) :-MIN( |f|, MAX( 2F-|f|,0)))
	; So we've reduced the left and right hand terms to be the same, except
	; for a negation.

	; Stall x3
	VABS.S16	Q5, Q0		; Q5 = |f| in U16s                  1,4
	VSHR.S16	Q0, Q0, #15	; Q0 = -1 or 0 according to sign    1,3
	; Stall x2
	VQSUB.U16	Q8, Q7, Q5	; Q8 = MAX(2F-|f|,0) in U16s        1,4
	VMOVL.U8	Q2, D4	   ; Q2 = __UU__QQ__MM__II__EE__AA__66__22  2,3
	; Stall x2
	VMIN.U16	Q5, Q8, Q5	; Q5 = MIN(|f|,MAX(2F-|f|))         1,4
	; Now we need to correct for the sign of f.
	; For negative elements of Q0, we want to subtract the appropriate
	; element of Q5. For positive elements we want to add them. No NEON
	; instruction exists to do this, so we need to negate the negative
	; elements, and we can then just add them. a-b = a-(1+!b) = a-1+!b
	; Stall x3
	VADD.S16	Q5, Q5, Q0	;				    1,3
	; Stall x2
	VEOR.S16	Q5, Q5, Q0	; Q5 = real value of f              1,3

	; Bah. No VRSBW.U8
	; Stall (just 1 as Q5 not needed to second pipeline stage. I think.)
	VADDW.U8	Q1, Q5, D2 ; Q1 = xxTTxxPPxxLLxxHHxxDDxx99xx55xx11  1,3
	VSUB.S16	Q2, Q2, Q5 ; Q2 = xxUUxxQQxxMMxxIIxxEExxAAxx66xx22  1,3

	VQMOVUN.S16	D2, Q1		; D2 = TTPPLLHHDD995511		    1,1
	VQMOVUN.S16	D4, Q2		; D4 = UUQQMMIIEEAA6622		    1,1

	VTRN.8	D2, D4		; D2 = QQPPIIHHAA992211	D4 = MMLLEEDD6655   1,1

	VST1.16	{D2[0]}, [r12], r1
	VST1.16	{D4[0]}, [r12], r1
	VST1.16	{D2[1]}, [r12], r1
	VST1.16	{D4[1]}, [r12], r1
	VST1.16	{D2[2]}, [r12], r1
	VST1.16	{D4[2]}, [r12], r1
	VST1.16	{D2[3]}, [r12], r1
	VST1.16	{D4[3]}, [r12], r1

	MOV	PC,R14
loop_filter_v_arm
	; r0 = unsigned char *pix
	; r1 = int            ystride
	; r2 = int           *bv
	; preserves r0-r3
	; We assume Q7 = 2*flimit in U16s

	;                    My best guesses at cycle counts (and latency)--vvv
	SUB	r12,r0, r1, LSL #1
	VLD1.64	{D0}, [r12@64], r1		; D0 = SSOOKKGGCC884400     2,1
	VLD1.64	{D2}, [r12@64], r1		; D2 = TTPPLLHHDD995511     2,1
	VLD1.64	{D4}, [r12@64], r1		; D4 = UUQQMMIIEEAA6622     2,1
	VLD1.64	{D6}, [r12@64], r1		; D6 = VVRRNNJJFFBB7733     2,1

	VSUBL.U8	Q0, D0, D6	; Q0 = 00 - 33 in S16s              1,3
	VSUBL.U8	Q4, D4, D2	; Q4 = 22 - 11 in S16s              1,3
	; Stall
	VADD.S16	Q0, Q0, Q4	;                                   1,3
	SUB	r12, r0, r1
	; Stall
	VADD.S16	Q0, Q0, Q4	;                                   1,3

	; Stall x2
	VADD.S16	Q0, Q0, Q4	; Q0 = [0-3]+3*[2-1]                1,3
	; Stall x2
	VRSHR.S16	Q0, Q0, #3	; Q0 = f = ([0-3]+3*[2-1]+4)>>3     1,4

	;  We want to do
	; f =             CLAMP(MIN(-2F-f,0), f, MAX(2F-f,0))
	;   = ((f >= 0) ? MIN( f ,MAX(2F- f ,0)) : MAX(  f , MIN(-2F- f ,0)))
	;   = ((f >= 0) ? MIN(|f|,MAX(2F-|f|,0)) : MAX(-|f|, MIN(-2F+|f|,0)))
	;   = ((f >= 0) ? MIN(|f|,MAX(2F-|f|,0)) :-MIN( |f|,-MIN(-2F+|f|,0)))
	;   = ((f >= 0) ? MIN(|f|,MAX(2F-|f|,0)) :-MIN( |f|, MAX( 2F-|f|,0)))
	; So we've reduced the left and right hand terms to be the same, except
	; for a negation.

	; Stall x3
	VABS.S16	Q5, Q0		; Q5 = |f| in U16s                  1,4
	VSHR.S16	Q0, Q0, #15	; Q0 = -1 or 0 according to sign    1,3
	; Stall x2
	VQSUB.U16	Q8, Q7, Q5	; Q7 = MAX(2F-|f|,0) in U16s        1,4
	VMOVL.U8	Q2, D4	   ; Q2 = __UU__QQ__MM__II__EE__AA__66__22  2,3
	; Stall x2
	VMIN.U16	Q5, Q8, Q5	; Q5 = MIN(|f|,MAX(2F-|f|))         1,4
	; Now we need to correct for the sign of f.
	; For negative elements of Q0, we want to subtract the appropriate
	; element of Q5. For positive elements we want to add them. No NEON
	; instruction exists to do this, so we need to negate the negative
	; elements, and we can then just add them. a-b = a-(1+!b) = a-1+!b
	; Stall x3
	VADD.S16	Q5, Q5, Q0	;				    1,3
	; Stall x2
	VEOR.S16	Q5, Q5, Q0	; Q5 = real value of f              1,3

	; Bah. No VRSBW.U8
	; Stall (just 1 as Q5 not needed to second pipeline stage. I think.)
	VADDW.U8	Q1, Q5, D2 ; Q1 = xxTTxxPPxxLLxxHHxxDDxx99xx55xx11  1,3
	VSUB.S16	Q2, Q2, Q5 ; Q2 = xxUUxxQQxxMMxxIIxxEExxAAxx66xx22  1,3

	VQMOVUN.S16	D2, Q1		; D2 = TTPPLLHHDD995511		    1,1
	VQMOVUN.S16	D4, Q2		; D4 = UUQQMMIIEEAA6622		    1,1

	VST1.64	{D2}, [r12], r1
	VST1.64	{D4}, [r12], r1

	MOV	PC,R14
 |
  [ ARMV6 = 1
loop_filter_h_arm
	; r0 = unsigned char *pix
	; r1 = int            ystride
	; r2 = int           *bv
	; preserves r0-r3
	STMFD	r13!,{r3-r5,r14}

	MOV	r14,#8
lfh_lp
	LDRB	r3, [r0, #-2]		; r3 = pix[0]
	LDRB	r12,[r0, #1]		; r12= pix[3]
	LDRB	r4, [r0, #-1]		; r4 = pix[1]
	LDRB	r5, [r0]		; r5 = pix[2]
	SUB	r3, r3, r12		; r3 = pix[0]-pix[3]+4
	ADD	r3, r3, #4
	SUB	r12,r5, r4		; r12= pix[2]-pix[1]
	ADD	r12,r12,r12,LSL #1	; r12= 3*(pix[2]-pix[1])
	ADD	r12,r12,r3	; r12= pix[0]-pix[3]+3*(pix[2]-pix[1])+4

	MOV	r12,r12,ASR #3
	LDRSB	r12,[r2, r12]
	SUBS	r14,r14,#1
	; Stall on Xscale
	ADD	r4, r4, r12
	USAT	r4, #8, r4
	SUB	r5, r5, r12
	USAT	r5, #8, r5
	STRB	r4, [r0, #-1]
	STRB	r5, [r0], r1
	BGT	lfh_lp
	SUB	r0, r0, r1, LSL #3

	LDMFD	r13!,{r3-r5,PC}
loop_filter_v_arm
	; r0 = unsigned char *pix
	; r1 = int            ystride
	; r2 = int           *bv
	; preserves r0-r3
	STMFD	r13!,{r3-r6,r14}

	MOV	r14,#8
	MOV	r6, #255
lfv_lp
	LDRB	r3, [r0, -r1, LSL #1]	; r3 = pix[0]
	LDRB	r12,[r0, r1]		; r12= pix[3]
	LDRB	r4, [r0, -r1]		; r4 = pix[1]
	LDRB	r5, [r0]		; r5 = pix[2]
	SUB	r3, r3, r12		; r3 = pix[0]-pix[3]+4
	ADD	r3, r3, #4
	SUB	r12,r5, r4		; r12= pix[2]-pix[1]
	ADD	r12,r12,r12,LSL #1	; r12= 3*(pix[2]-pix[1])
	ADD	r12,r12,r3	; r12= pix[0]-pix[3]+3*(pix[2]-pix[1])+4
	MOV	r12,r12,ASR #3
	LDRSB	r12,[r2, r12]
	SUBS	r14,r14,#1
	; Stall (on Xscale)
	ADD	r4, r4, r12
	USAT	r4, #8, r4
	SUB	r5, r5, r12
	USAT	r5, #8, r5
	STRB	r4, [r0, -r1]
	STRB	r5, [r0], #1
	BGT	lfv_lp

	SUB	r0, r0, #8

	LDMFD	r13!,{r3-r6,PC}
  |
	; Vanilla ARM v4 version
loop_filter_h_arm
	; r0 = unsigned char *pix
	; r1 = int            ystride
	; r2 = int           *bv
	; preserves r0-r3
	STMFD	r13!,{r3-r6,r14}

	MOV	r14,#8
	MOV	r6, #255
lfh_lp
	LDRB	r3, [r0, #-2]		; r3 = pix[0]
	LDRB	r12,[r0, #1]		; r12= pix[3]
	LDRB	r4, [r0, #-1]		; r4 = pix[1]
	LDRB	r5, [r0]		; r5 = pix[2]
	SUB	r3, r3, r12		; r3 = pix[0]-pix[3]+4
	ADD	r3, r3, #4
	SUB	r12,r5, r4		; r12= pix[2]-pix[1]
	ADD	r12,r12,r12,LSL #1	; r12= 3*(pix[2]-pix[1])
	ADD	r12,r12,r3	; r12= pix[0]-pix[3]+3*(pix[2]-pix[1])+4
   [ 1 = 1
	MOV	r12,r12,ASR #3
	LDRSB	r12,[r2, r12]
	; Stall (2 on Xscale)
   |
	MOVS	r12,r12,ASR #3
	ADDLE	r3, r2, r12
	SUBGT	r3, r2, r12		; r3 = 2F-|f|
	BIC	r3, r3, r3, ASR #32	; r3 = MAX(2F-|f|, 0)
	CMP	r3, r12			; if (r3 < r12)
	MOVLT	r12,r3			;   r12=r3   r12=MIN(r12,r3)
	CMN	r3, r12			; if (-r3 < r12)
	RSBLT	r12,r3, #0		;   r12=-r3
   ]
	ADDS	r4, r4, r12
	CMPGT	r6, r4
	EORLT	r4, r6, r4, ASR #32
	SUBS	r5, r5, r12
	CMPGT	r6, r5
	EORLT	r5, r6, r5, ASR #32
	STRB	r4, [r0, #-1]
	STRB	r5, [r0], r1
	SUBS	r14,r14,#1
	BGT	lfh_lp
	SUB	r0, r0, r1, LSL #3

	LDMFD	r13!,{r3-r6,PC}
loop_filter_v_arm
	; r0 = unsigned char *pix
	; r1 = int            ystride
	; r2 = int           *bv
	; preserves r0-r3
	STMFD	r13!,{r3-r6,r14}

	MOV	r14,#8
	MOV	r6, #255
lfv_lp
	LDRB	r3, [r0, -r1, LSL #1]	; r3 = pix[0]
	LDRB	r12,[r0, r1]		; r12= pix[3]
	LDRB	r4, [r0, -r1]		; r4 = pix[1]
	LDRB	r5, [r0]		; r5 = pix[2]
	SUB	r3, r3, r12		; r3 = pix[0]-pix[3]+4
	ADD	r3, r3, #4
	SUB	r12,r5, r4		; r12= pix[2]-pix[1]
	ADD	r12,r12,r12,LSL #1	; r12= 3*(pix[2]-pix[1])
	ADD	r12,r12,r3	; r12= pix[0]-pix[3]+3*(pix[2]-pix[1])+4
 [ 1 = 1
	MOV	r12,r12,ASR #3
	LDRSB	r12,[r2, r12]
	; Stall (2 on Xscale)
 |
	MOVS	r12,r12,ASR #3
	ADDLE	r3, r2, r12
	SUBGT	r3, r2, r12		; r3 = 2F-|f|
	BIC	r3, r3, r3, ASR #32	; r3 = MAX(2F-|f|, 0)
	CMP	r3, r12			; if (r3 < r12)
	MOVLT	r12,r3			;   r12=r3   r12=MIN(r12,r3)
	CMN	r3, r12			; if (-r3 < r12)
	RSBLT	r12,r3, #0		;   r12=-r3
 ]
	ADDS	r4, r4, r12
	CMPGT	r6, r4
	EORLT	r4, r6, r4, ASR #32
	SUBS	r5, r5, r12
	CMPGT	r6, r5
	EORLT	r5, r6, r5, ASR #32
	STRB	r4, [r0, -r1]
	STRB	r5, [r0], #1
	SUBS	r14,r14,#1
	BGT	lfv_lp

	SUB	r0, r0, #8

	LDMFD	r13!,{r3-r6,PC}

  ]
 ]

; Which bit this is depends on the order of packing within a bitfield.
CODED	*	1

oc_state_loop_filter_frag_rows_inner
	; r0 = ref_frame_data
	; r1 = ystride
	; r2 = bv
	; r3 = frags
	; r4 = fragi0
	; r5 = fragi0_end
	; r6 = fragi_top
	; r7 = fragi_bot
	; r8 = frag_buf_offs
	; r9 = nhfrags
	MOV	r12,r13
	STMFD	r13!,{r0,r4-r11,r14}
	LDMFD	r12,{r4-r9}

	CMP	r4, r5		; if(fragi0>=fragi0_end)
	BGE	oslffri_end	;   bale
	SUBS	r9, r9, #1	; r9 = nhfrags-1	if (r9<=0)
	BLE	oslffri_end	;			  bale
 [ ARM_HAS_NEON
	LDRSB	r12,[r2,#256-127]	; r2 = 2F = 2*flimit
 ]
	ADD	r3, r3, r4, LSL #2	; r3 = &frags[fragi]
	ADD	r8, r8, r4, LSL #2	; r8 = &frag_buf_oggs[fragi]
 [ ARM_HAS_NEON
	VDUP.S16	Q7, r12		; Q7 = 2F in U16s
 ]
	SUB	r7, r7, r9	; fragi_bot -= nfrags;
oslffri_lp1
	MOV	r10,r4		; r10= fragi = fragi0
	ADD	r11,r4, r9	; r11= fragi_end-1=fragi+nfrags-1
oslffri_lp2
	LDR	r14,[r3], #4	; r14= frags[fragi]	frags++
	LDR	r0, [r13]	; r0 = ref_frame_data
	LDR	r12,[r8], #4	; r12= frag_buf_offs[fragi]   frag_buf_offs++
	TST	r14,#CODED
	BEQ	oslffri_uncoded
	CMP	r10,r4		; if (fragi>fragi0)
	ADD	r0, r0, r12	; r0 = ref_frame_data + frag_buf_offs[fragi]
	BLGT	loop_filter_h_arm
	CMP	r4, r6		; if (fragi0>fragi_top)
	BLGT	loop_filter_v_arm
	CMP	r10,r11		; if(fragi+1<fragi_end)===(fragi<fragi_end-1)
	LDRLT	r12,[r3]	; r12 = frags[fragi+1]
	ADD	r0, r0, #8
	ADD	r10,r10,#1	; r10 = fragi+1;
	ANDLT	r12,r12,#CODED
	CMPLT	r12,#CODED	; && frags[fragi+1].coded==0
	BLLT	loop_filter_h_arm
	CMP	r10,r7		; if (fragi<fragi_bot)
	LDRLT	r12,[r3, r9, LSL #2]	; r12 = frags[fragi+1+nhfrags-1]
	SUB	r0, r0, #8
	ADD	r0, r0, r1, LSL #3
	ANDLT	r12,r12,#CODED
	CMPLT	r12,#CODED
	BLLT	loop_filter_v_arm

	CMP	r10,r11		; while(fragi<=fragiend-1)
	BLE	oslffri_lp2
	MOV	r4, r10		; r4 = fragi0 += nhfrags
	CMP	r4, r5
	BLT	oslffri_lp1

oslffri_end
	LDMFD	r13!,{r0,r4-r11,PC}
oslffri_uncoded
	ADD	r10,r10,#1
	CMP	r10,r11
	BLE	oslffri_lp2
	MOV	r4, r10		; r4 = fragi0 += nhfrags
	CMP	r4, r5
	BLT	oslffri_lp1

	LDMFD	r13!,{r0,r4-r11,PC}

 [ ARM_HAS_NEON
oc_state_border_fill_row_arm
	; r0 = apix
	; r1 = epix
	; r2 = bpix
	; r3 = hpadding (16 or 8)
	; <> = stride
	LDR	r12,[r13]
	STR	R14,[r13,#-4]!

	; This should never happen in any sane stream, but...
	CMP	r0, r1
	BEQ	osbrf_end
	SUB	r14,r0, r3
	CMP	r3, #8
	SUB	r3, r2, #1
	BEQ	osbrf_small
osbfr_lp
	VLD1.U8	{D0[],D1[]}, [r0], r12
	VLD1.U8	{D2[],D3[]}, [r3], r12
	CMP	r0, r1
	VST1.64	{Q0}, [r14@64],r12
	VST1.64	{Q1}, [r2 @64],r12
	BNE	osbfr_lp

	LDR	PC,[R13],#4

osbrf_small
osbfr_lp2
	VLD1.U8	{D0[]}, [r0], r12
	VLD1.U8	{D2[]}, [r3], r12
	CMP	r0, r1
	VST1.32	{D0}, [r14@64],r12
	VST1.32	{D2}, [r2 @64],r12
	BNE	osbfr_lp2
osbrf_end
	LDR	PC,[R13],#4
 |
  [ ARM_HAS_LDRD = 1
oc_state_border_fill_row_arm
	; r0 = apix
	; r1 = epix
	; r2 = bpix
	; r3 = hpadding (16 or 8)
	; <> = stride
	LDR	r12,[r13]
	STR	r14,[r13,#-4]!

	; This should never happen in any sane stream, but...
	CMP	r0, r1
	BEQ	osbrf_end
	SUB	r0, r0, r12
	SUB	r1, r1, r12
	CMP	r3, #8
	BEQ	osbrf_small
	STMFD	r13!,{r4}
	MOV	r14,r2
osbfr_lp
	LDRB	r2, [r0, r12]!
	LDRB	r4, [r14,#-1]
	CMP	r0, r1
	ORR	r2, r2, r2, LSL #8
	ORR	r4, r4, r4, LSL #8
	ORR	r2, r2, r2, LSL #16
	MOV	r3, r2
	STRD	r2, [r0, #-16]
	STRD	r2, [r0, #-8]
	ORR	r2, r4, r4, LSL #16
	MOV	r3, r2
	STRD	r2, [r14, #8]
	STRD	r2, [r14],r12
	BNE	osbfr_lp

	LDMFD	r13!,{r4,PC}

osbrf_small
osbfr_lp2
	LDRB	r14,[r0, r12]!
	LDRB	r3, [r2, #-1]
	CMP	r0, r1
	ORR	r14,r14,r14,LSL #8
	ORR	r3, r3, r3, LSL #8
	ORR	r14,r14,r14,LSL #16
	ORR	r3, r3, r3, LSL #16
	STR	r14,[r0, #-8]
	STR	r14,[r0, #-4]
	STR	r3, [r2, #4]
	STR	r3, [r2], r12
	BNE	osbfr_lp2
osbrf_end
	LDR	PC,[r13],#4
  |
oc_state_border_fill_row_arm
	; r0 = apix
	; r1 = epix
	; r2 = bpix
	; r3 = hpadding (16 or 8)
	; <> = stride
	LDR	r12,[r13]
	STR	r14,[r13,#-4]!

	; This should never happen in any sane stream, but...
	CMP	r0, r1
	BEQ	osbrf_end
	SUB	r0, r0, r12
	SUB	r1, r1, r12
	CMP	r3, #8
	BEQ	osbrf_small
osbfr_lp
	LDRB	r14,[r0, r12]!
	LDRB	r3, [r2, #-1]
	CMP	r0, r1
	ORR	r14,r14,r14,LSL #8
	ORR	r3, r3, r3, LSL #8
	ORR	r14,r14,r14,LSL #16
	ORR	r3, r3, r3, LSL #16
	STR	r14,[r0, #-16]
	STR	r14,[r0, #-12]
	STR	r14,[r0, #-8]
	STR	r14,[r0, #-4]
	STR	r3, [r2, #4]
	STR	r3, [r2, #8]
	STR	r3, [r2, #12]
	STR	r3, [r2], r12
	BNE	osbfr_lp

osbrf_end
	LDR	PC,[r13],#4

osbrf_small
osbfr_lp2
	LDRB	r14,[r0, r12]!
	LDRB	r3, [r2, #-1]
	CMP	r0, r1
	ORR	r14,r14,r14,LSL #8
	ORR	r3, r3, r3, LSL #8
	ORR	r14,r14,r14,LSL #16
	ORR	r3, r3, r3, LSL #16
	STR	r14,[r0, #-8]
	STR	r14,[r0, #-4]
	STR	r3, [r2, #4]
	STR	r3, [r2], r12
	BNE	osbfr_lp2

	LDR	PC,[r13],#4
  ]
 ]

oc_memzero_16_64ARM
	; r0 = ptr to fill with 2*64 0 bytes
 [ ARM_HAS_NEON
	VMOV.I8	Q0,#0
	VMOV.I8	Q1,#0
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
	MOV	PC,R14
 |
  [ ARM_HAS_LDRD
	MOV	r2, #0
	MOV	r3, #0
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	MOV	PC,R14
  |
	MOV	r1, #0
	MOV	r2, #0
	MOV	r3, #0
	MOV	r12, #0
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	MOV	PC,R14
  ]
 ]
oc_memzero_ptrdiff_64ARM
	; r0 = ptr to fill with 4*64 0 bytes
 [ ARM_HAS_NEON
	VMOV.I8	Q0,#0
	VMOV.I8	Q1,#0
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
 	VST1.32	{D0,D1,D2,D3}, [r0]!
	MOV	PC,R14
 |
  [ ARM_HAS_LDRD
	MOV	r2, #0
	MOV	r3, #0
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8

	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	STRD	r2,[r0],#8
	MOV	PC,R14
  |
	MOV	r1, #0
	MOV	r2, #0
	MOV	r3, #0
	MOV	r12, #0
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}

	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	STMIA	r0!,{r1,r2,r3,r12}
	MOV	PC,R14
  ]
 ]
oc_memset_al_mult8ARM
	; r0 = ptr to fill with a multiple of 8 bytes aligned to 8 bytes
	; r1 = size (in bytes)
	; r2 = value to fill
 [ ARM_HAS_NEON
	VDUP.U8	Q0,r2
	VDUP.U8	Q1,r2
	SUBS	r1, r1, #32
	BLT	omam8_thin
omam8_lp1
	VST1.64	{D0,D1,D2,D3}, [r0]!
	SUBS	r14,r14,#32
	BGT	omam8_lp1
omam8_thin
	ADDS	r14,r14,#24
	BLT	omam8_end
omam8_lp2
	VST1.32 {D0}, [r0]!
	SUBGES	r14,r14,#8
	BGT	omam8_lp2
omam8_end
	LDMFD	r13!,{r4-r11,PC}
 |
  [ ARM_HAS_LDRD
	STMFD	r13!,{r4-r11,r14}
	MOV	r12,r0		; r12 = address to store
	SUBS	r14,r1,#12*4	; r14 = counter
	ORR	r2, r2, r2, LSL #8
	ORR	r0, r2, r2, LSL #16
	MOV	r1,r0
	MOV	r2,r0
	MOV	r3,r0
	MOV	r4,r0
	MOV	r5,r0
	MOV	r6,r0
	MOV	r7,r0
	MOV	r8,r0
	MOV	r9,r0
	MOV	r10,r0
	MOV	r11,r0
	BLT	omam8_thin
omam8_lp1
	STMIA	r12!,{r0-r11}
	SUBS	r14,r14,#12*4
	BGT	omam8_lp1
omam8_thin
	ADDS	r14,r14,#10*4
omam8_lp2
	STRGED	r2, [r12],#4
	SUBGES	r14,r14,#8
	BGT	omam8_lp2

	LDMFD	r13!,{r4-r11,PC}
  |
	STMFD	r13!,{r4-r11,r14}
	MOV	r12,r0		; r12 = address to store
	SUBS	r14,r1,#12*4	; r14 = counter
	ORR	r2, r2, r2, LSL #8
	ORR	r0, r2, r2, LSL #16
	MOV	r1,r0
	MOV	r2,r0
	MOV	r3,r0
	MOV	r4,r0
	MOV	r5,r0
	MOV	r6,r0
	MOV	r7,r0
	MOV	r8,r0
	MOV	r9,r0
	MOV	r10,r0
	MOV	r11,r0
	BLT	omam8_thin
omam8_lp1
	STMIA	r12!,{r0-r11}
	SUBS	r14,r14,#12*4
	BGT	omam8_lp1
omam8_thin
	ADDS	r14,r14,#10*4
omam8_lp2
	STRGT	r2, [r12],#4
	STRGT	r2, [r12],#4
	SUBGTS	r14,r14,#8
	BGT	omam8_lp2

	LDMFD	r13!,{r4-r11,PC}
  ]
 ]
	END
