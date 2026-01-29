;********************************************************************
; Example program for Analog Devices EVAL-ADuC841 board.
; Based on example code from Analog Devices, 
; author        : ADI - Apps            www.analog.com/MicroConverter
; Date          : October 2003
;
; File          : software_frequency.asm
;
; Hardware      : ADuC841 with clock frequency 11.0592 MHz
;
; Description   : 
;
;********************************************************************

$NOMOD51			; for Keil uVision - do not pre-define 8051 SFRs
$INCLUDE (MOD841)	; load this definition file instead

SOUND		EQU	P3.6		; P3.4 is red LED on eval board 
;____________________________________________________________________
		; MAIN PROGRAM
CSEG		; working in code segment - program memory

		ORG	0000h			; starting at address 0
		
MAIN:

		CALL	DELAY   	; call software delay routine
		
		CPL SOUND;

;____________________________________________________________________
		; SUBROUTINES
DELAY:	; delay for time A x 10 ms.  A is not changed. 
		MOV	R5, #8		; set number of repetitions for outer loop      
DLY1:	MOV	R7, #255		; inner loop repeats 255 times      
		DJNZ	R7, $		; inner loop 255 x 3 cycles = 765 cycles            
		DJNZ	R5, DLY1		; + 5 to reload, x 144 = 110880 cycles
		RET				; return from subroutine
	

;____________________________________________________________________

END
