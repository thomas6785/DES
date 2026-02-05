; Demo Program - using timer interrupts.
; Written for ADuC841 evaluation board, with UCD extras.
; Generates a square wave on P3.6.
; Brian Mulkeen, September 2016

; Include the ADuC841 SFR definitions
$NOMOD51
$INCLUDE (MOD841)
	
SOUND	EQU  	P3.6		; P3.6 will drive a transducer

CSEG
		ORG		0000h		; set origin at start of code segment
		JMP		MAIN		; jump to start of main program
		
		ORG		000Bh		; Timer 0 overflow interrupt address
		JMP		TF0ISR		; jump to interrupt service routine

		ORG		0060h		; set origin above interrupt addresses	
MAIN:	
; ------ Setup part - happens once only ----------------------------
		MOV		TMOD, #00h	; Timer 0 as a timer, mode 0, not gated
		MOV		IE, #82h	; enable Timer 0 overflow interrupt
		SETB	TR0			; start Timer 0

; ------ Loop forever (in this example, doing nothing) -------------
LOOP:	NOP					; this does nothing, uses 1 clock cycle
		JMP		LOOP		; repeat - waiting for interrupt

		
; ------ Interrupt service routine ---------------------------------	
TF0ISR:		; Timer 0 overflow interrupt service routine
		CPL		SOUND		; change state of output pin
		RETI				; return from interrupt
; ------------------------------------------------------------------	
		
END