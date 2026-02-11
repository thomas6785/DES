;********************************************************************
; 900 Hz Frequency Synthesiser
; Hardware      : ADuC841 with clock frequency 11.0592 MHz
;********************************************************************

$NOMOD51			; for Keil uVision - do not pre-define 8051 SFRs
$INCLUDE (MOD841)	; load this definition file instead

SOUND		EQU	P3.6		; P3.6 is transducer on eval board
;____________________________________________________________________
		; MAIN PROGRAM
CSEG		; working in code segment - program memory

		ORG	0000h			; starting at address 0
MAIN:
LOOP:
		CALL  DELAY			; call software delay routine		(3 cycles for jump + see subroutine)
		CPL   SOUND			; flip the output bit 'SOUND'		(2 cycles)
		JMP   LOOP			; return to loop start				(3 cycles)
		; NOTE: next address is 0003h which is reserved for interrupts. If we need more setup instructions, we'll have to put in a jump to MAIN elsewhere
		; Loop is 6144 clocks total giving precisely 900 Hz wave at 11.0592 MHz

;____________________________________________________________________
		; SUBROUTINES
DELAY:	; Delay for using two nested FOR loops
		; Delay is 6139 clocks including CALL and RET
		; R5 and R7 are changed
		MOV	R5, #16			; set outer loop to 16 repetitions	(2 cycles)
DLY1:	MOV	R7, #126		; set inner loop to 126 repetitions	(2 cycles * 16)
		DJNZ	R7, $		; DJNZ inner loop					(3 cycles * 126 * 16)
		DJNZ	R5, DLY1	; DJNZ outer loop					(3 cycles * 16)
		NOP 				; (1 cycle)
		NOP					; Two "NOP" are necessary to get precisely 900 Hz (1 cycle each)
		RET					; return from subroutine (4 cycles)

;____________________________________________________________________

END
