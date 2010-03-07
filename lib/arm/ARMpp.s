; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

	AREA	|.text|, CODE, READONLY

	GET	ARMoptions.s

	EXPORT	oc_filter_hedge
	EXPORT	oc_filter_vedge
	EXPORT	oc_dering_block

 [ 0;ARM_HAS_NEON
	; Unfinished
oc_filter_hedge
	; r0 =       uint8_t *rdst
	; r1 =       int      dst_ystride
	; r2 = const uint8_t *rsrc
	; r3 =       int      src_ystride
	; <> =       int      qstep
	; <> =       int      flimit
	; <> =       int     *variance0
	; <> =       int     *variance1
	STMFD	r13!,{r4-r11,r14}

	; variance0sum is 8*255 at most.

	; r14 will hold variance0sum in the bottom 12 bits, variance1sum
	; in the next 12 bits, and loop count in the top 8 bits.
	MOV	r14,#4<<28		; r14= variancesums
ofhs_lp
	VLD1.64	{D0, D1 }, [r2], r3	; Q0 = s[0]
	VLD1.64	{D2, D3 }, [r2], r3	; Q1 = s[1]
	VLD1.64	{D4, D5 }, [r2], r3	; Q2 = s[2]
	VLD1.64	{D6, D7 }, [r2], r3	; Q3 = s[3]
	VLD1.64	{D8, D9 }, [r2], r3	; Q4 = s[4]
	VLD1.64	{D10,D11}, [r2], r3	; Q5 = s[5]
	VLD1.64	{D12,D13}, [r2], r3	; Q6 = s[6]
	VLD1.64	{D14,D15}, [r2], r3	; Q7 = s[7]
	VLD1.64	{D16,D17}, [r2], r3	; Q8 = s[8]
	VLD1.64	{D18,D19}, [r2], r3	; Q9 = s[9]
	VABDL.U8	Q10,D2, D0	; Q10= abs(s[1]-s[0]) (bottoms)
	VABDL.U8	Q11,D3, D1	; Q11= abs(s[1]-s[0]) (tops)
	VABDL.U8	Q14,D12,D10	; Q14= abs(s[6]-s[5]) (bottoms)
	VABDL.U8	Q15,D13,D11	; Q15= abs(s[6]-s[5]) (tops)
	VABAL.U8	Q10,D4, D2	; Q10+=abs(s[2]-s[1]) (bottoms)
	VABAL.U8	Q11,D5, D3	; Q11+=abs(s[2]-s[1]) (tops)
	VABDL.U8	Q14,D14,D12	; Q14= abs(s[7]-s[6]) (bottoms)
	VABDL.U8	Q15,D15,D13	; Q15= abs(s[7]-s[6]) (tops)
	VABAL.U8	Q10,D6, D4	; Q10+=abs(s[3]-s[2]) (bottoms)
	VABAL.U8	Q11,D7, D5	; Q11+=abs(s[3]-s[2]) (tops)
	VABDL.U8	Q14,D16,D14	; Q14= abs(s[8]-s[7]) (bottoms)
	VABDL.U8	Q15,D17,D15	; Q15= abs(s[8]-s[7]) (tops)
	VABAL.U8	Q10,D8, D6	; Q10+=abs(s[4]-s[3]) (bottoms)
	VABAL.U8	Q11,D9, D7	; Q11+=abs(s[4]-s[3]) (tops)
	VABDL.U8	Q14,D18,D16	; Q14= abs(s[8]-s[7]) (bottoms)
	VABDL.U8	Q15,D19,D17	; Q15= abs(s[8]-s[7]) (tops)
	VABDL.U8	Q12,D10,D8	; Q12= abs(s[5]-s[4]) (bottoms)
	VABDL.U8	Q13,D11,D9	; Q13= abs(s[5]-s[4]) (tops)

	; Q10/11=num0 Q12/13=abs(s[5]-s[4]) Q14/15=sum1
	MOV	r9, #0
	USADA8	r14,r9,r4,r14		; r14=variance0sum+=sum of sum0's
	USAD8	r9, r9,r6		; r9 =sum of sum1's
	ADD	r14,r14,r9, LSL #12	; r14=variance1sum+=sum of sum1's

	LDR	r7, [r13,#4*9]	; r9 = qstep
	LDR	r10,[r13,#4*10]	; r10= flimit
	MOV	r11,#0
	ORR	r7, r7, r7, LSL #8
	ORR	r7, r7, r7, LSL #16
	ORR	r10,r10,r10,LSL #8
	ORR	r10,r10,r10,LSL #16
	USUB8	r9, r5, r7	; Set GE bit if (abs(r[4]-r[5])>=qstep)
	SEL	r9, r11,r9	; bytes are NE if (abs(r[4]-r[5])<qstep)
	USUB8	r7, r4, r10	; Set GE bit if (sum0>=flimit)
	SEL	r9, r11,r9	; bytes are NE if (sum0<flimit) && above cond
	USUB8	r7, r6, r10	; Set GE bit if (sum1>=flimit)
	SEL	r9, r11,r9	; bytes are NE if (sum1<flimit) && above cond

	SUB	r2, r2, r3, LSL #3
	SUB	r0, r0, r1, LSL #3

	|
  [ ARMV6 = 1
oc_filter_hedge
	; r0 =       uint8_t *rdst
	; r1 =       int      dst_ystride
	; r2 = const uint8_t *rsrc
	; r3 =       int      src_ystride
	; <> =       int      qstep
	; <> =       int      flimit
	; <> =       int     *variance0
	; <> =       int     *variance1
	STMFD	r13!,{r4-r11,r14}

	; variance0sum is 8*255 at most.

	; r14 will hold variance0sum in the bottom 12 bits, variance1sum
	; in the next 12 bits, and loop count in the top 8 bits.
	MOV	r14,#4<<28		; r14= variancesums
ofhs_lp
	LDR	r4, [r2], r3		; r4 = s[0]
	LDR	r5, [r2], r3		; r5 = s[1]
	LDR	r6, [r2], r3		; r6 = s[2]
	LDR	r7, [r2], r3		; r7 = s[3]
	STR	r5, [r0], r1		; store s[1]
	STR	r6, [r0], r1		; store s[2]
	STR	r7, [r0], r1		; store s[3]
	USUB8	r9, r4, r5
	USUB8	r4, r5, r4
	SEL	r4, r4, r9		; r4 = sum0 = abs(s[0]-s[1])
	USUB8	r9, r5, r6
	USUB8	r5, r6, r5
	SEL	r5, r5, r9		; r5 = abs(s[2]-s[1]) in 4 bytes
	UQADD8	r4, r4, r5		; r4 = sum0 += abs(s[2]-s[1])
	LDR	r5, [r2], r3		; r5 = s[4]
	USUB8	r9, r6, r7
	USUB8	r6, r7, r6
	SEL	r6, r6, r9		; r6 = abs(s[3]-s[2]) in 4 bytes
	UQADD8	r4, r4, r6		; r4 = sum0 += abs(s[3]-s[2])
	LDR	r6, [r2], r3		; r6 = s[5]
	STR	r5, [r0], r1		; store s[4]
	USUB8	r9, r7, r5
	USUB8	r7, r5, r7
	SEL	r7, r7, r9		; r7 = abs(s[4]-s[3]) in 4 bytes
	UQADD8	r4, r4, r7		; r4 = sum0 += abs(s[4]-s[3])
	LDR	r7, [r2], r3		; r6 = s[6]
	STR	r6, [r0], r1		; store s[5]
	USUB8	r9, r5, r6
	USUB8	r5, r6, r5
	SEL	r5, r5, r9		; r5 = abs(s[5]-s[4]) in 4 bytes
	LDR	r10,[r2], r3		; r10= s[7]
	STR	r7, [r0], r1		; store s[6]
	USUB8	r9, r6, r7
	USUB8	r6, r7, r6
	SEL	r6, r6, r9		; r6 = sum1 = abs(s[6]-s[5])
	LDR	r11,[r2], r3		; r11= s[8]
	STR	r10,[r0], r1		; store s[7]
	USUB8	r9, r7, r10
	USUB8	r7, r10,r7
	SEL	r7, r7, r9		; r7 = abs(s[7]-s[6]) in 4 bytes
	UQADD8	r6, r6, r7		; r6 = sum1 += abs(s[7]-s[6])
	LDR	r7, [r2], -r3		; r7 = s[9]
	STR	r11,[r0], r1		; store s[8]
	USUB8	r9, r10,r11
	USUB8	r10,r11,r10
	SEL	r10,r10,r9		; r10= abs(s[8]-s[7]) in 4 bytes
	UQADD8	r6, r6, r10		; r6 = sum1 += abs(s[8]-s[7])
	USUB8	r9, r11,r7
	USUB8	r11,r7, r11
	SEL	r11,r11,r9		; r11= abs(s[9]-s[8]) in 4 bytes
	UQADD8	r6, r6, r11		; r6 = sum1 += abs(s[9]-s[8])

	; r4=sum0 r5=abs(s[5]-s[4]) r6=sum1
	MOV	r9, #0
	USADA8	r14,r9, r4, r14		; r14=variance0sum+=sum of sum0's
	USAD8	r9, r9, r6		; r9 =sum of sum1's
	ADD	r14,r14,r9, LSL #12	; r14=variance1sum+=sum of sum1's

	LDR	r7, [r13,#4*9]	; r9 = qstep
	LDR	r10,[r13,#4*10]	; r10= flimit
	MOV	r11,#0
	ORR	r7, r7, r7, LSL #8
	ORR	r7, r7, r7, LSL #16
	ORR	r10,r10,r10,LSL #8
	ORR	r10,r10,r10,LSL #16
	USUB8	r9, r5, r7	; Set GE bit if (abs(r[4]-r[5])>=qstep)
	SEL	r9, r11,r9	; bytes are NE if (abs(r[4]-r[5])<qstep)
	USUB8	r7, r4, r10	; Set GE bit if (sum0>=flimit)
	SEL	r9, r11,r9	; bytes are NE if (sum0<flimit) && above cond
	USUB8	r7, r6, r10	; Set GE bit if (sum1>=flimit)
	SEL	r9, r11,r9	; bytes are NE if (sum1<flimit) && above cond

	SUB	r2, r2, r3, LSL #3
	SUB	r0, r0, r1, LSL #3

	STMFD	r13!,{r9,r14}
	TST	r9,#0x000000FF
	BLNE	do_hedge
	ADD	r0, r0, #1
	ADD	r2, r2, #1
	TST	r9,#0x0000FF00
	BLNE	do_hedge
	ADD	r0, r0, #1
	ADD	r2, r2, #1
	TST	r9,#0x00FF0000
	BLNE	do_hedge
	ADD	r0, r0, #1
	ADD	r2, r2, #1
	TST	r9,#0xFF000000
	BLNE	do_hedge
	ADD	r0, r0, #1
	ADD	r2, r2, #1
	LDMFD	r13!,{r9,r14}

	SUBS	r14,r14,#4<<28
	BGE	ofhs_lp

	LDR	r4, [r13,#4*(9+2)]	; r4 = variance0
	LDR	r5, [r13,#4*(9+3)]	; r5 = variance1
	MOV	r12,r14,LSL #20		; r12= variance0sum<<20
	LDR	r6, [r4]		; r6 = *variance0
	LDR	r7, [r5]		; r7 = *variance1
	BIC	r14,r14,#0xFF000000
	ADD	r6, r6, r12,LSR #20	; r6 = *variance0 += variance0sum
	ADD	r7, r7, r14,LSR #12	; r7 = *variance1 += variance1sum
	STR	r6, [r4]		; r4 = *variance0
	STR	r7, [r5]		; r5 = *variance1

	LDMFD	r13!,{r4-r11,PC}
do_hedge
	; Do the filter...
	LDRB	r4, [r2], r3	; r4 = r[0]
	LDRB	r5, [r2], r3	; r5 = r[1]
	LDRB	r6, [r2], r3	; r6 = r[2]
	LDRB	r7, [r2], r3	; r7 = r[3]
	LDRB	r8, [r2], r3	; r8 = r[4]
	LDRB	r9, [r2], r3	; r9 = r[5]
	LDRB	r12,[r2], r3	; r12= r[6]
	ADD	r10,r4, r5
	ADD	r10,r10,r6
	ADD	r10,r10,r7
	ADD	r10,r10,r8	; r10= r[0]+r[1]+r[2]+r[3]+r[4]
	ADD	r10,r10,#4	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+4
	ADD	r11,r10,r4,LSL #1;r11= r[0]*3+r[1]+r[2]+r[3]+r[4]+4
	ADD	r11,r11,r5	; r11= r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4
	MOV	r11,r11,ASR #3	; r11= r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r9	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+4
	ADD	r11,r10,r4	; r11= r[0]*2+r[1]+r[2]+r[3]+r[4]+r[5]+4
	ADD	r11,r11,r6	; r11= r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4
	MOV	r11,r11,ASR #3	; r11= r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r12	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+4
	ADD	r11,r10,r7	; r11= r[0]+r[1]+r[2]+r[3]*2+r[4]+r[5]+r[6]+4
	SUB	r10,r10,r4	; r10= r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+4
	LDRB	r4, [r2], r3	; r4 = r[7]
	MOV	r11,r11,ASR #3;r11= r[0]+r[1]+r[2]+r[3]*2+r[4]+r[5]+r[6]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r4	; r10= r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+4
	ADD	r11,r10,r8	; r11= r[1]+r[2]+r[3]+r[4]*2+r[5]+r[6]+r[7]+4
	SUB	r10,r10,r5	; r10= r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+4
	LDRB	r5, [r2], r3	; r5 = r[8]
	MOV	r11,r11,ASR #3;r11= r[1]+r[2]+r[3]+r[4]*2+r[5]+r[6]+r[7]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r5	; r10= r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+4
	ADD	r11,r10,r9	; r11= r[2]+r[3]+r[4]+r[5]*2+r[6]+r[7]+r[8]+4
	SUB	r10,r10,r6	; r10= r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+4
	LDRB	r6, [r2], -r3	; r6 = r[9]
	MOV	r11,r11,ASR #3;r11= r[2]+r[3]+r[4]+r[5]*2+r[6]+r[7]+r[8]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r6	; r10= r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+r[9]+4
	ADD	r11,r10,r12	; r11= r[3]+r[4]+r[5]+r[6]*3+r[7]+r[8]+r[9]+4
	MOV	r11,r11,ASR #3;r11= r[3]+r[4]+r[5]+r[6]*3+r[7]+r[8]+r[9]+4>>3
	STRB	r11,[r0], r1
	SUB	r10,r10,r7	; r10= r[4]+r[5]+r[6]+r[7]+r[8]+r[9]+4
	ADD	r10,r10,r6	; r10= r[4]+r[5]+r[6]+r[7]+r[8]+r[9]*2+4
	ADD	r11,r10,r4	; r11= r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4
	MOV	r11,r11,ASR #3	; r11= r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4>>3
	STRB	r11,[r0], r1
	SUB	r10,r10,r8
	ADD	r10,r10,r6	; r10= r[5]+r[6]+r[7]+r[8]+r[9]*3+4
	ADD	r10,r10,r5	; r10= r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4
	MOV	r10,r10,ASR #3	; r10= r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4>>3
	STRB	r10,[r0], r1
	SUB	r2, r2, r3, LSL #3
	SUB	r0, r0, r1, LSL #3

	LDR	r9,[r13]
	MOV	PC,R14
  |
oc_filter_hedge
	; r0 =       uint8_t *rdst
	; r1 =       int      dst_ystride
	; r2 = const uint8_t *rsrc
	; r3 =       int      src_ystride
	; <> =       int      qstep
	; <> =       int      flimit
	; <> =       int     *variance0
	; <> =       int     *variance1
	STMFD	r13!,{r4-r11,r14}

	; variance0sum is 8*255 at most.

	; r14 will hold variance0sum in the bottom 12 bits, variance1sum
	; in the next 12 bits, and loop count in the top 8 bits.
	MOV	r14,#8<<24	; r14= variancesums = 0 | (bx<<24)
	SUB	r0, r0, #1
	SUB	r2, r2, #1
ofh_lp
	SUBS	r14,r14,#1<<24
	BLT	ofh_end
ofh_lp2
	ADD	r0, r0, #1
	ADD	r2, r2, #1
	LDRB	r4, [r2], r3	; r4 = r[0]
	LDRB	r5, [r2], r3	; r5 = r[1]
	LDRB	r6, [r2], r3	; r6 = r[2]
	LDRB	r7, [r2], r3	; r7 = r[3]
	STRB	r5, [r0], r1	; dst[1]
	STRB	r6, [r0], r1	; dst[2]
	STRB	r7, [r0], r1	; dst[3]
	SUBS	r4, r5, r4	; r4 = r[1]-r[0]
	RSBLT	r4, r4, #0	; r4 = sum0 = abs(r[1]-r[0])
	SUBS	r5, r6, r5	; r5 = r[2]-r[1]
	ADDGE	r4, r4, r5
	SUBLT	r4, r4, r5	; r4 = sum0 += abs(r[2]-r[1])
	LDRB	r5, [r2], r3	; r5 = r[4]
	SUBS	r6, r7, r6	; r6 = r[3]-r[2]
	ADDGE	r4, r4, r6
	SUBLT	r4, r4, r6	; r4 = sum0 += abs(r[3]-r[2])
	LDRB	r6, [r2], r3	; r6 = r[5]
	STRB	r5, [r0], r1	; dst[4]
	SUBS	r7, r5, r7	; r7 = r[4]-r[3]
	ADDGE	r4, r4, r7
	SUBLT	r4, r4, r7	; r4 = sum0 += abs(r[4]-r[3])
	LDRB	r7, [r2], r3	; r7 = r[6]
	STRB	r6, [r0], r1	; dst[5]
	SUBS	r5, r6, r5	; r5 = r[5]-r[4]
	RSBLT	r5, r5, #0	; r5 = abs(r[5]-r[4])
	LDRB	r8, [r2], r3	; r8 = r[7]
	STRB	r7, [r0], r1	; dst[6]
	SUBS	r6, r6, r7	; r6 = r[5]-r[6]
	RSBLT	r6, r6, #0	; r6 = sum1 = abs(r[5]-r[6])
	SUBS	r7, r7, r8	; r7 = r[6]-r[7]
	LDRB	r9, [r2], r3	; r9 = r[8]
	STRB	r8, [r0], r1	; dst[7]
	ADDGE	r6, r6, r7
	SUBLT	r6, r6, r7	; r6 = sum1 += abs(r[6]-r[7])
	SUBS	r8, r8, r9	; r8 = r[7]-r[8]
	LDRB	r7, [r2], -r3	; r[9]
	STRB	r9, [r0], r1	; dst[8]
	SUB	r2, r2, r3, LSL #3
	SUB	r0, r0, r1, LSL #3
	ADDGE	r6, r6, r8
	SUBLT	r6, r6, r8	; r6 = sum1 += abs(r[7]-r[8])
	SUBS	r9, r9, r7	; r9 = r[8]-r[9]
	ADDGE	r6, r6, r9
	SUBLT	r6, r6, r9	; r6 = sum1 += abs(r[8]-r[9])

	CMP	r4, #255
	ADDLT	r14,r14,r4
	ADDGE	r14,r14,#255	; variance0sum += min(255, sum0)

	LDR	r9, [r13,#4*9]	; r9 = qstep
	LDR	r10,[r13,#4*10]	; r10= flimit

	CMP	r6, #255
	ADDLT	r14,r14,r6, LSL #12
	ADDGE	r14,r14,#255<<12	; variance1sum += min(255, sum1)

	CMP	r4, r10		; if (sum0<flimit)
	CMPLT	r6, r10		;  &&(sum1<flimit)
	CMPLT	r5, r9		;  &&(abs(r[5]-r[4])<qstep)
	BGE	ofh_lp

	; Do the filter...
	LDRB	r4, [r2], r3	; r4 = r[0]
	LDRB	r5, [r2], r3	; r5 = r[1]
	LDRB	r6, [r2], r3	; r6 = r[2]
	LDRB	r7, [r2], r3	; r7 = r[3]
	LDRB	r8, [r2], r3	; r8 = r[4]
	LDRB	r9, [r2], r3	; r9 = r[5]
	LDRB	r12,[r2], r3	; r12= r[6]
	ADD	r10,r4, r5
	ADD	r10,r10,r6
	ADD	r10,r10,r7
	ADD	r10,r10,r8	; r10= r[0]+r[1]+r[2]+r[3]+r[4]
	ADD	r10,r10,#4	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+4
	ADD	r11,r10,r4,LSL #1;r11= r[0]*3+r[1]+r[2]+r[3]+r[4]+4
	ADD	r11,r11,r5	; r11= r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4
	MOV	r11,r11,ASR #3	; r11= r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r9	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+4
	ADD	r11,r10,r4	; r11= r[0]*2+r[1]+r[2]+r[3]+r[4]+r[5]+4
	ADD	r11,r11,r6	; r11= r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4
	MOV	r11,r11,ASR #3	; r11= r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r12	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+4
	ADD	r11,r10,r7	; r11= r[0]+r[1]+r[2]+r[3]*2+r[4]+r[5]+r[6]+4
	SUB	r10,r10,r4	; r10= r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+4
	LDRB	r4, [r2], r3	; r4 = r[7]
	MOV	r11,r11,ASR #3;r11= r[0]+r[1]+r[2]+r[3]*2+r[4]+r[5]+r[6]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r4	; r10= r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+4
	ADD	r11,r10,r8	; r11= r[1]+r[2]+r[3]+r[4]*2+r[5]+r[6]+r[7]+4
	SUB	r10,r10,r5	; r10= r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+4
	LDRB	r5, [r2], r3	; r5 = r[8]
	MOV	r11,r11,ASR #3;r11= r[1]+r[2]+r[3]+r[4]*2+r[5]+r[6]+r[7]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r5	; r10= r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+4
	ADD	r11,r10,r9	; r11= r[2]+r[3]+r[4]+r[5]*2+r[6]+r[7]+r[8]+4
	SUB	r10,r10,r6	; r10= r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+4
	LDRB	r6, [r2], -r3	; r6 = r[9]
	MOV	r11,r11,ASR #3;r11= r[2]+r[3]+r[4]+r[5]*2+r[6]+r[7]+r[8]+4>>3
	STRB	r11,[r0], r1
	ADD	r10,r10,r6	; r10= r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+r[9]+4
	ADD	r11,r10,r12	; r11= r[3]+r[4]+r[5]+r[6]*3+r[7]+r[8]+r[9]+4
	MOV	r11,r11,ASR #3;r11= r[3]+r[4]+r[5]+r[6]*3+r[7]+r[8]+r[9]+4>>3
	STRB	r11,[r0], r1
	SUB	r10,r10,r7	; r10= r[4]+r[5]+r[6]+r[7]+r[8]+r[9]+4
	ADD	r10,r10,r6	; r10= r[4]+r[5]+r[6]+r[7]+r[8]+r[9]*2+4
	ADD	r11,r10,r4	; r11= r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4
	MOV	r11,r11,ASR #3	; r11= r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4>>3
	STRB	r11,[r0], r1
	SUB	r10,r10,r8
	ADD	r10,r10,r6	; r10= r[5]+r[6]+r[7]+r[8]+r[9]*3+4
	ADD	r10,r10,r5	; r10= r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4
	MOV	r10,r10,ASR #3	; r10= r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4>>3
	STRB	r10,[r0], r1
	SUB	r2, r2, r3, LSL #3
	SUB	r0, r0, r1, LSL #3

	SUBS	r14,r14,#1<<24
	BGE	ofh_lp2
ofh_end
	LDR	r4, [r13,#4*(9+2)]	; r4 = variance0
	LDR	r5, [r13,#4*(9+3)]	; r5 = variance1
	MOV	r12,r14,LSL #20		; r12= variance0sum<<20
	LDR	r6, [r4]		; r6 = *variance0
	LDR	r7, [r5]		; r7 = *variance1
	BIC	r14,r14,#0xFF000000
	ADD	r6, r6, r12,LSR #20	; r6 = *variance0 += variance0sum
	ADD	r7, r7, r14,LSR #12	; r7 = *variance1 += variance1sum
	STR	r6, [r4]		; r4 = *variance0
	STR	r7, [r5]		; r5 = *variance1

	LDMFD	r13!,{r4-r11,PC}
  ]
 ]
oc_filter_vedge
	; r0 =       uint8_t *rdst
	; r1 =       int      dst_ystride
	; r2 =       int      qstep
	; r3 =       int      flimit
	; <> =       int     *variances
	STMFD	r13!,{r4-r11,r14}

	; variance0sum is 8*255 at most.

	; r14 will hold variance0sum in the bottom 12 bits, variance1sum
	; in the next 12 bits, and loop count in the top 8 bits.
	MOV	r14,#8<<24	; r14= variancesums = 0 | (bx<<24)
	SUB	r0, r0, r1
ofv_lp
	SUBS	r14,r14,#1<<24
	BLT	ofv_end
ofv_lp2
	ADD	r0, r0, r1
	LDRB	r4, [r0, #-1]	; r4 = r[0]
	LDRB	r5, [r0]	; r5 = r[1]
	LDRB	r6, [r0, #1]	; r6 = r[2]
	LDRB	r7, [r0, #2]	; r7 = r[3]
	SUBS	r4, r5, r4	; r4 = r[1]-r[0]
	RSBLT	r4, r4, #0	; r4 = sum0 = abs(r[1]-r[0])
	SUBS	r5, r6, r5	; r5 = r[2]-r[1]
	ADDGE	r4, r4, r5
	SUBLT	r4, r4, r5	; r4 = sum0 += abs(r[2]-r[1])
	LDRB	r5, [r0, #3]	; r5 = r[4]
	SUBS	r6, r7, r6	; r6 = r[3]-r[2]
	ADDGE	r4, r4, r6
	SUBLT	r4, r4, r6	; r4 = sum0 += abs(r[3]-r[2])
	LDRB	r6, [r0, #4]	; r6 = r[5]
	SUBS	r7, r5, r7	; r7 = r[4]-r[3]
	ADDGE	r4, r4, r7
	SUBLT	r4, r4, r7	; r4 = sum0 += abs(r[4]-r[3])
	LDRB	r7, [r0, #5]	; r7 = r[6]
	SUBS	r5, r6, r5	; r5 = r[5]-r[4]
	RSBLT	r5, r5, #0	; r5 = abs(r[5]-r[4])
	LDRB	r8, [r0, #6]	; r8 = r[7]
	SUBS	r6, r6, r7	; r6 = r[5]-r[6]
	RSBLT	r6, r6, #0	; r6 = sum1 = abs(r[5]-r[6])
	SUBS	r7, r7, r8	; r7 = r[6]-r[7]
	LDRB	r9, [r0, #7]	; r9 = r[8]
	ADDGE	r6, r6, r7
	SUBLT	r6, r6, r7	; r6 = sum1 += abs(r[6]-r[7])
	SUBS	r8, r8, r9	; r8 = r[7]-r[8]
	LDRB	r7, [r0, #8]	; r[9]
	ADDGE	r6, r6, r8
	SUBLT	r6, r6, r8	; r6 = sum1 += abs(r[7]-r[8])
	SUBS	r9, r9, r7	; r9 = r[8]-r[9]
	ADDGE	r6, r6, r9
	SUBLT	r6, r6, r9	; r6 = sum1 += abs(r[8]-r[9])

	CMP	r4, #255
	ADDLT	r14,r14,r4
	ADDGE	r14,r14,#255	; variance0sum += min(255, sum0)

	CMP	r6, #255
	ADDLT	r14,r14,r6, LSL #12
	ADDGE	r14,r14,#255<<12	; variance1sum += min(255, sum1)

	CMP	r4, r3		; if (sum0<flimit)
	CMPLT	r6, r3		;  &&(sum1<flimit)
	CMPLT	r5, r2		;  &&(abs(r[5]-r[4])<qstep)
	BGE	ofv_lp

	; Do the filter...
	LDRB	r4, [r0, #-1]	; r4 = r[0]
	LDRB	r5, [r0]	; r5 = r[1]
	LDRB	r6, [r0, #1]	; r6 = r[2]
	LDRB	r7, [r0, #2]	; r7 = r[3]
	LDRB	r8, [r0, #3]	; r8 = r[4]
	LDRB	r9, [r0, #4]	; r9 = r[5]
	LDRB	r12,[r0, #5]	; r12= r[6]
	ADD	r10,r4, r5
	ADD	r10,r10,r6
	ADD	r10,r10,r7
	ADD	r10,r10,r8	; r10= r[0]+r[1]+r[2]+r[3]+r[4]
	ADD	r10,r10,#4	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+4
	ADD	r11,r10,r4,LSL #1;r11= r[0]*3+r[1]+r[2]+r[3]+r[4]+4
	ADD	r11,r11,r5	; r11= r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4
	MOV	r11,r11,ASR #3	; r11= r[0]*3+r[1]*2+r[2]+r[3]+r[4]+4>>3
	STRB	r11,[r0]
	ADD	r10,r10,r9	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+4
	ADD	r11,r10,r4	; r11= r[0]*2+r[1]+r[2]+r[3]+r[4]+r[5]+4
	ADD	r11,r11,r6	; r11= r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4
	MOV	r11,r11,ASR #3	; r11= r[0]*2+r[1]+r[2]*2+r[3]+r[4]+r[5]+4>>3
	STRB	r11,[r0, #1]
	ADD	r10,r10,r12	; r10= r[0]+r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+4
	ADD	r11,r10,r7	; r11= r[0]+r[1]+r[2]+r[3]*2+r[4]+r[5]+r[6]+4
	SUB	r10,r10,r4	; r10= r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+4
	LDRB	r4, [r0, #6]	; r4 = r[7]
	MOV	r11,r11,ASR #3;r11= r[0]+r[1]+r[2]+r[3]*2+r[4]+r[5]+r[6]+4>>3
	STRB	r11,[r0, #2]
	ADD	r10,r10,r4	; r10= r[1]+r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+4
	ADD	r11,r10,r8	; r11= r[1]+r[2]+r[3]+r[4]*2+r[5]+r[6]+r[7]+4
	SUB	r10,r10,r5	; r10= r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+4
	LDRB	r5, [r0, #7]	; r5 = r[8]
	MOV	r11,r11,ASR #3;r11= r[1]+r[2]+r[3]+r[4]*2+r[5]+r[6]+r[7]+4>>3
	STRB	r11,[r0, #3]
	ADD	r10,r10,r5	; r10= r[2]+r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+4
	ADD	r11,r10,r9	; r11= r[2]+r[3]+r[4]+r[5]*2+r[6]+r[7]+r[8]+4
	SUB	r10,r10,r6	; r10= r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+4
	LDRB	r6, [r0, #8]	; r6 = r[9]
	MOV	r11,r11,ASR #3;r11= r[2]+r[3]+r[4]+r[5]*2+r[6]+r[7]+r[8]+4>>3
	STRB	r11,[r0, #4]
	ADD	r10,r10,r6	; r10= r[3]+r[4]+r[5]+r[6]+r[7]+r[8]+r[9]+4
	ADD	r11,r10,r12	; r11= r[3]+r[4]+r[5]+r[6]*3+r[7]+r[8]+r[9]+4
	MOV	r11,r11,ASR #3;r11= r[3]+r[4]+r[5]+r[6]*3+r[7]+r[8]+r[9]+4>>3
	STRB	r11,[r0, #5]
	SUB	r10,r10,r7	; r10= r[4]+r[5]+r[6]+r[7]+r[8]+r[9]+4
	ADD	r10,r10,r6	; r10= r[4]+r[5]+r[6]+r[7]+r[8]+r[9]*2+4
	ADD	r11,r10,r4	; r11= r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4
	MOV	r11,r11,ASR #3	; r11= r[4]+r[5]+r[6]+r[7]*2+r[8]+r[9]*2+4>>3
	STRB	r11,[r0, #6]
	SUB	r10,r10,r8
	ADD	r10,r10,r6	; r10= r[5]+r[6]+r[7]+r[8]+r[9]*3+4
	ADD	r10,r10,r5	; r10= r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4
	MOV	r10,r10,ASR #3	; r10= r[5]+r[6]+r[7]+r[8]*2+r[9]*3+4>>3
	STRB	r10,[r0, #7]

	SUBS	r14,r14,#1<<24
	BGE	ofv_lp2
ofv_end
	LDR	r4, [r13,#4*9]		; r4 = variances
	MOV	r12,r14,LSL #20		; r12= variance0sum<<20
	; Stall on Xscale
	LDR	r6, [r4]		; r6 = variances[0]
	LDR	r7, [r4, #4]		; r7 = variances[0]
	BIC	r14,r14,#0xFF000000
	ADD	r6, r6, r12,LSR #20	; r6 = variances[0] += variance0sum
	ADD	r7, r7, r14,LSR #12	; r7 = variances[1] += variance1sum
	STR	r6, [r4]		; r4 = variances[0]
	STR	r7, [r4, #4]		; r5 = variances[1]

	LDMFD	r13!,{r4-r11,PC}

oc_dering_block
	; r0 = unsigned char *dst
	; r1 = int            ystride
	; r2 = int            b
	; r3 = int            dc_scale
	; r4 = int            sharp_mod
	; r5 = int            strong
	STMFD	r13!,{r4-r11,r14}

	LDR	r4, [r13,#4*9]		; r4 = sharp_mod
	LDR	r5, [r13,#4*10]		; r5 = strong

	SUB	r13,r13,#72*2		; make space for vmod and hmod

	ADD	r7, r3, r3, LSL #1	; r7 = 3*_dc_scale
	MOV	r6, #24
	ADD	r6, r6, r5, LSL #3	; r6 = MOD_MAX[strong]
	CMP	r7, r6
	MOVLT	r6, r7			; r6 = mod_hi=MIN(3*_dc_scale,r6)
	ADD	r3, r3, #96		; r3 = _dc_scale += 96
	RSB	r5, r5, #1		; r5 = strong=MOD_SHIFT[strong]
	EOR	r2, r2, #15		; Reverse the sense of the bits

	MOV	r7, r0			; r7 = src = dst
	MOV	r8, r0			; r8 = psrc = src
	TST	r2, #4
	SUBNE	r8, r8, r1		; r8 = psrc = src-(ystride&-!!(b&4))
	MOV	r9, r13			; r9 = vmod
	MOV	r14,#8			; r14= by=8
odb_lp1
	MOV	r12,#8			; r12= bx=8
odb_lp2
	LDRB	r10,[r7], #1		; r10= *src++
	LDRB	r11,[r8], #1		; r11= *psrc++
	; Stall (2 on Xscale)
	SUBS	r10,r10,r11		; r10= *src++ - *psrc++
	RSBLT	r10,r10,#0		; r10= abs(*src++ - *psrc++)
	SUBS	r10,r3, r10,LSL r5	; r10= mod = dc_scale-(r10)<<strong
	MOVLT	r10,r4			; if (mod<0) r10= mod = sharp_mod
	BLT	odb_sharp1		; else ...
	SUBS	r10,r10,#64		; r10 = mod-64
	MOVLT	r10,#0
	CMP	r10,r6
	MOVGT	r10,r6			; r10= OC_CLAMPI(0,mod-64,mod_hi)
odb_sharp1
	STRB	r10,[r9], #1		; *pvmod++ = r10
	SUBS	r12,r12,#1
	BGT	odb_lp2
	SUB	r8, r7, #8		; r8 = psrc = src-8
	MOV	r7, r8			; r7 = src= psrc
	TST	r2, #8			; if (b&8)        (reversed earlier!)
	TSTEQ	r14,#0xFE		;          || (by>1)
	ADDNE	r7, r7, r1		; r7 = src= psrc+ystride&-(...)
	SUBS	r14,r14,#1
	BGE	odb_lp1

	MOV	r7, r0			; r7 = src = dst
	MOV	r8, r0			; r8 = psrc = src
	TST	r2, #1
	SUBNE	r8, r8, #1		; r8 = psrc = src-(b&1)
	ADD	r9, r13,#72		; r9 = hmod
	MOV	r14,#8			; r14= bx=8
odb_lp3
	MOV	r12,#8			; r12= by=8
odb_lp4
	LDRB	r10,[r7], r1		; r10= *src		src +=ystride
	LDRB	r11,[r8], r1		; r11= *psrc		psrc+=ystride
	; Stall (2 on Xscale)
	SUBS	r10,r10,r11		; r10= *src - *psrc
	RSBLT	r10,r10,#0		; r10= abs(*src - *psrc)
	SUBS	r10,r3, r10,LSL r5	; r10= mod = dc_scale-(r10)<<strong
	MOVLT	r10,r4			; if (mod<0) r10= mod = sharp_mod
	BLT	odb_sharp2		; else ...
	SUBS	r10,r10,#64		; r10 = mod-64
	MOVLT	r10,#0
	CMP	r10,r6
	MOVGT	r10,r6			; r10= OC_CLAMPI(0,mod-64,mod_hi)
odb_sharp2
	STRB	r10,[r9], #1		; *phmod++ = r10
	SUBS	r12,r12,#1
	BGT	odb_lp4
	SUB	r8, r7, r1, LSL #3	; r8 = psrc = src - (ystride<<3)
	MOV	r7, r8			; r7 = src= psrc
	TST	r2, #2			; if (b&2)        (reversed earlier!)
	TSTEQ	r14,#0xFE		;          || (bx>1)
	ADDNE	r7, r7, #1		; r7 = src= psrc+((b&2)|(bx>1))
	SUBS	r14,r14,#1
	BGE	odb_lp3

	; r0 = src = dst
	; r1 = ystride
	; r2 = b
	ADD	r3, r0, r1		; r3 = nsrc=src+ystride
	MOV	r4, r0			; r4 = psrc=src
	TST	r2, #4
	SUBNE	r4, r4, r1		; r4 = psrc=src-(ystride&-!(b&4))
	MOV	r5, r13			; r5 = pvmod = vmod
	ADD	r6, r13, #72		; r6 = phmod = hmod
	MOV	r14,#7			; r14= by=7
	MOV	r8, #255		; r8 = 255 = magic clipping constant
odb_lp5
	LDRSB	r10,[r6], #8		; r10= w0=*phmod	phmod+=8
	AND	r9, r2, #1		; r9 = (b&1)
	LDRB	r9, [r0, -r9]		; r9 = *(src-(b&1))
	MOV	r11,#64			; r11= d = 64
	LDRSB	r7, [r5], #1		; r7 = w1=*pvmod++
	MLA	r11,r9, r10,r11		; r12= d+=w0* *(src-(b&1))
	LDRB	r9, [r4], #1		; r9 = *psrc++
	RSB	r12,r10,#128		; r12= a = 128-w0
	LDRSB	r10,[r5, #7]		; r10= w2=pvmod[7]
	MLA	r11,r9, r7, r11		; r11= d+=w1 * *psrc++
	LDRB	r9, [r3], #1		; r9 = *nsrc++
	SUB	r12,r12,r7		; r12= a-=w1
	LDRSB	r7, [r6], #8		; r7 = w3=*phmod	phmod+=8
	MLA	r11,r9, r10,r11		; r11= d+=w2 * *nsrc++
	LDRB	r9, [r0, #1]!		; r9 = *++src
	SUB	r12,r12,r10		; r12= a-=w2
	LDRB	r10,[r0, #-1]		; r10= src[-1]
	MLA	r11,r9, r7, r11		; r11= d+=w3 * *++src
	SUB	r12,r12,r7		; r12= a-=w3
	MLA	r11,r10,r12,r11		; r11= a*src[-1]+d
	MOVS	r11,r11,ASR #7
	CMPPL	r8, r11
	EORMI	r11,r8, r11, ASR #32	; r11= a=CLAMP(...)
	STRB	r11,[r0, #-1]		; src[-1]=a
	SUB	r14,r14,#6<<4		; bx=6
odb_lp6
	; r7 = w3
	; r11= a
	MOV	r12,#64
	LDRSB	r10,[r5], #1		; r10= w0= *pvmod++
	LDRB	r9, [r4], #1		; r9 = *psrc++
	MLA	r11,r7, r11,r12		; r11= d = 64+w3*a
	RSB	r12,r7, #128		; r12= a = 128-w3
	LDRSB	r7, [r5, #7]		; r7 = w1= pvmod[7]
	MLA	r11,r9, r10,r11		; r11= d+= w0 * *psrc++
	LDRB	r9, [r3], #1		; r9 = *nsrc++
	SUB	r12,r12,r10		; r12= a -= w0
	LDRSB	r10, [r6], #8		; r10= w3=*phmod	phmod+=8
	MLA	r11,r9, r7, r11		; r11= d+= w1 * *nsrc++
	LDRB	r9, [r0, #1]!		; r9 = *++src
	SUB	r12,r12,r7		; r12= a -= w1
	LDRB	r7, [r0, #-1]		; r7 = src[-1]
	MLA	r11,r9, r10,r11		; r11= d+= w3 * *++src
	SUB	r12,r12,r10		; r12= a -= w3
	MLA	r11,r7, r12,r11		; r11= d+= a * src[-1]
	MOV	r7, r10			; r7 = w3
	MOVS	r11,r11,ASR #7
	CMPPL	r8, r11
	EORMI	r11,r8, r11,ASR #32	; r11= a=CLAMP(...)
	STRB	r11,[r0, #-1]		; src[-1]=a
	ADDS	r14,r14,#1<<4
	BLT	odb_lp6

	; r7 = w3
	; r11= a
	MOV	r12,#64
	LDRSB	r10,[r5], #1		; r10= w0= *pvmod++
	LDRB	r9, [r4], #1		; r9 = *psrc++
	MLA	r11,r7, r11,r12		; r11= d = 64+w3*a
	RSB	r12,r7, #128		; r12= a = 128-w3
	LDRSB	r7, [r5, #7]		; r7 = w1= pvmod[7]
	MLA	r11,r9, r10,r11		; r11= d+= w0 * *psrc++
	LDRB	r9, [r3], #-7		; r9 = *nsrc		nsrc-=7
	SUB	r12,r12,r10		; r12= a -= w0
	LDRSB	r10, [r6], #-63		; r10= w3=*phmod	phmod+=8
	MLA	r11,r9, r7, r11		; r11= d+= w1 * *nsrc
	TST	r2, #2			; if (b&2)==0
	LDREQB	r9, [r0]		;      r9 = *src
	LDRNEB	r9, [r0, #1]		; else r9 = src[1]
	SUB	r12,r12,r7		; r12= a -= w1
	LDRB	r7, [r0]		; r7 = src[0]
	MLA	r11,r9, r10,r11		; r11= d+= w3 * src[(b&2)>>1]
	SUB	r12,r12,r10		; r12= a -= w3
	MLA	r11,r7, r12,r11		; r11= d+= a * src[0]
	MOVS	r11,r11,ASR #7
	CMPPL	r8, r11
	EORMI	r11,r8, r11,ASR #32	; r11= a=CLAMP(...)
	STRB	r11,[r0]		; src[0]=a

	SUB	r4, r0, #7		; r4 = psrc = src-7
	MOV	r0, r3			; r0 = src = nsrc
	TST	r2, #8
	TSTEQ	r14,#0xFE
	ADDNE	r3, r3, r1

	SUBS	r14,r14,#1
	BGE	odb_lp5

	ADD	r13,r13,#72*2

	LDMFD	r13!,{r4-r11,PC}

 [ 0 = 1

;Some idle scribblings about doing hedge using SWAR
	; r10= 00FF00FF
	; r9 = 80008000

	LDR	r4, [r2], r3		; r4 = r[0]
	LDR	r5, [r2], r3		; r5 = r[1]
	LDR	r6, [r2], r3		; r6 = r[2]
	LDR	r7, [r2], r3		; r7 = r[3]
	STR	r5, [r0], r1		; dst[1]
	STR	r6, [r0], r1		; dst[2]
	STR	r7, [r0], r1		; dst[3]
	AND	r4, r4, r10		; r4 = ..aa..AA
	AND	r5, r5, r10		; r5 = ..bb..BB
	AND	r6, r6, r10		; r6 = ..cc..CC
	AND	r7, r7, r10		; r7 = ..dd..DD
	ORR	r4, r4, r9		; r4 = ^.aa^.AA
	SUB	r4, r4, r5		; r4 = r[0]-r[1]
	AND	r12,r4, r9, LSR #1	; r12= sign bits
	SUB	r4, r4, r12,LSR #14
	SUB	r12,r12,r12,LSR #14
	EOR	r4, r4, r12		; r4 = abs(r[0]-r[1])

	ORR	r5, r5, r9		; r5 = ^.bb^.BB
	SUB	r5, r5, r6		; r5 = r[1]-r[2]
	AND	r12,r5, r9, LSR #1	; r12= sign bits
	SUB	r5, r5, r12,LSR #14
	SUB	r12,r12,r12,LSR #14
	EOR	r5, r5, r12		; r5 = abs(r[2]-r[1])
	ADD	r4, r4, r5

	LDR	r5, [r2], r3		; r5 = r[1]
	ORR	r6, r6, r9		; r6 = ^.cc^.CC
	SUB	r6, r6, r7		; r6 = r[2]-r[3]
	AND	r12,r6, r9, LSR #1	; r12= sign bits
	SUB	r6, r6, r12,LSR #14
	SUB	r12,r12,r12,LSR #14
	EOR	r6, r6, r12		; r6 = abs(r[3]-r[2])
	ADD	r4, r4, r6

	AND	r5, r5, r10		; r5 = ..ee..EE
	ORR	r7, r7, r5		; r7 = ^.dd^.DD
	SUB	r7, r7, r8		; r7 = r[3]-r[4]
	AND	r12,r7, r9, LSR #1	; r12= sign bits
	SUB	r7, r7, r12,LSR #14
	SUB	r12,r12,r12,LSR #14
	EOR	r7, r7, r12		; r7 = abs(r[4]-r[3])
	ADD	r4, r4, r7

; Doesn't look like it'll pay off

 ]

 [ 0 = 1

; Some idle scribblings about using ARMV6 SIMD for hedge

	LDR	r4, [r2], r3		; r4 = s[0]
	LDR	r5, [r2], r3		; r5 = s[1]
	LDR	r7, [r2], r3		; r7 = s[2]
	ADD	r0, r0, r1
	SSUB8	r9, r4, r5
	STR	r5, [r0], r1		; store s[1]
	SSUB8	r4, r5, r4
	SEL	r4, r4, r9		; r4 = abs(s[0]-s[1]) in 4 bytes
	AND	r4, r6, r4		; r4 = sum0 (for bytes 0 and 2)
	AND	r8, r6, r4, LSR #8	; r8 = sum0 (for bytes 1 and 3)
	SSUB	r9, r5, r7
	SSUB	r5, r7, r5
	SEL	r5, r5, r9		; r5 = abs(s[2]-s[1]) in 4 bytes
	AND	r9, r6, r5
	ADD	r4, r4, r9		; r4 = sum0 (for bytes 0 and 2)
	AND	r9, r6, r5, LSR #8
	LDR	r5, [r2], r3		; r5 = s[3]
	STR	r7, [r0], r1		; store s[2]
	ADD	r8, r8, r9		; r8 = sum0 (for bytes 1 and 3)
	SSUB8	r9, r7, r5
	SSUB8	r7, r5, r7
	SEL	r7, r7, r9		; r7 = abs(s[3]-s[2]) in 4 bytes
	AND	r9, r6, r7
	ADD	r4, r4, r9		; r4 = sum0 (for bytes 0 and 2)
	AND	r9, r6, r7, LSR #8
	LDR	r7, [r2], r3		; r7 = s[4]
	STR	r5, [r0], r1		; store s[3]
	ADD	r8, r8, r9		; r8 = sum0 (for bytes 1 and 3)
	SSUB8	r9, r5, r7
	SSUB8	r5, r7, r5
	SEL	r5, r5, r7		; r5 = abs(s[4]-s[3]) in 4 bytes
	AND	r9, r6, r5
	ADD	r4, r4, r9		; r4 = sum0 (for bytes 0 and 2)
	AND	r9, r6, r5, LSR #8
	LDR	r5, [r2], r3		; r5 = s[5]
	STR	r7, [r0], r1		; store s[4]
	ADD	r8, r8, r9		; r8 = sum0 (for bytes 1 and 3)
	SSUB8	r9, r7, r5
	SSUB8	r7, r5, r7
	LDR	r10,[r2], r3		; r10= s[6]
	STR	r5, [r0], r1		; store s[5]
	SEL	r7, r7, r9		; r7 = abs(s[5]-s[4]) in 4 bytes
	SSUB8	r9, r10,r5
	SSUB8	r5, r5, r10
	SEL	r5, r5, r9		; r5 = abs(s[6]-s[5]) in 4 bytes
	AND	r9, r6, r5		; r9 = sum1 (for bytes 0 and 3)
	LDR	r11,[r2], r3		; r11= s[7]
	STR	r10,[r0], r1		; store s[6]
	AND	r5, r5, r6		; r5 = sum1 (for bytes 1 and 2)
	SSUB8	r9, r11,r10
	SSUB8	r10,r10,r11
	SEL	r10,r10,r11		; r10= abs(s[7]-s[6]) in 4 bytes
	AND	r12,r6, r10
	ADD	r9, r9,r12		; r9 = sum1 (for bytes 0 and 3)
	AND	r12,r6, r10,LSR #8
	ADD	r5, r5,r12		; r5 = sum1 (for bytes 1 and 2)
 ]

	END
