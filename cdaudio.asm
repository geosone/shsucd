; CDAUDIO.COM - switch the CD audio of SHSUCDX (/A:ON or /A:AUTO) at run
; time between digital (VSBHDA) and analog (the drive plays).
;
;   CDAUDIO          show the mode
;   CDAUDIO ON       PLAY goes to the digital audio player (VSBHDA)
;   CDAUDIO OFF      PLAY goes to the drive (analog output)
;   CDAUDIO AUTO     digital while a player is registered, else analog
;
; nasm -f bin -o CDAUDIO.COM cdaudio.asm

	cpu	8086
	org	100h

start:	mov	si, 81h
	call	skip
	mov	dl, 0FFh		; no argument: status only
	cmp	al, 0Dh
	je	query
	and	al, 0DFh
	mov	dl, 2
	cmp	al, 'A'
	je	query
	cmp	al, 'O'
	jne	usage
	mov	al, [si+1]
	and	al, 0DFh
	mov	dl, 1
	cmp	al, 'N'
	je	query
	mov	dl, 0
	cmp	al, 'F'
	jne	usage

query:	mov	cx, 1			; status
	cmp	dl, 0FFh
	je	.call
	mov	cx, 2			; set mode DL
.call:	mov	ax, 15FFh
	mov	bx, 0CDDAh
	int	2Fh
	cmp	ax, 0CDDAh
	jne	missing

	push	cx
	mov	dx, msg_mode
	call	print
	mov	dx, msg_off
	cmp	bl, 1
	jb	.m
	mov	dx, msg_on
	je	.m
	mov	dx, msg_auto
.m:	call	print
	mov	dx, msg_player
	call	print
	mov	dx, msg_no
	test	bh, bh
	jz	.p
	mov	dx, msg_yes
.p:	call	print
	mov	dx, msg_now
	call	print
	pop	cx
	mov	dx, msg_analog
	test	cl, cl
	jz	.n
	mov	dx, msg_digital
.n:	call	print
	mov	ax, 4C00h
	int	21h

missing:
	mov	dx, msg_missing
	call	print
	mov	ax, 4C02h
	int	21h

usage:	mov	dx, msg_usage
	call	print
	mov	ax, 4C01h
	int	21h

skip:	lodsb
	cmp	al, ' '
	je	skip
	cmp	al, 9
	je	skip
	dec	si
	ret

print:	mov	ah, 9
	int	21h
	ret

msg_mode	db	'CD audio mode: $'
msg_off		db	'OFF$'
msg_on		db	'ON$'
msg_auto	db	'AUTO$'
msg_player	db	13, 10, 'Digital audio player: $'
msg_yes		db	'registered$'
msg_no		db	'none$'
msg_now		db	13, 10, 'PLAY goes to: $'
msg_analog	db	'the drive (analog)', 13, 10, '$'
msg_digital	db	'the player (digital)', 13, 10, '$'
msg_missing	db	'SHSUCDX with CD audio (/A:ON or /A:AUTO) not loaded', 13, 10, '$'
msg_usage	db	'CDAUDIO [ON | OFF | AUTO]', 13, 10
		db	'  ON    digital CD audio through VSBHDA', 13, 10
		db	'  OFF   analog CD audio from the drive', 13, 10
		db	'  AUTO  digital while VSBHDA is loaded, else analog', 13, 10, '$'
