; Demo Program - using timer interrupts.
; Written for ADuC841 evaluation board, with UCD extras.
; Generates a square wave on P3.6.

; Include the ADuC841 SFR definitions
$NOMOD51
$INCLUDE (MOD841)

SOUND		EQU		P3.6		; P3.6 will drive a transducer
PULSE		EQU		P3.4		; P3.4 will drive a "heartbeat" or "pulse" LED
SWITCHES	EQU 	P2			; P2 are the eight input switches
DISPLAY		EQU		P0			; there are LED's for displaying info to the user on P0

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
LOOP:
		MOV A, SWITCHES						; Load the switch info into R3 (TODO MASK THIS ; TODO INDICATOR LIGHT NEEDED)
		ANL A, #07h							; Mask all but the last three bits
		MOV R3, A							; Store for future use
		
		MOV DPTR, #RELOAD_VALUE_TABLE_L		; Prepare the data pointer with the table of lower bytes for the reload values
		MOVC A, @A+DPTR						; Read from the table into A
		MOV RCAP2L, A						; Write A to the timer 2 config register (lower byte)

		MOV A, R3							; Retrieve which index in the lookup table we want
		MOV DPTR, #RELOAD_VALUE_TABLE_H		; Prepare the data pointer with the table of upper bytes for the reload values
		MOVC A, @A+DPTR						; Read from the table into A
		MOV RCAP2H, A						; Write A to the timer 2 config register (upper byte)

		MOV A, R3							; Retrieve which index in the lookup table we want
		MOV DPTR, #ONE_HOT_DECODE_BYTE		; Prepare the data pointer with the table of one-hot coded bytes (0-7)
		MOVC A, @A+DPTR						; Read from the table into A
		MOV DISPLAY, A						; Write A to the display LED's to show which frequency is selected

		JMP		LOOP						; repeat main loop (checking switches)

;--------------------------------------------------------------------
; Lookup table for the reload value for timer 2 to give various frequencies
;--------------------------------------------------------------------
RELOAD_VALUE_TABLE_H:	DB	0DFh,	0E5h,	0EAh,	0EFh,	0F5h,	0F7h,	0FAh,	0FBh;
RELOAD_VALUE_TABLE_L:	DB	03Ch,	0FFh,	022h,	09Eh,	011h,	0CFh,	088h,	0E8h;
ONE_HOT_DECODE_BYTE:	DB	01h,	02h,	04h,	08h,	10h,	20h,	40h,	80h;
;							E5		G#5		B5		E6		B6		E7		B7		E8
; Values are calculated such that (11.0592 MHz) / (2*(65536 - RELOAD_VALUE)) is the target frequency
; Output wave is square so expect to see harmonics

;--------------------------------------------------------------------
; Interrupt service routines
;--------------------------------------------------------------------

ISR_USER_BUTTON: ; user pressing the button can disable the 'heartbeat' LED
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