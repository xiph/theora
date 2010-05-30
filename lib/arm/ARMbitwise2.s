; Theorarm library
; Copyright (C) 2009 Robin Watts for Pinknoise Productions Ltd

	AREA	|.text|, CODE, READONLY

	EXPORT	theorapackB_lookARM
	EXPORT	theorapackB_readARM
	EXPORT	theorapackB_read1ARM

theorapackB_lookARM
	; r0 = oggpack_buffer *b
	; r1 = int             bits
	STMFD	r13!,{r9,r10,r11,r14}
	LDMIA	r0,{r2,r3,r12}
					; r2 = bitsLeftInSegment
					; r3 = ptr
					; r12= bitsLeftInWord
	SUBS	r2, r2, r1		; bitsLeftinSegment -= bits
	BLT	lookBE_slow		; Not enough bits in this segment for
					; this request. Do it slowly.
	LDR	r10,[r3]		; r10= ptr[0]
	RSB	r14,r12,#32		; r14= 32-bitsLeftInWord

	MVN	r9, #0xFF00
	EOR	r11,r10,r10,ROR #16
	AND	r11,r9, r11,LSR #8
	EOR	r10,r11,r10,ROR #8	; r10= REV(ptr[0])

	CMP	r12,r1			; if (bitsLeftInWord < bits)
	LDRLT	r11,[r3, #4]!		; r11= ptr[1]
	MOV	r10,r10,LSL r14		; r10= REV(ptr[0])<<(32-bitsLeftInWd)
	RSB	r1, r1, #32

	EOR	r0, r11,r11,ROR #16
	AND	r0, r9, r0, LSR #8
	EOR	r11,r0, r11,ROR #8	; r11 = REV(ptr[1])

	ORRLT	r10,r10,r11,LSR r12	; r10= Next 32 bits.
	MOV	r0, r10,LSR r1
	LDMFD	r13!,{r9,r10,r11,PC}

lookBE_slow
	STMFD	r13!,{r5,r6}
	ADDS	r10,r2, r1		; r10= bitsLeftInSegment + bits (i.e.
					; the initial value of bitsLeftInSeg)
	; r10 = bitsLeftInSegment (initial)
	; r12 = bitsLeftInWord
	RSB	r14,r12,#32		; r14= 32-bitsLeftInWord
	MOV	r5, r10			; r5 = bitsLeftInSegment (initial)
	BLT	lookBE_overrun
	BEQ	lookBE_next_segment	; r10= r12 = 0, if we branch
	CMP	r12,r10			; If bitsLeftInWord < bitsLeftInSeg
					; there must be more in the next word
	LDR	r10,[r3], #4		; r10= ptr[0]
	LDRLT	r6, [r3]		; r6 = ptr[1]

	MVN	r9, #0xFF00
	EOR	r11,r10,r10,ROR #16
	AND	r11,r9, r11,LSR #8
	EOR	r10,r11,r10,ROR #8	; r10= REV(ptr[0])
	MOV	r10,r10,LSL r14		; r10= first bitsLeftInWord bits

	EOR	r11,r6, r6, ROR #16
	AND	r11,r9, r11,LSR #8
	EOR	r6, r11,r6, ROR #8	; r6 = REV(ptr[1])
	ORRLT	r10,r10,r6, LSR r12	; r10= first bitsLeftInSeg bits+crap

	RSB	r11,r5, #32
	MOV	r10,r10,LSR r11		; r10= first r5 bits
	; Load the next segments data
lookBE_next_segment
	; At this point, r10 contains the first r5 bits of the result
	LDR	r11,[r0, #12]		; r11= head = b->head
	; Stall
	; Stall
lookBE_next_segment_2
	LDR	r11,[r11,#12]		; r11= head = head->next
	; Stall
	; Stall
	CMP	r11,#0
	BEQ	lookBE_out_of_data
	LDMIA	r11,{r6,r12,r14}	; r6 = buffer
					; r12= begin
					; r14= length
	LDR	r6, [r6]		; r6 = buffer->data
	CMP	r14,#0
	BEQ	lookBE_next_segment_2
	ADD	r6, r6, r12		; r6 = buffer->data+begin
lookBE_slow_loop
	LDRB	r12,[r6], #1		; r12= *buffer
	SUBS	r14,r14,#1		; r14= length
	ADD	r5, r5, #8
	ORR	r10,r12,r10,LSL #8	; r10= first r5+8 bits
	BLE	lookBE_really_slow
	SUBS	r9, r5, r1
	BLT	lookBE_slow_loop
	MOV	r0, r10,LSR r9
	LDMFD	r13!,{r5,r6,r9,r10,r11,PC}

lookBE_really_slow
	SUBS	r9, r5, r1
	BLT	lookBE_next_segment_2
	MOV	r0, r10,LSR r9
	LDMFD	r13!,{r5,r6,r9,r10,r11,PC}

lookBE_out_of_data
	; r10 holds the first r5 bits
	SUB	r9, r1, r5
	MOV	r0, r10,LSL r9			; return what we have
	LDMFD	r13!,{r5,r6,r9,r10,r11,PC}

lookBE_overrun
	; We had overrun when we started, so we need to skip -r10 bits.
	LDR	r11,[r0,#12]		; r11 = head = b->head
	; stall
	; stall
lookBE_overrun_next_segment
	LDR	r11,[r11,#12]		; r11 = head->next
	; stall
	; stall
	CMP	r11,#0
	BEQ	lookBE_out_of_data
	LDMIA	r11,{r6,r7,r14}		; r6 = buffer
					; r7 = begin
					; r14= length
	LDR	r6, [r6]		; r6 = buffer->data
	; stall
	; stall
	ADD	r6, r6, r7		; r6 = buffer->data+begin
	MOV	r14,r14,LSL #3		; r14= length in bits
	ADDS	r14,r14,r10		; r14= length in bits-bits to skip
	MOVLE	r10,r14
	BLE	lookBE_overrun_next_segment
	RSB	r10,r10,#0		; r10= bits to skip
	ADD	r6, r1, r10,LSR #3	; r6 = pointer to data
	MOV	r10,#0
	B	lookBE_slow_loop

theorapackB_readARM
	; r0 = oggpack_buffer *b
	; r1 = int             bits
	STMFD	r13!,{r9,r10,r11,r14}
	LDMIA	r0,{r2,r3,r12}
					; r2 = bitsLeftInSegment
					; r3 = ptr
					; r12= bitsLeftInWord
	SUBS	r2, r2, r1		; bitsLeftinSegment -= bits
	BLT	readBE_slow		; Not enough bits in this segment for
					; this request. Do it slowly.
	LDR	r10,[r3], #4		; r10= ptr[0]

	MVN	r9, #&FF00
	RSB	r14,r12,#32		; r14= 32-bitsLeftInWord
	EOR	r11,r10,r10,ROR #16
	AND	r11,r9, r11,LSR #8
	EOR	r10,r11,r10,ROR #8	; r10= REV(ptr[0])

	MOV	r10,r10,LSL r14		; r10= REV(ptr[0])<<(32-bitsLeftInWd)
	SUBS	r14,r12,r1		; r12= bitsLeftInWord -= bits
	LDRLT	r11,[r3]		; r11= ptr[1]
	STR	r2, [r0]
	STRLE	r3, [r0, #4]
	EOR	r3, r11,r11,ROR #16
	AND	r3, r9, r3, LSR #8
	EOR	r11,r3, r11,ROR #8	; r11= REV(ptr[1])
	ORR	r10,r10,r11,LSR r12	; r10= Next 32 bits.
	ADDLE	r14,r14,#32		; r12= bitsLeftInWord += 32
	STR	r14,[r0, #8]
	RSB	r1, r1, #32
	MOV	r0, r10,LSR r1
	LDMFD	r13!,{r9,r10,r11,PC}

readBE_slow
	STMFD	r13!,{r5,r6}
	ADDS	r10,r2, r1		; r10= bitsLeftInSegment + bits (i.e.
					; the initial value of bitsLeftInSeg)
	; r10 = bitsLeftInSegment (initial)
	; r12 = bitsLeftInWord
	RSB	r14,r12,#32		; r14= 32-bitsLeftInWord
	MOV	r5, r10			; r5 = bitsLeftInSegment (initial)
	BLT	readBE_overrun
	BEQ	readBE_next_segment	; r10= r12 = 0, if we branch
	CMP	r12,r10			; If bitsLeftInWord < bitsLeftInSeg
					; there must be more in the next word
	LDR	r10,[r3],#4		; r10= ptr[0]
	LDRLT	r6, [r3]		; r6 = ptr[1]

	MVN	r9, #0xFF00
	EOR	r11,r10,r10,ROR #16
	AND	r11,r9, r11,LSR #8
	EOR	r10,r11,r10,ROR #8	; r10= REV(ptr[0])
	MOV	r10,r10,LSL r14		; r10= first bitsLeftInWord bits

	EOR	r11,r6, r6, ROR #16
	AND	r11,r9, r11,LSR #8
	EOR	r6, r11,r6, ROR #8	; r6 = REV(ptr[1])
	ORRLT	r10,r10,r6, LSR r12	; r10= first bitsLeftInSeg bits+crap

	RSB	r11,r5, #32
	MOV	r10,r10,LSR r11		; r10= first r5 bits
	; Load the next segments data
readBE_next_segment
	; At this point, r10 contains the first r5 bits of the result
	LDR	r11,[r0, #12]		; r11= head = b->head
	; Stall
readBE_next_segment_2
	; r11 = head
	LDR	r6, [r0, #20]		; r6 = count
	LDR	r12,[r11,#8]		; r12= length
	LDR	r11,[r11,#12]		; r11= head = head->next
	; Stall
	ADD	r6, r6, r12		; count += length
	CMP	r11,#0
	BEQ	readBE_out_of_data
	STR	r11,[r0, #12]
	STR	r6, [r0, #20]		; b->count = count
	LDMIA	r11,{r6,r12,r14}	; r6 = buffer
					; r12= begin
					; r14= length
	LDR	r6, [r6]		; r6 = buffer->data
	CMP	r14,#0
	BEQ	readBE_next_segment_2
	ADD	r6, r6, r12		; r6 = buffer->data+begin
readBE_slow_loop
	LDRB	r12,[r6], #1		; r12= *buffer
	SUBS	r14,r14,#1		; r14= length
	ADD	r5, r5, #8
	ORR	r10,r12,r10,LSL #8	; r10= first r5+8 bits
	BLE	readBE_really_slow
	SUBS	r9, r5, r1
	BLT	readBE_slow_loop
readBE_end
	; Store back the new position
	; r2 = -number of bits to go from this segment
	; r6 = ptr
	; r14= bytesLeftInSegment
	; r11= New head value
	LDMIA	r11,{r3,r6,r14}		; r3 = buffer
					; r6 = begin
					; r14= length
	LDR	r3,[r3]			; r3 = buffer->data
	ADD	r1,r2,r14,LSL #3	; r1 = bitsLeftInSegment
	; stall
	ADD	r6,r3,r6		; r6 = pointer
	AND	r3,r6,#3		; r3 = bytes used in first word
	RSB	r3,r2,r3,LSL #3		; r3 = bits used in first word
	BIC	r2,r6,#3		; r2 = word ptr
	RSBS	r3,r3,#32		; r3 = bitsLeftInWord
	ADDLE	r3,r3,#32
	ADDLE	r2,r2,#4
	STMIA	r0,{r1,r2,r3}

	MOV	r0, r10,LSR r9
	LDMFD	r13!,{r5,r6,r9,r10,r11,PC}


readBE_really_slow
	SUBS	r9, r5, r1
	BGE	readBE_end
	LDR	r14,[r11,#8]		; r14= length of segment just done
	; stall
	; stall
	ADD	r2, r2, r14,LSL #3	; r2 = -bits to use from next seg
	B	readBE_next_segment_2

readBE_out_of_data
	; Store back the new position
	; r2 = -number of bits to go from this segment
	; r6 = ptr
	; r14= bytesLeftInSegment
	; RJW: This may be overkill - we leave the buffer empty, with -1
	; bits left in it. We might get away with just storing the
	; bitsLeftInSegment as -1.
	LDR	r11,[r0, #12]		; r11=head
	; Stall (2 on Xscale)
	LDMIA	r11,{r3,r6,r14}		; r3 = buffer
					; r6 = begin
					; r14= length
	LDR	r3, [r3]		; r3 = buffer->data
	MVN	r1, #0			; r1 = -1 = bitsLeftInSegment
	; Stall on Xscale
	ADD	r6, r3, r6		; r6 = pointer
	ADD	r6, r6, r14
	AND	r3, r6, #3		; r3 = bytes used in first word
	MOV	r3, r3, LSL #3		; r3 = bits used in first word
	BIC	r2, r6, #3		; r2 = word ptr
	RSBS	r3, r3, #32		; r3 = bitsLeftInWord
	STMIA	r0,{r1,r2,r3}
	MVN	r0, #0			; return -1
	LDMFD	r13!,{r5,r6,r9,r10,r11,PC}

readBE_overrun
	; We had overrun when we started, so we need to skip -r10 bits.
	LDR	r11,[r0,#12]		; r11 = head = b->head
	; stall
	; stall
readBE_overrun_next_segment
	LDR	r11,[r11,#12]		; r11 = head->next
	; stall
	; stall
	CMP	r11,#0
	BEQ	readBE_out_of_data
	LDMIA	r11,{r6,r7,r14}		; r6 = buffer
					; r7 = begin
					; r14= length
	LDR	r6, [r6]		; r6 = buffer->data
	; stall
	; stall
	ADD	r6, r6, r7		; r6 = buffer->data+begin
	MOV	r14,r14,LSL #3		; r14= length in bits
	ADDS	r14,r14,r10		; r14= length in bits-bits to skip
	MOVLE	r10,r14
	BLE	readBE_overrun_next_segment
	RSB	r10,r10,#0		; r10= bits to skip
	ADD	r6, r10,r10,LSR #3	; r6 = pointer to data
	MOV	r10,#0
	B	readBE_slow_loop

theorapackB_read1ARM
	; r0 = oggpack_buffer *b
	STMFD	r13!,{r10,r11,r14}
	LDMIA	r0,{r2,r3,r12}
					; r2 = bitsLeftInSegment
					; r3 = ptr
					; r12= bitsLeftInWord
	SUBS	r2, r2, #1		; bitsLeftinSegment -= bits
	BLT	read1BE_slow		; Not enough bits in this segment for
					; this request. Do it slowly.
	LDR	r10,[r3], #4		; r10= ptr[0]

	MVN	r1, #&FF00
	SUB	r14,r12,#1		; r14= 32-bitsLeftInWord
	EOR	r11,r10,r10,ROR #16
	AND	r11,r1, r11,LSR #8
	EOR	r10,r11,r10,ROR #8	; r10= REV(ptr[0])

	MOV	r10,r10,LSR r14		; r10= REV(ptr[0])>>(bitsLeftInWrd-1)
	SUBS	r14,r12,#1		; r12= bitsLeftInWord -= bits
	STR	r2, [r0]
	STRLE	r3, [r0, #4]
	ADDLE	r14,r14,#32		; r12= bitsLeftInWord += 32
	STR	r14,[r0, #8]
	AND	r0, r10,#1
	LDMFD	r13!,{r10,r11,PC}

read1BE_slow
	; r12 = bitsLeftInWord
	RSB	r14,r12,#32		; r14= 32-bitsLeftInWord
	; Load the next segments data
read1BE_next_segment
	LDR	r11,[r0, #12]		; r11= head = b->head
	; Stall
read1BE_next_segment_2
	; r11 = head
	LDR	r10,[r0, #20]		; r10= count
	LDR	r12,[r11,#8]		; r12= length
	LDR	r11,[r11,#12]		; r11= head = head->next
	; Stall
	ADD	r10,r10,r12		; count += length
	CMP	r11,#0
	BEQ	read1BE_out_of_data
	STR	r11,[r0, #12]
	STR	r10,[r0, #20]		; b->count = count
	LDMIA	r11,{r10,r12,r14}	; r10= buffer
					; r12= begin
					; r14= length
	LDR	r10,[r10]		; r10= buffer->data
	CMP	r14,#0
	BEQ	read1BE_next_segment_2
	LDRB	r10,[r10,r12]		; r10= *(buffer->data+begin)
read1BE_end
	; Store back the new position
	; r2 = -number of bits to go from this segment
	; r6 = ptr
	; r14= bytesLeftInSegment
	; r11= New head value
	LDMIA	r11,{r3,r12,r14}	; r3 = buffer
					; r12= begin
					; r14= length
	LDR	r3,[r3]			; r3 = buffer->data
	ADD	r1,r2,r14,LSL #3	; r1 = bitsLeftInSegment
	; stall
	ADD	r12,r3,r12		; r12= pointer
	AND	r3, r12,#3		; r3 = bytes used in first word
	RSB	r3, r2, r3,LSL #3	; r3 = bits used in first word
	BIC	r2, r12,#3		; r2 = word ptr
	RSBS	r3, r3, #32		; r3 = bitsLeftInWord
	ADDLE	r3, r3, #32
	ADDLE	r2, r2, #4
	STMIA	r0,{r1,r2,r3}

	MOV	r0, r10,LSR #7
	LDMFD	r13!,{r10,r11,PC}

read1BE_out_of_data
	; Store back the new position
	; r2 = -number of bits to go from this segment
	; r6 = ptr
	; r14= bytesLeftInSegment
	; RJW: This may be overkill - we leave the buffer empty, with -1
	; bits left in it. We might get away with just storing the
	; bitsLeftInSegment as -1.
	LDR	r11,[r0, #12]		; r11=head
	; Stall (2 on Xscale)
	LDMIA	r11,{r3,r12,r14}	; r3 = buffer
					; r12= begin
					; r14= length
	LDR	r3, [r3]		; r3 = buffer->data
	MVN	r1, #0			; r1 = -1 = bitsLeftInSegment
	; Stall on Xscale
	ADD	r12,r3, r12		; r12= pointer
	ADD	r12,r12,r14
	AND	r3, r12,#3		; r3 = bytes used in first word
	MOV	r3, r3, LSL #3		; r3 = bits used in first word
	BIC	r2, r12,#3		; r2 = word ptr
	RSBS	r3, r3, #32		; r3 = bitsLeftInWord
	STMIA	r0,{r1,r2,r3}
	MOV	r0, #0			; return 0
	LDMFD	r13!,{r10,r11,PC}

	END
