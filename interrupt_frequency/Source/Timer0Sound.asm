; Demo Program - using timer interrupts.
; Written for ADuC841 evaluation board, with UCD extras.
; Generates a square wave on P3.6.

; Include the ADuC841 SFR definitions
$NOMOD51
$INCLUDE (MOD841)

SOUND	EQU  	P3.6		; P3.6 will drive a transducer
PULSE	EQU		P3.4		; P3.4 will drive a "heartbeat" or "pulse" LED

CSEG
;--------------------------------------------------------------------
; Define ISR jumps
;--------------------------------------------------------------------
		ORG		0000h		; set origin at start of code segment
		JMP		MAIN		; jump to start of main program

		ORG		0003h		; External interrupt 0
		JMP		ISR_USER_BUTTON

		ORG		001Bh		; Timer 1 overflow interrupt address
		JMP		ISR_TIMER_1
		
		ORG		002Bh		; Timer 2 overflow interrupt address
		JMP		ISR_TIMER_2

		ORG		0060h		; set origin above interrupt addresses

;--------------------------------------------------------------------
; Setup and main loop
;--------------------------------------------------------------------
MAIN:
		MOV		T2CON, #04h	; Configure Timer 2 (flags off, not using serial ports, ignore external input, run in timer mode with autoreload)
		MOV		TCON,  #01000001b	; Configure timer 1 (simple 16-bit timer) and timer 0 (disabled) and INT0 as an external posedge-triggered interrupt
		MOV		TMOD,  #00010000b	; Configure timer 1 to mode 1 (16-bit timer) and disable timer 0
		MOV		IE,    #10101001b	; Disable interrupts except Timer 1, Timer 2, and external interrupt 0
		MOV		IP,    #00100000b	; Give timer 2 interupt priority
		; Fall through to main LOOP (indefinitely loop)
LOOP:	NOP					; this does nothing, uses 1 clock cycle
		; TODO set timer 2 reload value according to switches. for now it defaults to 0
		JMP		LOOP		; repeat - waiting for interrupt

;--------------------------------------------------------------------
; Interrupt service routines
;--------------------------------------------------------------------

ISR_USER_BUTTON: ; user pressing the button can disable the 'heartbeat' LED
		; Prevent another identical interrupt from coming due to bouncing (disable this external interrupt)
		CLR		EX0			; temporarily disable this interrupt to avoid physical bouncing causing multiple ISR's
		
		; Toggle timer 1 (the behaviour of this button)
		CPL		TR1			;
		
		; It would be best if the light immediately came on/off so the user knows the button worked
		MOV		C, TR1		; Copy TR1 to C as an intermediate storage
		MOV		PULSE, C	; Copy C to PULSE
		
		; If the light is being switched back on, best if it doesn't immediately toggle off, so reset the counter to 0
		MOV		R1, #0		; reset the counter that toggles the light (just in case it was at 255, we don't want the light to immediately switch off leaving the user wondering if they pressed the button properly)
		
		; Before re-enabling the IRQ, call a delay to avoid bounces (other interrupts can still happen)
		MOV		R5, #5		; load 5 as a duration argument for "DELAY" to give a ~50 ms delay. Can be interrupted but this is fine
		CALL	DELAY		; call a ~20 ms delay to avoid another ISR due to bouncing
		
		SETB	EX0			; re-enable this interrupt for future button pressed
		CLR		IE0			; ignore any subsequent interrupts that took place while this one was being handled (caused by bounces)
		
		; Return
		RETI				; Return from the ISR

ISR_TIMER_1: ; increment a counter in R1 - every time that counter overflows, toggle the pulse LED
		CLR		TF1				; Clear the overflow flag that caused the ISR
		XCH		A, R1			; Copy R1 into accumulator (we will undo at the end of the ISR)
		INC		A				; Increment the accumulator
		JNZ		END_ISR_TIMER_1	; If A is not zero, exit the ISR. We only toggle the LED each time A overflow
		CPL		PULSE			; Flip the 'pulse' LED IF the counter has gotten to 0 (so every 256 times this ISR is called)
END_ISR_TIMER_1: ; fall through if no jump took place
		XCH		A, R1		; Return A and R1 to their original positions
		RETI				; Return from ISR

ISR_TIMER_2: ; timer 2 is the main timer used for frequency synthesis, toggle the SOUND pin each time it overflows
		CLR		TF2			; Clear the overflow flag that caused the ISR
		CPL 	SOUND		; Flip 'sound' bit on timer 2 overflow
		RETI				; Return from ISR

;--------------------------------------------------------------------
; SUBROUTINES
;--------------------------------------------------------------------

DELAY:	; delay for time R5 x ~10 ms. Adapted from blink.asm. R5, R6, R7 are changed
		MOV		R6, #144		; middle loop repeats 144 times
DLY1:	MOV		R7, #255		; inner loop repeats 255 times
		DJNZ	R7, $			; inner loop 255 x 3 cycles = 765 cycles
		DJNZ	R6, DLY1		; + 5 to reload, x 144 = 110880 cycles
		DJNZ	R5, DELAY		; + 5 to reload = 110885 cycles = 10.0264 ms
		RET						; return from subroutine

END