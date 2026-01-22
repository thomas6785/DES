;********************************************************************
; Example program for Analog Devices EVAL-ADuC841 board.
; Based on example code from Analog Devices, 
; author        : ADI - Apps            www.analog.com/MicroConverter
; Date          : October 2003
;
; File          : blink.asm
;
; Hardware      : ADuC841 with clock frequency 11.0592 MHz
;
; Description   : Blinks LED on port 3 pin 4 continuously.
;                 400 ms period @ 50% duty cycle.
;
;********************************************************************

$NOMOD51			; for Keil uVision - do not pre-define 8051 SFRs
$INCLUDE (MOD841)	; load this definition file instead

LED			EQU	P0		; P3.4 is red LED on eval board
SWITCHES 	EQU P2

;____________________________________________________________________
		; MAIN PROGRAM
CSEG		; working in code segment - program memory

		ORG	0000h			; starting at address 0
BLINK:	
		MOV A, SWITCHES ; Copy the value from the switches
		ANL A, #11011111b; Or mask to exclude bit 5
		MOV LED, A; Pass on to the LEDs
		
		
		MOV	A, #090	; set delay length for 200 ms
		CALL	DELAY   	; call software delay routine
		MOV LED, #11011111b		; Turn off the LED to start
		MOV A, #030; Load in 
		CALL	DELAY   	; call software delay routine
		
		JMP	BLINK   	; repeat indefinately

;____________________________________________________________________
		; SUBROUTINES
DELAY:	; delay for time A x 10 ms.  A is not changed. 
		MOV	R5, A		; set number of repetitions for outer loop
DLY2:	MOV	R6, #144		; middle loop repeats 144 times         
DLY1:	MOV	R7, #255		; inner loop repeats 255 times      
		DJNZ	R7, $		; inner loop 255 x 3 cycles = 765 cycles            
		DJNZ	R6, DLY1		; + 5 to reload, x 144 = 110880 cycles
		DJNZ	R5, DLY2		; + 5 to reload = 110885 cycles = 10.0264 ms
		RET				; return from subroutine

;____________________________________________________________________

END
