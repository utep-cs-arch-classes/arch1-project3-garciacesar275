	.arch msp430
	.p2align 1,0
	.text

check_play_sound:
	mov &ml0, r12
	and &ml1, r12
	call #isCollision
	mov r12, r6
	cmp #1, r6
	jz play1_buzzer
	mov &ml0, r12
	and &ml2, r12
	call #isCollision
	mov r12, r6
	cmp #1, r6
	jz play2_buzzer

play1_buzzer:
	mov #2000, r12
	call #buzzer_play

play2_buzzer:
	mov #2000, r12
	call #buzzer_play
