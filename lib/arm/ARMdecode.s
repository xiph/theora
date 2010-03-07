; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

	AREA	|.text|, CODE, READONLY

	GET	common.s
	GET	ARMoffsets.s

	EXPORT	oc_sb_run_unpack
	EXPORT	oc_block_run_unpack
	EXPORT	oc_dec_partial_sb_flags_unpack
	EXPORT	oc_dec_coded_sb_flags_unpack
	EXPORT	oc_dec_coded_flags_unpack

	IMPORT	oc_pack_look
	IMPORT	oc_pack_read1
	IMPORT	oc_pack_adv

oc_sb_run_unpack
	STMFD	r13!,{r0,r4,r14}

	MOV	r1, #18
	BL	oc_pack_look
	MOV	r1, #1		; r1 = adv = 1
	MVN	r2, #0		; r2 = sub = -1
	TST	r0, #0x20000	; if (bits&0x20000)
	MOVNE	r1, #3		;   r1 = adv = 3
	MOVNE	r2, #2		;   r2 = sub = 2
	TSTNE	r0, #0x10000	;   if (bits&0x10000)
	MOVNE	r1, #4		;     r1 = adv = 4
	MOVNE	r2, #8		;     r2 = sub = 8
	TSTNE	r0, #0x08000	;     if (bits&0x08000)
	MOVNE	r1, #6		;       r1 = adv = 6
	MOVNE	r2, #50		;       r2 = sub = 50
	TSTNE	r0, #0x04000	;       if (bits&0x04000)
	MOVNE	r1, #8		;         r1 = adv = 8
	MOVNE	r2, #230	;         r2 = sub = 230
	TSTNE	r0, #0x02000	;         if (bits&0x02000)
	MOVNE	r1, #10		;           r1 = adv = 10
	ADDNE	r2, r2, #974-230;           r2 = sub = 974
	TSTNE	r0, #0x01000	;           if (bits&0x01000)
	MOVNE	r1, #18		;             r1 = adv = 18
	LDRNE	r2, =258014	;             r2 = sub = 258014
	RSB	r4, r1, #18
	RSB	r4, r2, r0, LSR r4; (r4 = bits>>(18-adv))-sub
	LDR	r0, [r13],#4
	BL	oc_pack_adv
	MOV	r0, r4

	LDMFD	r13!,{r4,PC}

oc_block_run_unpack
	STMFD	r13!,{r0,r4,r14}

	MOV	r1, #9
	BL	oc_pack_look
	MOV	r1, #2		; r1 = adv = 2
	MVN	r2, #0		; r2 = sub = -1
	TST	r0, #0x100	; if (bits&0x100)
	MOVNE	r1, #3		;   r1 = adv = 3
	MOVNE	r2, #1		;   r2 = sub = 1
	TSTNE	r0, #0x080	;   if (bits&0x080)
	MOVNE	r1, #4		;     r1 = adv = 4
	MOVNE	r2, #7		;     r2 = sub = 7
	TSTNE	r0, #0x040	;     if (bits&0x040)
	MOVNE	r1, #6		;       r1 = adv = 6
	MOVNE	r2, #49		;       r2 = sub = 49
	TSTNE	r0, #0x020	;       if (bits&0x020)
	MOVNE	r1, #7		;         r1 = adv = 8
	MOVNE	r2, #109	;         r2 = sub = 109
	TSTNE	r0, #0x010	;         if (bits&0x010)
	MOVNE	r1, #9		;           r1 = adv = 10
	ADDNE	r2, r2, #481-109;           r2 = sub = 481
	RSB	r4, r1, #9
	RSB	r4, r2, r0, LSR r4; (r4 = bits>>(9-adv))-sub
	LDR	r0, [r13],#4
	BL	oc_pack_adv
	SUB	r0, r4, #1

	LDMFD	r13!,{r4,PC}

oc_dec_partial_sb_flags_unpack
	; r0 = dec
	STMFD	r13!,{r5-r11,r14}

	LDR	r5, =DEC_OPB
	LDR	r9, [r0, #DEC_STATE_NSBS]	; r9 = nsbs
	LDR	r10,[r0, #DEC_STATE_SB_FLAGS]	; r10= sb_flags
	ADD	r11,r0, r5			; r11= dec->opb
	MOV	r8, #0				; r8 = npartial = 0
	MOV	r7, #0x1000			; r7 is >=0x1000 if full_run
	CMP	r9, #0
	BLE	odpsfu_end
odpsfu_lp
	CMP	r7, #0x1000		; if (full_run) (i.e. if >= 0)
	MOVGE	r0, r11
	BLGE	oc_pack_read1		; r0 = oc_pack_read1
	MOV	r6, r0			; r6 = flag
	MOV	r0, r11
	BL	oc_sb_run_unpack
	; r0 = run_count
	SUB	r7, r0, #0x21		; r7 is >= 0x1000 if full_run
	CMP	r0, r9			; if (run_count > nsbs)
	MOVGT	r0, r9			;    run_count = nsbs
	SUB	r9, r9, r0		; nsbs -= run_count
	MLA	r8, r6, r0, r8		; r8 = npartial+=run_count*flag
odpsfu_lp2
	LDRB	r1, [r10]
	SUBS	r0, r0, #1
	BIC	r1, r1, #CODED_PARTIALLY|CODED_FULLY
	ORR	r1, r1, r6, LSL #CODED_PARTIALLY_SHIFT
	STRB	r1, [r10], #1
	BGT	odpsfu_lp2

	; r0 = flag
	RSB	r0, r6, #1		; r0 = flag = !flag
	CMP	r9, #0
	BGT	odpsfu_lp
odpsfu_end
	MOV	r0, r8				; return npartial
	LDMFD	r13!,{r5-r11,PC}

oc_dec_coded_sb_flags_unpack
	; r0 = dec
	STMFD	r13!,{r5-r11,r14}

	LDR	r10,[r0, #DEC_STATE_SB_FLAGS]	; r10= sb_flags
	LDR	r9, [r0, #DEC_STATE_NSBS]	; r9 = nsbs
	LDR	r5, =DEC_OPB
	MOV	r7, #0x1000			; r7 is >=0x1000 if full_run
	ADD	r9, r10, r9			; r9 = sb_flags_end
	LDRB	r1, [r10], #1
	ADD	r11,r0, r5			; r11= dec->opb
odcsfu_lp
	TST	r1, #CODED_PARTIALLY	; while ((sbflags++)->coded_part)
	LDRNEB	r1, [r10], #1
	BNE	odcsfu_lp
	SUB	r10,r10,#1		; sb_flags--
odcsfu_lp2
	CMP	r7, #0x1000		; if (full_run) (i.e. if >= 0)
	MOVGE	r0, r11
	BLGE	oc_pack_read1		; r0 = oc_pack_read1
	MOV	r6, r0			; r6 = flag
	MOV	r0, r11
	BL	oc_sb_run_unpack
	; r0 = run_count
	LDRB	r1, [r10]
	SUB	r7, r0, #0x21		; r7 is >= 0x1000 if full_run
odcsfu_lp3
	TST	r1, #CODED_PARTIALLY
	BNE	odcsfu_end_lp3
	SUBS	r0, r0, #1
	BLT	odcsfu_break
	ORR	r1, r1, r6, LSL #CODED_FULLY_SHIFT
odcsfu_end_lp3
	STRB	r1, [r10],#1
	CMP	r10,r9
	LDRNEB	r1, [r10]
	BNE	odcsfu_lp3
odcsfu_break
	; r0 = flag
	RSB	r0, r6, #1		; r0 = flag = !flag
	CMP	r10,r9
	BNE	odcsfu_lp2

	LDMFD	r13!,{r5-r11,PC}

oc_dec_coded_flags_unpack
	; r0 = dec
	STMFD	r13!,{r4-r11,r14}

	MOV	r4, r0				; r4 = dec
	BL	oc_dec_partial_sb_flags_unpack	; r0 = npartial=oc_dec_par...
	LDR	r3, [r4, #DEC_STATE_NSBS]	; r3 = nsbs
	LDR	r5, =DEC_OPB
	MOV	r7, r0				; r7 = npartial
	CMP	r7, r3				; if (npartial < nsbs)
	MOVLT	r0, r4
	ADD	r5, r4, r5			; r5 = &dec->opb
	BLLT	oc_dec_coded_sb_flags_unpack	;  dec_cdd_sb_flags_unpk(dec)
	CMP	r7, #0				; if (npartial>0)
	MOVLE	r0, #1
	MOVGT	r0, r5
	BLGT	oc_pack_read1		;    flag=!oc_pack_read1(opb)
	EOR	r9, r0, #1			; else flag = 0
	STMFD	r13!,{r4,r5}

	LDR	r7, [r4, #DEC_STATE_CODED_FRAGIS];r7 = coded_fragis
	LDR	r12,[r4, #DEC_STATE_NFRAGS]	; r12= nfrags
	LDR	r6, [r4, #DEC_STATE_SB_MAPS]	; r6 = sb_maps
	LDR	r2, [r4, #DEC_STATE_SB_FLAGS]	; r2 = sb_flags
	LDR	r10,[r4, #DEC_STATE_FRAGS]	; r10= frags
	MOV	r8, #0				; r8 = run_count=0
	ADD	r12,r7, r12,LSL #2		; r12= uncoded_fragis
	MOV	r1, #3				; r1 = pli=3
	ADD	r14, r4, #DEC_STATE_FPLANES+FPLANE_NSBS	;r14= nsbs_ptr
odcfu_lp
	LDR	r3, [r14], #FPLANE_SIZE	; r3 = nsbs
	MOV	r11,#0			; r11= ncoded_fragis
odcfu_lp8
	; r0 = nsbs_ptr					r9 = flag
	; r1 = 			r5 =			r10= frags
	; r2 = sb_flags		r6 = sb_maps		r11= ncoded_fragis
	; r3 = nsbs		r7 = coded_fragis	r12= uncoded_fragis
	; r4 =			r8 = run_count		r14= nsbs_ptr
	; r1 = fragis=sb_maps
	LDRB	r4, [r2], #1		; r4 = flags =*sb_flags++
	AND	r5, r4, #CODED_FULLY|CODED_PARTIALLY
	MOV	r4, r4, LSL #4
	ORR	r4, r4, #0x8
 [ CODED_FULLY < CODED_PARTIALLY
	CMP	r5, #CODED_FULLY
	BGT	odcfu_partially_coded
 |
	CMP	r5, #CODED_PARTIALLY
	BEQ	odcfu_partially_coded
 ]
	BLT	odcfu_uncoded
	; Coded Fully case				r9 = flag
	; r1 = 			r5 =			r10= frags
	; r2 = sb_flags		r6 = sb_maps/fragip	r11= ncoded_fragis
	; r3 = nsbs		r7 = coded_fragis	r12= uncoded_fragis
	; r4 = flags/counter	r8 = run_count		r14= nsbs_ptr
odcfu_lp2
	TST	r4, #1<<(4+VALUE_BIT_SHIFT)
	ADDEQ	r6, r6, #16
	BEQ	odcfu_skip
	SUB	r4, r4, #4<<28
odcfu_lp3
	LDR	r5, [r6], #4		; r5 = fragi=*fragip++
	; Stall (2 on Xscale)
	CMP	r5, #0			; if(fragi>=0)
	LDRGE	r0, [r10,r5, LSL #2]	;
	STRGE	r5, [r7], #4		;   *coded_fragis++=fragi
	ADDGE	r11,r11,#1		;   ncoded_fragis++;
	ORRGE	r0, r0, #FRAGMENT_CODED
	STRGE	r0, [r10,r5, LSL #2]	;   frags[fragi].coded=1
	ADDS	r4, r4, #1<<28
	BLT	odcfu_lp3
odcfu_skip
	MOVS	r4, r4, LSR #1
	BCC	odcfu_lp2

	B	odcfu_common
odcfu_uncoded
	; Uncoded case					r9 = flag
	; r1 = 			r5 =			r10= frags
	; r2 = sb_flags		r6 = sb_maps/fragip	r11= ncoded_fragis
	; r3 = nsbs		r7 = coded_fragis	r12= uncoded_fragis
	; r4 = flags/counter	r8 = run_count		r14= nsbs_ptr
odcfu_lp4
	TST	r4, #1<<(4+VALUE_BIT_SHIFT)
	ADDEQ	r6, r6, #16
	BEQ	odcfu_skip2
	SUB	r4, r4, #4<<28
odcfu_lp5
	LDR	r5, [r6], #4		; r5 = fragi=*fragip++
	; Stall (2 on Xscale)
	CMP	r5, #0			; if(fragi>=0)
	LDRGE	r0, [r10,r5, LSL #2]	;
	STRGE	r5, [r12,#-4]!		;   *--uncoded_fragis=fragi
	; Stall (on Xscale)
	BICGE	r0, r0, #FRAGMENT_CODED
	STRGE	r0, [r10,r5, LSL #2]	;   frags[fragi].coded=1
	ADDS	r4, r4, #1<<28
	BLT	odcfu_lp5
odcfu_skip2
	MOVS	r4, r4, LSR #1
	BCC	odcfu_lp4

	B	odcfu_common
odcfu_partially_coded
	; Partially coded case				r9 = flag
	; r1 = 			r5 = scratch		r10= frags
	; r2 = sb_flags		r6 = sb_maps/fragip	r11= ncoded_fragis
	; r3 = nsbs		r7 = coded_fragis	r12= uncoded_fragis
	; r4 = flags/counter	r8 = run_count		r14= nsbs_ptr
odcfu_lp6
	TST	r4, #1<<(4+VALUE_BIT_SHIFT)
	ADDEQ	r6, r6, #16
	BEQ	odcfu_skip3
	SUB	r4, r4, #4<<28
odcfu_lp7
	LDR	r5, [r6], #4		; r5 = fragi=*fragip++
	; Stall (2 on Xscale)
	CMP	r5, #0			; if(fragi>=0)
	BLT	odcfu_skip4
	SUBS	r8, r8, #1		;   if (--run_count < 0)
	BGE	odcfu_skip5
	LDR	r0, [r13,#4]		;     r0 = &dec->opb
	STMFD	r13!,{r1-r3,r12,r14}
	BL	oc_block_run_unpack	;     run_count=oc_block_run_unpack
	MOV	r8, r0
	LDMFD	r13!,{r1-r3,r12,r14}
	EOR	r9, r9, #1		;     flag=!flag
odcfu_skip5
	LDR	r0, [r10,r5, LSL #2]	;
	CMP	r9, #0			; if (flag)
	STRNE	r5, [r7], #4		;   *coded_fragis++=fragi
	ADDNE	r11,r11,#1		;   ncoded_fragis++
	STREQ	r5, [r12,#-4]!		; else *--uncoded_fragis=fragi
	ORRNE	r0, r0, #FRAGMENT_CODED
	BICEQ	r0, r0, #FRAGMENT_CODED
	STR	r0, [r10,r5, LSL #2]	;   frags[fragi].coded=flag
odcfu_skip4
	ADDS	r4, r4, #1<<28
	BLT	odcfu_lp7
odcfu_skip3
	MOVS	r4, r4, LSR #1
	BCC	odcfu_lp6

odcfu_common
	; r0 = 						r9 = flag
	; r1 = 			r5 = 			r10= nsbs_ptr
	; r2 = sb_flags		r6 = sb_maps/fragip	r11= ncoded_fragis
	; r3 = nsbs		r7 = coded_fragis	r12= uncoded_fragis
	; r4 = flags/counter	r8 = run_count		r14= frags
	SUBS	r3, r3, #1		; nsbs--
	BGT	odcfu_lp8

	LDR	r4, [r13]
	; r0 = 						r9 = flag
	; r1 = pli		r5 =			r10= nsbs_ptr
	; r2 = sb_flags		r6 = fragip		r11= ncoded_fragis
	; r3 = nsbs		r7 = coded_fragis	r12= uncoded_fragis
	; r4 = dec		r8 = run_count		r14= frags
	; _dec->state.ncoded_fragis[pli]=ncoded_fragis
	SUBS	r1, r1, #1
	ADD	r5, r4, #DEC_STATE_NCODED_FRAGIS+2*4
	STR	r11,[r5, -r1, LSL #2]
	BGT	odcfu_lp

	; dec->state.ntotal_coded_fragis=coded_fragis-dec->state.coded_fragis
	LDR	r0, [r4, #DEC_STATE_CODED_FRAGIS]
	ADD	r13,r13,#8
	SUB	r0, r7, r0
	MOV	r0, r0, LSR #2
	STR	r0, [r4, #DEC_STATE_NTOTAL_CODED_FRAGIS]

	LDMFD	r13!,{r4-r11,PC}

	END
