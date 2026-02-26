;bin2bcd:
;	; Assume R1 points to some scratch memory
;	MOV @R1,R2		; Assume R2 is the LSB of
;	INC R1			; Move to the next location in memory
;	MOV @R2,R3
;	MOV A,@R1
;	MOV A,@R1		; Assume R1 contains the address of the first scratch byte
